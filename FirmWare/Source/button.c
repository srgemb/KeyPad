

//*************************************************************************************************
//
// Модуль сканирования клавиатуры
// 
//*************************************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "stm32f10x.h"
#include "gpio_stm32f10x.h"

#include "led.h"
#include "key.h"
#include "key_def.h"
#include "main.h"
#include "data.h"
#include "ports.h"
#include "config.h"
#include "parse.h"
#include "uart.h"
#include "button.h"
#include "events.h"
#include "message.h"

//*************************************************************************************************
// Внешние переменные
//*************************************************************************************************
extern CONFIG config;

//*************************************************************************************************
// Переменные с внешним доступом
//*************************************************************************************************
bool mode_speed = false;                    //режим отображения скорости обмена
bool mode_test = true;                      //тестовое включение всех индикаторов
uint8_t mode_change = 0;                    //номер скорости установленной в ручном режиме
osThreadId_t task_button = NULL;

//*************************************************************************************************
// Локальные константы
//*************************************************************************************************
#define TIME_SCAN           10              //интервал сканирования входов (ms)

#define COUNT_SCAN          8               //кол-во линий сканирования

#define PORT_SCAN           GPIOB

#define PIN_SCAN1           GPIO_Pin_9
#define PIN_SCAN2           GPIO_Pin_8
#define PIN_SCAN3           GPIO_Pin_15
#define PIN_SCAN4           GPIO_Pin_14
#define PIN_SCAN5           GPIO_Pin_13
#define PIN_SCAN6           GPIO_Pin_12
#define PIN_SCAN7           GPIO_Pin_11
#define PIN_SCAN8           GPIO_Pin_10

#define PIN_SCAN            ( PIN_SCAN1 | PIN_SCAN2 | PIN_SCAN3 | PIN_SCAN4 | \
                              PIN_SCAN5 | PIN_SCAN6 | PIN_SCAN7 | PIN_SCAN8 )

//ID пинов чтения входов клавиатуры
typedef enum {
    READ1,                                  //пин 1 чтения состояния клавиатуры
    READ2,                                  //пин 2 чтения состояния клавиатуры
    READ3,                                  //пин 3 чтения состояния клавиатуры
    READ4,                                  //пин 4 чтения состояния клавиатуры
    READ5,                                  //пин 5 чтения состояния клавиатуры
    READ6,                                  //пин 6 чтения состояния клавиатуры
    READ7,                                  //пин 7 чтения состояния клавиатуры
    READ8                                   //пин 8 чтения состояния клавиатуры
 } PinInId;

//Параметры конфигурирования портов ввода чтения состояния клавиатуры
static const GPIO_Config pin_read[] = {
    { READ1, GPIOB, PIN_1, GPIO_IN_PULL_UP, GPIO_MODE_INPUT, PIN_INPUT },
    { READ2, GPIOB, PIN_0, GPIO_IN_PULL_UP, GPIO_MODE_INPUT, PIN_INPUT },
    { READ3, GPIOA, PIN_7, GPIO_IN_PULL_UP, GPIO_MODE_INPUT, PIN_INPUT },
    { READ4, GPIOA, PIN_6, GPIO_IN_PULL_UP, GPIO_MODE_INPUT, PIN_INPUT },
    { READ5, GPIOA, PIN_5, GPIO_IN_PULL_UP, GPIO_MODE_INPUT, PIN_INPUT },
    { READ6, GPIOA, PIN_4, GPIO_IN_PULL_UP, GPIO_MODE_INPUT, PIN_INPUT },
    { READ7, GPIOA, PIN_1, GPIO_IN_PULL_UP, GPIO_MODE_INPUT, PIN_INPUT },
    { READ8, GPIOA, PIN_0, GPIO_IN_PULL_UP, GPIO_MODE_INPUT, PIN_INPUT }
 };

//*************************************************************************************************
// Локальные переменные
//*************************************************************************************************
static bool first = true;
static bool alpha = false;                  //состояние режима ALPHA (ввод символов) вкл/выкл
static uint8_t alt_key = 0;                 //состояние альтернативных кнопок
static uint8_t scan = 0x01;                 //маска сканирования
static uint8_t prev[8] = { 0, };
static uint8_t next[8] = { 0, };
static uint8_t scan1 = 0, scan2 = 0;

static char str[80];

//*************************************************************************************************
// Атрибуты объектов RTOS
//*************************************************************************************************
static const osThreadAttr_t task_attr = {
    .name = "Button", 
    .stack_size = 256,
    .priority = osPriorityNormal
 };

//*************************************************************************************************
// Прототипы локальных функций
//*************************************************************************************************
static void TaskButton( void *argument );
static uint8_t ScanInput( void );
static void ScanButton( void );
static void ScanSet( void );
static void CheckAltKey( void );
static void CheckAlphaKey( void );
static void ChangeSpeed( uint8_t key );

#if defined( FW_DEBUG )
static void UartDebug( const char *text, uint32_t par1 , uint32_t par2 );
#endif

