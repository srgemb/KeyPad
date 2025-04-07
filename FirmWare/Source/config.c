
//*************************************************************************************************
//
// Модуль чтения/записи параметров в/из FLASH память
//
//*************************************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"
#include "parse.h"
#include "crc16.h"
#include "stm32f10x.h"
#include "cmsis_os2.h"

//*************************************************************************************************
// Переменные с внешним доступом
//*************************************************************************************************
CONFIG config;
FLASH_DATA flash_data;
FLASH_Status flash_status;

//*************************************************************************************************
// Локальные константы
//*************************************************************************************************
#define FLASH_DATA_ADDRESS      0x0801FC00      //адрес для хранения параметров
                                                //последняя страница во FLASH (1Kb)

//значения скорости для последовательных портов
static const uint32_t uart_speed[] = { 600, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 56000, 57600, 115200 };

//*************************************************************************************************
// Локальные переменные
//*************************************************************************************************
//расшифровка кодов записи во FLASH память
static char result[20];
static char * const mask_error[]  = { "UNLOCK-", "ERASE-", "PROGRAM-", "LOCK-" };
static char * const flash_error[] = { "OK", "BUSY", "PROG", "WRP", "COMPLETE", "TIMEOUT" };

//*************************************************************************************************
// Прототипы локальных функций
//*************************************************************************************************
static void ConfigDefault( void );

//*************************************************************************************************
// Инициализация параметров настроек значениями из FLASH
//-------------------------------------------------------------------------------------------------
// return = FLASH_Status - результат чтения/записи параметров
//*************************************************************************************************
FLASH_Status ConfigInit( void ) {

    bool change = false;
    uint16_t dw, dw_cnt;
    uint32_t *source_addr, *dest_addr;
    uint32_t *ptr_cofig, ptr_flash;
    
    source_addr = (uint32_t *)FLASH_DATA_ADDRESS;
    dest_addr = (uint32_t *)&flash_data;
    dw_cnt = sizeof( flash_data ) / sizeof( uint32_t );
    //читаем значение только как WORD по 4 байта
    for ( dw = 0; dw < dw_cnt; dw++ ) {
        *dest_addr = *(__IO uint32_t *)source_addr;
        source_addr++, 
        dest_addr++;
       }
    //проверка контрольной суммы
    if ( CalcCRC16( (uint8_t *)&flash_data, sizeof( flash_data ) ) ) {
        change = true;
        ConfigDefault();
       }
    else memcpy( (uint8_t *)&config, (uint8_t *)&flash_data.data, sizeof( config ) );
    if ( change == false )
        return FLASH_COMPLETE;
    //было изменение параметра, сохраним новое значения
    //разблокируем память
    FLASH_Unlock();
    flash_status = FLASH_GetStatus();
    if ( flash_status != FLASH_COMPLETE )
        return flash_status = ( flash_status | ERR_FLASH_UNLOCK );
    //стирание страницы памяти
    flash_status = FLASH_ErasePage( FLASH_DATA_ADDRESS );
    if ( flash_status != FLASH_COMPLETE )
        return flash_status = ( flash_status | ERR_FLASH_ERASE );
    //запись в FLASH память по 4 байта (32 бита)
    ptr_cofig = (uint32_t *)&flash_data;
    ptr_flash = FLASH_DATA_ADDRESS;
    for ( dw = 0; dw < dw_cnt; dw++, ptr_cofig++, ptr_flash += sizeof( uint32_t ) ) {
        flash_status = FLASH_ProgramWord( ptr_flash, *ptr_cofig );
        if ( flash_status != FLASH_COMPLETE )
            return flash_status = ( flash_status | ERR_FLASH_PROGRAMM );
       }
    //блокировка памяти
    FLASH_Lock();
    flash_status = FLASH_GetStatus();
    if ( flash_status != FLASH_COMPLETE )
        return flash_status = ( flash_status | ERR_FLASH_LOCK );
    else return flash_status;
 }

