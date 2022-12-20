# User Level Threads
A functional static library, that creates and manages userlevel threads.
A potential user will be able to include your library and use it according to the package’s public interface:
the uthreads.h header file.

* __The Threads__

  Initially, a program is comprised of the default main thread, whose ID is 0. All other threads will be explicitly
  created. The maximal number of threads the library should support (including the main thread) is MAX_THREAD_NUM.

* __Thread State Diagram__

  At any given time during the running of the user's program, each of the threads in the program is in one of
  the states shown in the following state diagram. Transitions from state to state occur as a result of calling
  one of the library functions, or from elapsing of time, as explained below.

![image](https://user-images.githubusercontent.com/82065601/208743584-d9bb4539-d8fd-4023-9422-40fd47d07a23.png)

* __Scheduler__

  Round-Robin (RR) scheduling algorithm.
