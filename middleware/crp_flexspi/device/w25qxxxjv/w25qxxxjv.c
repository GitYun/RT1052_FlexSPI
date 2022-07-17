/**
 * @file w25qxxxjv.c
 * @brief 
 * @author yun (renqingyun@crprobot.com)
 * @date 2022-07-15
 * @version 0.1
 * @attention
 * @htmlonly
 * @endhtmlonly
 * 
 * @copyright Copyright (c) 2022 Chengdu CRP Technology Co., Ltd.
 */

#include "w25qxxxjv.h"

#include "crp_flexspi_private.h"

#define CMD_SDR        0x01
#define CMD_DDR        0x21
#define RADDR_SDR      0x02
#define RADDR_DDR      0x22
#define CADDR_SDR      0x03
#define CADDR_DDR      0x23
#define MODE1_SDR      0x04
#define MODE1_DDR      0x24
#define MODE2_SDR      0x05
#define MODE2_DDR      0x25
#define MODE4_SDR      0x06
#define MODE4_DDR      0x26
#define MODE8_SDR      0x07
#define MODE8_DDR      0x27
#define WRITE_SDR      0x08
#define WRITE_DDR      0x28
#define READ_SDR       0x09
#define READ_DDR       0x29
#define LEARN_SDR      0x0A
#define LEARN_DDR      0x2A
#define DATSZ_SDR      0x0B
#define DATSZ_DDR      0x2B
#define DUMMY_SDR      0x0C
#define DUMMY_DDR      0x2C
#define DUMMY_RWDS_SDR 0x0D
#define DUMMY_RWDS_DDR 0x2D
#define JMP_ON_CS      0x1F
#define STOP           0

#define FLEXSPI_1PAD 0
#define FLEXSPI_2PAD 1
#define FLEXSPI_4PAD 2
#define FLEXSPI_8PAD 3

flexspi_device_config_t g_flexspi_dev_cfg = {
    .flexspiRootClk = 133000000,
    .isSck2Enabled = false,
    .flashSize = 4u * 1024u * 1024u, // 4MB
    .CSIntervalUnit = kFLEXSPI_CsIntervalUnit1SckCycle,
    .CSInterval = 2, // 手册注明：最小也得2个时钟周期
    .CSHoldTime = 3,
    .CSSetupTime = 3,
    .dataValidTime = 0,
    .columnspace = 0,
    .enableWordAddress = 0,
    .AWRSeqIndex = 0,
    .AWRSeqNumber = 0,
    .ARDSeqIndex = 0,
    .ARDSeqNumber = 1,
    .AHBWriteWaitUnit = kFLEXSPI_AhbWriteWaitUnit2AhbCycle,
    .AHBWriteWaitInterval = 0,
};

