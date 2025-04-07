
//*************************************************************************************************
//
// Модуль обработка отладочных команд полученных по UART1
//
//*************************************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmsis_os2.h"
#include "stm32f10x.h"

#include "led.h"
#include "data.h"
#include "button.h"
#include "command.h"
#include "message.h"
#include "events.h"
#include "uart.h"
#include "parse.h"
#include "config.h"
#include "version.h"

//*************************************************************************************************
// Внешние переменные
//*************************************************************************************************
extern uint32_t reset_flg;
extern CONFIG config;
extern FLASH_Status flash_status;

//*************************************************************************************************
// Переменные с внешним доступом
//*************************************************************************************************
#ifdef FW_DEBUG
uint8_t dbg_press = 0;
uint8_t dbg_temp[3] = { 0, };
#endif
osEventFlagsId_t cmnd_event = NULL;

//*************************************************************************************************
// Локальные константы
//*************************************************************************************************
//структура хранения и выполнения команд
typedef struct {
    char    name_cmd[20];                 //имя команды
    void    (*func)( uint8_t cnt_par );   //указатель на функцию выполнения
} CMD;

//расшифровка статуса задач
static char * const state_name[] = {
    "Inactive",
    "Ready",
    "Running",
    "Blocked",
    "Terminated",
    "Error"
 };

//*************************************************************************************************
// Атрибуты объектов RTOS
//*************************************************************************************************
static const osThreadAttr_t task_attr = {
    .name = "Command", 
    .stack_size = 512,
    .priority = osPriorityBelowNormal
 };

static const osEventFlagsAttr_t evn_attr = { .name = "Cmnd" };

//*************************************************************************************************
// Прототипы локальных функций
//*************************************************************************************************
static void TaskCommand( void *argument );
static ExecResult ExecCommand( char *buff );
static char *TaskStateDesc( osThreadState_t state );
static void CmndHelp( uint8_t cnt_par );
static void CmndReset( uint8_t cnt_par );
static void CmndTask( uint8_t cnt_par );
static void CmndUart( uint8_t cnt_par );
static void CmndMacroCtrl( uint8_t cnt_par );
static void CmndMacroAlt( uint8_t cnt_par );
static void CmndConfig( uint8_t cnt_par );
static void CmndVersion( uint8_t cnt_par );

#ifdef FW_RELEASE
static void CmndWdt( uint8_t cnt_par );
#endif

//*************************************************************************************************
// Локальные переменные
//*************************************************************************************************
static char buffer[160];

static const char *help = {
    "task                    - Displaying a list of tasks\r\n"
    "reset                   - Software restart of the controller\r\n"
    "uart 600-115200 [save]  - Setting the data port speed\r\n"
    "mctrl 1-10 \"...\" [save] - Defining a macro for the CTRL keys (with saving)\r\n"
    "malt  1-10 \"...\" [save] - Defining a macro for the ALT keys (with saving)\r\n"
    "config                  - Outputting a set of macros\r\n"
    #ifdef FW_RELEASE
    "wdt                       - Forced stop WDT timer\r\n"
    #endif
    "?       - Help.\r\n"
  };

//Перечень доступных команд
static const CMD cmd[] = {
    { "task",           CmndTask },
    { "reset",          CmndReset },
    { "uart",           CmndUart },
    { "mctrl",          CmndMacroCtrl },
    { "malt",           CmndMacroAlt },
    { "config",         CmndConfig },
    { "version",        CmndVersion },
    #ifdef FW_RELEASE
    { "wdt",            CmndWdt },
    #endif
    { "?",              CmndHelp }
 };

//*************************************************************************************************
// Инициализация задачи
//*************************************************************************************************
void CommandInit( void ) {

    //очередь событий
    cmnd_event = osEventFlagsNew( &evn_attr );
    //создаем задачу обработки команд
    osThreadNew( TaskCommand, NULL, &task_attr );
}

