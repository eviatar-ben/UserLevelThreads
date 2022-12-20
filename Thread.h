//
// Created by eviatar on 04/05/2021.
//

#ifndef EX2_THREAD_H
#define EX2_THREAD_H

#include "setjmp.h"
#define STACK_SIZE 8192

class Thread {
    private:
        unsigned int tId;
        sigjmp_buf env;
        char stack[STACK_SIZE];
        void (*fp)();
        int quantumCounter;
        bool mutexBlocked;

    public:

        Thread(int id, void (*function)());
        sigjmp_buf* getEnv();
        int getId();
        void incrementCounter();
        int getQuantums() const;
        bool isMutexed();
        void blockMutex();
        void unblockMutex();



    };


#endif //EX2_THREAD_H





