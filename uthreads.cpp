#include "uthreads.h"
#include <iostream>
#include <deque>
#include <queue>
#include <vector>
#include "Thread.h"
#include <signal.h>
#include <sys/time.h>
#include <setjmp.h>
#include <algorithm>

#define SUCCESS 0
#define ERROR -1
#define SECOND 1000000

using namespace std;

/* global variables*/

deque<Thread*> readyQ = {};
deque<Thread*> mutexBlocked = {};



priority_queue <int, vector<int>, greater<int> > pq = {};
sigset_t signalsSet;

Thread* mainThread = nullptr;
Thread* runningThread = nullptr;
vector<Thread*> blocked = {};
struct sigaction sa ;
struct itimerval timer;



size_t quantumCounter;

struct Mutex{
    bool isLocked = false;
    int tId = -1;
}typedef Mutex;
Mutex mutex;

/* internal functions*/

void initPq()
{
    for (int i = 0 ; i < MAX_THREAD_NUM; i++ )
    {
        pq.push(i);
    }
}

int getNewId(){

    int ret;
    if (pq.empty()){
        return ERROR;
    }
    ret = pq.top();
    pq.pop();
    return ret;
}

void blockSignals(){
    sigprocmask(SIG_BLOCK, &signalsSet,NULL);
}

void unblockSignals(){
    sigprocmask(SIG_UNBLOCK, &signalsSet,NULL);
}


/*Based on RR that described in the ex's description*/
void scheduler(bool fromBlock = false) {
    blockSignals();
    if (not readyQ.empty()) {
        if (runningThread != nullptr){
            int ret = sigsetjmp(*(runningThread->getEnv()), 1);
            if (ret) {
                unblockSignals();
                return;
            }
        }

        if(runningThread != nullptr and !fromBlock){

            readyQ.push_back(runningThread);
        }
        
        runningThread = readyQ.front();
        if(readyQ.size() != 1 or !blocked.empty())
        {
            readyQ.pop_front();
        }
        // mutex handling
        if(runningThread->isMutexed() and mutex.isLocked)
        {
            while (runningThread->getId() != mutex.tId and mutexBlocked.front() == runningThread)
            {
                if (!runningThread->isMutexed())
                {
                    break;
                }
                readyQ.push_back(runningThread);
                runningThread = readyQ.front();
                readyQ.pop_front();
            }
        }
        else if (runningThread->isMutexed()  and  ! mutex.isLocked){
            while (runningThread->getId() != mutexBlocked.front()->getId())
            {
                if (!runningThread->isMutexed())
                {
                    break;
                }
                readyQ.push_back(runningThread);
                runningThread = readyQ.front();
                readyQ.pop_front();
            }
            if(runningThread->isMutexed())
            {
                mutexBlocked.pop_front();
                runningThread->unblockMutex();
                mutex.tId = runningThread->getId();
                mutex.isLocked = true;
            }

        }

        // mutex status is free:

        quantumCounter++;
        runningThread->incrementCounter();
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
            cerr<<"system error: setitimer error"<<endl;
            unblockSignals();
            exit(1);
        }
        unblockSignals();
        siglongjmp(*(runningThread->getEnv()), 1);
    }
}

void defineSignalsSet(){
    sigemptyset(&signalsSet);
    sigaddset(&signalsSet,SIGVTALRM);
}


void releaseLibMemory() {

}

void timer_handler(int sig) {
    if (sig == SIGVTALRM)
    {
        blockSignals();
        scheduler();
        unblockSignals();
    }
}

struct findById {
    explicit findById(const int & id) : id(id) {}
    bool operator()(Thread* thread) {
        return thread->getId() == id;
    }
private:
    int id;
};



/* External interface */


/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
 int uthread_init(int quantum_usecs)
 {

     blockSignals();
     if (quantum_usecs <= 0){
         cerr << "thread library error: It is an error to call this function with non-positive quantum_usecs" << endl;
         unblockSignals();
         return ERROR;
     }

     quantumCounter = 1;
     initPq();
     mainThread = new Thread(getNewId(), nullptr);
     mainThread->incrementCounter();
     defineSignalsSet();

     runningThread = mainThread;
     sa.sa_handler = &timer_handler;


     timer.it_value.tv_sec = (quantum_usecs/SECOND);
     timer.it_value.tv_usec = (quantum_usecs % SECOND);

     timer.it_interval.tv_sec = (quantum_usecs/SECOND);
     timer.it_interval.tv_usec = (quantum_usecs % SECOND);

     if (sigaction(SIGVTALRM, &sa,NULL) < 0) {
         cerr<<"system error: sigaction error."<<endl;
         unblockSignals();
         exit(1);
     }
     if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
         cerr<<"system error: setitimer error"<<endl;
         unblockSignals();
         exit(1);
     }

     unblockSignals();
     return SUCCESS;
 }




