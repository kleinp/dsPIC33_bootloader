/* mmri.h
 * Peter Klein
 * Created on November 3, 2013, 6:50 PM
 * Description:
 *
 */

#ifndef MMRI_H
#define	MMRI_H

// Number of variables in memory map !! NOTE: MAX OF 256 (8-bit value) !!
#define MMRI_NUM    256

#if MMRI_NUM > 256
#error "Number of memory map variables too big (256 max)"
#endif

// Allowable variable types for a register
#define UNDEF       0
#define UINT8       1
#define INT8        2
#define UINT16      3
#define INT16       4
#define UINT32      5
#define INT32       6
#define FLOAT       7
#define STRING      8       // strings are 20 characters long!

// Defines for variable init
#define RO          0      // Read only variable
#define RW          1      // Read/write variable
#define VOL         0      // Volatile variable
#define NVM         1      // Save/restore variable from NVM if possible
#define NOPW        0      // No password needed to read/write
#define PWWR        1      // Password required to write to variable
#define PWRW        2      // Password required to read/write the variable

// Defines for error codes
#define NOERROR      0        // All Good!
#define UNKNOWN      1        // Unknown error has occured
#define BADCS        2        // Bad checksum
#define BADADDR      3        // Address not used, recognized, read-only, or out of range
#define BADVAL       4        // Value too big, small, long, negative, or NAN
#define BADPASS      5        // Bad password entered, or not privileged to access

// Which DMA does MMRI use to send out
#define MMRI_DMA     0        // DMA0

// Structure of a single register
typedef struct
{
   union
   {
      uint16_t config;
      struct
      {
         uint16_t used : 1;
         uint16_t rw : 1;
         uint16_t nvm : 1;
         uint16_t pwp : 2;
         uint16_t type : 4;
      };
   };
   void *ptr; // pointer to the variable
} MMRIreg;
typedef union
{
   int32_t val32;
   int8_t val8[4];
} BYTEwise;

void mmriInit(void);
void mmriInitVar(uint8_t addr, uint8_t type, uint8_t rw, uint8_t nvm, uint8_t pwp, void *ptr);
void mmriPrintReg(uint8_t addr);
uint8_t mmriWriteRegAscii(uint8_t addr, char *value);
void mmriPrintAllReg(void);


void *mmriGetRegPtr(uint8_t addr);
int16_t mmriGetRegBin(uint8_t addr, uint8_t *buf);
int16_t mmriGetRegAscii(uint8_t addr, uint8_t *buf);
int16_t mmriSetRegBin(uint8_t addr, void *val);
int16_t mmriSetRegAscii(uint8_t addr, uint8_t *buf);
void mmriPrintError(int8_t ascii_bin, uint8_t error);
void mmriMsgHandler();
void mmriParseBinary(int8_t *buf);
void mmriParseAscii(int8_t *buf);

#endif	/* MMRI_H */

