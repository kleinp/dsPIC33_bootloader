/* mmri.c
 * Peter Klein
 * Created on November 3, 2013, 6:50 PM
 * Description:
 *
 */

#include "system.h"
#include "mmri.h"
#include "uart.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

// Memory map structure
MMRIreg MMRI[MMRI_NUM];

/*
 * Address  Description                            Type     R/W   Default Value
 * -----------------------------------------------------------------------------
 * 0        Programmer defined tag                 STRING   0     "cake!"
 * 1        Build date                             STRING   0     varies
 * 2        Build time                             STRING   0     varies
 * 3        Build version                          UINT16   0     varies
 *             Has format ABBCC where
 *             A=0 for main code and A=1 for bootloader
 *             BB = major version
 *             CC = minor version
 * 4        User defined tag                       STRING   1     ""
 * 5        Password                               STRING   1     "abc123"
 * 6        Sysconfig                              UINT8    1     0x00
 *          - write 0x01 to reset
 *          - write 0x02 to save registers to NVM
 *          - write 0x03 to relaod from NVM
 *          - write 0x04 to pretty print all registers
 *          - write 0x05 to enter bootloader
 * 7        Uart baud rate                         UINT32   1     921600
 * 8        Uart echo                              UINT8    1     1 (echo on U1)
 */

// System registers
char *programmer_tag = "cake!";
char *build_date = __DATE__;
char *build_time = __TIME__;
uint16_t build_version = 00001;
char user_tag[20] = "hi";
char password[20] = "abc123";
uint8_t mmri_config = 0;
uint32_t u1_baud_rate = 921600;


// Global variables
uint8_t permission_level = 0;
char rw_stat[] = "ro";
char *type[] = {"undef ", "uint8 ", "int8  ", "uint16", "int16 ", "uint32",
   "int32 ", "float", "string"};

int8_t binary_val_lengths[] = {0, 1, 1, 2, 2, 4, 4, 4, 20};
uint8_t response_buffer[200];

/*******************************************************************************
 * Function:      mmriInit
 * Inputs:        None
 * Outputs:       None
 * Description:   This function initializes the MMRI structure, populating
 *                system registers, and reading data from NVM, if applicable
 * ****************************************************************************/
void mmriInit()
{
   uint16_t i;
   // Clear the memory
   for (i = 0; i < MMRI_NUM; i++)
   {
      MMRI[i].used = 0;
      MMRI[i].type = UNDEF;
   }

   mmriInitVar(0, STRING, RO, VOL, NOPW, programmer_tag);
   mmriInitVar(1, STRING, RO, VOL, NOPW, build_date);
   mmriInitVar(2, STRING, RO, VOL, NOPW, build_time);
   mmriInitVar(3, UINT16, RO, VOL, NOPW, &build_version);
   mmriInitVar(4, STRING, RW, NVM, NOPW, &user_tag[0]);
   mmriInitVar(5, STRING, RW, NVM, PWRW, &password[0]);
   mmriInitVar(6, UINT8, RW, VOL, NOPW, &mmri_config);
   mmriInitVar(7, UINT32, RW, NVM, PWWR, &u1_baud_rate);
}

/*******************************************************************************
 * Function:      mmriInitVar
 * Inputs:        <uint8 addr> the register address to use. 0-3 are used by the
 *                             system, so only 4-255 are available to the user
 *                <uint8 type> the type of data type will be put here
 *                             (UINT8, INT8, UINT16, INT16, UINT32, INT32,
 *                              FLOAT, STRING)
 *                <uint8 rw> read-only (RO) or read-write (RW) by the user
 *                <uint8 nvm> (NVM) if the parameter should be stored in
 *                            non-volatile memory. If not, (VOL)atile
 *                <uint8 pwp> (NOPW) to allow anyone to write the register
 *                            (PWWR) to require privilege to write
 *                            (PWRD) to require privilege to read/write
 *                <void *ptr> A pointer to the data. Data does not need to be
 *                            a local variable
 * Outputs:       None
 * Description:   Initializes a single register will all of its possible options
 * ****************************************************************************/
void mmriInitVar(uint8_t addr, uint8_t type, uint8_t rw, uint8_t nvm,
    uint8_t pwp, void *ptr)
{
   MMRI[addr].used = 1;
   MMRI[addr].rw = rw;
   MMRI[addr].type = type;
   MMRI[addr].pwp = pwp;
   MMRI[addr].nvm = nvm;
   MMRI[addr].ptr = ptr;
}

