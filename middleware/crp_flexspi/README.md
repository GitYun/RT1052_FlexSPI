### MIMXRT1052 FlexSPI操作Nor Flash的中间件

#### 1. 目录结构

```
.
├── crp_flexspi.h
├── device
│   └── w25qxxxjv
│       ├── w25qxxxjv.c
│       └── w25qxxxjv.h
├── include
│   └── crp_flexspi_private.h
├── README.md
└── src
    └── crp_flexspi_private.c
```

- `crp_flexspi_private.c/.h`是私有代码，应用层不用关心

- `crp_flexspi.h`是提供给应用层一个同一的接口，应用层include到代码中

- device目录用于放具体的Nor Flash芯片的配置与操作代码，目前支持Winbond的W25QxxxJV系列的Nor Flash，仅测试了W25Q256JV；应用层需要include头文件，目前为`225qxxxjv.h`



#### 2. 用法

##### 2.1 初始化

```c
crp_flexspi_cfg_t flexspi_cfg = {
     .enable_non_block_xfer = false,
     .xfer_cplt_callback_info = {
          .pfn_callback = NULL,
          .arg = NULL,
     },
};

crp_flexspi_drv_t* crp_flexspi_handle = crp_flexspi_handle_get(crp_w25qxxxjv_handle_get());

crp_flexspi_handle->funcs->init(crp_flexspi_handle->dev, &flexspi_cfg);
```

首先配置通过`crp_flexspi_drv_t* crp_flexspi_handle_get(void* flexspi_dev)`函数获取一个FlexSPI 操作Nor Flash的驱动句柄；其参数`void* flexspi_dev`则是具体的Nor Flash设备的配置句柄，目前仅仅支持 **W25QXXXJV** 系列芯片，在 **目录结构** 一章中的`device/w25qxxjv/w25qxxxjv.h`中，提供了获取此配置句柄的函数

`void * crp_w25qxxxjv_handle_get(void)`



然后使用FlexSPI驱动句柄，对FlexSPI外设进行初始化`crp_flexspi_handle->funcs->init(crp_flexspi_handle->dev, &flexspi_cfg)`，初始化函数的第二个参数是一个`crp_flexspi_cfg_t`结构体指针，这个结构体参数用于控制FlexSPI操作Nor Flash设备时是否使用中断以及中断回调的信息。



##### 2.2 提供的操作函数

中间件提供的操作函数，可在`crp_flexspi.h`中看到：

```c
typedef struct {
    crp_flexspi_error_t (*init)(void* dev, crp_flexspi_cfg_t* cfg);
    crp_flexspi_error_t (*nor_write_enable)(void* dev, uint32_t addr);
    crp_flexspi_error_t (*nor_wait_bus_busy)(void* dev);
    crp_flexspi_error_t (*nor_enable_quad_mode)(void* dev);
    crp_flexspi_error_t (*nor_read)(void* dev, uint32_t addr, uint32_t* buf, uint32_t length);
    crp_flexspi_error_t (*nor_page_program)(void* dev, uint32_t addr, uint32_t* data, uint32_t length);
    crp_flexspi_error_t (*nor_erase_sector)(void* dev, uint32_t addr);
    crp_flexspi_error_t (*nor_erase_block)(void* dev, uint32_t addr);
    crp_flexspi_error_t (*nor_read_uid)(void* dev, uint8_t* data, uint32_t length);
} crp_flexspi_funcs_t;
```

其调用形式如 **2.1 初始化** 章节中一样：

    `crp_flexspi_handle->funcs->op(crp_flexspi_handle->dev, ...)`

其中，应用层只需要关心以下函数：

- init: 初始化FlexSPI外设

- nor_erase_sector：在写数据到Flash前，先擦除相关的Flash扇区

- nor_erase_block：在写数据到Flash前，先擦除相关的Flash块

- nor_page_program：写数据到Flash，支持跨页写

- nor_read：读Flash数据



##### 2.3 使用示例



```c
#include "crp_flexspi.h"
#include "w25qxxxjv.h"

uint8_t uid[8] = {0}; // 用于保存Flash的UID
uint32_t w_buf[64 + 32] = {0}; // 写数据buffer, 需要4Byte对齐
uint32_t r_buf[64 + 32] = {0}; // 读数据buffer, 需要4Byte对齐

void flash_simple_example(void)
{
    // 获取FlexSPI驱动句柄
    crp_flexspi_drv_t* crp_flexspi_handle = crp_flexspi_handle_get(crp_w25qxxxjv_handle_get());

    // 初始化配置结构体参数，不适用中断方式操作Flash (当前未支持)
    crp_flexspi_cfg_t flexspi_cfg = {
         .enable_non_block_xfer = false,
         .xfer_cplt_callback_info = {
              .pfn_callback = NULL,
              .arg = NULL,
         },
    };

    // 初始化FlexSPI
    crp_flexspi_handle->funcs->init(crp_flexspi_handle->dev, &flexspi_cfg);

    // 读取Flash的UID
    crp_flexspi_handle->funcs->nor_read_uid(crp_flexspi_handle->dev, (uint8_t *)uid, sizeof(uid));

    // 使能Quad SPI模式
    crp_flexspi_handle->funcs->nor_enable_quad_mode(crp_flexspi_handle->dev);
    
    // 删除Flash的第2048个扇区
    crp_flexspi_handle->funcs->nor_erase_sector(crp_flexspi_handle->dev, 2048 * 0x1000u);

    // 填充写buffer数据
    for (int i = 0; i < sizeof(w_buf); ++i)
    {
        w_buf[i] = i + (i > 255 ? 1 : 0);
    }

    // 跨页写数据到Flash, 从Flash的第2048个扇区开始写, W25Q256JV的一页有256字节
    crp_flexspi_handle->funcs->nor_page_program(crp_flexspi_handle->dev, 2048 * 0x1000u, w_buf, sizeof(w_buf));

    // 从Flash读取前，先清D-Cache, 防止数据从Cache中读取
    DCACHE_InvalidateByRange((FlexSPI_AMBA_BASE + 2048 * 0x1000u), sizeof(r_buf));
    
    // 使用AHB方式读，即通过Flash的映射地址直接去读取，FlexSPI_AMBA_BASE是Flash映射的起始地址
    memcpy(read, (void *)(FlexSPI_AMBA_BASE + 2048 * 0x1000u), sizeof(r_buf));
}
```