/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
 int uthread_spawn(void (*f)(void))
 {

     blockSignals();
     if(readyQ.size() + blocked.size() + 1 >= MAX_THREAD_NUM )
     {
         cerr<< "thread library error: too many threads" << endl;
         unblockSignals();
         return ERROR;
     }
     int newId = getNewId();

    if (newId == ERROR){
         cerr << "thread library error: too many threads" << endl;
         unblockSignals();
         return ERROR;
     }
     auto newThread = new Thread(newId, f);
     readyQ.push_back(newThread);
//     scheduler();
     unblockSignals();
     return newId;
 }

/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
 int uthread_terminate(int tid)
 {
     blockSignals();
     if(tid < 0)
     {
         cerr<< "thread library error: illegal tid"<<endl;
         unblockSignals();
         return ERROR;
     }
     if (tid == 0)
     {
         releaseLibMemory();
         unblockSignals();
         exit(SUCCESS);
     }

     // unlock the mutex if the terminated thread locked the mutex
     if (tid == mutex.tId)
     {
         mutex.isLocked = false;
         mutex.tId = -1;
     }
     if(! mutexBlocked.empty())
     {
         auto foundMutex = find_if(mutexBlocked.begin(), mutexBlocked.end(), findById(tid));
         if ((*foundMutex) != NULL and foundMutex != mutexBlocked.end() )
         {
             mutexBlocked.erase(foundMutex);
         }
     }

     // running thread
     if(runningThread->getId() == tid)
     {
         delete(runningThread);
         pq.push(tid);
         runningThread = nullptr;
         unblockSignals();
         scheduler();
         return SUCCESS;
     }

     if (! blocked.empty())
     {
         auto foundInBlocked = find_if(blocked.begin(), blocked.end(), findById(tid));

         // blocked thread:
         if (foundInBlocked != blocked.end() and *foundInBlocked != NULL)
         {
             delete *foundInBlocked;
             pq.push(tid);
             blocked.erase(foundInBlocked);
             unblockSignals();
             scheduler();
             return SUCCESS;
         }
     }
     if( ! readyQ.empty())
     {
         //search thread in 'readyQ'
         auto foundInReadyQ = find_if(readyQ.begin(), readyQ.end(), findById(tid));

         //check if the correct thread is found
         if (*foundInReadyQ != NULL &&(*foundInReadyQ)->getId() == tid)
         {
             delete *foundInReadyQ;
             readyQ.erase(foundInReadyQ);
             pq.push(tid);
             unblockSignals();
             return SUCCESS;
         }

     }


     // tid don't exist
     cerr<< "thread library error: no thread with ID tid exist"<<endl;
     unblockSignals();
     return ERROR;

 }



/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
 int uthread_block(int tid)
 {

     blockSignals();
     if(tid == 0){
         cerr<<"thread library error: can't block the main thread"<<endl;
         unblockSignals();
         return ERROR;
     }

    //search thread in 'blocked'
    if (! blocked.empty())
    {
        if(! blocked.empty())
        {
            auto foundInBlocked = find_if(blocked.begin(), blocked.end(), findById(tid));
            if (*foundInBlocked != NULL &&foundInBlocked != blocked.end() ){
                unblockSignals();
                return SUCCESS;
            }
        }
    }

     // find the thread with the given ID, remove it from where it is stored
     // and add it to the 'blocked' vector.
     if (runningThread->getId() == tid){
        blocked.push_back(runningThread);
        scheduler(true);
        unblockSignals();
        return SUCCESS;
     }


     //search thread in 'readyQ'
     if (! readyQ.empty())
     {
         auto foundInReadyQ = find_if(readyQ.begin(), readyQ.end(), findById(tid));

         //check if the correct thread is found
         if (*foundInReadyQ != NULL &&(*foundInReadyQ)->getId() == tid){
             blocked.push_back(*foundInReadyQ);
             readyQ.erase(foundInReadyQ);
             unblockSignals();
             return SUCCESS;
         }
     }


     cerr<<"thread library error: thread with id "<<tid<<" didn't found"<<endl;
     unblockSignals();
     return ERROR;
 }


