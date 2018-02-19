/*
 * -------------------------------------------
 *    MSP432 DriverLib - v3_21_00_05 
 * -------------------------------------------
 *
 * --COPYRIGHT--,BSD,BSD
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/******************************************************************************
 * MSP432 Empty Project
 *
 * Description: An empty project that uses DriverLib
 *
 *                MSP432P401
 *             ------------------
 *         /|\|                  |
 *          | |                  |
 *          --|RST               |
 *            |                  |
 *            |                  |
 *            |                  |
 *            |                  |
 *            |                  |
 * Author: 
*******************************************************************************/
/* DriverLib Includes */
#include "driverlib.h"

/* Standard Includes */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "utils/cmdline.h"
#include "fatfs/src/ff.h"
#include "fatfs/src/diskio.h"

#include "spiDriver.h"
#include "clockConfig.h"

// Defines the size of the buffers that hold the path, or temporary data from
// the SD card.  There are two buffers allocated of this size.  The buffer size
// must be large enough to hold the longest expected full path name, including
// the file name, and a trailing null character.
#define PATH_BUF_SIZE           80

// Defines the size of the buffer that holds the command line.
#define CMD_BUF_SIZE            64

// This buffer holds the full path to the current working directory.  Initially
// it is root ("/").
static char g_pcCwdBuf[PATH_BUF_SIZE] = "/";

// A temporary data buffer used when manipulating file paths, or reading data
// from the SD card.
static char g_pcTmpBuf[PATH_BUF_SIZE];

// The buffer that holds the command line.
static char g_pcCmdBuf[CMD_BUF_SIZE];

// The following are data structures used by FatFs.
static FATFS g_sFatFs;
static DIR g_sDirObject;
static FILINFO g_sFileInfo;
static FIL g_sFileObject;

volatile int ACLK_Freq;
// A macro that holds the number of result codes.
#define NUM_FRESULT_CODES       (sizeof(g_psFResultStrings) /                 \
                                 sizeof(tFResultString))

uint8_t gucCommandReady = 0;

// A structure that holds a mapping between an FRESULT numerical code, and a
// string representation.  FRESULT codes are returned from the FatFs FAT file
// system driver.
typedef struct {
    FRESULT iFResult;
    char *pcResultStr;
} tFResultString;

// A macro to make it easy to add result codes to the table.
#define FRESULT_ENTRY(f)        { (f), (#f) }

// A table that holds a mapping between the numerical FRESULT code and it's
// name as a string.  This is used for looking up error codes for printing to
// the console.
tFResultString g_psFResultStrings[] = {
FRESULT_ENTRY(FR_OK),
FRESULT_ENTRY(FR_DISK_ERR),
FRESULT_ENTRY(FR_INT_ERR),
FRESULT_ENTRY(FR_NOT_READY),
FRESULT_ENTRY(FR_NO_FILE),
FRESULT_ENTRY(FR_NO_PATH),
FRESULT_ENTRY(FR_INVALID_NAME),
FRESULT_ENTRY(FR_DENIED),
FRESULT_ENTRY(FR_EXIST),
FRESULT_ENTRY(FR_INVALID_OBJECT),
FRESULT_ENTRY(FR_WRITE_PROTECTED),
FRESULT_ENTRY(FR_INVALID_DRIVE),
FRESULT_ENTRY(FR_NOT_ENABLED),
FRESULT_ENTRY(FR_NO_FILESYSTEM),
FRESULT_ENTRY(FR_MKFS_ABORTED),
FRESULT_ENTRY(FR_TIMEOUT),
FRESULT_ENTRY(FR_LOCKED),
FRESULT_ENTRY(FR_NOT_ENOUGH_CORE),
FRESULT_ENTRY(FR_TOO_MANY_OPEN_FILES),
FRESULT_ENTRY(FR_INVALID_PARAMETER), };

// This function returns a string representation of an error code that was
// returned from a function call to FatFs.  It can be used for printing human
// readable error messages.
const char *
StringFromFResult(FRESULT iFResult) {
    uint_fast8_t ui8Idx;

    // Enter a loop to search the error code table for a matching error code.
    for (ui8Idx = 0; ui8Idx < NUM_FRESULT_CODES; ui8Idx++) {
        // If a match is found, then return the string name of the error code.
        if (g_psFResultStrings[ui8Idx].iFResult == iFResult) {
            return (g_psFResultStrings[ui8Idx].pcResultStr);
        }
    }

    // At this point no matching code was found, so return a string indicating
    // an unknown error.
    return ("UNKNOWN ERROR CODE");
}

/* UART Configuration Parameter. These are the configuration parameters to
 * make the eUSCI A UART module to operate with a 115200 baud rate. These
 * values were calculated using the online calculator that TI provides
 * at:
 * http://processors.wiki.ti.com/index.php/
 *               USCI_UART_Baud_Rate_Gen_Mode_Selection
 */
const eUSCI_UART_Config uartConfig = {
EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
        26,                                      // BRDIV = 104
        0,                                       // UCxBRF = 0
        0,                                       // UCxBRS = 0
        EUSCI_A_UART_NO_PARITY,                  // No Parity
        EUSCI_A_UART_LSB_FIRST,                  // MSB First
        EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
        EUSCI_A_UART_MODE,                       // UART mode
        EUSCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION  // Low Frequency Mode
        };

