/**
 * @file crp_flexspi.h
 * @author yun (renqingyun@crprobot.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef CRP_FLEXSPI_H
#define CRP_FLEXSPI_H

#ifdef __cplusplus
extern "C"{
#endif

#include "fsl_flexspi.h"

#define CRP_GET_FLASH_ADDR_BY_SECTOR_NO(x, sector_size) ((x) * (sector_size))u

typedef struct {
    flexspi_transfer_callback_t pfn_callback;
    void* arg;
} crp_flexspi_xfer_cplt_callback_info_t;

typedef int32_t crp_flexspi_error_t;

typedef enum {
    CRP_FLEXSPI_ERR_NO  = 0,

    CRP_FLEXSPI_NULL_VFT,

    CRP_FLEXSPI_NULL_PTR,
} crp_flexspi_status_t;

typedef struct {
    bool enable_non_block_xfer;
    crp_flexspi_xfer_cplt_callback_info_t xfer_cplt_callback_info;
} crp_flexspi_cfg_t;

typedef struct {
    crp_flexspi_error_t (*init)(void* dev, crp_flexspi_cfg_t* cfg);
    crp_flexspi_error_t (*nor_write_enable)(void* dev, uint32_t addr);
    crp_flexspi_error_t (*nor_wait_bus_busy)(void* dev);
    crp_flexspi_error_t (*nor_enable_quad_mode)(void* dev);
    crp_flexspi_error_t (*nor_read)(void* dev, uint32_t addr, uint32_t* buf, uint32_t length);
    crp_flexspi_error_t (*nor_read_by_ahb)(void* dev, uint32_t addr, uint32_t* buf, uint32_t length);
    crp_flexspi_error_t (*nor_page_program)(void* dev, uint32_t addr, uint32_t* data, uint32_t length);
    crp_flexspi_error_t (*nor_erase_sector)(void* dev, uint32_t addr);
    crp_flexspi_error_t (*nor_erase_block)(void* dev, uint32_t addr);
    crp_flexspi_error_t (*nor_read_uid)(void* dev, uint8_t* data, uint32_t length);
} crp_flexspi_funcs_t;

typedef struct {
    crp_flexspi_funcs_t* funcs;

    void* dev;
} crp_flexspi_drv_t;

extern crp_flexspi_drv_t* crp_flexspi_handle_get(void* flexspi_dev);

#ifdef __cplusplus
}
#endif

#endif /* CRP_FLEXSPI_H */

//-----------------------------------结束-------------------------------------------------
