/* main.c
 * Peter Klein
 * Created on March 3, 2013, 9:01 PM
 * Description:
 *
 */

#include "system.h"
#include "init.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mmri.h"
#include "uart.h"

/** Configuration bits *********************************************/
_FGS(GWRP_OFF & GSS_OFF & GSSK_OFF);
_FAS(AWRP_OFF & APL_OFF & APLK_OFF);
_FOSCSEL(FNOSC_FRC & IESO_OFF);
_FOSC(POSCMD_XT & OSCIOFNC_OFF & IOL1WAY_OFF & FCKSM_CSECME);
_FWDT(WDTPOST_PS1 & WDTPRE_PR32 & PLLKEN_ON & WINDIS_OFF & FWDTEN_OFF);
_FPOR(FPWRT_PWR8 & BOREN_OFF & ALTI2C1_OFF);
_FICD(ICS_PGD2 & RSTPRI_PF & JTAGEN_OFF);

int main(void)
{
   initIO();

   changeClockFreq(120);

   initPeripherals0();
   mmriInit();

   printf("\nBuilt: %s, %s\nVersion: %05i\n\n", (char*)mmriGetRegPtr(1),
       (char*)mmriGetRegPtr(2), *(uint16_t*)mmriGetRegPtr(3));

   while (1)
   {
      // Do Low priority stuff
   }

   return 0;
}

void __attribute__((__interrupt__, no_auto_psv)) _T3Interrupt(void)
{
   led5Toggle();

   // Do critical timing stuff

   _T3IF = 0;
}

void __attribute__((__interrupt__, no_auto_psv)) _CNInterrupt(void)
{
   Nop();

   if (SWITCH1)
   {
      led3Toggle();
   }

   if (SWITCH2)
   {
      led4Toggle();
   }

   // If both switches are pressed, reset microcontroller
   if (SWITCH1 && SWITCH2)
      reset();

   _CNIF = 0;
}