/*******************************************************************************
 * Function:      mmriPrintReg
 * Inputs:        <uint8 addr> address of the register to read
 * Outputs:       None
 * Description:   Prints the value associated with the register address
 *                provided. If the user does not have password privileges, it
 *                does not display pasword read protected registers (PWRD), but
 *                5 '*' characters instead
 * ****************************************************************************/
void mmriPrintReg(uint8_t addr)
{
   if (permission_level >= MMRI[addr].pwp)
   {
      switch (MMRI[addr].type)
      {
         case UINT8: printf("%u", *(uint8_t*)MMRI[addr].ptr);
            break;
         case INT8: printf("%i", *(int8_t*)MMRI[addr].ptr);
            break;
         case UINT16: printf("%u", *(uint16_t*)MMRI[addr].ptr);
            break;
         case INT16: printf("%i", *(int16_t*)MMRI[addr].ptr);
            break;
         case UINT32: printf("%lu", *(uint32_t*)MMRI[addr].ptr);
            break;
         case INT32: printf("%li", *(int32_t*)MMRI[addr].ptr);
            break;
         case FLOAT: printf("%f", *(double*)MMRI[addr].ptr);
            break;
         case STRING: printf("%s", (char*)MMRI[addr].ptr);
            break;
         default: printf("?");
      }
   }
   else
      printf("*****");
}

/*******************************************************************************
 * Function:      mmriWriteRegAscii
 * Inputs:        <uint8 addr> address of the register to write
 *                <char *value> value in ASCII string to write to register
 * Outputs:       <uint8> error code, or 0 if none
 * Description:   This function will write the value given by the ASCII string
 *                to the register address. Simple checks performed for correct
 *                data type. Also handles special register, such as password
 *                unlock/lock and config registers
 * ****************************************************************************/
uint8_t mmriWriteRegAscii(uint8_t addr, char *value)
{
   char *val = value;
   char *end;
   uint8_t tmp;
   uint32_t unsigned_val;
   int32_t signed_val;
   float float_val;

   // special case, register 2 - password
   if (addr == 2)
   {
      if (permission_level) // if we already have permission, write new password
      {
         strcpy(&password[0], val);
         return(NOERROR);
      }
      else // if not, compare the value given and stored password
      {
         if (strcmp(&password[0], val) == 0)
         {
            permission_level = 2; // give privileges if they match
            return(NOERROR);
         }
         else
            return(BADPASS);
      }
   }
   // special case register 3 - MMRI config
   if (addr == 3)
   {
      tmp = (uint8_t)atoi(val);
      switch (tmp)
      {
         case 1: // reset device
            reset();
            break;
         case 2: // Save register values to NVM
            return(NOERROR);
         case 3: // undo password permission level
            permission_level = 0;
            return(NOERROR);
         case 4:
            mmriPrintAllReg();
            return(NOERROR);
         default:
            return(BADVAL);
      }
   }
   // special case register 4 - UART baud rate
   if (addr == 4)
   {

   }
   if (MMRI[addr].rw && MMRI[addr].used) // the address is available and writable
   {
      switch (MMRI[addr].type)
      {
         case UINT8:
            unsigned_val = strtoul(val, &end, 0);
            if (unsigned_val > UCHAR_MAX)
               return(BADVAL);
            if (*end != '\0')
               return(BADVAL);
            *(uint8_t*)MMRI[addr].ptr = (uint8_t)unsigned_val;
            return(NOERROR);
         case INT8:
            signed_val = strtol(val, &end, 0);
            if (signed_val > CHAR_MAX || signed_val < CHAR_MIN)
               return(BADVAL);
            if (*end != '\0')
               return(BADVAL);
            *(int8_t*)MMRI[addr].ptr = (int8_t)signed_val;
            return(NOERROR);
         case UINT16:
            unsigned_val = strtoul(val, &end, 0);
            if (unsigned_val > UINT_MAX)
               return(BADVAL);
            if (*end != '\0')
               return(BADVAL);
            *(uint16_t*)MMRI[addr].ptr = (uint16_t)unsigned_val;
            return(NOERROR);
         case INT16:
            signed_val = strtol(val, &end, 0);
            if (signed_val > INT_MAX || signed_val < INT_MIN)
               return(BADVAL);
            if (*end != '\0')
               return(BADVAL);
            *(int16_t*)MMRI[addr].ptr = (int16_t)signed_val;
            return(NOERROR);
         case UINT32:
            unsigned_val = strtoul(val, &end, 0);
            if (*end != '\0')
               return(BADVAL);
            *(uint32_t*)MMRI[addr].ptr = unsigned_val;
            return(NOERROR);
         case INT32:
            signed_val = strtol(val, &end, 0);
            if (*end != '\0')
               return(BADVAL);
            *(int32_t*)MMRI[addr].ptr = signed_val;
            return(NOERROR);
         case FLOAT:
            float_val = strtod(val, &end);
            if (*end != '\0')
               return(BADVAL);
            *(float*)MMRI[addr].ptr = float_val;
            return(NOERROR);
         case STRING:
            if (strlen(val) > 20)
               return(BADVAL);
            strcpy((char*)MMRI[addr].ptr, val);
            return(NOERROR);
         default:
            return(UNKNOWN);
      }
   }
   else
      return(BADADDR);

   return(UNKNOWN); // should never get here
}