//*************************************************************************************************
// Задача обработки очереди сообщений на выполнение команд полученных по UART1
//*************************************************************************************************
static void TaskCommand( void *argument ) {

    int32_t event;
    ExecResult exec;

    osEventFlagsSet( cmnd_event, EVN_CMND_STATUS );
    for ( ;; ) {
        event = osEventFlagsWait( cmnd_event, EVN_CMND_MASK, osFlagsWaitAny, osWaitForever );
        if ( event > 0 && event & EVN_CMND_STATUS ) {
            //вывод версии прошивки
            UartSendStr( (char *)msg_crlr );
            sprintf( buffer, msg_read_par, FlashDescErr( flash_status ) );
            UartSendStr( buffer );
            CmndVersion( 0 );
            UartSendStr( (char *)msg_prompt );
           }
        if ( event > 0 && event & EVN_CMND_EXEC ) {
            //выполнение команды полученной по UART1
            ExecCommand( UartBuffer() );
            //разрешаем прием по UART1 следующей команды
            osEventFlagsSet( uart_event, EVN_UART_START1 );
           }
        if ( event > 0 && event & EVN_CMND_DATA ) {
            //выполнение команды полученной по UART2
            UartSendStr( "\r\n#: " );
            UartSendStr( DataBuffer() );
            exec = ExecCommand( DataBuffer() );
            //разрешаем прием по UART2 следующей команды
            osEventFlagsSet( data_event, EVN_UART_START2 | exec );
           }
        if ( event > 0 && event & EVN_CMND_PROMPT )
            UartSendStr( (char *)msg_prompt );
       }
 }

//*************************************************************************************************
// Обработка команд полученных по UART
//-------------------------------------------------------------------------------------------------
// char *buff - указатель на буфер с командой
//*************************************************************************************************
static ExecResult ExecCommand( char *buff ) {

    uint8_t i, cnt_par;

    //разбор параметров команды
    cnt_par = ParseCommand( buff );
    //проверка и выполнение команды
    for ( i = 0; i < SIZE_ARRAY( cmd ); i++ ) {
        if ( strcasecmp( (const char *)&cmd[i].name_cmd, GetParamVal( IND_PAR_CMND ) ) )
            continue;
        UartSendStr( (char *)msg_crlr );
        //выполнение команды
        cmd[i].func( cnt_par );
        UartSendStr( (char *)msg_prompt );
        return EXEC_OK;
       }
    if ( i == SIZE_ARRAY( cmd ) && strlen( GetParamVal( IND_PAR_CMND ) ) ) {
        //команда не найдена в списке доступных
        UartSendStr( (char *)msg_no_command );
        UartSendStr( (char *)msg_prompt );
        return EXEC_NO_FIND;
       }
    if ( i == SIZE_ARRAY( cmd ) && !strlen( GetParamVal( IND_PAR_CMND ) ) ) {
        //пустой ввод
        UartSendStr( (char *)msg_prompt );
        return EXEC_NO_COMMAND;
       }
    return EXEC_NO_COMMAND;
 }

//*************************************************************************************************
// Останов таймера WDT
//-------------------------------------------------------------------------------------------------
// uint8_t cnt_par - кол-во параметров
//*************************************************************************************************
#if defined( FW_RELEASE )
static void CmndWdt( uint8_t cnt_par ) {

    osThreadSuspend( task_button );
 }
#endif

//*************************************************************************************************
// Перезапуск контроллера
//-------------------------------------------------------------------------------------------------
// uint8_t cnt_par - кол-во параметров
//*************************************************************************************************
static void CmndReset( uint8_t cnt_par ) {

    NVIC_SystemReset();
}

//*************************************************************************************************
// Вывод подсказки по доступным командам
//-------------------------------------------------------------------------------------------------
// uint8_t cnt_par - кол-во параметров
//*************************************************************************************************
static void CmndHelp( uint8_t cnt_par ) {

    UartSendStr( (char *)help );
 }

