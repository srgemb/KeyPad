
//*************************************************************************************************
//
// Модуль управления LED индикаторами и звуковым сигналом
//
//*************************************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "cmsis_os2.h"
#include "stm32f10x.h"
#include "gpio_stm32f10x.h"

#include "main.h"
#include "ports.h"
#include "parse.h"
#include "config.h"
#include "led.h"
#include "events.h"

//*************************************************************************************************
// Внешние переменные
//*************************************************************************************************
extern CONFIG config;
extern bool mode_speed;
extern bool mode_test;
extern uint8_t mode_change;

//*************************************************************************************************
// Переменные с внешним доступом
//*************************************************************************************************
osEventFlagsId_t beep_event = NULL;

//*************************************************************************************************
// Локальные константы
//*************************************************************************************************
#define TIME_BLINK              300         //интервал мигания индикатора (ms)
#define TIME_BEEP_SHORT         80          //длительность короткого звукового сигнала (ms)
#define TIME_BEEP_LONG          300         //длительность длинного звукового сигнала (ms)
#define TIME_BEEP_LONG1         1000        //длительность длинного звукового сигнала (ms)

#define TIME_TEST_LED           1000        //длительность тестовой индикции ALPHA, ALT, SHIFT, CTRL
#define TIME_MODE_SPEED         2000        //длительность отображения скорости обмена

//Параметры конфигурирования портов вывода для управления индикаторами
static const GPIO_Config pin_led[] = {
    { LED_ALPHA, GPIOB, PIN_3,  GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT10MHZ, PIN_HIGH },
    { LED_ALT,   GPIOB, PIN_4,  GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT10MHZ, PIN_HIGH },
    { LED_SHIFT, GPIOB, PIN_5,  GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT10MHZ, PIN_HIGH },
    { LED_CTRL,  GPIOB, PIN_6,  GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT10MHZ, PIN_HIGH },
    { LED_CHK,   GPIOB, PIN_7,  GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT10MHZ, PIN_LOW },
    { BEEP,      GPIOA, PIN_11, GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT10MHZ, PIN_LOW }
 };

//*************************************************************************************************
// Локальные переменные
//*************************************************************************************************
static uint8_t ind;
static uint8_t beep_cnt = 0;
static osTimerId_t timer_beep = NULL, timer_pause = NULL, timer_test = NULL, timer_speed = NULL;

//*************************************************************************************************
// Атрибуты объектов RTOS
//*************************************************************************************************
static const osThreadAttr_t task1_attr = {
    .name = "Led", 
    .stack_size = 256,
    .priority = osPriorityNormal
 };

static const osThreadAttr_t task2_attr = {
    .name = "Beep", 
    .stack_size = 256,
    .priority = osPriorityNormal
 };

static const osEventFlagsAttr_t evn_attr = { .name = "Beep" };

static const osTimerAttr_t tmr_attr1 = { .name = "Beep" };
static const osTimerAttr_t tmr_attr2 = { .name = "Pause" };
static const osTimerAttr_t tmr_attr3 = { .name = "Test" };
static const osTimerAttr_t tmr_attr4 = { .name = "Speed" };

//*************************************************************************************************
// Прототипы локальных функций
//*************************************************************************************************
static void TaskLed( void *argument );
static void TaskBeep( void *argument );
static void Timer1CallBack( void *arg );
static void Timer2CallBack( void *arg );
static void Timer3CallBack( void *arg );
static void Timer4CallBack( void *arg );
static void LedSpeed( void );

