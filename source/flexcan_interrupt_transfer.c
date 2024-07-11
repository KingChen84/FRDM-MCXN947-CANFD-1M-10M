/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "fsl_flexcan.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define USE_CANFD             (1)
#define EXAMPLE_CAN           CAN0
#define RX_MESSAGE_BUFFER_NUM (0)
#define TX_MESSAGE_BUFFER_NUM (1)
#define TX_MESSAGE_BUFFER_NUM2 (2)
#define TX_MESSAGE_BUFFER_NUM3 (3)

#define EXAMPLE_CAN_CLK_FREQ       CLOCK_GetFlexcanClkFreq(0U)
#define USE_IMPROVED_TIMING_CONFIG (1)
/* Fix MISRA_C-2012 Rule 17.7. */
#define LOG_INFO (void)PRINTF
#if (defined(USE_CANFD) && USE_CANFD)
/*
 *    DWORD_IN_MB    DLC    BYTES_IN_MB             Maximum MBs
 *    2              8      kFLEXCAN_8BperMB        64
 *    4              10     kFLEXCAN_16BperMB       42
 *    8              13     kFLEXCAN_32BperMB       25
 *    16             15     kFLEXCAN_64BperMB       14
 *
 * Dword in each message buffer, Length of data in bytes, Payload size must align,
 * and the Message Buffers are limited corresponding to each payload configuration:
 */
#define DLC         (15)
#define BYTES_IN_MB kFLEXCAN_64BperMB
#else
#define DLC (8)
#endif
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
flexcan_handle_t flexcanHandle;
volatile bool txComplete = false;
volatile bool rxComplete = false;
volatile bool wakenUp    = false;
flexcan_mb_transfer_t txXfer, rxXfer;
#if (defined(USE_CANFD) && USE_CANFD)
flexcan_fd_frame_t frame;
#else
flexcan_frame_t frame;
#endif
uint32_t txIdentifier;
uint32_t rxIdentifier;

volatile uint32_t g_systickCounter;

/*******************************************************************************
 * Code
 ******************************************************************************/
void SysTick_Handler(void)
{
    if (g_systickCounter != 0U)
    {
        g_systickCounter--;
    }
}

void SysTick_DelayTicks(uint32_t n)
{
    g_systickCounter = n;
    while (g_systickCounter != 0U)
    {
    }
}

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief FlexCAN Call Back function
 */
static FLEXCAN_CALLBACK(flexcan_callback)
{
    switch (status)
    {
        case kStatus_FLEXCAN_RxIdle:
            if (RX_MESSAGE_BUFFER_NUM == result)
            {
                rxComplete = true;
            }
            break;

        case kStatus_FLEXCAN_TxIdle:
            if (TX_MESSAGE_BUFFER_NUM == result)
            {
                txComplete = true;
            }
            break;

        case kStatus_FLEXCAN_WakeUp:
            wakenUp = true;
            break;

        default:
            break;
    }
}

/*!
 * @brief Main function
 */
