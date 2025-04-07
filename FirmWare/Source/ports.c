
//*************************************************************************************************
//
// Модуль конфигурирования портов ввода/вывода
//
//*************************************************************************************************

#include <stdint.h>
#include <stdbool.h>

#include "cmsis_os2.h"
#include "stm32f10x.h"
#include "gpio_stm32f10x.h"

#include "ports.h"
#include "parse.h"
#include "uart.h"

//*************************************************************************************************
// Инициализация выходов в соответствии с параметрами заданными в структуре GPIO_Config
//-------------------------------------------------------------------------------------------------
// const GPIO_Config *gpio_cfg - указатель на структуру 
// uint8_t size                - кол-во записей в структуре
//*************************************************************************************************
void PinInit( const GPIO_Config *gpio_cfg, uint8_t size ) {

    uint8_t i;

    for ( i = 0; i < size; i++ ) {
        GPIO_PortClock( gpio_cfg[i].port, true );
        if ( gpio_cfg[i].config == GPIO_OUT_PUSH_PULL || gpio_cfg[i].config == GPIO_OUT_OPENDRAIN )
            GPIO_PinWrite( gpio_cfg[i].port, gpio_cfg[i].num, gpio_cfg[i].init );
        GPIO_PinConfigure( gpio_cfg[i].port, gpio_cfg[i].num, gpio_cfg[i].config, gpio_cfg[i].mode );
       }
 }
