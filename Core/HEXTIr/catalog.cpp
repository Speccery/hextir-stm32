/*
    HEXTIr-SD - Texas Instruments HEX-BUS SD Mass Storage Device
    Copyright Jim Brain and RETRO Innovations, 2017

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License only.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    catalog.cpp: Helper functions to produce disk catalogs (directory listings).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "config.h"

#include "debug.h"
#include "drive.h"
#include "ff.h"
#include "hexbus.h"
#include "catalog.h"

#ifdef INCLUDE_DRIVE
// Global references
extern FATFS fs;

// ------------------------ local functions --------------------------
static const uint8_t FILE_SIZE_WIDTH = 5; // width of file size format

static int wild_cmp(const char *pattern, const char *string);
static uint32_t number_of_digits(uint32_t num);
static char* left_pad_with_blanks(char *buf, uint8_t width);
static char* format_file_size(uint32_t bytes, char* buf, uint8_t width);

// ------------------------- OLD/PGM catalog -------------------------

//static const PROGMEM
UCHAR   pgm_header[] = {0x80, 0x03};
//static const PROGMEM
UCHAR   pgm_trailer[] = {0xff, 0x7f, 0x03, 0x86, 0x00, 0x20, 0x00};

// constants
static const uint8_t PGM_HEADER_LEN = 4;  // number of bytes in pgm_header + 2 bytes for file length
static const uint8_t PGM_TRAILER_LEN = 7; // number of bytes in the pgm_trailer
static const uint8_t PGM_RECORD_LEN = 33; // length record (constant here), includes len. of line number
static const uint8_t PGM_LINE_LEN = 31;   // length of line, just the recordlen without len of line number (2 bytes)
static const uint8_t PGM_STR_LEN = 26;    // length of string without terminating zero

// called once at the beginning
void cat_open_pgm(uint16_t num_entries) {
  uint16_t i = PGM_RECORD_LEN * num_entries + PGM_HEADER_LEN;
  hex_send_byte(pgm_header[0]);
  hex_send_byte(pgm_header[1]);
  hex_send_byte(i & 255);
  hex_send_byte(i >> 8);
}

// called onece at the end
void cat_close_pgm(void) {
  for (uint8_t i = 0; i < sizeof(pgm_trailer); i++) {
    hex_send_byte(pgm_trailer[i]);
  }
}

// called multiple times, once for each catalog entry
void cat_write_record_pgm(uint16_t lineno, uint32_t fsize, const char* filename, uint8_t namelen, char attrib) {

  uint8_t i;
  uint8_t width = FILE_SIZE_WIDTH;
  char buf[width + 1];
  char* file_size = format_file_size(fsize, buf, width);

  hex_send_word(lineno);                // line number, 2 bytes
  hex_send_byte(PGM_LINE_LEN);          // length of next "code" line (without len. of line number), 1 byte
  hex_send_byte(0xa0);                  // 0xa0 : token for exclamation mark, next data is comment, 1 byte
  hex_send_byte(0xca);                  // 0xca : token for unquoted string, next data is string, 1 byte
  hex_send_byte(PGM_STR_LEN);           // length of the string without terminating zero, 1 byte
  for (i = 0; i < width; i++) {         // file size in bytes or kiB, 5 bytes
    hex_send_byte(file_size[i]);        //
  }                                     //
  hex_send_byte(' ');                   // blank, 1 byte
  hex_send_byte('\"');                  // quote, 1 byte
  for (i = 0; i < _MAX_LFN_LENGTH && i < namelen ; i++) { // file name padded with trailing blanks, at most _MAX_LFN_LENGTH bytes
    hex_send_byte(filename[i]);
  }
  hex_send_byte('\"');                  // quote, 1 byte
  for ( ; i < 17; i++) {
    hex_send_byte(' ');
  }
  hex_send_byte(attrib);                // file attribute, 1 byte
  hex_send_byte(0);                     // null termination of string, 1 byte
  // in total 33 bytes
}

uint16_t cat_file_length_pgm(uint16_t num_entries) {
  uint16_t len = num_entries * PGM_RECORD_LEN + PGM_HEADER_LEN + PGM_TRAILER_LEN;
  return len;
}

// ------------------------- OPEN/INPUT catalog -------------------------

uint16_t cat_max_file_length_txt(void) {
  // 1 byte for " to enclose 
  // FILE_SIZE_WIDTH bytes for file size in kB plus
  // 1 byte for " plus
  // 1 byte for "," separator plus
  // _MAX_LFN_LENGTH bytes max. for file name plus
  // 1 byte for "," separator plus
  // 1 byte for file attribute (F,D,V,..)
  uint16_t len = 1 + FILE_SIZE_WIDTH + 1 + 1 + _MAX_LFN_LENGTH + 4 + 1 + 1;
  return len;
}

// Output looks like : 10.2,HELLO.PGM,F
void cat_write_txt(uint16_t* dirnum, uint32_t fsize, const char* filename, uint8_t namelen, char attrib) {
  uint8_t i;
  uint8_t width = FILE_SIZE_WIDTH;
  char buf[width + 1];
  char* file_size = format_file_size(fsize, buf, width);

  int len = 1 + strlen(file_size) + 1 + 1 + namelen + 1 + 1; // length of data transmitted
  hex_send_word(len);  // length
  hex_send_byte('\"'); // because we have leading whitespaces
  for (i = 0; i < strlen(file_size)  ; i++) { // file size in kilo bytes, 4 byte
    hex_send_byte(file_size[i]);              //
  }                                           //
  hex_send_byte('\"');
  hex_send_byte(',');                         // "," separator, 1 byte
  for (i = 0; i < namelen; i++) {    // file name , max. _MAX_LFN_LENGTH bytes
    hex_send_byte(filename[i]);               //
  }                                           //
  hex_send_byte(',');                         // "," separator, 1 byte
  hex_send_byte(attrib);                      // file attribute, 1 byte

  *dirnum = *dirnum - 1; // decrement dirnum, used here as entries left to detect EOF for catalog when dirnum = 0
}

// ----------------------------- common -----------------------------------
// Return true if catalog entry shall be skipped.
BOOL cat_skip_file(const char* filename, const char* pattern) {
	BOOL skip = FALSE;
	if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
		skip = TRUE;
	}
	else if (pattern != (char*)NULL) {
		skip = (wild_cmp(pattern, filename) == 0 ? TRUE : FALSE); // skip, if pattern does not match
	}
	return skip;
}

// Get number of directory (=catalog) entries.
uint16_t cat_get_num_entries(FATFS* fsp, const char* directory, const char* pattern) {
  DIR dir;
  FILINFO fno;
# ifdef _MAX_LFN_LENGTH
  UCHAR lfn[_MAX_LFN_LENGTH + 1];
  fno.lfn = lfn;
#endif
  FRESULT res;
  uint16_t count = 0;
  char* filename;

  res = f_opendir(fsp, &dir, (UCHAR*)directory); // open the directory
  while (res == FR_OK) {
    res = f_readdir(&dir, &fno);                   // read a directory item
    if (res != FR_OK || fno.fname[0] == 0)
      break;  // break on error or end of dir
    filename = (char*)(fno.lfn[0] != 0 ? fno.lfn : fno.fname );
    if (cat_skip_file(filename, pattern))
      continue; // skip certain files like "." and ".."
    count++;
  }
  // no closedir needed.
  return count;
}

// Return true if string matches the pattern.
static int wild_cmp(const char *pattern, const char *string)
{
  if (*pattern == '\0' && *string == '\0') // Check if string is at end or not
    return 1;

  if ((*pattern == '?' && *string != '\0') || *pattern == *string) //Check for single character missing or match
    return wild_cmp(pattern + 1, string + 1);

  if (*pattern == '*') // Check for multiple character missing
    return wild_cmp((char*)(pattern + 1), string) || (*string != '\0' && wild_cmp(pattern, string + 1));


  return 0;
}

// Return the number of digits of a decimal number.
static uint32_t number_of_digits(uint32_t num) {
  return  (num == 0) ? 1  : ((uint32_t)log10(num) + 1);
}

static char* left_pad_with_blanks(char *buf, uint8_t width) {
  int shift = width - strlen(buf);
  if (shift > 0) {
    memmove(&buf[shift], buf, strlen(buf) + 1);
    memset(buf, ' ', shift);
  }
  return buf;
}

/**
 * Format the file size in bytes to an output of certain width.
 * Returns a char* pointer to the formatted output.
 * The working buffer buf must be at least of size width+1.
 * If the file size number of digits is smaller or equal the given
 * width, the output is in bytes, else the output is in kiB with an
 * accuracy of 0.1 kiB. If the file size it too large to fit into the
 * given width, a number of '?' is returned.
 * Example : width=5
              12345
 * 100    -> "  100"    bytes
 * 99999  -> "99999"    bytes
 * 100000 -> " 97.6"    kiB
 * 1023999-> "999.9"    kiB
 * 1024000-> "?????"    width too small
 */
