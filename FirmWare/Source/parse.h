
#ifndef __PARSE_H
#define __PARSE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_CNT_PARAM               6           //максимальное кол-во параметров, включая команду
#define MAX_LEN_PARAM               40          //максимальный размер (длина) одного параметра в командной строке

#define SIZE_ARRAY( array )         ( sizeof( array )/sizeof( array[0] ) )

//индексы для получения параметров команды
typedef enum {
    IND_PAR_CMND,                               //команда
    IND_PARAM1,                                 //параметр 1
    IND_PARAM2,                                 //параметр 2
    IND_PARAM3,                                 //параметр 3
    IND_PARAM4,                                 //параметр 4
    IND_PARAM5,                                 //параметр 5
    IND_PARAM6                                  //параметр 6
 } CmndParam;

//*************************************************************************************************
// Функции статуса/состояния
//*************************************************************************************************
uint8_t ParseCommand( char *src );
uint8_t GetParamCnt( void );
char *GetParamVal( CmndParam index );
char *GetParamList( void );

#endif
