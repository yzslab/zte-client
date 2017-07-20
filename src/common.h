//
// Created by root on 7/19/17.
//

#ifndef ZTE_CLIENT_COMMON_H
#define ZTE_CLIENT_COMMON_H

#include <setjmp.h>
#include "exception.h"

#define USERNAME_MAX_LENGTH 32
#define PASSWORD_MAX_LENGTH 32
#define DEV_MAX_LENGTH 16

extern jmp_buf mainEnv;


#endif //ZTE_CLIENT_COMMON_H
