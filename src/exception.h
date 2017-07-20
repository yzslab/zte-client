//
// Created by root on 7/19/17.
//

#ifndef ZTE_CLIENT_EXCEPTION_H
#define ZTE_CLIENT_EXCEPTION_H

struct exception {
    char *fileName;
    int line;
    int code;
    char *message;
};

typedef struct exception exception;

#if _SEM_SEMUN_UNDEFINED == 1
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
#if defined(__linux__)
    struct seminfo *__buf;
#endif
};
#endif

void initException();
void throwException(jmp_buf env, const char *fileName, int line, int code, const char *message, int *index);
exception *getException(int index);
void destroyException(int index);

#endif //ZTE_CLIENT_EXCEPTION_H
