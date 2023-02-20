/**
 * @file crp_flexspi_private.c
 * @author yun (renqingyun@crprobot.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "crp_flexspi_private.h"

static crp_flexspi_error_t crp_flexspi_nor_open(void* dev, crp_flexspi_cfg_t* cfg);
static crp_flexspi_error_t crp_flexspi_nor_write_enable(void* dev, uint32_t addr);
static crp_flexspi_error_t crp_flexspi_nor_wait_bus_busy(void* dev);
static crp_flexspi_error_t crp_flexspi_nor_enable_quad_mode(void* dev);
static crp_flexspi_error_t crp_flexspi_nor_read(void* dev, uint32_t addr, uint32_t* buf, uint32_t length);
static crp_flexspi_error_t crp_flexspi_nor_read_by_ahb(void* dev, uint32_t addr, uint32_t* buf, uint32_t length);
static crp_flexspi_error_t crp_flexspi_nor_page_program(void* dev, uint32_t addr, uint32_t* data, uint32_t length);
static crp_flexspi_error_t crp_flexspi_nor_erase_sector(void* dev, uint32_t addr);
static crp_flexspi_error_t crp_flexspi_nor_erase_block(void* dev, uint32_t addr);
static crp_flexspi_error_t crp_flexspi_nor_read_uid(void* dev, uint8_t* data, uint32_t length);

crp_flexspi_funcs_t g_flexspi_funcs = {
    .init = crp_flexspi_nor_open,
    .nor_write_enable = crp_flexspi_nor_write_enable,
    .nor_wait_bus_busy = crp_flexspi_nor_wait_bus_busy,
    .nor_enable_quad_mode = crp_flexspi_nor_enable_quad_mode,
    .nor_read = crp_flexspi_nor_read,
    .nor_read_by_ahb = crp_flexspi_nor_read_by_ahb,
    .nor_page_program = crp_flexspi_nor_page_program,
    .nor_erase_sector = crp_flexspi_nor_erase_sector,
    .nor_erase_block = crp_flexspi_nor_erase_block,
    .nor_read_uid = crp_flexspi_nor_read_uid,
};

crp_flexspi_drv_t* crp_flexspi_handle_get(void* flexspi_dev)
{
    assert(flexspi_dev != NULL);

    crp_flexspi_dev_t* dev = flexspi_dev;

    if (dev == NULL) {
        return NULL;
    }

    dev->drv.funcs = &g_flexspi_funcs;

    return &dev->drv;
}

crp_flexspi_error_t crp_flexspi_lut_get(FLEXSPI_Type* base, uint32_t* lut)
{
    assert(base != NULL);
    assert(lut != NULL);

    uint32_t i = 0;
    volatile uint32_t* lut_base = 0;

    if (base == NULL || lut == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

    while (!FLEXSPI_GetBusIdleStatus(base)) {

    }

    lut_base = &base->LUT[0];

    for (i = 0; i < CRP_LUT_LENGTH; ++i) {
        *lut++ = *lut_base++;
    }

    return CRP_FLEXSPI_ERR_NO;
}

/*************************************************************************************************/
/*************************************** 私有函数 *************************************************/
/*************************************************************************************************/

static inline void flexspi_clock_init(void)
{
#if !(defined(XIP_EXTERNAL_FLASH) && (XIP_EXTERNAL_FLASH == 1))
    /* Disable Flexspi clock gate. */
    CLOCK_DisableClock(kCLOCK_FlexSpi);
    /* Set FLEXSPI_PODF. */
    CLOCK_SetDiv(kCLOCK_FlexspiDiv, 3);
    /* Set Flexspi clock source. */
    CLOCK_SetMux(kCLOCK_FlexspiMux, 2);

    CLOCK_EnableClock(kCLOCK_FlexSpi);
#endif
}

