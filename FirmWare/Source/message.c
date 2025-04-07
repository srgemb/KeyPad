                                 
//*************************************************************************************************
//
// Модуль сообщений
//
//*************************************************************************************************

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "cmsis_os2.h"

#include "message.h"
#include "parse.h"

//*************************************************************************************************
// Переменные с внешним доступом
//*************************************************************************************************
static char temp[100];

const char msg_crlr[]       = "\r\n";

const char msg_prompt[]     = "\r\n>";
const char msg_no_command[] = "\r\nUnknown command";
const char msg_str_delim[]  = "----------------------------------------------------\r\n";
const char msg_err_param[]  = "\r\nInvalid parameters\r\n";

const char msg_data_ok[]    = "\r\nOK\r\n";
const char msg_data_err[]   = "\r\nERROR\r\n";

const char msg_read_par[]   = "Reading parameters: ... %s\r\n";
const char msg_save_par[]   = "Saving parameters: ... %s\r\n";

const char msg_ok[]         = "OK\r\n";

static char * const reset_desc[] = { "PINRST ", "PORRST ", "SFTRST ", "IWDGRST ", "WWDGRST ", "LPWRRST " };

//*************************************************************************************************
// Функция озвращает расшифровку источника сброса микроконтроллера
//-------------------------------------------------------------------------------------------------
// return - указатель на строку с расшифровкой источника сброса
//*************************************************************************************************
char *ResetSrcDesc( uint32_t res_flg ) {

    uint8_t i, mask = 0x01;

    memset( temp, 0x00, sizeof( temp ) );
    for ( i = 0; i < SIZE_ARRAY( reset_desc ); i++, mask <<= 1 ) {
        if ( res_flg & mask )
            strcat( temp, reset_desc[i] );
       }
    return temp;
 }
