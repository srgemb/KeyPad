
//*************************************************************************************************
//
// Модуль чтения/записи параметров в FLASH память
//
//*************************************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "param.h"
#include "parse.h"
#include "stm32f10x.h"
#include "cmsis_os2.h"

//*************************************************************************************************
// Переменные с внешним доступом
//*************************************************************************************************
CONFIG config;
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
static char * const flash_error[] = { "OK", "BUSY", "PROG", "WRP", "COMPLT", "TIMEOUT" };

//*************************************************************************************************
// Инициализация параметров настроек значениями из FLASH
//-------------------------------------------------------------------------------------------------
// return = FLASH_Status - результат чтения/записи параметров
//*************************************************************************************************
FLASH_Status ParamInit( void ) {

    bool change = false;
    uint8_t i, dw, dw_cnt;
    uint32_t *source_addr, *dest_addr;
    uint32_t *ptr_cofig, ptr_flash;
    
    source_addr = (uint32_t *)FLASH_DATA_ADDRESS;
    dest_addr = (uint32_t *)&config;
    dw_cnt = ( sizeof( config ) * 8 )/32;
    //читаем значение только как WORD по 4 байта
    for ( dw = 0; dw < dw_cnt; dw++ ) {
        *dest_addr = *(__IO uint32_t *)source_addr;
        source_addr++, 
        dest_addr++;
       }
    //проверим параметры на допустимостимые значения
    if ( config.uart_speed == 0xFFFFFFFF ) {
        change = true;
        config.uart_speed = 57600;
       }
    for ( i = 0; i < MACROS_CNT; i++ ) {
        if ( (uint8_t)config.macro_ctrl[i][0] == 0xFF ) {
            change = true;
            memset( config.macro_ctrl[i], 0x00, MACROS_SIZE );
           }
       }
    for ( i = 0; i < MACROS_CNT; i++ ) {
        if ( (uint8_t)config.macro_alt[i][0] == 0xFF ) {
            change = true;
            memset( config.macro_alt[i], 0x00, MACROS_SIZE );
           }
       }
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
    ptr_cofig = (uint32_t *)&config;
    ptr_flash = FLASH_DATA_ADDRESS;
    for ( dw = 0; dw < dw_cnt; dw++, ptr_cofig++, ptr_flash += 4 ) {
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
uint8_t ParamSave( void  ) {

    uint8_t dw, dw_cnt;
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
    //запись в FLASH память по 4 байта (32 бита)
    ptr_cofig = (uint32_t *)&config;
    ptr_flash = FLASH_DATA_ADDRESS;
    dw_cnt = ( sizeof( config ) * 8 )/32;
    for ( dw = 0; dw < dw_cnt; dw++, ptr_cofig++, ptr_flash += 4 ) {
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