//*************************************************************************************************
// Установка скорости UART2
//-------------------------------------------------------------------------------------------------
// uint8_t cnt_par - кол-во параметров
//*************************************************************************************************
static void CmndUart( uint8_t cnt_par ) {

    uint32_t speed, stat;
    
    //установка скорости
    speed = atoi( GetParamVal( IND_PARAM1 ) );
    if ( CheckSpeed( speed ) ) {
        //смена скорости
        config.uart_speed = speed;
        DataUartInit();
        //сохранение параметра
        if ( strcasecmp( GetParamVal( IND_PARAM2 ), "save" ) == 0 ) {
            stat = ConfigSave();
            sprintf( buffer, msg_save_par, FlashDescErr( stat ) );
            UartSendStr( buffer );
           }
        else UartSendStr( (char *)msg_ok );
       }
    else UartSendStr( (char *)msg_err_param );
 }

//*************************************************************************************************
// Сохранение макроса
//-------------------------------------------------------------------------------------------------
// uint8_t cnt_par - кол-во параметров
//*************************************************************************************************
static void CmndMacroCtrl( uint8_t cnt_par ) {

    uint8_t id;
    uint32_t stat;
    
    //проверка кол-ва параметров
    if ( cnt_par < 2 ) {
        UartSendStr( (char *)msg_err_param );
        return;
       }
    //проверка на допустимое значение индекса макроса
    id = atoi( GetParamVal( IND_PARAM1 ) );
    if ( !id || id > MACROS_CNT ) {
        UartSendStr( (char *)msg_err_param );
        return;
       }
    id--;
    memset( config.macro_ctrl[id], 0x00, sizeof( config.macro_ctrl[id] ) );
    if ( strlen( GetParamVal( IND_PARAM2 ) ) )
        strcpy( config.macro_ctrl[id], GetParamVal( IND_PARAM2 ) );
    //сохранение параметра
    if ( strcasecmp( GetParamVal( IND_PARAM3 ), "save" ) == 0 ) {
        stat = ConfigSave();
        sprintf( buffer, msg_save_par, FlashDescErr( stat ) );
        UartSendStr( buffer );
       }
    else UartSendStr( (char *)msg_ok );
 }

//*************************************************************************************************
// Сохранение макроса
//-------------------------------------------------------------------------------------------------
// uint8_t cnt_par - кол-во параметров
//*************************************************************************************************
static void CmndMacroAlt( uint8_t cnt_par ) {

    uint8_t id;
    uint32_t stat;
    
    //проверка кол-ва параметров
    if ( cnt_par < 2 ) {
        UartSendStr( (char *)msg_err_param );
        return;
       }
    //проверка на допустимое значение индекса макроса
    id = atoi( GetParamVal( IND_PARAM1 ) );
    if ( !id || id > MACROS_CNT ) {
        UartSendStr( (char *)msg_err_param );
        return;
       }
    id--;
    memset( config.macro_alt[id], 0x00, sizeof( config.macro_alt[id] ) );
    if ( strlen( GetParamVal( IND_PARAM2 ) ) )
        strcpy( config.macro_alt[id], GetParamVal( IND_PARAM2 ) );
    //сохранение параметра
    if ( strcasecmp( GetParamVal( IND_PARAM3 ), "save" ) == 0 ) {
        stat = ConfigSave();
        sprintf( buffer, msg_save_par, FlashDescErr( stat ) );
        UartSendStr( buffer );
       }
    else UartSendStr( (char *)msg_ok );
 }

//*************************************************************************************************
// Вывод значений параметров
//-------------------------------------------------------------------------------------------------
// uint8_t cnt_par - кол-во параметров
//*************************************************************************************************
static void CmndConfig( uint8_t cnt_par ) {

    uint8_t i;
    
    sprintf( buffer, "Data port speed: %u\r\n", config.uart_speed );
    UartSendStr( buffer );
    for ( i = 0; i < MACROS_CNT; i++ ) {
        sprintf( buffer, "  Macro CTRL %02u: %s\r\n", i + 1, config.macro_ctrl[i] );
        UartSendStr( buffer );
       }
    UartSendStr( (char *)msg_crlr );
    for ( i = 0; i < MACROS_CNT; i++ ) {
        sprintf( buffer, "  Macro  ALT %02u: %s\r\n", i + 1, config.macro_alt[i] );
        UartSendStr( buffer );
       }
 }

