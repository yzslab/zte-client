//
// Created by root on 7/19/17.
//

#include <setjmp.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include "exception.h"

#define EXCEPTION_SET_SIZ 10

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static int semid = -1;

exception *exceptionSet[EXCEPTION_SET_SIZ];

void initException() {
    static pthread_mutex_t initMtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&initMtx);
    if (semid == -1) {
        semid = semget(IPC_PRIVATE, 1, S_IRUSR | S_IWUSR);
        union semun arg;
        arg.val = EXCEPTION_SET_SIZ;
        semctl(semid, 0, SETVAL, arg);
    }
    pthread_mutex_unlock(&initMtx);
}

void throwException(jmp_buf env, const char *fileName, int line, int code, const char *message, int *index) {
    int errorTag = 0;
    if (pthread_mutex_lock(&mtx) != 0 || semid == -1)
        longjmp(env, -1);
    struct sembuf sops;
    sops.sem_num = 0;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    semop(semid, &sops, 1);
    int i = 0;
    for (; i < EXCEPTION_SET_SIZ; ++i)
        if (!exceptionSet[EXCEPTION_SET_SIZ])
            break;
    exceptionSet[i] = malloc(sizeof(exception));
    if (!exceptionSet[i])
        errorTag = 1;
    pthread_mutex_unlock(&mtx);
    if (errorTag)
        longjmp(env, -1);

    int len = strlen(fileName) + 1;
    exceptionSet[i]->fileName = malloc(sizeof(char) * len);
    if (exceptionSet[i]->fileName)
        strcpy(exceptionSet[i]->fileName, fileName);
    exceptionSet[i]->line = line;
    exceptionSet[i]->code = code;
    len = strlen(message) + 1;
    exceptionSet[i]->message = malloc(sizeof(char) * len);
    if (exceptionSet[i]->message)
        strcpy(exceptionSet[i]->message, message);
    if (index)
        *index = i + 1;
    longjmp(env, code);
}

exception *getException(int index) {
    return exceptionSet[index - 1];
}

void destroyException(int index) {
    exception *ex = exceptionSet[index - 1];
    if (!ex)
        return;
    if (ex->fileName)
        free(ex->fileName);
    if (ex->message)
        free(ex->message);
    free(ex);
    exceptionSet[index - 1] = NULL;
    struct sembuf sops;
    sops.sem_num = 0;
    sops.sem_op = 1;
    sops.sem_flg = 0;
    semop(semid, &sops, 1);
}