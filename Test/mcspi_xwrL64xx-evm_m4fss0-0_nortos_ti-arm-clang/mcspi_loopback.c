/*
 * Updated MCSPI test for communication with ESP32
 */

#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

#define APP_MCSPI_MSGSIZE (8)
#define APP_MCSPI_TRANSFER_LOOPCOUNT (10U)

uint8_t gMcspiTxBuffer[APP_MCSPI_MSGSIZE];
uint8_t gMcspiRxBuffer[APP_MCSPI_MSGSIZE];

/* Function for communication with ESP32 */
void *mcspi_transfer_main(void *args)
{
    int32_t status = SystemP_SUCCESS;
    uint32_t i;
    int32_t transferOK;
    MCSPI_Transaction spiTransaction;
    uint64_t startTimeInUSec, elapsedTimeInUsecs;

    /* Initial delay of 100ms */
    ClockP_usleep(100000);

    Drivers_open();
    Board_driversOpen();

    DebugP_log("[MCSPI] ESP32 Communication started ...\r\n");

    /* Configure SPI transaction for ESP32 */
    MCSPI_Transaction_init(&spiTransaction);
    spiTransaction.channel = gConfigMcspi0ChCfg[0].chNum;
    spiTransaction.dataSize = 8;     // ESP32 uses 8-bit
    spiTransaction.csDisable = TRUE;  // De-assert CS after transfer (proper framing)
    spiTransaction.count = APP_MCSPI_MSGSIZE;
    spiTransaction.txBuf = (void *)gMcspiTxBuffer;
    spiTransaction.rxBuf = (void *)gMcspiRxBuffer;
    spiTransaction.args = NULL;

    /* Print SPI configuration */
    DebugP_log("[MCSPI] Configuration:\r\n");
    DebugP_log("CS Line: %d\r\n", spiTransaction.channel);
    DebugP_log("CS Control: %s\r\n", spiTransaction.csDisable ? "Auto de-assert" : "Keep CS asserted");
    DebugP_log("Data Size: %d bits\r\n", spiTransaction.dataSize);
    DebugP_log("Clock: 1MHz\r\n");
    DebugP_log("Mode: POL0/PHA0\r\n");

    uint32_t transferCount = 0;
    while (1)
    {
        /* Initialize TX buffer with incrementing pattern */
        for (i = 0U; i < APP_MCSPI_MSGSIZE; i++)
        {
            // Use a predictable pattern that ESP32 can validate
            gMcspiTxBuffer[i] = i + transferCount;
            gMcspiRxBuffer[i] = 0U;
        }

        /* Add delay between transfers to give ESP32 time to process */
        ClockP_usleep(5000);  // 5ms gap before transfer

        /* Perform transfer */
        startTimeInUSec = ClockP_getTimeUsec();
        transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);
        elapsedTimeInUsecs = ClockP_getTimeUsec() - startTimeInUSec;

        /* Print transfer results */
        DebugP_log("\r\n----------------------------------------------------------\r\n");
        DebugP_log("Transfer #%d Status: %s\r\n", 
                  transferCount, 
                  (transferOK == SystemP_SUCCESS) ? "Success" : "Failed");
        DebugP_log("Transfer Time: %llu us\r\n", elapsedTimeInUsecs);

        /* Print TX data */
        DebugP_log("TX Data: ");
        for (i = 0; i < APP_MCSPI_MSGSIZE; i++)
        {
            DebugP_log("0x%02X ", gMcspiTxBuffer[i]);
        }
        DebugP_log("\r\n");

        /* Print RX data */
        if (transferOK == SystemP_SUCCESS)
        {
            DebugP_log("RX Data: ");
            for (i = 0; i < APP_MCSPI_MSGSIZE; i++)
            {
                DebugP_log("0x%02X ", gMcspiRxBuffer[i]);
            }
            DebugP_log("\r\n");
        }

        transferCount++;
        /* Wait between transfers */
        ClockP_usleep(500000);  // 500ms between transfers
    }

    /* Note: This code will never be reached due to infinite loop */
    Board_driversClose();
    Drivers_close();

    return NULL;
}