static crp_flexspi_error_t crp_flexspi_nor_open(void* dev, crp_flexspi_cfg_t* cfg)
{
    assert(dev != NULL);
    assert(cfg != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    FLEXSPI_Type* flexspi_base = flexspi_dev->devinfo->init_info->flexspi_base;
    flexspi_config_t flexspi_cfg;

    if (dev == NULL || cfg == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

    flexspi_clock_init();

    FLEXSPI_GetDefaultConfig(&flexspi_cfg);

    flexspi_cfg.ahbConfig.enableAHBPrefetch = true;
    flexspi_cfg.ahbConfig.enableAHBBufferable = true;
    flexspi_cfg.ahbConfig.enableReadAddressOpt = true;
    flexspi_cfg.ahbConfig.enableAHBCachable = true;
    FLEXSPI_Init(flexspi_base, &flexspi_cfg);

    FLEXSPI_SetFlashConfig(flexspi_base, 
        flexspi_dev->devinfo->init_info->flexspi_dev_cfg,
        flexspi_dev->devinfo->init_info->flexspi_port);

    FLEXSPI_UpdateLUT(flexspi_base, 0, *flexspi_dev->devinfo->init_info->crp_lut, CRP_LUT_LENGTH);

    if (cfg->enable_non_block_xfer) {
        FLEXSPI_TransferCreateHandle(flexspi_base, 
            &flexspi_dev->devinfo->xfer_state,
            cfg->xfer_cplt_callback_info.pfn_callback,
            cfg->xfer_cplt_callback_info.arg);
    }

    flexspi_dev->devinfo->non_block_xfer_enable = cfg->enable_non_block_xfer;

    FLEXSPI_SoftwareReset(flexspi_base);

    return CRP_FLEXSPI_ERR_NO;
}

static crp_flexspi_error_t crp_flexspi_nor_write_enable(void* dev, uint32_t addr)
{
    assert(dev != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    FLEXSPI_Type* flexspi_base = flexspi_dev->devinfo->init_info->flexspi_base;
    flexspi_transfer_t xfer;
    status_t status;

    if (dev == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

    xfer.deviceAddress = addr;
    xfer.port = flexspi_dev->devinfo->init_info->flexspi_port;
    xfer.cmdType = kFLEXSPI_Command;
    xfer.SeqNumber = 1;
    xfer.seqIndex = NOR_CMD_LUT_SEQ_IDX_WRITEENABLE;

    status = FLEXSPI_TransferBlocking(flexspi_base, &xfer);

    return (crp_flexspi_error_t)status;
}

static crp_flexspi_error_t crp_flexspi_nor_wait_bus_busy(void* dev)
{
    assert(dev != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    FLEXSPI_Type* flexspi_base = flexspi_dev->devinfo->init_info->flexspi_base;
    flexspi_transfer_t xfer;
    status_t status;
    bool is_busy;
    uint32_t data;

    if (dev == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

//    SCB_CleanInvalidateDCache();

    xfer.deviceAddress = 0,
    xfer.port = flexspi_dev->devinfo->init_info->flexspi_port;
    xfer.cmdType = kFLEXSPI_Read;
    xfer.SeqNumber = 1;
    xfer.seqIndex = flexspi_dev->devinfo->busy_status_cmd_seq_index;
    xfer.data = &data;
    xfer.dataSize = 1;

    do {
        status = FLEXSPI_TransferBlocking(flexspi_base, &xfer);

        if (status != kStatus_Success) {
            return status;
        }

        if (flexspi_dev->devinfo->busy_status_bit_polarity) {
            is_busy = 0 != (data & (1u << flexspi_dev->devinfo->busy_status_bit_offset));
        } else {
            is_busy = 0 == (data & (1u << flexspi_dev->devinfo->busy_status_bit_offset));
        }
    } while (is_busy);

    return status;
}

static crp_flexspi_error_t crp_flexspi_nor_enable_quad_mode(void* dev)
{
    assert(dev != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    FLEXSPI_Type* flexspi_base = flexspi_dev->devinfo->init_info->flexspi_base;
    flexspi_transfer_t xfer;
    status_t status = kStatus_Success;
    uint32_t data = 1u << flexspi_dev->devinfo->quad_mode_bit_offset;

    if (dev == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

    SCB_CleanInvalidateDCache();

    status = crp_flexspi_nor_write_enable(dev, 0);

    if (status != kStatus_Success) {
        return status;
    }

    xfer.deviceAddress = 0,
    xfer.port = flexspi_dev->devinfo->init_info->flexspi_port;
    xfer.cmdType = kFLEXSPI_Write;
    xfer.SeqNumber = 1;
    xfer.seqIndex = flexspi_dev->devinfo->quad_mode_cmd_seq_index;
    xfer.data = &data;
    xfer.dataSize = 1;

    status = FLEXSPI_TransferBlocking(flexspi_base, &xfer);
    if (status != kStatus_Success) {
        return status;
    }

    status = crp_flexspi_nor_wait_bus_busy(dev);

    FLEXSPI_SoftwareReset(flexspi_base);

    return status;
}

static crp_flexspi_error_t crp_flexspi_nor_read(void* dev, uint32_t addr, uint32_t* buf, uint32_t length)
{
    assert(dev != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    FLEXSPI_Type* flexspi_base = flexspi_dev->devinfo->init_info->flexspi_base;
    flexspi_transfer_t xfer;
    status_t status;

    if (dev == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

    SCB_InvalidateDCache_by_Addr((void *)addr, (length + 0x20) & (~0x1Fu));

    status = crp_flexspi_nor_wait_bus_busy(dev);

    if (status != kStatus_Success)
    {
        return status;
    }

    xfer.deviceAddress = addr,
    xfer.port = flexspi_dev->devinfo->init_info->flexspi_port;
    xfer.cmdType = kFLEXSPI_Read;
    xfer.SeqNumber = 1;
    xfer.seqIndex = NOR_CMD_LUT_SEQ_IDX_READ;
    xfer.data = buf;
    xfer.dataSize = length;

    if (flexspi_dev->devinfo->non_block_xfer_enable) {
        status = FLEXSPI_TransferNonBlocking(flexspi_base, &flexspi_dev->devinfo->xfer_state, &xfer);
    } else {
        status = FLEXSPI_TransferBlocking(flexspi_base, &xfer);

        if (status != kStatus_Success) {
            return status;
        }

        status = crp_flexspi_nor_wait_bus_busy(dev);

        FLEXSPI_SoftwareReset(flexspi_base);
    }

    return status;
}

static crp_flexspi_error_t crp_flexspi_nor_read_by_ahb(void* dev, uint32_t addr, uint32_t* buf, uint32_t length)
{
    assert(dev != NULL);
    assert(buf != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    uint32_t flash_size = flexspi_dev->devinfo->init_info->flexspi_dev_cfg->flashSize;

    assert(((addr < FlexSPI_AMBA_BASE) && ((addr + length) < flash_size))
           || ((addr >= FlexSPI_AMBA_BASE) && ((addr + length) < (FlexSPI_AMBA_BASE + flash_size))));

    if (addr < FlexSPI_AMBA_BASE) {
        addr += FlexSPI_AMBA_BASE;
    }

    memcpy(buf, (void *)addr, length);

    return CRP_FLEXSPI_ERR_NO;
}

static crp_flexspi_error_t crp_flexspi_nor_page_program(void* dev, uint32_t addr, uint32_t* data, uint32_t length)
{
    assert(dev != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    FLEXSPI_Type* flexspi_base = flexspi_dev->devinfo->init_info->flexspi_base;
    flexspi_transfer_t xfer = {0};
    uint16_t data_nbr = length;
    status_t status;

    if (dev == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

//    SCB_CleanInvalidateDCache_by_Addr((void *)addr, (length + 0x20) & (~0x1Fu));
    SCB_CleanInvalidateDCache();

    status = crp_flexspi_nor_wait_bus_busy(dev);

    if (status != kStatus_Success) {
        return status;
    }

    {
        uint16_t data_nbr_of_align = flexspi_dev->devinfo->page_size - addr % flexspi_dev->devinfo->page_size;
        if (data_nbr > data_nbr_of_align) {
            data_nbr = data_nbr_of_align;
        }
    }

    xfer.deviceAddress = addr,
    xfer.port = flexspi_dev->devinfo->init_info->flexspi_port;
    xfer.cmdType = kFLEXSPI_Write;
    xfer.SeqNumber = 1;
    xfer.seqIndex = NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM;
    xfer.data = data;
    xfer.dataSize = 0;

    if (flexspi_dev->devinfo->non_block_xfer_enable) {
        xfer.dataSize = length;

        status = crp_flexspi_nor_write_enable(dev, addr);

        if (status != kStatus_Success) {
            return status;
        }

        status = FLEXSPI_TransferNonBlocking(flexspi_base, &flexspi_dev->devinfo->xfer_state, &xfer);
    } else {
        while (length != 0) {
            xfer.deviceAddress += xfer.dataSize,
            xfer.data += ((xfer.dataSize + 3) / 4);
            xfer.dataSize = data_nbr;

            status = crp_flexspi_nor_write_enable(dev, xfer.deviceAddress);

            if (status != kStatus_Success) {
                return status;
            }

            status = FLEXSPI_TransferBlocking(flexspi_base, &xfer);

            if (status != kStatus_Success) {
                return status;
            }

            status = crp_flexspi_nor_wait_bus_busy(dev);

            FLEXSPI_SoftwareReset(flexspi_base);

            length -= xfer.dataSize;
            data_nbr = length < flexspi_dev->devinfo->page_size ? length : flexspi_dev->devinfo->page_size;

//            SCB_CleanDCache();
        }
    }

    return status;
}

static crp_flexspi_error_t crp_flexspi_nor_erase_sector(void* dev, uint32_t addr)
{
    assert(dev != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    FLEXSPI_Type* flexspi_base = flexspi_dev->devinfo->init_info->flexspi_base;
    flexspi_transfer_t xfer;
    status_t status;

    if (dev == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

    SCB_CleanInvalidateDCache();

    status = crp_flexspi_nor_write_enable(dev, addr);

    if (status != kStatus_Success) {
        return status;
    }

    xfer.deviceAddress = addr,
    xfer.port = flexspi_dev->devinfo->init_info->flexspi_port;
    xfer.cmdType = kFLEXSPI_Command;
    xfer.SeqNumber = 1;
    xfer.seqIndex = NOR_CMD_LUT_SEQ_IDX_ERASESECTOR;

    if (flexspi_dev->devinfo->non_block_xfer_enable) {
        status = FLEXSPI_TransferNonBlocking(flexspi_base, &flexspi_dev->devinfo->xfer_state, &xfer);
    } else {
        status = FLEXSPI_TransferBlocking(flexspi_base, &xfer);

        if (status != kStatus_Success) {
            return status;
        }

        status = crp_flexspi_nor_wait_bus_busy(dev);

        FLEXSPI_SoftwareReset(flexspi_base);
    }

    return status;
}

static crp_flexspi_error_t crp_flexspi_nor_erase_block(void* dev, uint32_t addr)
{
    assert(dev != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    FLEXSPI_Type* flexspi_base = flexspi_dev->devinfo->init_info->flexspi_base;
    flexspi_transfer_t xfer;
    status_t status;

    if (dev == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

    status = crp_flexspi_nor_write_enable(dev, addr);

    if (status != kStatus_Success) {
        return status;
    }

    xfer.deviceAddress = addr,
    xfer.port = flexspi_dev->devinfo->init_info->flexspi_port;
    xfer.cmdType = kFLEXSPI_Command;
    xfer.SeqNumber = 1;
    xfer.seqIndex = NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK;

    if (flexspi_dev->devinfo->non_block_xfer_enable) {
        status = FLEXSPI_TransferNonBlocking(flexspi_base, &flexspi_dev->devinfo->xfer_state, &xfer);
    } else {
        status = FLEXSPI_TransferBlocking(flexspi_base, &xfer);

        if (status != kStatus_Success) {
            return status;
        }

        status = crp_flexspi_nor_wait_bus_busy(dev);

        FLEXSPI_SoftwareReset(flexspi_base);
    }

    return status;
}

static crp_flexspi_error_t crp_flexspi_nor_read_uid(void* dev, uint8_t* data, uint32_t length)
{
    assert(dev != NULL);

    crp_flexspi_dev_t* flexspi_dev = (crp_flexspi_dev_t *)dev;
    FLEXSPI_Type* flexspi_base = flexspi_dev->devinfo->init_info->flexspi_base;
    flexspi_transfer_t xfer;
    status_t status;

    if (dev == NULL) {
        return CRP_FLEXSPI_NULL_PTR;
    }

    xfer.deviceAddress = 0,
    xfer.port = flexspi_dev->devinfo->init_info->flexspi_port;
    xfer.cmdType = kFLEXSPI_Read;
    xfer.SeqNumber = 1;
    xfer.seqIndex = NOR_CMD_LUT_SEQ_IDX_READ_UID;
    xfer.data = (uint32_t *)data;
    xfer.dataSize = length;

    status = FLEXSPI_TransferBlocking(flexspi_base, &xfer);

    FLEXSPI_SoftwareReset(flexspi_base);

    return status;
}