uint32_t g_crp_lut[CRP_LUT_LENGTH] = {
      [4 * NOR_CMD_LUT_SEQ_IDX_READ + 0] = FLEXSPI_LUT_SEQ(CMD_SDR,   FLEXSPI_1PAD, 0xEB, RADDR_SDR, FLEXSPI_4PAD, 0x18),
      [4 * NOR_CMD_LUT_SEQ_IDX_READ + 1] = FLEXSPI_LUT_SEQ(MODE8_SDR, FLEXSPI_4PAD, 0xFF, DUMMY_SDR, FLEXSPI_4PAD, 0x04),
      [4 * NOR_CMD_LUT_SEQ_IDX_READ + 2] = FLEXSPI_LUT_SEQ(READ_SDR,  FLEXSPI_4PAD, 0x04, STOP,      FLEXSPI_1PAD, 0x00),
      [4 * NOR_CMD_LUT_SEQ_IDX_READ + 3] = 0,

      // 1th Seq: Read Status Register 1
      [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS1 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x05, READ_SDR, FLEXSPI_1PAD, 0x01),
      [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS1 + 1] = FLEXSPI_LUT_SEQ(STOP,    FLEXSPI_1PAD, 0x00, 0, 0, 0),

      // 2th Seq: Read Status Register 2
      [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS2 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x35, READ_SDR, FLEXSPI_1PAD, 0x01),
      [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS2 + 1] = FLEXSPI_LUT_SEQ(STOP,    FLEXSPI_1PAD, 0x00, 0, 0, 0),

      // 3th Seq: Write Enable
      [4 * NOR_CMD_LUT_SEQ_IDX_WRITEENABLE + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x06, STOP, FLEXSPI_1PAD, 0x00),

      // 4th Seq: Read Status Register 3
      [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS3 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x15, READ_SDR, FLEXSPI_1PAD, 0x01),
      [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS3 + 1] = FLEXSPI_LUT_SEQ(STOP,    FLEXSPI_1PAD, 0x00, 0, 0, 0),

      // 5th Seq: Sector Erase with 3-Bytes Address
      [4 * NOR_CMD_LUT_SEQ_IDX_ERASESECTOR + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x20, RADDR_SDR, FLEXSPI_1PAD, 0x18),
      [4 * NOR_CMD_LUT_SEQ_IDX_ERASESECTOR + 1] = FLEXSPI_LUT_SEQ(STOP,    FLEXSPI_1PAD, 0x00, 0, 0, 0),

      // 6th Seq: Write Status Register 1
      [4 * NOR_CMD_LUT_SEQ_IDX_WRITESTATUS1 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x01, WRITE_SDR, FLEXSPI_1PAD, 0x01),
      [4 * NOR_CMD_LUT_SEQ_IDX_WRITESTATUS1 + 1] = FLEXSPI_LUT_SEQ(STOP,    FLEXSPI_1PAD, 0x00, 0, 0, 0),

      // 7th Seq: Write Status Register 2
      [4 * NOR_CMD_LUT_SEQ_IDX_WRITESTATUS2 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x31, WRITE_SDR, FLEXSPI_1PAD, 0x01),
      [4 * NOR_CMD_LUT_SEQ_IDX_WRITESTATUS2 + 1] = FLEXSPI_LUT_SEQ(STOP,    FLEXSPI_1PAD, 0x00, 0, 0, 0),

      // 8th Seq: Write Status Register 3
      [4 * NOR_CMD_LUT_SEQ_IDX_WRITESTATUS3 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x11, WRITE_SDR, FLEXSPI_1PAD, 0x01),
      [4 * NOR_CMD_LUT_SEQ_IDX_WRITESTATUS3 + 1] = FLEXSPI_LUT_SEQ(STOP,    FLEXSPI_1PAD, 0x00, 0, 0, 0),

      // 9th Seq: Qaud Page Program with 3-Bytes Address
      [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM + 0] = FLEXSPI_LUT_SEQ(CMD_SDR,  FLEXSPI_1PAD, 0x32, RADDR_SDR, FLEXSPI_1PAD, 0x18),
      [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM + 1] = FLEXSPI_LUT_SEQ(WRITE_SDR,FLEXSPI_4PAD, 0x04, STOP,      FLEXSPI_1PAD, 0x00),

      // 10th Seq: 32KB Block Block Erase
      [4 * NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x52, RADDR_SDR, FLEXSPI_1PAD, 0x18),
      [4 * NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK + 1] = FLEXSPI_LUT_SEQ(STOP,    FLEXSPI_1PAD, 0x00, 0, 0, 0),

      // 11th Seq: Chip Erase
      [4 * NOR_CMD_LUT_SEQ_IDX_CHIPERASE + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0xC7, STOP, FLEXSPI_1PAD, 0x00),

      // 12th Seq: Read Unique ID Number
      [4 * NOR_CMD_LUT_SEQ_IDX_READ_UID + 0] = FLEXSPI_LUT_SEQ(CMD_SDR,  FLEXSPI_1PAD, 0x4B, DUMMY_SDR, FLEXSPI_1PAD, 0x20),
      [4 * NOR_CMD_LUT_SEQ_IDX_READ_UID + 1] = FLEXSPI_LUT_SEQ(READ_SDR, FLEXSPI_1PAD, 0x04, STOP,    FLEXSPI_1PAD, 0x00),

      // 13th Seq: Read SFDP Register
      [4 * NOR_CMD_LUT_SEQ_IDX_READ_SFDP + 0] = FLEXSPI_LUT_SEQ(CMD_SDR,   FLEXSPI_1PAD, 0x5A, RADDR_SDR, FLEXSPI_1PAD, 0x18),
      [4 * NOR_CMD_LUT_SEQ_IDX_READ_SFDP + 1] = FLEXSPI_LUT_SEQ(DUMMY_SDR, FLEXSPI_1PAD, 0x08, READ_SDR,  FLEXSPI_1PAD, 0x04),
      [4 * NOR_CMD_LUT_SEQ_IDX_READ_SFDP + 2] = FLEXSPI_LUT_SEQ(STOP,      FLEXSPI_1PAD, 0x00, 0, 0, 0),
};

crp_flexspi_init_info_t g_crp_flexspi_init_info = {
    .flexspi_base = FLEXSPI,
    .flexspi_port = kFLEXSPI_PortA1,
    .flexspi_dev_cfg = &g_flexspi_dev_cfg,
    .crp_lut = &g_crp_lut,
};

crp_flexspi_devinfo_t g_crp_w25qxxxjv_devinfo = {
    .page_size = 256,
    .sector_size = 4096,
    .block_size = 32 * 1024,
    .init_info = &g_crp_flexspi_init_info,

    .quad_mode_cmd_seq_index = NOR_CMD_LUT_SEQ_IDX_WRITESTATUS2,
    .quad_mode_bit_offset = 1,

    .busy_status_cmd_seq_index = NOR_CMD_LUT_SEQ_IDX_READSTATUS1,
    .busy_status_bit_offset = 0,
    .busy_status_bit_polarity = true,

    .non_block_xfer_enable = true,
};

crp_flexspi_dev_t g_crp_w25qxxxjv_dev = {
//    .name = "W25QXXXJV",
    .drv = {
        .dev = &g_crp_w25qxxxjv_dev,
        .funcs = NULL,
    },
    .devinfo = &g_crp_w25qxxxjv_devinfo,
    .last_error = CRP_FLEXSPI_ERR_NO,
};

void* crp_w25qxxxjv_handle_get(void)
{
    char name[] = "W25QXXXJV";
    uint8_t i = 0;

    // g_crp_w25qxxxjv_dev.last_error = crp_flexspi_lut_get(g_crp_flexspi_init_info.flexspi_base, (uint32_t *)g_crp_lut);

    for (i = 0; i < sizeof(name); ++i) {
        g_crp_w25qxxxjv_dev.name[i] = name[i];
    }

    return &g_crp_w25qxxxjv_dev;
}
