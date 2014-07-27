/* init.c
 * Peter Klein
 * Created on March 7, 2013, 5:40 PM
 * Description:
 *
 */

#include "system.h"
#include "uart.h"

void initIO(void)
{
   // ** Port A ****************************************************************
   // Pin      I/O?  Default  Description
   // 0-15     N/A   N/A      Unimplemented on this device

   // ** Port B ****************************************************************
   // Pin      I/O?  Default  Description
   // 0        I     N/A      Analog Distance
   // 1        I     N/A      Analog Vsys (scaled)
   // 2        I     N/A      A0 general purpose analog in
   // 3        I     N/A      A1 general purpose analog in
   // 4        I     N/A      A2 general purpose analog in
   // 5        I     N/A      A3 general purpose analog in
   // 6-11     O     0        Unused (low output)
   // 12       O     1        LED "D5"
   // 13       O     1        LED "D4"
   // 14       O     1        LED "D3"
   // 15       O     0        Unused (low output)
   ANSELB = 0x003F;
   LATB = 0x7000;
   TRISB = 0x003F;

   // ** Port C ****************************************************************
   // Pin      I/O?  Default  Description
   // 0-11     N/A   N/A      Unimplemented on this device
   // 12       I     N/A      XTAL-1
   // 13       I     N/A      PGD (programming)
   // 14       I     N/A      PGC (programming)
   // 15       I     N/A      XTAL-2
   ANSELC = 0x0000;
   LATC = 0x0000;
   TRISC = 0xF000;

   // ** Port D ****************************************************************
   // Pin      I/O?  Default  Description
   // 0        O     0        LMA (left motor A)
   // 1        O     1        CS_G (chip select gyro data)
   // 2        O     1        CS_A (chip select accel data)
   // 3        O     1        CLK_A/G (clock output for accel/gyro)
   // 4        I     N/A      MISO_G (master in for gyro data)
   // 5        O     1        MOSI_A/G (master out for accel/gyro data)
   // 6        I     N/A      MISO_A (master in for accel data)
   // 7        O     0        ~SLEEP (h-bridge standby)
   // 8        I     N/A      DIR_L (quadrature direction input)
   // 9        O     1        DIR_L (direction output for quadrature)
   // 10       I     N/A      DIR_R (quadrature direction input)
   // 11       O     1        DIR_R (direction otuput for quadrature)
   // 12-15    N/A   N/A      Unimplemented on this device
   ANSELD = 0x0000;
   LATD = 0x0A2E;
   TRISD = 0x0550;

   // ** Port E ****************************************************************
   // Pin      I/O?  Default  Description
   // 0        I     N/A      ENC_R (quadrature encoder input)
   // 1        I     N/A      Push button (S1)
   // 2        I     N/A      Push button (S0)
   // 3-4      O     0        Unused (low output)
   // 5        O     0        D3 (general purpose digital I/O)
   // 6        O     0        D2 (general purpose digital I/O)
   // 7        O     0        D1 (general purpose digital I/O)
   // 8-15     N/A   N/A      Unimplemented on this device
   ANSELE = 0x0000;
   LATE = 0x0000;
   TRISE = 0x0007;

   // ** Port F ****************************************************************
   // Pin      I/O?  Default  Description
   // 0        I     N/A      FAULT (h-bridge fault)
   // 1        I     N/A      ENC_L (quadrature encoder input)
   // 2        O     0        RMB (right motor B)
   // 3        O     0        LMB (left motor B)
   // 4        I     N/A      RX (bluetooth UART)
   // 5        O     0        TX (bluetooth UART)
   // 6        O     0        RMA (right motor A)
   // 7-15     N/A   N/A      Unimplemented on this device
   LATF = 0x0000;
   TRISF = 0x0013;

   // ** Port G ****************************************************************
   // Pin      I/O?  Default  Description
   // 0-1      N/A   N/A      Unimplemented on this device
   // 2        O     0        SCL (I2C clock line)
   // 3        O     0        SDA (I2C data line)
   // 4-5      N/A   N/A      Unimplemented on this device
   // 6        O     0        D0 (general purpose digital I/O)
   // 7        O     0        Unused (low output)
   // 8        O     0        Unused (low output)
   // 9        O     0        Unused (low output)
   // 10-15    N/A   N/A      Unimplemented on this device
   ANSELG = 0x0000;
   LATG = 0x0000;
   TRISG = 0x0000;

   // Unlock the pin configuration registers
   __builtin_write_OSCCONL(OSCCON & 0xBF);

   // UART1 bluetooth serial port
   _RP101R = 0b000001; // Tx
   _U1RXR = 100; // Rx

   // Encoder left
   _QEA1R = 97; // ENC_L
   _QEB1R = 72; // DIR_L

   // Encoder right
   _QEA2R = 80; // ENC_R
   _QEB2R = 74; // DIR_R

   // SPI for LSM330
   _SDI1R = 70; // MISO_A
   _SDI3R = 68; // MISO_G
   _RP69R = 0b000101; // MOSI_AG
   _RP67R = 0b000110; // CLK_AG

   // PWM for h-bridge
   _RP64R = 0b010000; // OC1
   _RP99R = 0b010001; // OC2
   _RP102R = 0b010010; // OC3
   _RP98R = 0b010011; // OC4

   // Lock the pin configuration registers
   __builtin_write_OSCCONL(OSCCON | 0x40);
}

void initPeripherals0(void)
{
   // Set up CN interrupt for pushbuttons (PB0=S1, PB1=S2)
   CNENEbits.CNIEE1 = 1;
   CNENEbits.CNIEE2 = 1;
   _CNIP = 0x02;
   _CNIF = 0;
   _CNIE = 1;

   // Combine timer 2/3 for main interrupt
   T2CON = 0x0000;
   T3CON = 0x0000;
   T2CONbits.T32 = 1;
   PR2 = (uint16_t)((getClockFreq() / 2 / 5) & 0xFFFF);
   PR3 = (uint16_t)((getClockFreq() / 2 / 5) >> 16 & 0xFFFF);
   _T3IP = 0x03;
   _T3IF = 0;
   _T3IE = 1;
   T2CONbits.TON = 1;

   u1Init(921600, NO_PARITY, ONE_STOP_BIT);
}