//*************************************************************************************************
// Функция инициализации
//*************************************************************************************************
void ButtonInit( void ) {

    GPIO_InitTypeDef gpio;

    //инициализация входов чтения состояния клавиатуры
    PinInit( pin_read, SIZE_ARRAY( pin_read ) );
    //настраиваем пины данных на вывод
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = PIN_SCAN;
    GPIO_Init( GPIOB, &gpio );
    //создаем задачу обработки команд
    task_button = osThreadNew( TaskButton, NULL, &task_attr );
    //чтение состояния F1 для выбора режима установки скорости обмена
    scan = 0x02;
    ScanSet();
    if ( ScanInput() == MASK_KEY_02 )
        mode_change = CheckSpeed( config.uart_speed );
    scan = 0x01;
 }

//*************************************************************************************************
// Задача сканирования входов ошибок
//*************************************************************************************************
static void TaskButton( void *argument ) {

    //проверка входа в режим ручной коррекции скорости
    if ( mode_change )
        osEventFlagsSet( beep_event, EVN_BEEP_LONG1 );
    for ( ;; ) {
        ScanButton();
        CheckAltKey();
        osDelay( TIME_SCAN );
        WdtReload();
       }
 }

//*************************************************************************************************
// Функция обрабатывает результат сканирования клавиатуры
//*************************************************************************************************
static void ScanButton( void ) {

    uint8_t ch, mask = 0x01, value;
    int8_t i, index;
    
    if ( first == true ) {
        //установка битов сканирования
        ScanSet();
        scan1 = ScanInput();
       }
    else scan2 = ScanInput();

    if ( first == false ) {
        //сканирование кнопок
        index = GetNumBit( scan );
        if ( index != -1 ) {
            next[index] = scan1 & scan2;
            //поиск установленых/сброшенных битов
            for ( i = 0; i < COUNT_SCAN; i++, mask <<= 1 ) {
                if ( ( !( prev[index] & mask ) ) && ( next[index] & mask ) ) {
                    //нажатие кнопки
                    CheckAlphaKey();

                    value = next[index];
                    //для index = 3 исключаем скан коды клавиш ALT и SHIFT
                    if ( index == INDEX_SCAN_3 )
                        CLEAR_BIT( value, SCAN_KEY_ALT | SCAN_KEY_SHIFT );
                    //для index = 4 исключаем скан коды клавиши CTRL
                    if ( index == INDEX_SCAN_4 )
                        CLEAR_BIT( value, SCAN_KEY_CTRL );
                    if ( index == INDEX_SCAN_0 || index == INDEX_SCAN_1 || index == INDEX_SCAN_2 || index == INDEX_SCAN_7 ) {
                        //для указанных индексов исключаем скан коды клавиши ALT т.к. ее скан код = 0x04 и суммируется 
                        //с скан кодами линейки клавиш Fn = 0x02 получаем 0x06, для исключения повторной отправки кодов 
                        //клавиш ALT + SHIFT + Fn новое значение value передаем с учетом маски проверки битов
                        CLEAR_BIT( value, SCAN_KEY_ALT );
                        ch = GetKey( alt_key, index, value & mask );
                       }
                    else ch = GetKey( alt_key, index, value );
                    if ( ch ) {
                        #if defined( FW_DEBUG )
                        UartDebug( "PRESS", ch, ch );
                        #endif
                        //в режиме коррекции скорости данные не передаем
                        if ( mode_change )
                            ChangeSpeed( ch );
                        else DataSend( ch, alt_key );
                       }
                    osEventFlagsSet( beep_event, EVN_BEEP_SHORT );
                   }
               }
            //фиксируем текущее состояние кнопок
            prev[index] = next[index];
           }
        //начинаем сканировать заново
        first = true; 
        scan <<= 1;
        if ( !scan )
            scan = 0x01;
       }
    else first = false;
 }

//*************************************************************************************************
// Функция обрабатывает состояние клавиш ALT/SHIFT/CTRL
//*************************************************************************************************
static void CheckAltKey( void ) {

    //в режиме отображения скорости обмена не отображаем состояние кнопок
    if ( mode_speed == true || mode_test == true )
        return;

    //клавиша ALT
    if ( prev[INDEX_SCAN_3] & MASK_KEY_04 ) {
        SET_BIT( alt_key, KEY_ALT );
        LedCtrl( LED_ALT, MODE_ON );
       }
    else {
        CLEAR_BIT( alt_key, KEY_ALT );
        LedCtrl( LED_ALT, MODE_OFF );
       }

    //клавиша CTRL
    if ( prev[INDEX_SCAN_4] & MASK_KEY_10 ) {
        SET_BIT( alt_key, KEY_CTRL );
        LedCtrl( LED_CTRL, MODE_ON );
       }
    else {
        CLEAR_BIT( alt_key, KEY_CTRL );
        LedCtrl( LED_CTRL, MODE_OFF );
       }

    //клавиша SHIFT
    if ( prev[INDEX_SCAN_3] & MASK_KEY_02 ) {
        SET_BIT( alt_key, KEY_SHIFT );
        LedCtrl( LED_SHIFT, MODE_ON );
       }
    else {
        CLEAR_BIT( alt_key, KEY_SHIFT );
        LedCtrl( LED_SHIFT, MODE_OFF );
       }
 }