/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
 int uthread_resume(int tid)
 {

     blockSignals();
     if(tid == 0)
     {
         unblockSignals();
         return SUCCESS;
     }
     if (tid < 0 or tid > MAX_THREAD_NUM)
     {
         cerr << "thread library error: thread with id " << tid << "is invalid" << endl;
         unblockSignals();

         return ERROR;
     }
     if (runningThread->getId() == tid){
         unblockSignals();
         return SUCCESS;
     }

     //search thread in 'readyQ'
     if (! readyQ.empty())
     {
         auto foundInReadyQ = find_if(readyQ.begin(), readyQ.end(), findById(tid));

         //check if the correct thread is found
         if (*foundInReadyQ != NULL && foundInReadyQ != readyQ.end() && (*foundInReadyQ)->getId() == tid){
             unblockSignals();
             return SUCCESS;
         }
     }
     //search thread in 'blocked'
     if (! blocked.empty())
     {
         auto foundInBlocked = find_if(blocked.begin(), blocked.end(), findById(tid));
         if (foundInBlocked != blocked.end()){
             readyQ.push_back(*foundInBlocked);
             blocked.erase(foundInBlocked);
             unblockSignals();
             return SUCCESS;
         }
     }

     cerr<<"thread library error: thread with id "<<tid<<" didn't found"<<endl;
     unblockSignals();
     return ERROR;
 }


/*
 * Description: This function tries to acquire a mutex.
 * If the mutex is unlocked, it locks it and returns.
 * If the mutex is already locked by different thread, the thread moves to BLOCK state.
 * In the future when this thread will be back to RUNNING state,
 * it will try again to acquire the mutex.
 * If the mutex is already locked by this thread, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
 *
*/
 int uthread_mutex_lock()
 {
     blockSignals();
     if (mutex.isLocked)
     {
         if (mutex.tId == runningThread->getId())
         {
             cerr<< "thread library error: the mutex is already locked by this thread" << endl;
             unblockSignals();
             return ERROR;
         }
         runningThread->blockMutex();
         mutexBlocked.push_back(runningThread);
         scheduler();
         unblockSignals();
         return SUCCESS;
     }
     else if (! mutex.isLocked)
     {
         mutex.tId = runningThread->getId();
         mutex.isLocked = true;
     }
     unblockSignals();
     return SUCCESS;
 }


/*
 * Description: This function releases a mutex.
 * If there are blocked threads waiting for this mutex,
 * one of them (no matter which one) moves to READY state.
 * If the mutex is already unlocked, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
 int uthread_mutex_unlock()
 {
    blockSignals();
    if (!mutex.isLocked)
    {
        unblockSignals();
        return ERROR;
    }
    if (runningThread->getId() != mutex.tId){
         cerr << "thread library error: the mutex is already unlocked by another thread" << endl;
         unblockSignals();
         return ERROR;
    }
    if (runningThread->getId() == mutex.tId)
    {
        runningThread->unblockMutex();
        mutex.tId = -1;
        mutex.isLocked = false;
    }
    unblockSignals();
    return SUCCESS;
 }


/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
 int uthread_get_tid()
 {
     return runningThread->getId();
 }


/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
 int uthread_get_total_quantums()
 {
     return quantumCounter;

 }


/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
 int uthread_get_quantums(int tid) {
    blockSignals();
    //check the running thread
    if (runningThread->getId() == tid) {
        unblockSignals();
        return runningThread->getQuantums();
    } else if (! readyQ.empty()){
        //search in 'readyQ'
        auto foundInReadyQ = find_if(readyQ.cbegin(), readyQ.cend(), findById(tid));
        if (*foundInReadyQ != NULL && (*foundInReadyQ)->getId() == tid) {
            unblockSignals();
            return (*foundInReadyQ)->getQuantums();
        } else if (! blocked.empty()){
            //search in 'blocked'
            auto foundInBlocked = find_if(readyQ.cbegin(), readyQ.cend(), findById(tid));
            if (*foundInReadyQ != NULL && (*foundInBlocked)->getId() == tid) {
                unblockSignals();
                return (*foundInBlocked)->getQuantums();
            }
        }
    }

    cerr << "thread library error: thread with id " << tid << " didn't found" << endl;
    unblockSignals();
    return ERROR;
}