//*********************************************************************************************
// Вывод статистики по задачам
//-------------------------------------------------------------------------------------------------
// uint8_t cnt_par - кол-во параметров включая команду
//*********************************************************************************************
static void CmndTask( uint8_t cnt_par ) {

    uint8_t i;
    const char *name;
    osThreadState_t state;
    osPriority_t priority;
    osThreadId_t th_id[20];
    uint32_t cnt_task, stack_space, stack_size; 
    
    //вывод шапки параметров
    UartSendStr( "\r\n   Name thread     Priority  State      Stack Unused\r\n" );
    UartSendStr( (char *)msg_str_delim );
    //заполним весь массив th_id значением NULL
    memset( th_id, 0x00, sizeof( th_id ) );
    cnt_task = osThreadGetCount();
    cnt_task = osThreadEnumerate( &th_id[0], sizeof( th_id )/sizeof( th_id[0] ) );
    for ( i = 0; i < cnt_task; i++ ) {
        state = osThreadGetState( th_id[i] );
        priority = osThreadGetPriority( th_id[i] );
        stack_size = osThreadGetStackSize( th_id[i] );
        stack_space = osThreadGetStackSpace( th_id[i] );
        name = osThreadGetName( th_id[i] );
        if ( name != NULL && strlen( name ) )
            sprintf( buffer, "%2u %-16s    %2u    %-10s %5u %5u\r\n", i + 1, name, priority, TaskStateDesc( state ), stack_size, stack_space );
        else sprintf( buffer, "%2u ID = %-11u    %2u    %-10s %5u %5u\r\n", i + 1, (uint32_t)th_id[i], priority, TaskStateDesc( state ), stack_size, stack_space );
        UartSendStr( buffer );
       }
    UartSendStr( (char *)msg_str_delim );
 }

//*************************************************************************************************
// Вывод номера и даты версий: FirmWare, RTOS, HAL
//-------------------------------------------------------------------------------------------------
// uint8_t cnt_par - кол-во параметров
//*************************************************************************************************
static void CmndVersion( uint8_t cnt_par ) {

    #ifdef FW_DEBUG
    RCC_ClocksTypeDef RCC_Clocks;
    #endif

    //источник перезапуска контроллера
    sprintf( buffer, "Source reset: ......... %s\r\n", ResetSrcDesc( reset_flg ) );
    UartSendStr( buffer );

    sprintf( buffer, "FirmWare version: ..... %s\r\n", FWVersion( GetFwVersion() ) );
    UartSendStr( buffer );
    sprintf( buffer, "FirmWare date build: .. %s\r\n", FWDate( GetFwDate() ) );
    UartSendStr( buffer );
    sprintf( buffer, "FirmWare time build: .. %s\r\n", FWTime( GetFwTime() ) );
    UartSendStr( buffer );
    #ifdef FW_DEBUG
    RCC_GetClocksFreq( &RCC_Clocks );
    sprintf( buffer, "\r\nSYSCLK = %u\r\nHCLK   = %u\r\nPCLK1  = %u\r\nPCLK2  = %u\r\nADCCLK = %u\r\n", 
             RCC_Clocks.SYSCLK_Frequency, RCC_Clocks.HCLK_Frequency, RCC_Clocks.PCLK1_Frequency, RCC_Clocks.PCLK2_Frequency, RCC_Clocks.ADCCLK_Frequency );
    UartSendStr( buffer );
    #endif
}

//*************************************************************************************************
// Возвращает расшифровку статуса задачи
//-------------------------------------------------------------------------------------------------
// osThreadState_t state - код статуса
//*************************************************************************************************
static char *TaskStateDesc( osThreadState_t state ) {

    if ( state == osThreadInactive )
        return state_name[0];
    if ( state == osThreadReady )
        return state_name[1];
    if ( state == osThreadRunning )
        return state_name[2];
    if ( state == osThreadBlocked )
        return state_name[3];
    if ( state == osThreadTerminated )
        return state_name[4];
    if ( state == osThreadError )
        return state_name[5];
    return NULL;
 }