static char* format_file_size(uint32_t bytes, char* buf, uint8_t width) {
  if (number_of_digits(bytes) <= width)  {
#ifdef STM32
	utoa(bytes, buf, 10);
#else
    ltoa(bytes, buf, 10);
#endif
    left_pad_with_blanks(buf, width);
  }
  else {
    int kb = bytes / 1024;
    if ( number_of_digits(kb) + 2 <= width) {
      int rb = (bytes % 1024) / (1024 / 10.0);
#ifdef STM32
      utoa(kb, buf, 10);
#else
      ltoa(kb, buf, 10);
#endif
      int l = strlen(buf);
      buf[l] = '.';
#ifdef STM32
      utoa(rb, &buf[l + 1], 10);
#else
      ltoa(rb, &buf[l + 1], 10);
#endif
      left_pad_with_blanks(buf, width);
    }
    else {
      memset(buf, '?', width);
      buf[width] = 0;
    }
  }
  return buf;
}


void hex_read_catalog_pgm(file_t *file) {
  hexstatus_t rc;
  BYTE res = FR_OK;
  char *filename;
  char attrib;
  uint32_t fsize;
  FILINFO fno;
  uint8_t namelen;
  #ifdef _MAX_LFN_LENGTH
  UCHAR lfn[_MAX_LFN_LENGTH + 1];
  fno.lfn = lfn;
  #endif

  debug_puts_P("Read PGM Catalog\r\n");
  hex_send_word(cat_file_length_pgm(file->dirnum));  // send full length of file
  cat_open_pgm(file->dirnum);
  uint16_t i = 1;
  while(i <= file->dirnum && res == FR_OK) {
#ifdef _MAX_LFN_LENGTH
    memset(lfn, 0, sizeof(lfn));
#endif
    res = f_readdir(&file->dir, &fno);                   // read a directory item
    if (res != FR_OK || fno.fname[0] == 0)
      break;  // break on error or end of dir
    filename = (char*)(fno.lfn[0] != 0 ? fno.lfn : fno.fname );
    attrib = ((fno.fattrib & AM_DIR) ? 'D' : ((fno.fattrib & AM_VOL) ? 'V' : 'F'));
    fsize = fno.fsize;
    namelen = strlen(filename);

    if (cat_skip_file(filename, file->pattern))
      continue; // skip certain files like "." and ".."

    debug_trace(filename, 0, namelen);

    cat_write_record_pgm(i++, fsize, filename, namelen, attrib);
  }
  cat_close_pgm();
  //debug_putc('>');
  rc = HEXSTAT_SUCCESS;
  hex_send_byte( rc ); // status byte transmit
  hex_finish();
}

