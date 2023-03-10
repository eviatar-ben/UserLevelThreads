//
// Created by eviatar on 04/05/2021.
//

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr){
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif
#include <signal.h>
#include "Thread.h"
#include "setjmp.h"

Thread::Thread(int id, void (*function)(void)):
        tId(id),fp(function), quantumCounter(0), mutexBlocked(false)
{
    address_t  sp,pc;
    sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t)fp;
    int ret = sigsetjmp(env, 1);

    if (ret ==0){
        (env->__jmpbuf)[JB_SP] = translate_address(sp);
        (env->__jmpbuf)[JB_PC] = translate_address(pc);
        sigemptyset(&env->__saved_mask);
    }
}

int Thread::getId() {
    return tId;
}

sigjmp_buf* Thread::getEnv(){
    return &env;
}

void Thread::incrementCounter() {
    quantumCounter++;
}


int Thread::getQuantums() const {
    return quantumCounter;
}

bool Thread::isMutexed(){
    return mutexBlocked;
}

void Thread::blockMutex(){
    mutexBlocked = true;
}
void Thread::unblockMutex(){
    mutexBlocked = false;
}