int main(void)
{
    flexcan_config_t flexcanConfig;
    flexcan_rx_mb_config_t mbConfig;
    uint8_t node_type;
    uint32_t Getfreq;

    /* Initialize board hardware. */
    /* attach FRO 12M to FLEXCOMM4 (debug console) */
    CLOCK_SetClkDiv(kCLOCK_DivFlexcom4Clk, 1u);
    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

//    /* attach PLL1Clk0 to FLEXCAN0 */
//    CLOCK_SetClkDiv(kCLOCK_DivPLL1Clk0, 2U);
//
//    CLOCK_SetClkDiv(kCLOCK_DivFlexcan0Clk, 1U);
//    CLOCK_AttachClk(kPLL1_CLK0_to_FLEXCAN0);
//
//    CLOCK_SetClkDiv(kCLOCK_DivFlexcan1Clk, 1U);
//    CLOCK_AttachClk(kPLL1_CLK0_to_FLEXCAN1);

    BOARD_InitPins();
    BOARD_InitBootClocks();
//    BOARD_BootClockPLL150M();
    BOARD_InitDebugConsole();
    Getfreq = EXAMPLE_CAN_CLK_FREQ;
    LOG_INFO("********* FLEXCAN Interrupt EXAMPLE ********* Freq=%d\r\n",Getfreq);
    LOG_INFO("    Message format: Standard (11 bit id)\r\n");
    LOG_INFO("    Message buffer %d used for Rx.\r\n", RX_MESSAGE_BUFFER_NUM);
    LOG_INFO("    Message buffer %d used for Tx.\r\n", TX_MESSAGE_BUFFER_NUM);
    LOG_INFO("    Interrupt Mode: Enabled\r\n");
    LOG_INFO("    Operation Mode: TX and RX --> Normal\r\n");
    LOG_INFO("*********************************************\r\n\r\n");

    /* Set systick reload value to generate 1ms interrupt */
    if (SysTick_Config(SystemCoreClock / 1000U))
    {
        while (1)
        {
        }
    }

//    node_type = 'A';
    /* Select mailbox ID. */
//    if ((node_type == 'A') || (node_type == 'a'))
//    {
        txIdentifier = 0x321;
        rxIdentifier = 0x123;
//    }
//    else
//    {
//        txIdentifier = 0x123;
//        rxIdentifier = 0x321;
//    }

    /* Get FlexCAN module default Configuration. */
    /*
     * flexcanConfig.clkSrc                 = kFLEXCAN_ClkSrc0;
     * flexcanConfig.bitRate               = 1000000U;
     * flexcanConfig.bitRateFD             = 2000000U;
     * flexcanConfig.maxMbNum               = 16;
     * flexcanConfig.enableLoopBack         = false;
     * flexcanConfig.enableSelfWakeup       = false;
     * flexcanConfig.enableIndividMask      = false;
     * flexcanConfig.disableSelfReception   = false;
     * flexcanConfig.enableListenOnlyMode   = false;
     * flexcanConfig.enableDoze             = false;
     */
    FLEXCAN_GetDefaultConfig(&flexcanConfig);

    flexcanConfig.bitRate = 1000000U;
    flexcanConfig.bitRateFD = 10000000U;

#if defined(EXAMPLE_CAN_CLK_SOURCE)
    flexcanConfig.clkSrc = EXAMPLE_CAN_CLK_SOURCE;
#endif

#if defined(EXAMPLE_CAN_BIT_RATE)
    flexcanConfig.bitRate = EXAMPLE_CAN_BIT_RATE;
#endif

/* If special quantum setting is needed, set the timing parameters. */
#if (defined(SET_CAN_QUANTUM) && SET_CAN_QUANTUM)
    flexcanConfig.timingConfig.phaseSeg1 = PSEG1;
    flexcanConfig.timingConfig.phaseSeg2 = PSEG2;
    flexcanConfig.timingConfig.propSeg   = PROPSEG;
#if (defined(FSL_FEATURE_FLEXCAN_HAS_FLEXIBLE_DATA_RATE) && FSL_FEATURE_FLEXCAN_HAS_FLEXIBLE_DATA_RATE)
    flexcanConfig.timingConfig.fphaseSeg1 = FPSEG1;
    flexcanConfig.timingConfig.fphaseSeg2 = FPSEG2;
    flexcanConfig.timingConfig.fpropSeg   = FPROPSEG;
#endif
#endif

#if (defined(USE_IMPROVED_TIMING_CONFIG) && USE_IMPROVED_TIMING_CONFIG)
    flexcan_timing_config_t timing_config;
    memset(&timing_config, 0, sizeof(flexcan_timing_config_t));
#if 0
    timing_config.preDivider  =  0;//+1   1
    timing_config.phaseSeg1 = 62;//+1  63
    timing_config.phaseSeg2 = 15;//+1  16
    timing_config.rJumpwidth = 15;//+1 =  16
    timing_config.propSeg = 0;//+1 =  1

    timing_config.fpreDivider  =  0;//+1
    timing_config.fphaseSeg1 = 6;//+1  =  7
    timing_config.fphaseSeg2 = 1;//+1 =  2
    timing_config.frJumpwidth = 0;//+1 =  1
    timing_config.fpropSeg = 0;//+1 =  1

    memcpy(&(flexcanConfig.timingConfig), &timing_config, sizeof(flexcan_timing_config_t));
#else
    if (FLEXCAN_FDCalculateImprovedTimingValues(EXAMPLE_CAN, flexcanConfig.bitRate, flexcanConfig.bitRateFD,
                                                EXAMPLE_CAN_CLK_FREQ, &timing_config))
    {
        /* Update the improved timing configuration*/
        memcpy(&(flexcanConfig.timingConfig), &timing_config, sizeof(flexcan_timing_config_t));
    }
    else
    {
        LOG_INFO("No found Improved Timing Configuration. Just used default configuration\r\n\r\n");
    }
#endif
#endif

#if (defined(USE_CANFD) && USE_CANFD)
    FLEXCAN_FDInit(EXAMPLE_CAN, &flexcanConfig, EXAMPLE_CAN_CLK_FREQ, BYTES_IN_MB, true);
#else
    FLEXCAN_Init(EXAMPLE_CAN, &flexcanConfig, EXAMPLE_CAN_CLK_FREQ);
#endif

    /* Create FlexCAN handle structure and set call back function. */
    FLEXCAN_TransferCreateHandle(EXAMPLE_CAN, &flexcanHandle, flexcan_callback, NULL);

    /* Set Rx Masking mechanism. */
    FLEXCAN_SetRxMbGlobalMask(EXAMPLE_CAN, FLEXCAN_RX_MB_STD_MASK(rxIdentifier, 0, 0));

    /* Setup Rx Message Buffer. */
    mbConfig.format = kFLEXCAN_FrameFormatStandard;
    mbConfig.type   = kFLEXCAN_FrameTypeData;
    mbConfig.id     = FLEXCAN_ID_STD(rxIdentifier);
#if (defined(USE_CANFD) && USE_CANFD)
    FLEXCAN_SetFDRxMbConfig(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &mbConfig, true);
#else
    FLEXCAN_SetRxMbConfig(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &mbConfig, true);
#endif

/* Setup Tx Message Buffer. */
#if (defined(USE_CANFD) && USE_CANFD)
    FLEXCAN_SetFDTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, true);
    FLEXCAN_SetFDTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM2, true);
    FLEXCAN_SetFDTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM3, true);