void SysTick_ISR(void) {
    // Call the FatFs tick timer.
    disk_timerproc();

    GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);
}

/*
 * USCIA0 interrupt handler.
 */
void EusciA0_ISR(void) {
    int16_t receiveByte = UCA0RXBUF;
    static uint32_t ui32Count = 0;
    static int8_t bLastWasCR = 0;

    // See if the backspace key was pressed.
    if (receiveByte == '\b') {
        // If there are any characters already in the buffer, then delete
        // the last.
        if (ui32Count) {
            // Rub out the previous character.
            printf("\b");

            // Decrement the number of characters in the buffer.
            ui32Count--;
        }
    }

    // If this character is LF and last was CR, then just gobble up the
    // character because the EOL processing was taken care of with the CR.
    if ((receiveByte == '\n') && bLastWasCR) {
        //   bLastWasCR = 0;
    }

    // See if a newline or escape character was received.
    if ((receiveByte == '\r') || (receiveByte == '\n')
            || (receiveByte == 0x1b)) {
        // If the character is a CR, then it may be followed by a LF which
        // should be paired with the CR.  So remember that a CR was
        // received.
        if (receiveByte == '\r') {
            //  bLastWasCR = 1;
        }

        // Reset the count.
        ui32Count = 0;

        gucCommandReady = 1;
        // Stop processing the input and end the line.
        return;
    }

    // Process the received character as long as we are not at the end of
    // the buffer.  If the end of the buffer has been reached then all
    // additional characters are ignored until a newline is received.
    if (ui32Count < sizeof(g_pcCmdBuf)) {
        // Store the character in the caller supplied buffer.
        g_pcCmdBuf[ui32Count] = receiveByte;

        // Increment the count of characters received.
        ui32Count++;

        /* Echo the character back. */
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, receiveByte);
    }

}

int main(void)
{
    /* Stop Watchdog  */
    MAP_WDT_A_holdTimer();

    //clockStartUp();
    /*
     * Change spiDriver.c to work of ACLK and setup ACLK below
     */
    /*MAP_CS_initClockSignal(CS_ACLK, CS_HFXTCLK_SELECT, CS_CLOCK_DIVIDER_16);
    ACLK_Freq=MAP_CS_getACLK();  // get ACLK value*/
    /* Initialize main clock to 3MHz */
        MAP_CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_3);
        CS_initClockSignal(CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
        CS_initClockSignal(CS_HSMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
        CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);


    /* Selecting P1.2 and P1.3 in UART mode. */
        MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1,
        GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);

        /* Configuring UART Module */
        MAP_UART_initModule(EUSCI_A0_BASE, &uartConfig);

        /* Enable UART module */
        MAP_UART_enableModule(EUSCI_A0_BASE);

        UART_enableInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
        Interrupt_enableInterrupt(INT_EUSCIA0);


        /* Configure SysTick for a 100Hz interrupt.  The FatFs driver wants a 10 ms
             * tick.
             */
            SysTick_setPeriod(3000000 / 100);

        //clock is 48Mhz
        //SysTick_setPeriod((48000000 / 100));
            SysTick_enableModule();
            SysTick_enableInterrupt();

            spi_Open();


        Interrupt_enableMaster();

    // Print message to user.
    printf("\n\nSD Card Test Program\n");

    FRESULT iFResult;
    UINT readBytes,writeBytes;
    //WORD readBytes, writeBytes;

    // Mount the file system, using logical disk 0.
    iFResult = f_mount(0, &g_sFatFs);
    if (iFResult != FR_OK) {
            printf("f_mount error: %s\n", StringFromFResult(iFResult));
    }else{
        printf("SD Card Mounted\n\r");
    }
    int i;
    for(i=0; i<10000; i++){
        ;
    }

    iFResult = f_open(&g_sFileObject, "nFilex", FA_WRITE | FA_CREATE_ALWAYS);
    if (iFResult != FR_OK) {
        printf("f_open error: %s\n", StringFromFResult(iFResult));
    }else{
        printf("SD File Opened!\n\r");
    }


    iFResult = f_write(&g_sFileObject,"This is a test1\r\n",17,&writeBytes);
    if (iFResult != FR_OK) {
        printf("f_write error: %s\n", StringFromFResult(iFResult));
    }
    printf("%d characters written\n",writeBytes);

    iFResult = f_close(&g_sFileObject);
    if (iFResult != FR_OK) {
        printf("f_close error: %s\n", StringFromFResult(iFResult));
    }

    iFResult = f_open(&g_sFileObject, "nFilex", FA_READ);
    if (iFResult != FR_OK) {
        printf("f_open error: %s\n", StringFromFResult(iFResult));
    }

    iFResult = f_read(&g_sFileObject, g_pcTmpBuf, sizeof(g_pcTmpBuf) - 1, &readBytes);
    if (iFResult != FR_OK) {
        printf("f_read error: %s\n", StringFromFResult(iFResult));
    }

    printf("%d characters read\n",readBytes);
    printf("%s\n", g_pcTmpBuf);

    iFResult = f_close(&g_sFileObject);
    if (iFResult != FR_OK) {
        printf("f_close error: %s\n", StringFromFResult(iFResult));
    }
    while(1){
        ;
    }
}
