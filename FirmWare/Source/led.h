
#ifndef __LED_H
#define __LED_H

#include <stdint.h>
#include <stdbool.h>

#include "ports.h"
#include "gpio_stm32f10x.h"

//ID пинов управления индикаторами
typedef enum {
    LED_ALPHA,
    LED_ALT, 
    LED_SHIFT,
    LED_CTRL,
    LED_CHK, 
    BEEP
 } LedId;
 
//ID управления индикаторами
typedef enum {
    MODE_OFF,
    MODE_ON
 } PinSet;
 
//*************************************************************************************************
// Функции управления
//*************************************************************************************************
void LedInit( void );
void LedSwitch( LedId led_id );
void LedCtrl( LedId led_id, PinSet mode );
void BeepCtrl( PinSet mode );
void LedModeSpeed( uint32_t speed );

#endif 