//*************************************************************************************************
// Инициализация таймера, прерывания и задачи управления
//*************************************************************************************************
void LedInit( void ) {

    //конфигурирование пинов JTAG-DP Disabled and SW-DP Enabled (PB3, PB4, PA15) -> free
    GPIO_AFConfigure( AFIO_SWJ_JTAG_NO_SW );
    //инициализация выходов
    PinInit( pin_led, SIZE_ARRAY( pin_led ) );
    //таймер длительности звукового сигнала
    timer_beep = osTimerNew( Timer1CallBack, osTimerOnce, NULL, &tmr_attr1 );
    timer_pause = osTimerNew( Timer2CallBack, osTimerOnce, NULL, &tmr_attr2 );
    timer_test = osTimerNew( Timer3CallBack, osTimerOnce, NULL, &tmr_attr3 );
    timer_speed = osTimerNew( Timer4CallBack, osTimerOnce, NULL, &tmr_attr4 );
    //очередь событий
    beep_event = osEventFlagsNew( &evn_attr );
    //создаем задачу обработки команд
    osThreadNew( TaskLed, NULL, &task1_attr );
    osThreadNew( TaskBeep, NULL, &task2_attr );
 }

//*************************************************************************************************
// Задача управления миганием индикатора
//*************************************************************************************************
static void TaskLed( void *argument ) {

    osTimerStart( timer_test, TIME_TEST_LED );
    for ( ;; ) {
        LedSwitch( LED_CHK );
        LedSpeed();
        osDelay( TIME_BLINK );
       }
 }

//*************************************************************************************************
// Задача управления звуковым сигналом
//*************************************************************************************************
static void TaskBeep( void *argument ) {

    int32_t event;
    
    for ( ;; ) {
        event = osEventFlagsWait( beep_event, EVN_BEEP_MASK, osFlagsWaitAny, osWaitForever );
        if ( event > 0 && event & EVN_BEEP_SHORT && !osTimerIsRunning( timer_beep ) ) {
            osTimerStart( timer_beep, TIME_BEEP_SHORT );
            BeepCtrl( MODE_ON );
           }
        if ( event > 0 && event & EVN_BEEP_LONG && !osTimerIsRunning( timer_beep ) ) {
            osTimerStart( timer_beep, TIME_BEEP_LONG );
            BeepCtrl( MODE_ON );
           }
        if ( event > 0 && event & EVN_BEEP_LONG1 && !osTimerIsRunning( timer_beep ) ) {
            osTimerStart( timer_beep, TIME_BEEP_LONG1 );
            BeepCtrl( MODE_ON );
           }
        if ( event > 0 && event & EVN_BEEP_SHORT3 ) {
            if ( !beep_cnt )
                beep_cnt = 3;
            osTimerStart( timer_beep, TIME_BEEP_SHORT );
            BeepCtrl( MODE_ON );
           }
       }
 }

//*************************************************************************************************
// Функция обратного вызова таймера - длительности сигнала
//*************************************************************************************************
static void Timer1CallBack( void *arg ) {

    //выключение звукового сигнала
    BeepCtrl( MODE_OFF );
    if ( beep_cnt )
        osTimerStart( timer_pause, TIME_BEEP_SHORT );
 }

//*************************************************************************************************
// Функция обратного вызова таймера - пауза между сигналами
//*************************************************************************************************
static void Timer2CallBack( void *arg ) {

    if ( --beep_cnt )
        osEventFlagsSet( beep_event, EVN_BEEP_SHORT3 );
 }

//*************************************************************************************************
// Функция обратного вызова таймера - завершение тестовой индикации
//*************************************************************************************************
static void Timer3CallBack( void *arg ) {

    LedCtrl( LED_ALPHA, MODE_OFF );
    LedCtrl( LED_ALT, MODE_OFF );
    LedCtrl( LED_SHIFT, MODE_OFF );
    LedCtrl( LED_CTRL, MODE_OFF );
    mode_test = false;
    LedModeSpeed( config.uart_speed );
 }

//*************************************************************************************************
// Функция обратного вызова таймера - завершение индикации скорости
//*************************************************************************************************
static void Timer4CallBack( void *arg ) {

    LedCtrl( LED_ALPHA, MODE_OFF );
    LedCtrl( LED_ALT, MODE_OFF );
    LedCtrl( LED_SHIFT, MODE_OFF );
    LedCtrl( LED_CTRL, MODE_OFF );
    mode_speed = false;
 }

