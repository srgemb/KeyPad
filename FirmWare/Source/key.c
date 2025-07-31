
//*************************************************************************************************
//
// Модуль формирования кодов клавиш
//
//*************************************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "cmsis_os2.h"
#include "stm32f10x.h"
#include "gpio_stm32f10x.h"
#include "rte_device.h"

#include "driver_usart.h"

#include "key.h"
#include "key_def.h"
#include "data.h"
#include "events.h"
#include "button.h"

//*************************************************************************************************
// Локальные константы
//*************************************************************************************************

//первый индекс отвечает за индекс сканирования, второй - скан код клавиши
const uint8_t data_def[8][8] = {
    //скан коды клавиш
    //0x01--------0x02--------0x04--------0x08--------0x10--------0x20--------0x40--------0x80
    { KEY_DOWN,   KEY_F3,     0x00,       0x00,       0x00,       KEY_LEFT,   KEY_RIGHT,  KEY_MULTPL },
    { KEY_UP,     KEY_F1,     0x00,       0x00,       0x00,       KEY_F2,     KEY_ESC,    KEY_BS     },
    { KEY_5,      KEY_F5,     0x00,       0x00,       0x00,       KEY_4,      KEY_6,      KEY_DOT    },
    { KEY_COMMA,  0x00,       0x00,       0x00,       0x00,       KEY_0,      0x00,       0x00       },
    { 0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00       },
    { 0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00       },
    { KEY_2,      0x00,       0x00,       0x00,       0x00,       KEY_1,      KEY_3,      KEY_CR     },
    { KEY_8,      KEY_F4,     0x00,       0x00,       0x00,       KEY_7,      KEY_9,      KEY_PLUS   }
 };
 
const uint8_t data_shift[8][8] = {
    //скан коды клавиш
    //0x01--------0x02--------0x04--------0x08--------0x10--------0x20--------0x40--------0x80
    { KEY_PGDN,   KEY_F8,     0x00,       0x00,       0x00,       KEY_HOME,   KEY_END,    KEY_FSLASH },
    { KEY_PGUP,   KEY_F6,     0x00,       0x00,       0x00,       KEY_F7,     KEY_BSLASH, KEY_SPACE  },
    { KEY_5,      KEY_F10,    0x00,       0x00,       0x00,       KEY_4,      KEY_6,      KEY_COLON  },
    { KEY_DEL,    0x00,       0x00,       0x00,       0x00,       KEY_INS,    0x00,       0x00       },
    { 0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00       },
    { 0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00       },
    { KEY_2,      0x00,       0x00,       0x00,       0x00,       KEY_1,      KEY_3,      KEY_CR     },
    { KEY_8,      KEY_F9,     0x00,       0x00,       0x00,       KEY_7,      KEY_9,      KEY_MINUS  }
 };
 
const uint8_t data_alpha_up[8][8] = {
    //скан коды клавиш
    //0x01--------0x02--------0x04--------0x08--------0x10--------0x20--------0x40--------0x80
    { KEY_UP_H,   KEY_UP_F,   0x00,       0x00,       0x00,       KEY_UP_G,   KEY_UP_I,   KEY_UP_J   },
    { KEY_UP_C,   KEY_UP_A,   0x00,       0x00,       0x00,       KEY_UP_B,   KEY_UP_D,   KEY_UP_E   },
    { KEY_UP_R,   KEY_UP_P,   0x00,       0x00,       0x00,       KEY_UP_Q,   KEY_UP_S,   KEY_UP_T   },
    { KEY_UP_Z,   0x00,       0x00,       0x00,       0x00,       KEY_UP_Y,   0x00,       0x00       },
    { 0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00       },
    { 0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00       },
    { KEY_UP_V,   0x00,       0x00,       0x00,       0x00,       KEY_UP_U,   KEY_UP_W,   KEY_UP_X   },
    { KEY_UP_M,   KEY_UP_K,   0x00,       0x00,       0x00,       KEY_UP_L,   KEY_UP_N,   KEY_UP_O   }
 };
 
const uint8_t data_alpha_lo[8][8] = {
    //скан коды клавиш
    //0x01--------0x02--------0x04--------0x08--------0x10--------0x20--------0x40--------0x80
    { KEY_LO_H,   KEY_LO_F,   0x00,       0x00,       0x00,       KEY_LO_G,   KEY_LO_I,   KEY_LO_J   },
    { KEY_LO_C,   KEY_LO_A,   0x00,       0x00,       0x00,       KEY_LO_B,   KEY_LO_D,   KEY_LO_E   },
    { KEY_LO_R,   KEY_LO_P,   0x00,       0x00,       0x00,       KEY_LO_Q,   KEY_LO_S,   KEY_LO_T   },
    { KEY_LO_Z,   0x00,       0x00,       0x00,       0x00,       KEY_LO_Y,   0x00,       0x00       },
    { 0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00       },
    { 0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00,       0x00       },
    { KEY_LO_V,   0x00,       0x00,       0x00,       0x00,       KEY_LO_U,   KEY_LO_W,   KEY_LO_X   },
    { KEY_LO_M,   KEY_LO_K,   0x00,       0x00,       0x00,       KEY_LO_L,   KEY_LO_N,   KEY_LO_O   }
 };

//*************************************************************************************************
// Функция возвращает код нажатой кнопки
//-------------------------------------------------------------------------------------------------
// uint8_t alt_key - маска состояния альтернативных клавиш
// uint8_t index   - индекс сканирования
// uint8_t value   - скан коды клавиши
// return = 0      - в таблице нет соотвествия кода
//        > 0      - код нажатой кнопки
//*************************************************************************************************
uint8_t GetKey( uint8_t alt_key, uint8_t index, uint8_t value ) {

    int8_t sel;

    sel = GetNumBit( value );
    if ( sel == -1 )
        return 0; //установлено более 1 бита или не установлен ни одного бита
    
    if ( !alt_key )
        return data_def[index][sel];

    //нажата только клавиша ALT
    if ( alt_key & KEY_ALT && !( alt_key & KEY_SHIFT ) )
        return data_def[index][sel];
    if ( alt_key & KEY_ALT && ( alt_key & KEY_SHIFT ) )
        return data_shift[index][sel];
    
    //нажата только клавиша CTRL
    if ( alt_key & KEY_CTRL && !( alt_key & KEY_SHIFT ) )
        return data_def[index][sel];
    if ( alt_key & KEY_CTRL && ( alt_key & KEY_SHIFT ) )
        return data_shift[index][sel];

    //буквы в верхнем регистре
    if ( alt_key & KEY_ALPHA && ( alt_key & KEY_SHIFT ) )
        return data_alpha_up[index][sel];
    
    //буквы в нижнем регистре
    if ( alt_key & KEY_ALPHA && !( alt_key & KEY_SHIFT ) )
        return data_alpha_lo[index][sel];

    //нажата только клавиша SHIFT
    if ( alt_key & KEY_SHIFT )
        return data_shift[index][sel];

    return 0;
 }

