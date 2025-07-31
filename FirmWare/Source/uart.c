
//*************************************************************************************************
//
// Модуль обработка управления отладочным UART1 (отладочный порт)
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

#include "key.h"
#include "key_def.h"
#include "uart.h"
#include "events.h"

//*************************************************************************************************
// Локальные константы
//*************************************************************************************************
#define RECV_BUFF_SIZE      40              //размер приемного буфера
#define SEND_BUFF_SIZE      512             //размер передающего буфера

#define KEY_BACKSPACE       0x08            //удаление символа
#define KEY_ESC_CMD         0x1B            //код префикса команды ESC

//*************************************************************************************************
// Внешние переменные
//*************************************************************************************************
extern ARM_DRIVER_USART Driver_USART1;

//*************************************************************************************************
// Переменные с внешним доступом
//*************************************************************************************************
osEventFlagsId_t uart_event = NULL;

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
    .name = "Uart", 
    .stack_size = 320,
    .priority = osPriorityBelowNormal
 };

static const osEventFlagsAttr_t evn_attr = { .name = "Uart1" };
static const osSemaphoreAttr_t sem_attr = { .name = "Uart1" };

//*************************************************************************************************
// Инициализация UART
//*************************************************************************************************
void UartInit( void ) {

    NVIC_InitTypeDef nvic_init;
    
    USARTdrv = &Driver_USART1;
    USARTdrv->Initialize( &CallBackUsart );
    
    USARTdrv->PowerControl( ARM_POWER_FULL );
    USARTdrv->Control( ARM_USART_MODE_ASYNCHRONOUS | ARM_USART_DATA_BITS_8 | ARM_USART_PARITY_NONE | 
                       ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, 57600 );

    #if ( RTE_USART1_TX_DMA == 1 )
    //настройка прерывания для DMA1-channel4 - USART1-TX
    nvic_init.NVIC_IRQChannel = DMA1_Channel4_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 4;
    nvic_init.NVIC_IRQChannelSubPriority = 0;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init( &nvic_init );
    #endif

    #if ( RTE_USART1_RX_DMA == 1 )
    //настройка прерывания для DMA1-channel5 - USART1-RX
    nvic_init.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 5;
    nvic_init.NVIC_IRQChannelSubPriority = 0;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init( &nvic_init );
    #endif

    #if ( RTE_USART1_TX_DMA != 1 && RTE_USART1_RX_DMA != 1 )
    //настройка прерывания для USART1
    nvic_init.NVIC_IRQChannel = USART1_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 5;
    nvic_init.NVIC_IRQChannelSubPriority = 0;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init( &nvic_init );
    #endif
    
    //разрешение передачи
    USARTdrv->Control( ARM_USART_CONTROL_TX, 1 );
    //разрешение приема
    USARTdrv->Control( ARM_USART_CONTROL_RX, 1 );

    //очередь событий
    uart_event = osEventFlagsNew( &evn_attr );
    //семафор блокировки, начальное состояние: семафор = 0 - занят
    sem_busy = osSemaphoreNew( 1, 0, &sem_attr );
    //создаем задачу обработки команд
    osThreadNew( TaskUart, NULL, &task_attr );
    //инициализация приема по UART
    memset( send_buff, 0x00, sizeof( send_buff ) );
    memset( recv_buff, 0x00, sizeof( recv_buff ) );
    //инициализация приема по UART
    USARTdrv->Receive( &recv_ch, 1 );
 }

//*************************************************************************************************
// Задача обработки очереди сообщений приема данных по UART1
//*************************************************************************************************
static void TaskUart( void *argument ) {

    int32_t event;

    for ( ;; ) {
        event = osEventFlagsWait( uart_event, EVN_UART_MASK, osFlagsWaitAny, osWaitForever );
        if ( event > 0 && event & EVN_UART_START1 ) {
            //команда выполнена, возобновляем прием команд по UART1
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
        if ( recv_ch == KEY_BACKSPACE && recv_ind ) {
            recv_ind--;
            recv_buff[recv_ind] = 0x00;
            //продолжаем прием
            USARTdrv->Receive( &recv_ch, 1 );
            return;
           }
        //проверим получения CR - обработка команды
        if ( recv_ch == KEY_CR ) {
            //выполнение команды в TaskCommand()
            osEventFlagsSet( cmnd_event, EVN_CMND_EXEC );
            return; //не выполняем запуск приема по UART1
           }
        recv_buff[recv_ind++] = recv_ch;
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
// Добавляем строку в буфер и запускаем передачу в UART1
//-------------------------------------------------------------------------------------------------
// char *str - указатель на строку для добавления
//*************************************************************************************************
void UartSendStr( char *str ) {

    ARM_USART_STATUS stat;
    uint16_t length, free, cnt_send = 0, offset = 0;
    
    length = strlen( str );
    do {
        //доступное место в буфере
        free = sizeof( send_buff ) - tail;
        if ( !free ) {
            //ждем свободного места в буфере
            if ( osSemaphoreGetCount( sem_busy ) )
                osSemaphoreAcquire( sem_busy, osWaitForever ); //т.к. есть доступый токен - заберем его
            //ждем пока токен освободится из CallBackUsart()
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
// Возвращает адрес приемного буфера UART1
//-------------------------------------------------------------------------------------------------
// return - указатель на буфер
//*************************************************************************************************
char *UartBuffer( void ) {

    return recv_buff;
 }
