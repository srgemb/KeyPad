
#ifndef __PORTS_H
#define __PORTS_H

#include <stdint.h>
#include <stdbool.h>

#include "gpio_stm32f10x.h"

//Номера пинов порта
#define PIN_0                      0
#define PIN_1                      1
#define PIN_2                      2
#define PIN_3                      3
#define PIN_4                      4
#define PIN_5                      5
#define PIN_6                      6
#define PIN_7                      7
#define PIN_8                      8
#define PIN_9                      9
#define PIN_10                     10
#define PIN_11                     11
#define PIN_12                     12
#define PIN_13                     13
#define PIN_14                     14
#define PIN_15                     15

//Макросы операций с битами, передача бита в байте
#define SetBit( var, val )          ( var |= val )
#define ClrBit( var, val )          ( var &= ~val )
#define InvBit( var, val )          ( var ^= val )
#define BitIsSet( var, val )        ( ( var & val ) != 0 )
#define BitIsClr( var, val )        ( ( var & val ) == 0 )

//Макросы операций с битами, передача номера бита
#define SetNBit( var, nbit )        var |= ( 1 << nbit )
#define ClrNBit( var, nbit )        var &= ( ~(1 << nbit) )
#define InvNBit( var, nbit )        var ^= ( 1 << nbit )
#define BitNIsSet( var, nbit )      ( ( var & ( 1 << nbit ) ) != 0 )
#define BitNIsClr( var, nbit )      ( ( var & ( 1 << nbit ) ) == 0 )

//Тип значения для выхода
typedef enum {
    PIN_LOW,                            //низкий уровень
    PIN_HIGH,                           //высокий уровень
    PIN_ALT,                            //импульсный режим
    PIN_INPUT                           //режим входа
 } PinMode;

//Структура параметров для конфигурирования портов ввод/вывод
typedef struct {
    uint8_t         pin_id;             //ID пина
    GPIO_TypeDef    *port;              //порт
    uint8_t         num;                //номер пина
    GPIO_CONF       config;             //ввод/вывод/аналог/альтернативный функционал
    GPIO_MODE       mode;               //вввод/вывод - частота
    PinMode         init;               //состояние пина при инициализации
} GPIO_Config;

//*************************************************************************************************
// Функции управления
//*************************************************************************************************
void PinInit( const GPIO_Config *gpio_cfg, uint8_t size );

#endif