void hex_read_catalog_txt(file_t * file) {
  hexstatus_t rc;
  BYTE res = FR_OK;
  FILINFO fno;
  #ifdef _MAX_LFN_LENGTH
  UCHAR lfn[_MAX_LFN_LENGTH+1];
  fno.lfn = lfn;
  #endif
  char* filename;
  uint8_t namelen;
  char attrib;
  uint32_t size;

  debug_puts_P("Read TXT Catalog\r\n");
  // the loop is to be able to skip files that shall not be listed in the catalog
  // else we only go through the loop once
  do {
# ifdef _MAX_LFN_LENGTH
    memset(lfn, 0, sizeof(lfn));
# endif
    res = f_readdir(&file->dir, &fno); // read a directory item
    if (res != FR_OK) {
      break; // break on error, leave do .. while loop
    }
    filename = (char*)(fno.lfn[0] != 0 ? fno.lfn : fno.fname );
    namelen = strlen(filename);
    attrib = ((fno.fattrib & AM_DIR) ? 'D' : ((fno.fattrib & AM_VOL) ? 'V' : 'F'));
    size = fno.fsize;
    if (filename[0] == 0) {
      res = FR_NO_FILE; // OK  to set this ?
      break;  // break on end of dir, leave do .. while loop
    }

    if (cat_skip_file(filename, file->pattern))
      continue; // skip certain files like "." and "..", next do .. while
    break; // success, leave the do .. while loop
  } while(1);

  switch(res) {
    case FR_OK:
      debug_trace(filename, 0, namelen);

      // write the calatog entry for OPEN/INPUT
      // TODO can the below have a bad return code?
      cat_write_txt(&(file->dirnum), size, filename, namelen, attrib);
      rc = HEXSTAT_SUCCESS;
      break;
    case FR_NO_FILE:
      hex_send_word(0); // zero length data
      rc = HEXSTAT_EOF;
      break;
    default:
      hex_send_word(0); // zero length data
      rc = HEXSTAT_DEVICE_ERR;
      break;
  }
  hex_send_byte(rc);    // status code
  hex_finish();
}

