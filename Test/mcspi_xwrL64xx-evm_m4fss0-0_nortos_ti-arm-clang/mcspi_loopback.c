/*
 * Updated MCSPI test for communication with ESP32
 */

#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

#define APP_MCSPI_MSGSIZE (1) // Single byte transfer

uint8_t gMcspiTxBuffer[APP_MCSPI_MSGSIZE];
uint8_t gMcspiRxBuffer[APP_MCSPI_MSGSIZE];

#define CMD_BYTE 0x04   // Command byte to send
#define DUMMY_BYTE 0xFF // Dummy byte for reading response

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
    spiTransaction.csDisable = TRUE; // De-assert CS after transfer (proper framing)
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
        /* Send command byte */
        gMcspiTxBuffer[0] = CMD_BYTE;
        gMcspiRxBuffer[0] = 0U;

        /* Perform command transfer */
        startTimeInUSec = ClockP_getTimeUsec();
        transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);

        /* Wait for slave processing - 100 microseconds like Arduino code */
        ClockP_usleep(100);

        /* Read response using dummy byte */
        gMcspiTxBuffer[0] = DUMMY_BYTE;
        gMcspiRxBuffer[0] = 0U;

        transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);
        elapsedTimeInUsecs = ClockP_getTimeUsec() - startTimeInUSec;

        /* Print transfer results */
        DebugP_log("\r\n----------------------------------------------------------\r\n");
        DebugP_log("Transfer #%d Status: %s\r\n",
                   transferCount,
                   (transferOK == SystemP_SUCCESS) ? "Success" : "Failed");
        DebugP_log("Transfer Time: %llu us\r\n", elapsedTimeInUsecs);
        DebugP_log("Sent: 0x%02X, Received: 0x%02X\r\n", CMD_BYTE, gMcspiRxBuffer[0]);

        transferCount++;
        /* Wait between transfers - 1 second like Arduino code */
        ClockP_usleep(1000000);
    }

    /* Note: This code will never be reached due to infinite loop */
    Board_driversClose();
    Drivers_close();

    return NULL;
}
