
#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#include "stm32f10x.h"

#include "parse.h"

#define MACROS_CNT              10
#define MACROS_SIZE             MAX_LEN_PARAM - 1

//маски ошибок источника ошибки при работе с FLASH памятью
#define ERR_FLASH_UNLOCK        0x10        //разблокировка памяти
#define ERR_FLASH_ERASE         0x20        //стирание FLASH
#define ERR_FLASH_PROGRAMM      0x40        //сохранение параметров
#define ERR_FLASH_LOCK          0x80        //блокировка памяти

//результат обработки команды
typedef enum {
    EXEC_OK,                                //команда распознана и выполнена
    EXEC_NO_FIND,                           //команда не найдена
    EXEC_NO_COMMAND                         //команда не введена
 } ExecResult;

#pragma pack( push, 1 )

//*************************************************************************************************
// Структура хранения параметров уст-ва
//*************************************************************************************************
typedef struct {
    uint32_t uart_speed;                    //скорость обмена данными c терминалом
    char macro_ctrl[MACROS_CNT][MACROS_SIZE];
    char macro_alt[MACROS_CNT][MACROS_SIZE];
 } CONFIG;

//структура чтения/записи параметров из/в FLASH памяти
typedef struct {
    uint8_t  data[816 - sizeof( uint16_t )];
    uint16_t crc;
 } FLASH_DATA;

#pragma pack( pop )

//*************************************************************************************************
// Прототипы функций
//*************************************************************************************************
FLASH_Status ConfigInit( void );
uint8_t ConfigSave( void );
uint8_t CheckSpeed( uint32_t speed );
uint32_t GetSpeed( uint8_t index );
uint8_t GetMaxIndex( void );
char *FlashDescErr( uint8_t error );

#endif
