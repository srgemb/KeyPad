
//*************************************************************************************************
//
// Основной модуль инициализации
// 
//*************************************************************************************************

#include <stdint.h>
#include <stdbool.h>

#include "stm32f10x.h"
#include "cmsis_os2.h"
#include "gpio_stm32f10x.h"

#include "led.h"
#include "uart.h"
#include "data.h"
#include "button.h"
#include "config.h"
#include "command.h"

#ifdef HARDWARE_TEST
#include "test.h"
#endif

//*************************************************************************************************
// Прототипы локальных функций
//*************************************************************************************************
#if defined( FW_RELEASE )
static void WdtInit( void );
#endif

//*************************************************************************************************
// Переменные с внешним доступом
//*************************************************************************************************
uint32_t reset_flg;
 
//*************************************************************************************************
// Основная функция
//*************************************************************************************************
int main( void ) {

    //сохраним источник сброса
    reset_flg = ( RCC->CSR >>= 26 );
    RCC_ClearFlag();

    //конфигурирование пинов JTAG (PB3, PB15) -> off
    GPIO_AFConfigure( AFIO_SWJ_JTAG_NO_SW );

    //установка частоты шины APB2 (EXTI, GPIO, TIM1, SPI1, USART1)
    RCC_PCLK2Config( RCC_HCLK_Div16 );

    //Установка группы приоритетов
    //4 bits for pre-emption priority
    //0 bits for subpriority
    NVIC_SetPriorityGrouping( 3 );
    
    ConfigInit();
    LedInit();
    ButtonInit();
    UartInit();
    DataInit();
    CommandInit();
    #if defined( FW_RELEASE )
    WdtInit();
    #endif

    osKernelInitialize();
    osKernelStart();

    while( 1 );
 }

//*************************************************************************************************
// Инициализация таймера WDT
//-------------------------------------------------------------------------------------------------
// Интервал перезапуска: 1/40000 * 64 * 2000 = 3.2 sec
//*************************************************************************************************
#if defined( FW_RELEASE )
static void WdtInit( void ) {

    IWDG_WriteAccessCmd( IWDG_WriteAccess_Enable );
    IWDG_SetPrescaler( IWDG_Prescaler_64 );
    IWDG_WriteAccessCmd( IWDG_WriteAccess_Enable );
    IWDG_SetReload( 2000 );
    IWDG_WriteAccessCmd( IWDG_WriteAccess_Enable );
    IWDG_Enable();
 }
#endif

//*************************************************************************************************
// Функция перезапуска WDT таймера
//*************************************************************************************************
void WdtReload( void ) {

    IWDG_WriteAccessCmd( IWDG_WriteAccess_Enable );
    IWDG_ReloadCounter();
    IWDG_WriteAccessCmd( IWDG_WriteAccess_Disable );
 }