//*************************************************************************************************
// Функция обрабатывает состояние клавиши ALPHA
//*************************************************************************************************
static void CheckAlphaKey( void ) {

    //в режиме отображения скорости обмена не отображаем состояние кнопок
    if ( mode_speed == true || mode_test == true )
        return;

    //клавиша ALPHA - вкл режима
    if ( next[INDEX_SCAN_5] & MASK_KEY_10 && alpha == false ) {
        alpha = true;
        SET_BIT( alt_key, KEY_ALPHA );
        LedCtrl( LED_ALPHA, MODE_ON );
        return;
       }
    //клавиша ALPHA - выкл режима
    if ( next[INDEX_SCAN_5] & MASK_KEY_10 && alpha == true ) {
        alpha = false;
        CLEAR_BIT( alt_key, KEY_ALPHA );
        LedCtrl( LED_ALPHA, MODE_OFF );
       }
 }


//*************************************************************************************************
// Функция вывода сигналов сканирования клавиатуры
//*************************************************************************************************
static void ScanSet( void ) {

    //сброс битов сканирования
    GPIO_SetBits( PORT_SCAN, PIN_SCAN );
    //установка битов сканирования
    GPIO_WriteBit( PORT_SCAN, (uint16_t)scan << 8, Bit_RESET );
 }

//*************************************************************************************************
// Функция выполняет чтение состояния клавиатуры
//-------------------------------------------------------------------------------------------------
// return - прочитанное значение
//*************************************************************************************************
static uint8_t ScanInput( void ) {

    uint8_t read1 = 0, read2 = 0;

    read1 = (uint8_t)GPIO_PortRead( GPIOA ) & 0x00F3;
    read2 = (uint8_t)GPIO_PortRead( GPIOB ) & 0x0003;
    return ~( read1 | ( read2 << 2 ) );
 }

//*************************************************************************************************
// Подсчет кол-ва бит установленных в "1" в uint8_t
//-------------------------------------------------------------------------------------------------
// uint8_t value - исходное значение
// return        - результат подсчета
//*************************************************************************************************
uint8_t BitsCount( uint8_t value ) {

    return ( value ) ? ( value & 1 ) + BitsCount( value >> 1 ) : 0;
 }

//*************************************************************************************************
// Функция возвращает номер установленного бита
//-------------------------------------------------------------------------------------------------
// uint8_t value - маска с установленным битом
// return =  -1  - маске установлено более 1 бита или не установлен ни одного бита
//        <> -1  - номер установленного бита 0 ... 7
//*************************************************************************************************
int8_t GetNumBit( uint8_t value ) {
    
    uint8_t i, mn;
    
    if ( BitsCount( value ) > 1 )
        return -1;
    for ( i = 0, mn = 1; i < 8; i++, mn *= 2 )
        if ( value & mn )
            return i;
    return -1;
 }

//*************************************************************************************************
// Переключение скорости обмена в ручном режиме
//*************************************************************************************************
static void ChangeSpeed( uint8_t key ) {

    uint8_t backup;
    uint32_t speed;
    
    if ( !mode_change )
        return;
    if ( key == KEY_UP ) {
        if ( mode_change < GetMaxIndex() )
            mode_change++;
        else mode_change = 1;
        speed = GetSpeed( mode_change );
        LedModeSpeed( speed );
        #if defined( FW_DEBUG )
        UartDebug( "SEL SPEED", speed, 0 );
        #endif
       }
    if ( key == KEY_DOWN ) {
        if ( mode_change > 1 )
            mode_change--;
        else mode_change = GetMaxIndex();
        speed = GetSpeed( mode_change );
        LedModeSpeed( speed );
        #if defined( FW_DEBUG )
        UartDebug( "SEL SPEED", speed, 0 );
        #endif
       }
    if ( key == KEY_CR ) {
        //сохранение скорости
        backup = mode_change;
        speed = GetSpeed( backup );
        mode_change = 0;
        LedModeSpeed( GetSpeed( backup ) );
        #if defined( FW_DEBUG )
        UartDebug( "SET SPEED", speed, 0 );
        #endif
        //смена скорости UART2
        config.uart_speed = speed;
        DataUartInit();
        sprintf( str, msg_save_par, FlashDescErr( ConfigSave() ) );
        UartSendStr( str );
       }
    if ( key == KEY_ESC ) {
        //отмена изменения скорости
        mode_change = 0;
        LedModeSpeed( config.uart_speed );
        #if defined( FW_DEBUG )
        UartDebug( "CANCEL", 0, 0 );
        #endif
       }
 }

//*************************************************************************************************
// Вывод отладочной информации
//-------------------------------------------------------------------------------------------------
// const char *text - строка вывода
// uint32_t par     - значение для вывода в HEX формате
//*************************************************************************************************
#if defined( FW_DEBUG )
static void UartDebug( const char *text, uint32_t par1, uint32_t par2 ) {

    sprintf( str, "%s 0x%08X (%u) %c\r\n", text, par1, par1, par2 );
    UartSendStr( str );
 }

#endif
