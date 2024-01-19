//
// Created by xiaoqj on 2024/1/19.
//

#include "semaphore.h"
#include <malloc.h>

#ifdef __GNUC__
#include <semaphore.h>
#else
#include <winbase.h>
#endif

Semaphore::Semaphore() {
#ifdef __GNUC__
    sem_t* sem = (sem_t*)malloc(sizeof(sem_t));
    sem_init(sem, 0, 0);
    this->_sem = sem;
#else
    this->_sem = (void*)CreateSemaphore(NULL, 1, 0, NULL);
#endif
}

Semaphore::~Semaphore() {
#ifdef __GNUC__
    sem_destroy((sem_t*)this->_sem);
    free(this->_sem);
#else
    CloseHandle((HANDLE)this->_sem);
#endif
}

void Semaphore::wait() {
#ifdef __GNUC__
    sem_wait((sem_t*)this->_sem);
#else
    WaitForSingleObject((HANDLE)this->_sem, INFINITE);
#endif
}

void Semaphore::post() {
#ifdef __GNUC__
    sem_post((sem_t*)this->_sem);
#else
    ReleaseSemaphore((HANDLE)this->_sem, 1, NULL);
#endif
}