#else
    FLEXCAN_SetTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, true);
#endif



    while (true)
    {
        /* Delay 1000 ms */
        SysTick_DelayTicks(100U);
        LOG_INFO("Start send a frame\r\n");
#if 1
        txIdentifier = 0x7FE;
        frame.id     = FLEXCAN_ID_STD(txIdentifier);
        frame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
        frame.type   = (uint8_t)kFLEXCAN_FrameTypeData;
        frame.length = (uint8_t)DLC;
#if (defined(USE_CANFD) && USE_CANFD)
        frame.brs = 1U;
        frame.edl = 1U;
#endif
        txXfer.mbIdx = (uint8_t)TX_MESSAGE_BUFFER_NUM;
#if (defined(USE_CANFD) && USE_CANFD)
        txXfer.framefd = &frame;
        frame.length = 5U;
    #if (defined(USE_CANFD) && USE_CANFD)
        frame.dataWord[0] = CAN_WORD_DATA_BYTE_0(0x60) | CAN_WORD_DATA_BYTE_1(0x98) | CAN_WORD_DATA_BYTE_2(0xDA) |
                            CAN_WORD_DATA_BYTE_3(0x00);
        frame.dataWord[1] = CAN_WORD_DATA_BYTE_4(0xF1);
        (void)FLEXCAN_TransferFDSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &txXfer);
///////////////////////////////Frame2//////////////////////////////////
        txIdentifier = 0x7FF;
        frame.id     = FLEXCAN_ID_STD(txIdentifier);
        txXfer.mbIdx = (uint8_t)TX_MESSAGE_BUFFER_NUM2;
        txXfer.framefd = &frame;
        frame.length = 8U;
        frame.dataWord[0] = CAN_WORD_DATA_BYTE_0(0x66) | CAN_WORD_DATA_BYTE_1(0x99) | CAN_WORD_DATA_BYTE_2(0xAD) |
                            CAN_WORD_DATA_BYTE_3(0x01);
        frame.dataWord[1] = CAN_WORD_DATA_BYTE_4(0xF3)| CAN_WORD_DATA_BYTE_1(0xB8) | CAN_WORD_DATA_BYTE_2(0xBA) |
        					CAN_WORD_DATA_BYTE_3(0xFF);
        (void)FLEXCAN_TransferFDSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &txXfer);
///////////////////////////////Frame3//////////////////////////////////
        txIdentifier = 0x7EF;
        frame.id     = FLEXCAN_ID_STD(txIdentifier);
        txXfer.mbIdx = (uint8_t)TX_MESSAGE_BUFFER_NUM3;
        txXfer.framefd = &frame;
        frame.length = 12U;
        frame.dataWord[0] = CAN_WORD_DATA_BYTE_0(0x77) | CAN_WORD_DATA_BYTE_1(0x99) | CAN_WORD_DATA_BYTE_2(0xAD) |
                            CAN_WORD_DATA_BYTE_3(0x01);
        frame.dataWord[1] = CAN_WORD_DATA_BYTE_4(0xF8)| CAN_WORD_DATA_BYTE_1(0xB8) | CAN_WORD_DATA_BYTE_2(0xBA) |
        					CAN_WORD_DATA_BYTE_3(0xFF);
        frame.dataWord[2] = CAN_WORD_DATA_BYTE_4(0x99)| CAN_WORD_DATA_BYTE_1(0xB8) | CAN_WORD_DATA_BYTE_2(0xBA) |
        					CAN_WORD_DATA_BYTE_3(0xFF);
        (void)FLEXCAN_TransferFDSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &txXfer);

    #else
        frame.dataWord0 = CAN_WORD0_DATA_BYTE_0(0x60) | CAN_WORD0_DATA_BYTE_1(0x98) | CAN_WORD0_DATA_BYTE_2(0xDA) |
                          CAN_WORD0_DATA_BYTE_3(0x00);
        frame.dataWord1 = CAN_WORD1_DATA_BYTE_4(0xF1);
        (void)FLEXCAN_TransferSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &txXfer);
    #endif

#else
        txXfer.frame = &frame;
        (void)FLEXCAN_TransferSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &txXfer);
#endif

#endif


    }
}
