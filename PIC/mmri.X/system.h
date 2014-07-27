/* system.h
 * Peter Klein
 * Created on March 3, 2013, 8:06 PM
 * Description:
 *
 * This file contains #defines for use throughout the code. Oscillator frequency,
 * special I/O pins, etc. should be defined here. Also contains function prototypes
 * for system.c, which relate to system wide functions.
 *
 * Naming conventions:
 * functionName(), awesome_variable, p_ointer_variable, STATIC_DEFINE
 */

#ifndef SYSTEM_H
#define SYSTEM_H

// Include the microcontroller
#include "p33EP512MC806.h"
#include "stdint.h"

/** Oscillator related *********************************************/


/** Special I/O pins ***********************************************/

/** LEDs ***********************************************************/
#define LED3                     LATBbits.LATB14
#define LED4                     LATBbits.LATB13
#define LED5                     LATBbits.LATB12

#define led3On()                 LED3 = 0      // assumes LEDs are wired
#define led4On()                 LED4 = 0      // to sink current through
#define led5On()                 LED5 = 0      // the microcontroller

#define led3Off()                LED3 = 1
#define led4Off()                LED4 = 1
#define led5Off()                LED5 = 1

#define led3Toggle()             LED3 = !LED3
#define led4Toggle()             LED4 = !LED4
#define led5Toggle()             LED5 = !LED5

/** SWITCHES *******************************************************/
#define SWITCH1                  PORTEbits.RE2
#define SWITCH2                  PORTEbits.RE1

/** Basic definitions **********************************************/
#define ON                       1
#define OFF                      0
#define HIGH                     1
#define LOW                      0
#define TRUE                     1
#define FALSE                    0
#define YES                      1
#define NO                       0
#define NULL                     0

#define reset()                  __asm__ volatile("reset")
#define disiOn()                 __asm__ volatile("disi #0x3FFF")
#define disiOff()                __asm__ volatile("disi #0x0000")
#define disableInterrupts()      INTCON2bits.GIE = 0
#define enableInterrupts()       INTCON2bits.GIE = 1

// Function declarations
void changeClockFreq(unsigned char mhz);
unsigned long getClockFreq(void);

#endif	/* SYSTEM_H */