/*******************************************************************************
 * Function:      mmriPrintAllReg
 * Inputs:        None
 * Outputs:       None
 * Description:   Prints a table of all known register along with the selected
 *                options for that register. This function should mainly be used
 *                for program debugging
 * ****************************************************************************/
void mmriPrintAllReg(void)
{
   uint16_t i;
   printf("\n| ADR | RW | N | P | TYPE   | VALUE");
   for (i = 0; i < MMRI_NUM; i++)
   {
      if (MMRI[i].used)
      {
         if (MMRI[i].rw) // change 'rw' flag
            rw_stat[1] = 'w';
         else
            rw_stat[1] = 'o';

         printf("\n| %03i | %s | %i | %i | %s | ", i, &rw_stat[0], MMRI[i].nvm,
             MMRI[i].pwp, type[MMRI[i].type]);
         mmriPrintReg(i);
      }
   }
}

/*******************************************************************************
 * Function:      mmriGetRegPtr
 * Inputs:        <uint8_t addr> Which address' pointer to return
 * Outputs:       <void *> pointer to the memory location of the variable
 * Description:   Simply returns the pointer to where the variable at <addr> is
 *                located
 * ****************************************************************************/
void *mmriGetRegPtr(uint8_t addr)
{
   return(MMRI[addr].ptr);
}

/*******************************************************************************
 * Function:      mmriGetRegBin
 * Inputs:        <uint8_t addr> Which address' value to get
 *                <uint8_t *buf> Where to put the value result
 * Outputs:       <int16_t> how many bytes were written
 * Description:   Puts the value at <addr> into <*buf> in binary form.
 *                Returns how many bytes were written, which
 *                depends on size of variable
 * ****************************************************************************/
int16_t mmriGetRegBin(uint8_t addr, uint8_t *buf)
{
   static BYTEwise byte_separated;
   static int8_t num_bytes_copy;
   static uint8_t *str_ptr;

   int8_t num_bytes = binary_val_lengths[MMRI[addr].type];

   num_bytes_copy = num_bytes;

   if (permission_level >= MMRI[addr].pwp && MMRI[addr].used)
   {
      switch (MMRI[addr].type)
      {
         case UINT8:
         case INT8:
            *buf = *(uint8_t *)mmriGetRegPtr(addr);
            break;
         case UINT16:
         case INT16:
            byte_separated.val32 = (int32_t) *(uint16_t *)mmriGetRegPtr(addr);
            *buf++ = byte_separated.val8[0];
            *buf = byte_separated.val8[1];
            break;
         case UINT32:
         case INT32:
         case FLOAT:
            byte_separated.val32 = (int32_t) *(uint32_t *)mmriGetRegPtr(addr);
            *buf++ = byte_separated.val8[0];
            *buf++ = byte_separated.val8[1];
            *buf++ = byte_separated.val8[2];
            *buf = byte_separated.val8[3];
            break;
         case STRING:
            str_ptr = (uint8_t*)mmriGetRegPtr(addr);
            while (num_bytes--)
               *buf++ = *str_ptr++;
            break;
         default:
            while (num_bytes--)
               *buf++ = 0;
            break;
      }
   }
   else
   {
      while (num_bytes--)
         *buf++ = 0;
   }

   return(num_bytes_copy);
}

