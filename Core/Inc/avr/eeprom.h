
#include <stdint.h>
#define EEMEM
uint8_t eeprom_read_byte (const uint8_t *__p);
void eeprom_write_byte (uint8_t *__p, uint8_t __value);
void eeprom_read_block (void *__dst, const void *__src, size_t __n);
uint16_t eeprom_read_word (const uint16_t *__p);
void eeprom_write_byte (uint8_t *__p, uint8_t __value);
void eeprom_write_block (const void *__src, void *__dst, size_t __n);

