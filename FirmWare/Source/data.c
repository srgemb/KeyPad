
//*************************************************************************************************
//
// Модуль управления портом UART2 (передача данных клавиатуры)
//
//*************************************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "cmsis_os2.h"
#include "stm32f10x.h"
#include "gpio_stm32f10x.h"
#include "rte_device.h"

#include "driver_usart.h"

#include "data.h"
#include "key.h"
#include "config.h"
#include "events.h"
#include "button.h"
#include "message.h"

//*************************************************************************************************
// Внешние переменные
//*************************************************************************************************
extern CONFIG config;
extern ARM_DRIVER_USART Driver_USART2;

//*************************************************************************************************
// Локальные константы
//*************************************************************************************************
#define RECV_BUFF_SIZE      80              //размер приемного буфера
#define SEND_BUFF_SIZE      80              //размер передающего буфера

//*************************************************************************************************
// Переменные с внешним доступом
//*************************************************************************************************
osEventFlagsId_t data_event = NULL;

//*************************************************************************************************
// Локальные переменные
//*************************************************************************************************
static char recv_ch;
static uint16_t recv_ind = 0;
static uint16_t tail = 0, head_tx = 0, tail_tx = 0;
static char recv_buff[RECV_BUFF_SIZE], send_buff[SEND_BUFF_SIZE];

static ARM_DRIVER_USART *USARTdrv;
static osSemaphoreId_t sem_busy;

//*************************************************************************************************
// Прототипы локальных функций
//*************************************************************************************************
static void TaskUart( void *argument );
static void CallBackUsart( uint32_t event );

//*************************************************************************************************
// Атрибуты объектов RTOS
//*************************************************************************************************
static const osThreadAttr_t task_attr = {
    .name = "Data", 
    .stack_size = 256,
    .priority = osPriorityBelowNormal
 };

static const osEventFlagsAttr_t evn_attr = { .name = "Uart2" };
static const osSemaphoreAttr_t sem_attr = { .name = "Uart2" };

//*************************************************************************************************
// Инициализация задачи управления UART2
//*************************************************************************************************
void DataInit( void ) {

    DataUartInit();
    //очередь событий
    data_event = osEventFlagsNew( &evn_attr );
    //семафор блокировки, начальное состояние: семафор = 0 - занят
    sem_busy = osSemaphoreNew( 1, 0, &sem_attr );
    //создаем задачу обработки команд
    osThreadNew( TaskUart, NULL, &task_attr );
    //инициализация приема по UART2
    memset( send_buff, 0x00, sizeof( send_buff ) );
    memset( recv_buff, 0x00, sizeof( recv_buff ) );
    //инициализация приема по UART2
    //USARTdrv->Receive( &recv_ch, 1 );
 }

//*************************************************************************************************
// Инициализация UART2
//*************************************************************************************************
void DataUartInit( void ) {

    NVIC_InitTypeDef nvic_init;
    
    USARTdrv = &Driver_USART2;
    USARTdrv->PowerControl( ARM_POWER_OFF );
    USARTdrv->Uninitialize();
    USARTdrv->Initialize( &CallBackUsart );
    USARTdrv->PowerControl( ARM_POWER_FULL );
    USARTdrv->Control( ARM_USART_MODE_ASYNCHRONOUS | ARM_USART_DATA_BITS_8 | ARM_USART_PARITY_NONE | 
                       ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, config.uart_speed );

    #if ( RTE_USART1_TX_DMA == 1 )
    //настройка прерывания для DMA1-channel4 - USART2-TX
    nvic_init.NVIC_IRQChannel = DMA1_Channel4_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 4;
    nvic_init.NVIC_IRQChannelSubPriority = 0;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init( &nvic_init );
    #endif

    #if ( RTE_USART1_RX_DMA == 1 )
    //настройка прерывания для DMA1-channel5 - USART2-RX
    nvic_init.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 5;
    nvic_init.NVIC_IRQChannelSubPriority = 0;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init( &nvic_init );
    #endif

    #if ( RTE_USART1_TX_DMA != 1 && RTE_USART1_RX_DMA != 1 )
    //настройка прерывания для USART2
    nvic_init.NVIC_IRQChannel = USART2_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 6;
    nvic_init.NVIC_IRQChannelSubPriority = 0;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init( &nvic_init );
    #endif
    
    //разрешение передачи
    USARTdrv->Control( ARM_USART_CONTROL_TX, 1 );
    //разрешение приема
    USARTdrv->Control( ARM_USART_CONTROL_RX, 1 );
 }

//*************************************************************************************************
// Задача обработки очереди сообщений приема данных по UART2
//*************************************************************************************************
static void TaskUart( void *argument ) {

    int32_t event;
    ExecResult exec;

    osEventFlagsSet( data_event, EVN_UART_START2 );
    for ( ;; ) {
        event = osEventFlagsWait( data_event, EVN_UART_MASK, osFlagsWaitAny, osWaitForever );
        if ( event > 0 && event & EVN_UART_START2 ) {
            exec = (ExecResult)( event & EVN_MASK_RESULT );
            if ( exec == EXEC_OK )
                DataSendStr( (char *)msg_data_ok );
            else DataSendStr( (char *)msg_data_err );
            //команда выполнена, возобновляем прием команд по UART2
            recv_ind = 0;
            memset( recv_buff, 0x00, sizeof( recv_buff ) );
            USARTdrv->Receive( &recv_ch, 1 );
           }
       }
 }

