
#ifndef __UART_H
#define __UART_H

void UartInit( void );
void UartInitTask( void );
void UartSend( char ch );
void UartSendStr( char *str );
char *UartBuffer( void );

#endif
