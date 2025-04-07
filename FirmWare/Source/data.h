
#ifndef __DATA_H
#define __DATA_H

#include <stdint.h>
#include <stdbool.h>

void DataInit( void );
void DataUartInit( void );
void DataSend( uint8_t data, uint8_t alt_key );
void DataSendStr( char *str );
char *DataBuffer( void );

#endif
