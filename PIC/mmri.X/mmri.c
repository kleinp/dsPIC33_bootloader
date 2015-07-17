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
   "int32 ", "float ", "string"};

uint8_t binary_val_lengths[] = {0, 1, 1, 2, 2, 4, 4, 4, 20};

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
      led3On();

      if (*(msg_ptr+1) == ':')
         mmriParseBinary(msg_ptr);
      else
         mmriParseAscii(msg_ptr);
      
      led3Off();
   }
}

/*******************************************************************************
 * Function:      mmriParseBinary
 * Inputs:        <int8_t *> buffer
 * Outputs:       <none>
 * Description:   Takes the string received by U1 and parses it assuming it
 *                is in binary format. Reads/write appropriate register(s)
 *                See mmri documentation for details
 * ****************************************************************************/
void mmriParseBinary(int8_t *buf)
{
   uint8_t *ptr_A = &gp_buff_A[0], *ptr_B = &gp_buff_B[0];

   uint16_t i, print_len = 0;
   int16_t var_len, msg_len = (int16_t)*buf++;
   uint8_t addr, num, error = 0;

   //printf("msg len: %i\n", msg_len);

   // Start of binary message
   gp_buff_A[print_len++] = ':';

   // if message length is less than 3, it contains nothing useful
   // first char is ':' followed by cmd type
   if (msg_len < 3)
   {
      error = BADLEN;  // error
   }
   else
   {
      // ignore the ':' and switch off of the next byte (command type)
      buf++;

      // take off the ':' and command type
      msg_len-=2;
      
      switch (*buf++)
      {
         case 1: // Get register(s)
            while(msg_len > 0)
            {
               addr = (uint8_t)*buf++;
               msg_len--;

               print_len += mmriGetRegBin(addr, &gp_buff_A[print_len]);
            }
            break;
         case 2: // Multi-get register(s)
            while(msg_len > 0)
            {
               addr = (uint8_t)*buf++;
               msg_len--;

               if (msg_len == 0) // incomplete command
               {
                  error = INCOMPLETE;
                  break;
               }

               num = *buf++;
               msg_len--;

               while(num)
               {
                  print_len += mmriGetRegBin(addr++, &gp_buff_A[print_len]);
                  num--;
               }
            }
            break;
         case 3: // Set register(s)
            while(msg_len > 0)
            {
               addr = (uint8_t)*buf++;
               msg_len--;

               var_len = (int16_t)binary_val_lengths[MMRI[addr].type];
               if (msg_len < var_len) // incomplete command
               {
                  error = INCOMPLETE;
                  break;
               }

               msg_len-=var_len;
               gp_buff_A[print_len] = mmriSetRegBin(addr, (void *)buf);
               print_len++;
               buf+=var_len;
            }
            break;
         case 4: // Get register(s) format
            while(msg_len > 0)
            {
               addr = *buf++;
               msg_len--;

               print_len += mmriGetRegTypeBin(addr, &gp_buff_A[print_len]);
            }
            break;
         case 5: // Multi-get register(s) format
            while(msg_len > 0)
            {
               addr = *buf++;
               msg_len--;

               if (msg_len == 0)
               {
                  error = INCOMPLETE;
                  break;
               }

               num = *buf++;
               msg_len--;

               while(num)
               {
                  print_len += mmriGetRegTypeBin(addr++, &gp_buff_A[print_len]);
                  num--;
               }
            }
            break;
         default: // unknown command type
            error = UNKNOWN;
            break;
      }
   }

   // If there was an error, just return error code and not the rest of message
   // since we can't tell in which part of the command the error occured
   if (error)
   {
      gp_buff_A[0] = ';';
      gp_buff_A[1] = error;
      print_len = 2;
   }

   // wait for gp_buff_B to be finished transmitting
   while (uDmaStatus(MMRI_DMA));
   
   // Add escape characters as required by transferring the data to another buffer
   i = print_len;
   while (i--)
   {
      if (*ptr_A == 0x10 || *ptr_A == 0xA)
      {
         *ptr_B++ = 0x10;
         *ptr_B++ = *ptr_A++;
         print_len++;
      }
      else
         *ptr_B++ = *ptr_A++;
   }

   gp_buff_B[print_len++] = '\n';
   uDmaTx(&gp_buff_B[0], print_len, MMRI_DMA, MMRI_U);

}

/*******************************************************************************
 * Function:      mmriParseAscii
 * Inputs:        <int8_t *> buffer
 * Outputs:       <none>
 * Description:   Takes the string received by U1 and parses it assuming it
 *                is in ASCII. Reads/writes appropriate register(s)
 *                See mmri documentation for details
 * ****************************************************************************/
