/*
 * Copyright 2016-2022 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of NXP Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file    RT1052_FlexSPI.c
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MIMXRT1052.h"
#include "fsl_debug_console.h"
/* TODO: insert other include files here. */
#include "fsl_cache.h"
#include "crp_flexspi.h"
#include "w25qxxxjv.h"

/* TODO: insert other definitions and declarations here. */

extern uint32_t __section_table_start;
extern uint32_t __Vectors[];
#define INT_VECTOR_ADDRESS ((uint32_t)__Vectors)

__attribute__ ((section(".after_vectors.relocate_ivt")))
void SystemInitHook (void)
{
    unsigned int* int_vector_start = (void *)INT_VECTOR_ADDRESS;
    unsigned int* int_vector_end = &__section_table_start;
    unsigned int* sdram_ivt_start = (void *)0x20000000;

    while (int_vector_start < int_vector_end)
    {
        *sdram_ivt_start++ = *int_vector_start++;
    }

    SCB->VTOR = 0x20000000;
}

/*
 * @brief   Application entry point.
 */
int main(void) {
    SCB_InvalidateICache();
    __DSB();

    /* Init board hardware. */
    BOARD_ConfigMPU();
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();
#endif
    uint32_t pc = 0;
    __asm volatile ("MOV %0, R15" : "=r"(pc) : : );
    PRINTF("PC=0x%x\n", pc);
    PRINTF("Start Init FlexSPI\n");

    crp_flexspi_drv_t* crp_flexspi_handle = crp_flexspi_handle_get(crp_w25qxxxjv_handle_get());

    uint8_t uid[8] = {0};
    crp_flexspi_cfg_t flexspi_cfg = {
         .enable_non_block_xfer = false,
         .xfer_cplt_callback_info = {
              .pfn_callback = NULL,
              .arg = NULL,
         },
    };
    crp_flexspi_handle->funcs->init(crp_flexspi_handle->dev, &flexspi_cfg);

    crp_flexspi_handle->funcs->nor_read_uid(crp_flexspi_handle->dev, (uint8_t *)uid, 8);

    for (int i = 0; i < 8; ++i) {
        PRINTF("0x%02x ", uid[i]);
    }
    PRINTF("\n");

#if 1
    crp_flexspi_handle->funcs->nor_enable_quad_mode(crp_flexspi_handle->dev);
    crp_flexspi_handle->funcs->nor_erase_sector(crp_flexspi_handle->dev, 2048 * 0x1000u);

    uint32_t read[4 + 132] = {0};

    DCACHE_InvalidateByRange((FlexSPI_AMBA_BASE + 0x2000u), sizeof(read));
    memcpy(read, (void *)(FlexSPI_AMBA_BASE + 0x2000), sizeof(read));

    PRINTF("\tFlash Code:\n");
    for (int i = 0; i < 136; ++i) {
        PRINTF("0x%x\n", read[i]);
    }
    PRINTF("\n\n");

    uint32_t data[4 + 132] = {0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00, 0};
    for (int i = 0, j = 0; i < 132 * 4; ++i) {
        ((uint8_t *)&(data[4]))[i] = i + j;
        j = i / 255;
    }

    crp_flexspi_handle->funcs->nor_page_program(crp_flexspi_handle->dev, 2048 * 0x1000u, &data[0], sizeof(data));

    DCACHE_InvalidateByRange((FlexSPI_AMBA_BASE + 2048 * 0x1000u), sizeof(read));
    memcpy(read, (void *)(FlexSPI_AMBA_BASE + 2048 * 0x1000u), sizeof(read));

    PRINTF("\t\twrite <-----> read\n");
    for (int i = 0; i < 136; ++i) {
        PRINTF("%d:\t0x%x <----> 0x%x\n", i, data[i], read[i]);
    }
#endif

    NVIC_SetPriorityGrouping(4);
    SysTick_Config(SystemCoreClock / 1000);

    /* Force the counter to be placed into memory. */
    volatile static int i = 0 ;
    /* Enter an infinite loop, just incrementing a counter. */
    while(1) {
        i++ ;
        /* 'Dummy' NOP to allow source level single stepping of
            tight while() loop */
        __asm volatile ("nop");
    }
    return 0 ;
}

void SysTick_Handler(void)
{
    static uint32_t timecnt = 0;

    if (++timecnt % 500 == 0)
    {
        GPIO_PortToggle(GPIO1, 1u << BOARD_INITPINS_LED1_GPIO_PIN);
    }
}
