/* system.c
 * Peter Klein
 * Created on March 3, 2013, 8:06 PM
 * Description:
 *
 */

#include "system.h"

uint32_t clock_frequency = 7370000;  // FRC frequency

/*******************************************************************************
 * Function:      changeClockFreq
 * Inputs:        <uint8_t mhz> desired new frequency of internal clock
 * Outputs:       None
 * Description:   This function assumes an 8MHz XT oscillator is connected to
 *                the microcontroller. It uses the PLL module to allow the user
 *                to select a frequency between 15MHz and 140MHz. Note that
 *                rounding may occur at 80-140MHz freqencies. Clock switching
 *                takes about 1.6ms to complete and may be performed as many
 *                times as desired.
 *
 *                DON'T FORGET TO UPDATE THE BAUD RATE OF ANY
 *                UARTS YOU ARE USING AFTER CALLING THIS FUNCTION!
 * ****************************************************************************/
void changeClockFreq(uint8_t mhz)
{
   clock_frequency = (uint32_t)mhz*1000000;

   // Temporarily disable interrupts level 1-6
   disiOn();

   // If the CPU is currently running on something other than the FRC
   // switch to the FRC.
   if (OSCCONbits.COSC != 0b000)
   {
      // Initiate clock switch to FRC Oscillator
      __builtin_write_OSCCONH(0x00);
      __builtin_write_OSCCONL(OSCCON | 0x01);
      // Wait for clock switch to occur
      while (OSCCONbits.COSC != 0b000);
   }

   // Conform the desired frequency if necessary
   if (mhz < 15)
      mhz = 15;
   if (mhz > 140)
      mhz = 140;

   CLKDIVbits.PLLPRE = 0;

   if (mhz <= 40)             // 15-40 MHz
   {
      CLKDIVbits.PLLPOST = 3;
      PLLFBD = (mhz*2)-2;
   }
   else if (mhz <= 80)        // 40-80 MHz
   {
      CLKDIVbits.PLLPOST = 1;
      PLLFBD = mhz-2;
   }
   else                       // 80-140 MHz
   {
      CLKDIVbits.PLLPOST = 0;
      PLLFBD = (mhz/2)-2;
   }

   // Initiate clock switch to primary oscillator w/PLL
   __builtin_write_OSCCONH(0x03);
   __builtin_write_OSCCONL(OSCCON | 0x01);
   // Wait for clock switch to occur
   while (OSCCONbits.COSC != 0b011);
   while (OSCCONbits.LOCK != 1);

   // Re-enable interrupts level 1-6
   disiOff();   
}

/*******************************************************************************
 * Function:      getClockFreq
 * Inputs:        None
 * Outputs:       <uint32_t> clock frequency in Hz
 * Description:   Returns the clock frequency as set by changeClockFreq, or the
 *                default 7.37MHz of the internal FRC.
 *                DOES NOT WORK -- if you don't use changeClockFreq() to set
 *                oscillator operating speed
 * ****************************************************************************/
uint32_t getClockFreq(void)
{
   return clock_frequency;
}