//*************************************************************************************************
// Обработчик событий последовательного порта
//*************************************************************************************************
static void CallBackUsart( uint32_t event ) {

    if ( event & ARM_USART_EVENT_RECEIVE_COMPLETE ) {
        //принят один байт
        if ( recv_ind >= sizeof( recv_buff ) ) {
            recv_ind = 0;
            memset( recv_buff, 0x00, sizeof( recv_buff ) );
           }
        recv_buff[recv_ind++] = recv_ch;
        //проверим последний принятый байт, если CR - обработка команды
        if ( recv_buff[recv_ind - 1] == KEY_CR ) {
            recv_buff[recv_ind - 1] = 0x00; //уберем код CR
            //выполнение команды в TaskCommand()
            osEventFlagsSet( cmnd_event, EVN_CMND_DATA );
            return; //не выполняем запуск приема по UART2
           }
        //продолжаем прием
        USARTdrv->Receive( &recv_ch, 1 );
       }
    if ( event & ARM_USART_EVENT_TX_COMPLETE ) {
        //передача блока завершена
        if ( tail == tail_tx ) {
            //индекс хвоста буфера и индекс хвоста передачи совпадает - все передано
            tail = head_tx = tail_tx = 0;
            memset( send_buff, 0x00, sizeof( send_buff ) );
            osSemaphoreRelease( sem_busy );
           }
        else {
            //передача продолжается, добавлены данные
            head_tx = tail_tx;  //индекс головы перемещаем в конец переданного ранее блока
            tail_tx = tail;     //индекс своста перемещаем на окончание добавленных данных
            USARTdrv->Send( (uint8_t *)( send_buff + head_tx ), tail_tx - head_tx );
           }
      }
 }

//*************************************************************************************************
// Передача байта/макроса в UART2
//-------------------------------------------------------------------------------------------------
// uint8_t data    - передаваемые данные
// uint8_t alt_key - состояние клавиш ALT CTRL
//*************************************************************************************************
void DataSend( uint8_t data, uint8_t alt_key ) {

    char str[5];
    
    //исключаем бит KEY_SHIFT
    CLEAR_BIT( alt_key, KEY_SHIFT );
    if ( ( data == KEY_F1 || data == KEY_F2 || data == KEY_F3 || data == KEY_F4 || data == KEY_F5 || 
         data == KEY_F6 || data == KEY_F7 || data == KEY_F8 || data == KEY_F9 || data == KEY_F10 ) && 
         ( alt_key & KEY_ALT || alt_key & KEY_CTRL ) ) {
        if ( alt_key == KEY_ALT ) {
            DataSendStr( config.macro_alt[data & 0x0F] );
            DataSendStr( (char *)msg_crlr );
           }
        if ( alt_key == KEY_CTRL ) {
            DataSendStr( config.macro_ctrl[data & 0x0F] );
            DataSendStr( (char *)msg_crlr );
           }
       }
    else {
        memset( str, 0x00, sizeof( str ) );
        if ( data == KEY_CR ) {
            str[0] = KEY_CR;
            str[1] = KEY_LF;
           }
        else str[0] = data;
        DataSendStr( str );
       }
 }
 
//*************************************************************************************************
// Добавляем строку в буфер и запускаем передачу в UART2
//-------------------------------------------------------------------------------------------------
// char *str - указатель на строку для добавления
//*************************************************************************************************
void DataSendStr( char *str ) {

    ARM_USART_STATUS stat;
    uint16_t length, free, cnt_send = 0, offset = 0;
    
    if ( !strlen( str ) )
        return;
    length = strlen( str );
    do {
        //доступное место в буфере
        free = sizeof( send_buff ) - tail;
        if ( !free ) {
            //ждем свободного места в буфере
            osSemaphoreAcquire( sem_busy, osWaitForever );
            free = sizeof( send_buff ) - tail;
           }
        //проверка места в буфере
        if ( length > free ) {
            //места для размещения всей строки не достаточно, добавляем в буфер часть строки
            memcpy( send_buff + tail, str + offset, free );
            length -= free; //осталось для передачи
            offset += free; //смещение на следующий фрагмент
            tail += free;   //установим новое значение хвоста после добавления фрагмента
            cnt_send = free;
           }
        else {
            //места для размещения всей строки достаточно, добавляем в буфер всю строку
            memcpy( send_buff + tail, str + offset, length );
            tail += length; //установим новое значение хвоста после добавления строки
            cnt_send = length;
            length = 0;     //размер оставшейся части строки
           }

        stat = USARTdrv->GetStatus();
        if ( !stat.tx_busy ) {
            //передача не идет, запускаем передачу
            tail_tx = tail;
            USARTdrv->Send( send_buff, cnt_send );
           }
       } while ( length );
 }

//*************************************************************************************************
// Возвращает адрес приемного буфера UART2
//-------------------------------------------------------------------------------------------------
// return - указатель на буфер
//*************************************************************************************************
char *DataBuffer( void ) {

    return recv_buff;
 }
