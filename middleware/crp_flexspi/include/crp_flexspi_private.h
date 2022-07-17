/**
 * @file crp_flexspi_private.h
 * @author yun (renqingyun@crprobot.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef CRP_FLEXSPI_PRIVATE_H
#define CRP_FLEXSPI_PRIVATE_H

#include "fsl_flexspi.h"
#include "fsl_cache.h"

#include "crp_flexspi.h"

#ifdef __cplusplus
extern "C"{
#endif

#define NOR_CMD_LUT_SEQ_IDX_READ            0 //!< 0  READ LUT sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_READSTATUS1     1 //!< 1  Read Status Register 1 sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_READSTATUS2     2 //!< 2  Read Status Register 2 sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_WRITEENABLE     3 //!< 3  Write Enable sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_READSTATUS3     4 //!< 4  Read Status Register 3 sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_ERASESECTOR     5 //!< 5  Erase Sector sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_WRITESTATUS1    6 //!< 6  Write Status Register 1 sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_WRITESTATUS2    7 //!< 7  Write Status Register 2 sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_WRITESTATUS3    8 //!< 8  Write Status Register 3 sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM     9 //!< 9  Program sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK     10 //!< 10  Erase Block sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_CHIPERASE      11 //!< 11 Chip Erase sequence in lookupTable id stored in config block
#define NOR_CMD_LUT_SEQ_IDX_READ_UID       12 //!< 12 Read Unique ID sequence in loopupTable id stored in config block
#define NOR_CMD_LUT_SEQ_IDX_READ_SFDP      13 //!< 13 Read SFDP sequence in lookupTable id stored in config block
#define NOR_CMD_LUT_SEQ_IDX_RESTORE_NOCMD  14 //!< 14 Restore 0-4-4/0-8-8 mode sequence id in lookupTable stored in config block
#define NOR_CMD_LUT_SEQ_IDX_EXIT_NOCMD     15 //!< 15 Exit 0-4-4/0-8-8 mode sequence id in lookupTable stored in config blobk

#define CRP_LUT_LENGTH 64

typedef struct {
    FLEXSPI_Type* flexspi_base;

    flexspi_port_t flexspi_port;

    flexspi_device_config_t * flexspi_dev_cfg;

    uint32_t (*crp_lut)[CRP_LUT_LENGTH];
} crp_flexspi_init_info_t;

typedef struct {
    crp_flexspi_init_info_t * init_info;

    uint16_t page_size;
    uint16_t sector_size;
    uint32_t block_size;

    uint8_t quad_mode_cmd_seq_index;
    uint8_t quad_mode_bit_offset;

    uint8_t busy_status_cmd_seq_index;
    uint8_t busy_status_bit_offset;
    bool busy_status_bit_polarity;

    bool non_block_xfer_enable;
    flexspi_handle_t xfer_state;
} crp_flexspi_devinfo_t;

typedef struct {
    char name[20];

    crp_flexspi_drv_t drv;

    crp_flexspi_devinfo_t * devinfo;

    crp_flexspi_error_t last_error;
} crp_flexspi_dev_t;

crp_flexspi_error_t crp_flexspi_lut_get(FLEXSPI_Type* base, uint32_t* lut);

#ifdef __cplusplus
}
#endif

#endif /* CRP_FLEXSPI_PRIVATE_H */

//-----------------------------------结束-------------------------------------------------