//*************************************************************************************************
// Функция сохраняет значения параметров в FLASH памяти
//-------------------------------------------------------------------------------------------------
// return = FLASH_Status - результат чтения/записи параметров
//*************************************************************************************************
uint8_t ConfigSave( void  ) {

    uint16_t dw, dw_cnt;
    uint32_t *ptr_cofig, ptr_flash;
    
    //начало критической секция кода
    osKernelLock();
    //разблокируем память
    FLASH_Unlock();
    flash_status = FLASH_GetStatus();
    if ( flash_status != FLASH_COMPLETE ) {
        //окончание критической секция кода
        osKernelUnlock();
        return flash_status = ( flash_status | ERR_FLASH_UNLOCK );
       }
    //стирание страницы памяти
    flash_status = FLASH_ErasePage( FLASH_DATA_ADDRESS );
    if ( flash_status != FLASH_COMPLETE ) {
        //окончание критической секция кода
        osKernelUnlock();
        return flash_status = ( flash_status | ERR_FLASH_ERASE );
       }
    //подготовка блока с параметрами для записи
    memset( (uint8_t *)&flash_data, 0x00, sizeof( flash_data ) );
    memcpy( (uint8_t *)&flash_data.data, (uint8_t *)&config, sizeof( config ) );
    flash_data.crc = CalcCRC16( (uint8_t *)&flash_data.data, sizeof( flash_data.data ) );
    //запись в FLASH память только как WORD по 4 байта
    ptr_cofig = (uint32_t *)&flash_data;
    ptr_flash = FLASH_DATA_ADDRESS;
    dw_cnt = sizeof( flash_data ) / sizeof( uint32_t );
    for ( dw = 0; dw < dw_cnt; dw++, ptr_cofig++, ptr_flash += sizeof( uint32_t ) ) {
        flash_status = FLASH_ProgramWord( ptr_flash, *ptr_cofig );
        if ( flash_status != FLASH_COMPLETE ) {
            //окончание критической секция кода
            osKernelUnlock();
            return flash_status = ( flash_status | ERR_FLASH_PROGRAMM );
           }
       }
    //блокировка памяти
    FLASH_Lock();
    //окончание критической секция кода
    osKernelUnlock();
    flash_status = FLASH_GetStatus();
    if ( flash_status != FLASH_COMPLETE )
        return flash_status = ( flash_status | ERR_FLASH_LOCK );
    else return flash_status;
 }

//*************************************************************************************************
// Установка параметров по умолчанию
//*************************************************************************************************
static void ConfigDefault( void ) {

    uint8_t i;
    
    config.uart_speed = 57600;
    for ( i = 0; i < MACROS_CNT; i++ )
        memset( config.macro_ctrl[i], 0x00, MACROS_SIZE );
    for ( i = 0; i < MACROS_CNT; i++ )
        memset( config.macro_alt[i], 0x00, MACROS_SIZE );
    memset( (uint8_t *)&flash_data, 0x00, sizeof( flash_data ) );
    memcpy( (uint8_t *)&flash_data.data, (uint8_t *)&config, sizeof( config ) );
    flash_data.crc = CalcCRC16( (uint8_t *)&flash_data.data, sizeof( flash_data.data ) );
 }

//*************************************************************************************************
// Функция проверяет значение скорости UART
//-------------------------------------------------------------------------------------------------
// return > 0 - значение скорости 600, 1200 ... 57600, 115200
//        = 0 - значение скорости указано не правильно
//*************************************************************************************************
uint8_t CheckSpeed( uint32_t speed ) {

    uint8_t i;
    
    for ( i = 0; i < SIZE_ARRAY( uart_speed ); i++ ) 
        if ( speed == uart_speed[i] )
            return i + 1;
    return 0;
 }
 
//*************************************************************************************************
// Функция возвращает значение скорости по индексу
//-------------------------------------------------------------------------------------------------
// uint8_t index - значение индекса скорости 1 ... MAX
// return > 0    - значение скорости 600, 1200 ... 57600, 115200
//        = 0    - значение индекса скорости указано не правильно
//*************************************************************************************************
uint32_t GetSpeed( uint8_t index ) {

    if ( index < SIZE_ARRAY( uart_speed ) + 1 )
        return uart_speed[index - 1];
    else return 0;
 }
 
//*************************************************************************************************
// Функция возвращает максимальное значение индекса скорости
//-------------------------------------------------------------------------------------------------
// uint8_t index - значение индекса скорости
//*************************************************************************************************
uint8_t GetMaxIndex( void ) {

    return SIZE_ARRAY( uart_speed );
 }
 
//*************************************************************************************************
// Функция возвращает текстовую расшифровку ошибок при записи во FLASH
//-------------------------------------------------------------------------------------------------
// uint8_t error - результат чтения/записи параметров
// return        - адрес строки с расшифровкой кода ошибки
//*************************************************************************************************
char *FlashDescErr( uint8_t error ) {

    if ( !error )
        return flash_error[error];
    memset( result, 0x00, sizeof( result ) );
    //имя источника ошибки
    if ( error & ERR_FLASH_UNLOCK )
        strcpy( result, mask_error[0] );
    if ( error & ERR_FLASH_ERASE )
        strcpy( result, mask_error[1] );
    if ( error & ERR_FLASH_PROGRAMM )
        strcpy( result, mask_error[2] );
    if ( error & ERR_FLASH_LOCK )
        strcpy( result, mask_error[3] );
    //ошибка функции
    strcat( result, flash_error[error & 0x0F] );
    return result;
 }