/*******************************************************************************
 * Function:      mmriGetRegAscii
 * Inputs:        <uint8_t addr> Which address' value to get
 *                <uint8_t *buf> Where to put the value result
 * Outputs:       <int16_t> how many bytes were written
 * Description:   Puts the value at <addr> into <*buf> in ASCII form.
 *                Returns how many bytes were written, which may be variable
 *                length depending on variable value and type
 * ****************************************************************************/
int16_t mmriGetRegAscii(uint8_t addr, uint8_t *buf)
{
   if (permission_level >= MMRI[addr].pwp && MMRI[addr].used)
   {
      switch (MMRI[addr].type)
      {
         case UINT8: return(sprintf((char *)buf, "%u", *(uint8_t*)MMRI[addr].ptr));
         case INT8: return(sprintf((char *)buf, "%i", *(int8_t*)MMRI[addr].ptr));
         case UINT16: return(sprintf((char *)buf, "%u", *(uint16_t*)MMRI[addr].ptr));
         case INT16: return(sprintf((char *)buf, "%i", *(int16_t*)MMRI[addr].ptr));
         case UINT32: return(sprintf((char *)buf, "%lu", *(uint32_t*)MMRI[addr].ptr));
         case INT32: return(sprintf((char *)buf, "%li", *(int32_t*)MMRI[addr].ptr));
         case FLOAT: return(sprintf((char *)buf, "%f", *(double*)MMRI[addr].ptr));
         case STRING: return(sprintf((char *)buf, "%s", (char*)MMRI[addr].ptr));
         default: return(sprintf((char *)buf, "?"));
      }
   }
   else if (MMRI[addr].used)
      return(sprintf((char *)buf, "**"));
   else
      return(sprintf((char *)buf, "?"));
}

/*******************************************************************************
 * Function:      mmriSetRegBin
 * Inputs:        <uint8_t addr> Which address' value to write
 *                <void *val> A pointer to the variable value
 * Outputs:       <int16_t> error code or 0 for sucess
 * Description:   Sets the variable at <addr> to value specified by <val> pointer
 * ****************************************************************************/
int16_t mmriSetRegBin(uint8_t addr, void *val)
{
   // special case, register 2 - password
   if (addr == 5)
   {
      if (permission_level) // if we already have permission, write new password
      {
         strcpy(&password[0], (char *)val);
         return(NOERROR);
      }
      else // if not, compare the value given and stored password
      {
         if (strcmp(&password[0], (char *)val) == 0)
         {
            permission_level = 2; // give privileges if they match
            return(NOERROR);
         }
         else
            return(BADPASS);
      }
   }
   // special case register 3 - MMRI config
   if (addr == 6)
   {
      switch (*(uint8_t*)val)
      {
         case 1: // reset device
            reset();
            break;
         case 2: // Save register values to NVM
            return(NOERROR);
         case 3: // undo password permission level
            permission_level = 0;
            return(NOERROR);
         case 4:
            mmriPrintAllReg();
            return(NOERROR);
         default:
            return(BADVAL);
      }
   }
   // special case register 4 - UART baud rate
   if (addr == 4)
   {
      return(BADVAL);
   }
   if (MMRI[addr].rw && MMRI[addr].used) // the address is available and writable
   {
      switch (MMRI[addr].type)
      {
         case UINT8:
            *(uint8_t*)MMRI[addr].ptr = *(uint8_t*)val;
            break;
         case INT8:
            *(int8_t*)MMRI[addr].ptr = *(int8_t*)val;
            break;
         case UINT16:
            *(uint16_t*)MMRI[addr].ptr = *(uint16_t*)val;
            break;
         case INT16:
            *(int16_t*)MMRI[addr].ptr = *(int16_t*)val;
            break;
         case UINT32:
            *(uint32_t*)MMRI[addr].ptr = *(uint32_t*)val;
            break;
         case INT32:
            *(int32_t*)MMRI[addr].ptr = *(int32_t*)val;
            break;
         case FLOAT:
            *(float*)MMRI[addr].ptr = *(float*)val;
            break;
         case STRING:
            strcpy((char*)MMRI[addr].ptr, (char*)val);
            break;
         default:
            return(UNKNOWN);
      }

      return(NOERROR);
   }

   return(UNKNOWN);
}

