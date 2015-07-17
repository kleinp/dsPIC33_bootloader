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
   uint16_t reset = RCON;
   uint8_t uint8 = 0xA;
   int8_t int8 = 0x10;
   uint16_t uint16 = 67;
   int16_t int16 = 43;
   uint32_t uint32 = 0;
   int32_t int32 = 0;
   float fval = 0;

   initIO();

   changeClockFreq(120);

   mmriInit();
   initPeripherals0();
   
   mmriInitVar(10, UINT8, RW, VOL, NOPW, &uint8);
   mmriInitVar(11, INT8, RW, VOL, NOPW, &int8);
   mmriInitVar(12, UINT16, RW, VOL, NOPW, &uint16);
   mmriInitVar(13, INT16, RW, VOL, NOPW, &int16);
   mmriInitVar(14, UINT32, RW, VOL, NOPW, &uint32);
   mmriInitVar(15, INT32, RW, VOL, NOPW, &int32);
   mmriInitVar(16, FLOAT, RW, VOL, NOPW, &fval);

   printf("\nBuilt: %s, %s\nVersion: %05i %X\n\n", (char*)mmriGetRegPtr(1),
       (char*)mmriGetRegPtr(2), *(uint16_t*)mmriGetRegPtr(3), reset);

   while (1)
   {
      // Do Low priority stuff
      mmriMsgHandler();
      uint32++;
      fval+=0.05;
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

   if (SWITCH2)
   {
      led4Toggle();
   }

   // If both switches are pressed, reset microcontroller
   if (SWITCH1 && SWITCH2)
      reset();

   _CNIF = 0;
}