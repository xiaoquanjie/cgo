//
// Created by xiaoqj on 2024/1/19.
//

#include "semaphore.h"
#include <malloc.h>
#include <cassert>

#ifdef __GNUC__
#include <semaphore.h>
#else
#include <Windows.h>
#pragma comment(lib, "Kernel32.lib")
#endif

Semaphore::Semaphore() {
#ifdef __GNUC__
    sem_t* sem = (sem_t*)malloc(sizeof(sem_t));
    sem_init(sem, 0, 0);
    this->_sem = sem;
#else
    this->_sem = (void*)CreateSemaphore(nullptr, 0, 1, nullptr);
    assert(this->_sem);
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
    ReleaseSemaphore((HANDLE)this->_sem, 1, nullptr);
#endif
}