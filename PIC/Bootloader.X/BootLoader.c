/*********************************************************************
 *
 *                  dsPIC33E/PIC24E Bootloader
 *
 *********************************************************************
 * FileName:        Bootloader.c
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
 * (the “Company”) for its dsPIC3E/PIC24E Microcontroller is intended
 * and supplied to you, the Company’s customer, for use solely and
 * exclusively on Microchip dsPIC3E/PIC24E Microcontroller products.
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
#include "Bootloader.h"

#include "Framework.h"
#include "Uart.h"
#include "init.h"

/** Configuration bits *********************************************/
_FGS( GWRP_OFF & GSS_OFF & GSSK_OFF );
_FAS(AWRP_OFF & APL_OFF & APLK_OFF);
_FOSCSEL(FNOSC_FRC & IESO_OFF);  // internal oscillator will always work
_FOSC(POSCMD_XT & OSCIOFNC_OFF & IOL1WAY_OFF & FCKSM_CSECME);
_FWDT(WDTPOST_PS1 & WDTPRE_PR32 & PLLKEN_ON & WINDIS_OFF & FWDTEN_OFF);
_FPOR(FPWRT_PWR1 & BOREN_OFF & ALTI2C1_OFF);
_FICD(ICS_PGD2 & RSTPRI_AF & JTAGEN_OFF);    // note reset to AUX Flash!

BYTE blink_mode = 0;    // 0=no blinking, 1=flash, 2=progress
BYTE blink_state = 0;

/********************************************************************
* Function: 	main()
********************************************************************/
INT main(void)
{
   DWORD count1 = 0;
   BYTE timer_ro = 1;

   // switch to FRC w/PLL to keep up at higher baud rates (120MHz!
   PLLFBD=63;
   CLKDIVbits.PLLPOST = 0;
   CLKDIVbits.PLLPRE = 0;

    __builtin_write_OSCCONH(0x01);
   __builtin_write_OSCCONL(OSCCON | 0x01);

   while (OSCCONbits.COSC != 0b001);
   while (OSCCONbits.LOCK != 1);

   // Initialize I/O, UART and timer (interrupt)
   initIO();

   led1Off();
   led2Off();
   led3Off();

   printString("BL:V1.00:");
   
   if (ValidAppPresent())
   {
      while(count1<20)
      {
         if ((SWITCH1 == 0) || (SWITCH2 == 0))  // if either switch gets released, start app
            JumpToApp();

         // Blink LEDs
         if (timer_ro)
         {
            if (TMR1 > 7000)
            {
               blinkLEDs();
               count1++;
               timer_ro = 0;
            }
         }
         else if (TMR1 < 7000)
            timer_ro = 1;
      }
      printString("PB:");
   }
   else
   {
      printString("NA:");                    // No app present, enter bootloader regardless
   }

   T1CONbits.TON = 0;
   PR1 = 50000;                              // slow down blinking
   T1CONbits.TON = 1;

   blink_mode = 1;

   // Be in loop till framework recieves "run application" command from PC
   while(!ExitFirmwareUpgradeMode()) 
   {
      uartTask();          // Run Transport layer tasks
      if(FrameWorkTask())  // Run frame work related tasks (Handling Rx frame, process frame and so on)
      {
         blink_mode = 2;   // If we've communicated with the PC, use progress flashing
      }

      // Blink LEDs
      if (timer_ro)
      {
         if (TMR1 > 25000)
         {
            if (SWITCH1 && (SWITCH2 == 0))   // reset the device on SWITCH1 press
               reset();
            
            blinkLEDs();
            timer_ro = 0;
         }
      }
      else if (TMR1 < 25000)
         timer_ro = 1;
   }
   
	JumpToApp();
	return 0;
}			

/********************************************************************
* Function: 	JumpToApp()
********************************************************************/
void JumpToApp(void)
{
   printString("APP");
   void (*fptr)(void);
   fptr = (void (*)(void))0;
   fptr();
}

/********************************************************************
* Function: 	ValidAppPresent()
********************************************************************/
BOOL ValidAppPresent(void)
{
   volatile DWORD AppPtr;

   TBLPAG = 0x00;

   AppPtr = ((DWORD)__builtin_tblrdh(0) << 16);
   AppPtr = AppPtr | ((DWORD)__builtin_tblrdl(0));

   if(AppPtr == 0xFFFFFF)
      return FALSE;
   else
      return TRUE;
}

// Resets the microcontroller if SW1 is pressed (but SW2 is not pressed)
void __attribute__((__interrupt__,no_auto_psv)) _CNInterrupt(void)
{
   //if (SWITCH1 && (SWITCH2 == 0))
   //   reset();
   IFS1bits.CNIF = 0;
}

void blinkLEDs(void)
{
   if (blink_mode == 0)
   {
      led1Toggle();
      led2Toggle();
      led3Toggle();
   }
   else if (blink_mode == 1)
   {
      switch(blink_state)
      {
         case 0:
            blink_state = 1;
            led1On(); led2Off(); led3On();
            break;
         case 1:
            blink_state = 0;
            led1Off(); led2On(); led3Off();
            break;
      }
   }
   else if (blink_mode == 2)
   {
      switch(blink_state)
      {
         case 0:
            blink_state = 1;
            led1Off(); led2Off(); led3Off();
            break;
         case 1:
            blink_state = 2;
            led1On(); led2Off(); led3Off();
            break;
         case 2:
            blink_state = 3;
            led1On(); led2On(); led3Off();
            break;
         case 3:
            blink_state = 0;
            led1On(); led2On(); led3On();
            break;
      }
   }
   
   IFS0bits.T1IF = 0;
}