/*******************************************************************************
 * Function:      mmriSetRegAscii
 * Inputs:        <uint8_t addr> Which address' value to write
 *                <uint8_t *buf> An ASCII buffer which contains value
 * Outputs:       <int16_t> error code or 0 for sucess
 * Description:   Sets the variable at <addr> to value specified by <buf>
 * ****************************************************************************/
int16_t mmriSetRegAscii(uint8_t addr, uint8_t *buf)
{
   char *str = (char *)buf;
   static char *end;
   static uint32_t unsigned_val;
   static int32_t signed_val;
   static float float_val;
   // Convert ASCII to binary, and call binary function!

   switch (MMRI[addr].type)
   {
      case UINT8:
         unsigned_val = strtoul(str, &end, 0);
         if (unsigned_val > UCHAR_MAX)
            return(BADVAL);
         if (*end != '\0')
            return(BADVAL);
         return(mmriSetRegBin(addr, (void *)&unsigned_val));
      case INT8:
         signed_val = strtol(str, &end, 0);
         if (signed_val > CHAR_MAX || signed_val < CHAR_MIN)
            return(BADVAL);
         if (*end != '\0')
            return(BADVAL);
         return(mmriSetRegBin(addr, (void *)&signed_val));
      case UINT16:
         unsigned_val = strtoul(str, &end, 0);
         if (unsigned_val > UINT_MAX)
            return(BADVAL);
         if (*end != '\0')
            return(BADVAL);
         return(mmriSetRegBin(addr, (void *)&unsigned_val));
      case INT16:
         signed_val = strtol(str, &end, 0);
         if (signed_val > INT_MAX || signed_val < INT_MIN)
            return(BADVAL);
         if (*end != '\0')
            return(BADVAL);
         return(mmriSetRegBin(addr, (void *)&signed_val));
      case UINT32:
         unsigned_val = strtoul(str, &end, 0);
         if (*end != '\0')
            return(BADVAL);
         return(mmriSetRegBin(addr, (void *)&unsigned_val));
      case INT32:
         signed_val = strtol(str, &end, 0);
         if (*end != '\0')
            return(BADVAL);
         return(mmriSetRegBin(addr, (void *)&signed_val));
      case FLOAT:
         float_val = strtod(str, &end);
         if (*end != '\0')
            return(BADVAL);
         return(mmriSetRegBin(addr, (void *)&float_val));
      case STRING:
         if (strlen(str) > 20)
            return(BADVAL);
         if (*str)   // only use strcpy if the string isn't blank
            strcpy((char*)MMRI[addr].ptr, str);
         else
            *(char*)MMRI[addr].ptr = 0;

         return(NOERROR);
      default:
         return(UNKNOWN);
   }

   return(UNKNOWN);
}

/*******************************************************************************
 * Function:      mmriGetRegTypeBin
 * Inputs:        <uint8_t addr> Which address' type to get
 *                <uint8_t *buf> Where to put the type result
 * Outputs:       <int16_t> how many bytes were written
 * Description:   Puts the type of the value at <addr> into <*buf> in binary
 *                form. Returns how many bytes were written
 * ****************************************************************************/
int16_t mmriGetRegTypeBin(uint8_t addr, uint8_t *buf)
{
   switch (MMRI[addr].type)
   {
      case UINT8: *buf = UINT8;
         break;
      case INT8: *buf = INT8;
         break;
      case UINT16: *buf = UINT16;
         break;
      case INT16: *buf = INT16;
         break;
      case UINT32: *buf = UINT32;
         break;
      case INT32: *buf = INT32;
         break;
      case FLOAT: *buf = FLOAT;
         break;
      case STRING: *buf = STRING;
         break;
      default: *buf = UNDEF;
         break;
   }

   return(1);
}

/*******************************************************************************
 * Function:      mmriGetRegTypeAscii
 * Inputs:        <uint8_t addr> Which address' type to get
 *                <uint8_t *buf> Where to put the type result
 * Outputs:       <int16_t> how many bytes were written
 * Description:   Puts the type of the value at <addr> into <*buf> in ASCII
 *                form. Returns how many bytes were written
 * ****************************************************************************/
