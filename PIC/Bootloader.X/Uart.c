/*********************************************************************
 *
 *                  dsPIC33E/PIC24E Boot Loader
 *
 *********************************************************************
 * FileName:        Uart.c
 * Dependencies:
 * Processor:       dsPIC33E/PIC24E
 *
 * Compiler:        MPLAB C30
 *                  MPLAB IDE
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the “Company”) for its PIC32 Microcontroller is intended
 * and supplied to you, the Company’s customer, for use solely and
 * exclusively on Microchip PIC32 Microcontroller products.
 * The software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *
 * $Id:  $
 * $Name: $
 *
 **********************************************************************/
#include "system.h"
#include "Uart.h"

#include "BootLoader.h"
#include "Framework.h"


static UINT8 TxBuff[255];

/********************************************************************
* Function: 	UartTasks()
********************************************************************/
void uartTask(void)
{
   unsigned char TxLen;
   unsigned char Rx;
   unsigned char *ptr;
   // Check any character is received.
   if(getChar(&Rx))
   {
      // Pass the bytes to frame work.
      BuildRxFrame(&Rx, 1);
   }

   ptr = TxBuff;
   // Get transmit frame from frame work.
   TxLen = GetTransmitFrame(ptr);

   if(TxLen)
   {
      // There is something to transmit.
      while(TxLen--)
         putChar(*(ptr++));
   } 


}

/********************************************************************
* Function: 	getChar(), nonblocking
********************************************************************/
BOOL getChar(UINT8 *byte)
{
	if(U1STAbits.URXDA)
	{
		*byte = (UINT8)U1RXREG;		        // get data from UART RX FIFO
		// Clear error flag
    	if(U1STAbits.OERR)
    	{
        	U1STAbits.OERR = 0;
    	} 
		return TRUE;
	}
	
	return FALSE;
}

/********************************************************************
* Function: 	putChar(), blocking
********************************************************************/
void putChar(UINT8 tx_char)
{
    while(U1STAbits.UTXBF); // wait for TX buffer to be empty
    U1TXREG = tx_char;
}

/********************************************************************
* Function: 	printString()
********************************************************************/
void printString(char *s)
{
	char *index = s;

	while (*index != 0)   // terminating NULL character not yet reached
	{
		putChar(*index);
		index++;
	} 
}