
#ifndef __MESSAGE_H
#define __MESSAGE_H

#include <stdint.h>
#include <stdbool.h>

extern const char msg_crlr[];
extern const char msg_prompt[];
extern const char msg_ok[];
extern const char msg_no_command[];
extern const char msg_str_delim[];
extern const char msg_err_param[];

extern const char msg_data_ok[];
extern const char msg_data_err[];

extern const char msg_read_par[];
extern const char msg_save_par[];

char *ResetSrcDesc( uint32_t res_flg );

#endif 
