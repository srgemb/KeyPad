
#ifndef __BUTTON_H
#define __BUTTON_H

#include <stdint.h>
#include <stdbool.h>

#include "stm32f10x.h"

#define INDEX_SCAN_0    0                   //номер бита сканирования
#define INDEX_SCAN_1    1                   //номер бита сканирования
#define INDEX_SCAN_2    2                   //номер бита сканирования
#define INDEX_SCAN_3    3                   //номер бита сканирования
#define INDEX_SCAN_4    4                   //номер бита сканирования
#define INDEX_SCAN_5    5                   //номер бита сканирования
#define INDEX_SCAN_6    6                   //номер бита сканирования
#define INDEX_SCAN_7    7                   //номер бита сканирования

#define MASK_KEY_01     0x01                //маска сканирования
#define MASK_KEY_02     0x02                //маска сканирования
#define MASK_KEY_04     0x04                //маска сканирования
#define MASK_KEY_08     0x08                //маска сканирования
#define MASK_KEY_10     0x10                //маска сканирования
#define MASK_KEY_20     0x20                //маска сканирования
#define MASK_KEY_40     0x40                //маска сканирования
#define MASK_KEY_80     0x80                //маска сканирования

#define SCAN_KEY_ALT    0x04                //коды сканирования клавиша ALT (индекс сканирования = 3)
#define SCAN_KEY_CTRL   0x10                //коды сканирования клавиша CTRL (индекс сканирования = 4)
#define SCAN_KEY_SHIFT  0x02                //коды сканирования клавиша SHIFT (индекс сканирования = 3)

#define KEY_ALT         0x01                //маски обработанных клавиш
#define KEY_CTRL        0x02                //маски обработанных клавиш
#define KEY_SHIFT       0x04                //маски обработанных клавиш
#define KEY_ALPHA       0x08                //маски обработанных клавиш

//*************************************************************************************************
// Функции управления
//*************************************************************************************************
void ButtonInit( void );
uint8_t BitsCount( uint8_t value );
int8_t GetNumBit( uint8_t value );

#endif 