void mmriParseAscii(int8_t *buf)
{
   static char *sub_str;
   const char *delim = ",\n\r\0";
   static uint8_t addr, num_regs;
   static int16_t error;

   char * str_buf = (char *)buf;
   uint16_t print_len = 0;

   // Ignore message length
   str_buf++;

   // Ignore the ASCII start of message, if given
   if (*str_buf == '#')
      str_buf++;

   // First part of message is register
   sub_str = strtok(str_buf, delim);

   // wait for gp_buff_B to be finished transmitting
   while (uDmaStatus(MMRI_DMA));

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
               gp_buff_A[print_len++] = '#';
            else
               gp_buff_A[print_len++] = ',';

            // Add as many values as requested to the print buffer
            while (num_regs)
            {
               print_len += mmriGetRegAscii(addr++, &gp_buff_A[print_len]);
               num_regs--;

               if (num_regs)
                  gp_buff_A[print_len++] = ',';
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
               gp_buff_A[print_len++] = '#';
            else
               gp_buff_A[print_len++] = ',';

            // Add as many types as requested to the print buffer
            while (num_regs)
            {
               print_len += mmriGetRegTypeAscii(addr++, & gp_buff_A[print_len]);
               num_regs--;

               if (num_regs)
                  gp_buff_A[print_len++] = ',';
            }
            break;
         default:
            // We want to write to a register
            error = mmriSetRegAscii(addr, (uint8_t*)sub_str);

            if (error)
            {
               // Print either # (beginning of message), or ',' to separate multiple values
               if (print_len == 0)
                  gp_buff_A[print_len++] = '#';
               else
                  gp_buff_A[print_len++] = ',';

               gp_buff_A[print_len++] = 'E';
               gp_buff_A[print_len++] = error + '0';
            }
            break;
      }

      // Attempt to get another value
      sub_str = strtok(NULL, delim);
   }

   // If there is anything to print, add a newline to it, and start DMA transfer
   if (print_len)
   {
      gp_buff_A[print_len++] = '\n';
      uDmaTx(&gp_buff_A[0], print_len, MMRI_DMA, MMRI_U);
   }
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
   uint16_t i, print_len = 0;

   // print header
   print_len += sprintf((char *)&gp_buff_A[print_len], "\n| ADR | RW | N | P | TYPE   | VALUE");

   for (i = 0; i < MMRI_NUM; i++)
   {
      // If the register is used, print its info
      if (MMRI[i].used)
      {
         if (MMRI[i].rw) // change 'rw' flag as necessary
            rw_stat[1] = 'w';
         else
            rw_stat[1] = 'o';

         // print type, read/write, password protection, and non-volatile about the register
         print_len += sprintf((char *)&gp_buff_A[print_len], "\n| %03i | %s | %i | %i | %s | ",
             i, &rw_stat[0], MMRI[i].nvm, MMRI[i].pwp, type[MMRI[i].type]);
         // print value in register
         print_len += mmriGetRegAscii(i, &gp_buff_A[print_len]);
      }
   }

   // If there is something to print, add a newline and start the DMA
   if (print_len)
   {
      gp_buff_A[print_len++] = '\n';
      while (uDmaStatus(MMRI_DMA));
      uDmaTx(&gp_buff_A[0], print_len, MMRI_DMA, MMRI_U);
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
 * Outputs:       <uint8_t> how many bytes were written
 * Description:   Puts the value at <addr> into <*buf> in binary form.
 *                Returns how many bytes were written, which
 *                depends on size of variable
 * ****************************************************************************/
uint8_t mmriGetRegBin(uint8_t addr, uint8_t *buf)
{
   static BYTEwise byte_separated;
   static uint8_t *str_ptr;

   int16_t i, remaining, num_bytes = 0;

   if (permission_level >= MMRI[addr].pwp && MMRI[addr].used)
   {
      switch (MMRI[addr].type)
      {
         case UINT8:
         case INT8:
            *buf = *(uint8_t *)mmriGetRegPtr(addr);
            num_bytes++;
            break;
         case UINT16:
         case INT16:
            byte_separated.val32 = (int32_t) *(uint16_t *)mmriGetRegPtr(addr);
            *buf++ = byte_separated.val8[0];
            *buf = byte_separated.val8[1];
            num_bytes+=2;
            break;
         case UINT32:
         case INT32:
         case FLOAT:
            byte_separated.val32 = (int32_t) *(uint32_t *)mmriGetRegPtr(addr);
            *buf++ = byte_separated.val8[0];
            *buf++ = byte_separated.val8[1];
            *buf++ = byte_separated.val8[2];
            *buf = byte_separated.val8[3];
            num_bytes+=4;
            break;
         case STRING:
            // First character of string response is length of string in bytes
            // followed by that many characters
            str_ptr = (uint8_t*)mmriGetRegPtr(addr);
            i = strlen((char *)str_ptr);
            remaining = 20-i;

            while (*str_ptr && i--)    // copy the string
               *buf++ = *str_ptr++;

            while(remaining--)         // pad with zeros
               *buf++ = 0;

            num_bytes+=20;
            break;
         default:
            break;
      }
   }
   else if (MMRI[addr].used)
   {
      i = binary_val_lengths[MMRI[addr].type];

      while (i--)
      {
         *buf++ = 0;
         num_bytes++;
      }
   }

   return(num_bytes);
}

/*******************************************************************************
 * Function:      mmriGetRegAscii
 * Inputs:        <uint8_t addr> Which address' value to get
 *                <uint8_t *buf> Where to put the value result
 * Outputs:       <uint8_t> how many bytes were written
 * Description:   Puts the value at <addr> into <*buf> in ASCII form.
 *                Returns how many bytes were written, which may be variable
 *                length depending on variable value and type
 * ****************************************************************************/
uint8_t mmriGetRegAscii(uint8_t addr, uint8_t *buf)
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
 * Outputs:       <uint8_t> error code or 0 for sucess
 * Description:   Sets the variable at <addr> to value specified by <val> pointer
 * ****************************************************************************/
uint8_t mmriSetRegBin(uint8_t addr, void *val)
{
   // special case, register 5 - password
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
   // special case register 6 - MMRI config
   if (addr == 6)
   {
      switch (*(uint8_t*)val)
      {
         case 1: // reset device
            reset();
            break;
         case 2: // Save register values to NVM
            // TODO: Implement NVM
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
   if (addr == 7)
   {
      // TODO: Implement baud rate changing
      return(BADVAL);
   }
   if (MMRI[addr].rw && MMRI[addr].used) // the address is available and writable
   {
      switch (MMRI[addr].type)
      {
         case UINT8:
         case INT8:
            memcpy(MMRI[addr].ptr, val, 1);
            break;
         case UINT16:
         case INT16:
            memcpy(MMRI[addr].ptr, val, 2);
            break;
         case UINT32:
         case INT32:
         case FLOAT:
            memcpy(MMRI[addr].ptr, val, 4);
            break;
         case STRING:
            memcpy(MMRI[addr].ptr, val, 20);
            break;
         default:
            return(UNKNOWN);
      }

      return(NOERROR);
   }
   else if (MMRI[addr].used)
   {
      // Bad permission to write to the register
      return(BADADDR);
   }

   return(UNKNOWN);
}

/*******************************************************************************
 * Function:      mmriSetRegAscii
 * Inputs:        <uint8_t addr> Which address' value to write
 *                <uint8_t *buf> An ASCII buffer which contains value
 * Outputs:       <uint8_t> error code or 0 for sucess
 * Description:   Sets the variable at <addr> to value specified by <buf> by
 *                converting ASCII to binary and calling the binary function
 *                (except for strings, which are written directly)
 * ****************************************************************************/
uint8_t mmriSetRegAscii(uint8_t addr, uint8_t *buf)
{
   char *str = (char *)buf;
   static char *end;
   static uint32_t unsigned_val;
   static int32_t signed_val;
   static float float_val;

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
         // The ASCII format doesn't require a length of 20, so this case
         // doesn't call the binary equivalent
         if (strlen(str) > 20)
            return(BADVAL);
         if (MMRI[addr].rw)
         {
            if (*str) // only use strcpy if the string isn't blank
               strcpy((char*)MMRI[addr].ptr, str);
            else
               *(char*)MMRI[addr].ptr = 0;
            
            return(NOERROR);
         }
         else
            return(BADADDR);
      default:
         return(UNKNOWN);
   }

   return(UNKNOWN);
}

/*******************************************************************************
 * Function:      mmriGetRegTypeBin
 * Inputs:        <uint8_t addr> Which address' type to get
 *                <uint8_t *buf> Where to put the type result
 * Outputs:       <uint8_t> how many bytes were written
 * Description:   Puts the type of the value at <addr> into <*buf> in binary
 *                form. Returns how many bytes were written
 * ****************************************************************************/
uint8_t mmriGetRegTypeBin(uint8_t addr, uint8_t *buf)
{
   *buf = (uint8_t)(MMRI[addr].config >> 1);

   return(1);
}

/*******************************************************************************
 * Function:      mmriGetRegTypeAscii
 * Inputs:        <uint8_t addr> Which address' type to get
 *                <uint8_t *buf> Where to put the type result
 * Outputs:       <uint8_t> how many bytes were written
 * Description:   Puts the type of the value at <addr> into <*buf> in ASCII
 *                form. Returns how many bytes were written
 * ****************************************************************************/
uint8_t mmriGetRegTypeAscii(uint8_t addr, uint8_t *buf)
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
      default: return(sprintf((char *)buf, "UNDEF"));
   }
}