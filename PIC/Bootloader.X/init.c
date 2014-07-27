/* init.c
 * Peter Klein
 * Created on March 7, 2013, 5:40 PM
 * Description:
 *
 */

#include "system.h"

void initIO(void)
{

   // Disable analog pin functionality
   ANSELB = 0x00;
   ANSELC = 0x00;
   ANSELD = 0x00;
   ANSELE = 0x00;
   ANSELG = 0x00;

   // Set some pins as outputs
   TRISBbits.TRISB12 = 0;      // LED1
   TRISBbits.TRISB13 = 0;      // LED2
   TRISBbits.TRISB14 = 0;      // LED3

   // UART
   TRISFbits.TRISF5 = 0;      // UART1 RX (bluetooth)
   TRISFbits.TRISF4 = 1;      // UART1 TX (bluetooth)

   // Unlock the pin configuration registers
   __builtin_write_OSCCONL(OSCCON & 0xBF);

   RPOR9bits.RP101R = 1;             // UART1 TX
   RPINR18bits.U1RXR = 100;          // UART1 RX

   // Lock the pin configuration registers
   __builtin_write_OSCCONL(OSCCON | 0x40);

   // Timer setup for LED blinking
   T1CONbits.TON = 0;
   T1CONbits.TGATE = 0;
   T1CONbits.TCKPS = 3;
   T1CONbits.TCS = 0;
   PR1 = 14000;
   T1CONbits.TON = 1;
   
   // Initialize UART1
   U1BRG = 32;              // 460800
   U1MODE = 0;
   U1STA = 0;
   U1MODEbits.BRGH = 1;
   U1MODEbits.UARTEN = 1;
   U1MODEbits.STSEL = 0;
   U1STAbits.UTXEN = 1;
   U1STAbits.OERR = 0;

}