void hex_open_catalog(file_t *file, uint8_t lun, uint8_t att, char* path) {
  hexstatus_t rc = HEXSTAT_SUCCESS;
  uint16_t fsize = 0;
  uint8_t len;
  BYTE res = FR_OK;

  debug_puts_P("Open Catalog\r\n");
  if (!(att & OPENMODE_READ)) {
    // send back error on anything not OPENMODE_READ
    res =  FR_IS_READONLY;
  } else {
    if(file != NULL) {
      file->attr |= FILEATTR_CATALOG;
      // remove the leading $
      char* string = path;
      /* this will force the root directory to be read
      if (strlen(string) < 2 || string[1] != '/') { // if just $ or $ABC..
        string[0] = '/'; // $ -> /, $ABC/ -> /ABC/
      } else {
        string = (char*)&path[1]; // $/ABC/... -> /ABC/...
      }
      */
      string = (char*)&path[1]; // $subdir... -> subdir...
      if (string[0] == '\0') { // empty path, add a pattern
        string[0] = '*';
        string[1] = '\0';
      }
      len = strlen(string);
      trim(&string, &len);
      // separate into directory path and pattern
      char* dirpath = string;
      char* pattern = (char*)NULL;
      char* s =  strrchr(string, '/');
      if (strlen(s) > 1) {       // there is a pattern
        pattern = strdup(s + 1); // copy pattern to store it, will be freed in free_lun
        *(s + 1) = '\0';         // set new terminating zero for dirpath
        debug_trace(pattern, 0, strlen(pattern));
      }
      else if (s==NULL){ // no directory, dirpath is the pattern
        pattern = strdup(dirpath);
        dirpath[0] = '\0';
      }
      // if not the root slash, remove slash from dirpath
      if (strlen(dirpath) > 1 && dirpath[strlen(dirpath) - 1] == '/')
        dirpath[strlen(dirpath) - 1] = '\0';
      // get the number of catalog entries from dirpath that match the pattern
      file->dirnum = cat_get_num_entries(&fs, dirpath, pattern);
      if (pattern != (char*)NULL)
        file->pattern = pattern; // store pattern, will be freed in free_lun
      // the file size is either the length of the PGM file for OLD/PGM or the max. length of the txt file for OPEN/INPUT.
      fsize = (lun == 0 ? cat_file_length_pgm(file->dirnum)  : cat_max_file_length_txt());
      res = f_opendir(&fs, &(file->dir), (UCHAR*)dirpath); // open the director
    } else {
      // too many open files.
      rc = HEXSTAT_MAX_LUNS;
    }
  }
  if (rc == HEXSTAT_SUCCESS) {
    switch(res) {
      case FR_OK:
        rc = HEXSTAT_SUCCESS;
        break;
      case FR_IS_READONLY:
        rc = HEXSTAT_OUTPUT_MODE_ERR; // is this the right return value ?
        break;
      default:
        rc = HEXSTAT_NOT_FOUND;
        break;
    }
  }
  if(rc == HEXSTAT_SUCCESS) {
    if(!hex_is_bav()) { // we can send response
      hex_send_word(4);    // claims it is accepted buffer length, but looks to really be my return buffer length...
      hex_send_word(fsize);
      hex_send_word(0);
      hex_send_byte(HEXSTAT_SUCCESS);    // status code
      hex_finish();
    }
  } else {
    hex_send_final_response( rc );
  }
}
#endif