int16_t mmriGetRegTypeAscii(uint8_t addr, uint8_t *buf)
{
   switch (MMRI[addr].type)
   {
      case UINT8: return(sprintf((char *)buf, "UINT8"));
      case INT8: return(sprintf((char *)buf, "INT8"));
      case UINT16: return(sprintf((char *)buf, "UINT16"));
      case INT16: return(sprintf((char *)buf, "INT16"));
      case UINT32: return(sprintf((char *)buf, "UINT32"));
      case INT32: return(sprintf((char *)buf, "INT32"));
      case FLOAT: return(sprintf((char *)buf, "FLOAT"));
      case STRING: return(sprintf((char *)buf, "STRING"));
      default: return(sprintf((char *)buf, "?"));
   }
}

/*******************************************************************************
 * Function:      mmriMsgHandler
 * Inputs:        <none>
 * Outputs:       <none>
 * Description:   Checks if there are any messages available in the buffer
 *                (populated by U1 interrupt) and calls the appropriate
 *                parser
 *
 *                Note: The mmri interface hogs DMA0 and U1.. Try not to use
 *                      it for anything else
 * ****************************************************************************/
void mmriMsgHandler()
{
   int8_t * msg_ptr = uGetMmriMsg();

   if (msg_ptr) // a message is available
   {
      if (*msg_ptr == ':')
         mmriParseBinary(msg_ptr);
      else
         mmriParseAscii(msg_ptr);
   }
}

void mmriParseBinary(int8_t *buf)
{
   printf("#parsing binary\n");
}

void mmriParseAscii(int8_t *buf)
{
   static char *sub_str;
   const char *delim = ",\n\r\0";
   static uint8_t addr, num_regs;
   static int16_t error;

   char * str_buf = (char *)buf;
   uint16_t print_len = 0;

   // Ignore the ASCII start of message, if given
   if (*str_buf == '#')
      str_buf++;

   // First part of message is register
   sub_str = strtok(str_buf, delim);

   while (*sub_str)
   {
      // Convert string into value
      addr = (uint8_t)atoi(sub_str);

      // Second part is value, or '?'
      sub_str = strtok(NULL, delim);

      // If it is a '?', print contents of register. If ? is followed by a
      // number, print the contents of that number of registers. If it is a %,
      // or % followed by a number, display the type of that register(s). If it
      // is simply a number (or string), write to the register
      switch (*sub_str)
      {
         case '?':
            // If the next character is a newline or ',' the user did not
            // enter a number of registers to print.
            sub_str++;
            if (*sub_str == '\n' || *sub_str == ',' || *sub_str == '\r' || *sub_str == 0)
               num_regs = 1;
            else
               num_regs = (uint8_t)atoi(sub_str);

            // Print either # (beginning of message), or ',' to separate values
            if (print_len == 0)
               gp_buff[print_len++] = '#';
            else
               gp_buff[print_len++] = ',';

            // Add as many values as requested to the print buffer
            while (num_regs)
            {
               print_len += mmriGetRegAscii(addr++, &gp_buff[print_len]);
               num_regs--;

               if (num_regs)
                  gp_buff[print_len++] = ',';
            }
            break;
         case '%':
            // If the next character is a newline or ',' the user did not
            // enter a number of registers to print.
            sub_str++;
            if (*sub_str == '\n' || *sub_str == ',' || *sub_str == '\r' || *sub_str == 0)
               num_regs = 1;
            else
               num_regs = (uint8_t)atoi(sub_str);

            // Print either # (beginning of message), or ',' to separate multiple values
            if (print_len == 0)
               gp_buff[print_len++] = '#';
            else
               gp_buff[print_len++] = ',';

            // Add as many types as requested to the print buffer
            while (num_regs)
            {
               print_len += mmriGetRegTypeAscii(addr++, & gp_buff[print_len]);
               num_regs--;

               if (num_regs)
                  gp_buff[print_len++] = ',';
            }
            break;
         default:
            // We want to write to a register
            error = mmriSetRegAscii(addr, (uint8_t*)sub_str);

            if (error)
            {
               // Print either # (beginning of message), or ',' to separate multiple values
               if (print_len == 0)
                  gp_buff[print_len++] = '#';
               else
                  gp_buff[print_len++] = ',';

               gp_buff[print_len++] = 'E';
               gp_buff[print_len++] = error+'0';
            }
            break;
      }

      // Attempt to get another value
      sub_str = strtok(NULL, delim);
   }

   // If there is anything to print, add a newline to it, and start DMA transfer
   if (print_len)
   {
      gp_buff[print_len++] = '\n';
      while (uDmaStatus(MMRI_DMA));
      uDmaTx(&gp_buff[0], print_len, MMRI_DMA, U1);
   }
}