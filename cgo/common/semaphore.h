//
// Created by xiaoqj on 2024/1/19.
//

#pragma once

class Semaphore {
protected:
    void* _sem;

public:
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    Semaphore();

    ~Semaphore();

    void wait();

    void post();
};