//*************************************************************************************************
// Переключение заданного индикатора
//-------------------------------------------------------------------------------------------------
// LedId led_id - ID индикатора
//*************************************************************************************************
void LedSwitch( LedId led_id ) {

    if ( GPIO_PinRead( pin_led[led_id].port, pin_led[led_id].num ) )
        GPIO_PinWrite( pin_led[led_id].port, pin_led[led_id].num, PIN_LOW );
    else GPIO_PinWrite( pin_led[led_id].port, pin_led[led_id].num, PIN_HIGH );
 }

//*************************************************************************************************
// Управление индикатором ВКЛ/ВЫКЛ
//-------------------------------------------------------------------------------------------------
// LedId led_id - ID индикатора
// PinSet mode  - режим управления вкл/выкл
//*************************************************************************************************
void LedCtrl( LedId led_id, PinSet mode ) {

    GPIO_PinWrite( pin_led[led_id].port, pin_led[led_id].num, mode );
 }

//*************************************************************************************************
// Управление звуковым сигналом ВКЛ/ВЫКЛ
//-------------------------------------------------------------------------------------------------
// PinSet mode - режим управления вкл/выкл
//*************************************************************************************************
void BeepCtrl( PinSet mode ) {

    GPIO_PinWrite( pin_led[BEEP].port, pin_led[BEEP].num, mode );
 }

//*************************************************************************************************
// Функция включает отобращение скорости обмена в двоичном коде на индикаторах CTRL-SHIFT-ALPHA-ALT
// 600   - 0001, 1200  - 0010, 2400  - 0011, 4800   - 0100
// 9600  - 0101, 14400 - 0110, 19200 - 0111, 28800  - 1000
// 38400 - 1001, 56000 - 1010, 57600 - 1011, 115200 - 1100
//-------------------------------------------------------------------------------------------------
// uint32_t speed - установленная скорость обмена
//*************************************************************************************************
void LedModeSpeed( uint32_t speed ) {

    ind = CheckSpeed( speed );
    if ( !ind )
        return;
    mode_speed = true;
    if ( !mode_change ) {
        osEventFlagsSet( beep_event, EVN_BEEP_SHORT3 );
        //вкл таймер длительности отображения скорости
        osTimerStart( timer_speed, TIME_MODE_SPEED );
       }
    LedCtrl( LED_CTRL, (PinSet)BitIsSet( ind, 0x08 ) );
    LedCtrl( LED_SHIFT, (PinSet)BitIsSet( ind, 0x04 ) );
    LedCtrl( LED_ALPHA, (PinSet)BitIsSet( ind, 0x02 ) );
    LedCtrl( LED_ALT, (PinSet)BitIsSet( ind, 0x01 ) );
 }

//*************************************************************************************************
// Функция управляет миганием индикаторов скорости обмена в режиме ручной коррекции
//*************************************************************************************************
static void LedSpeed( void ) {

    if ( !mode_change )
        return;
    //мигаем индикаторами
    if ( GPIO_PinRead( pin_led[LED_CHK].port, pin_led[LED_CHK].num ) ) {
        LedCtrl( LED_CTRL, (PinSet)BitIsSet( ind, 0x08 ) );
        LedCtrl( LED_SHIFT, (PinSet)BitIsSet( ind, 0x04 ) );
        LedCtrl( LED_ALPHA, (PinSet)BitIsSet( ind, 0x02 ) );
        LedCtrl( LED_ALT, (PinSet)BitIsSet( ind, 0x01 ) );
       }
    else {
        LedCtrl( LED_CTRL, MODE_OFF );
        LedCtrl( LED_SHIFT, MODE_OFF );
        LedCtrl( LED_ALPHA, MODE_OFF );
        LedCtrl( LED_ALT, MODE_OFF );
       }
 }
