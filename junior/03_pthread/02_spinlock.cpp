#include <iostream>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

using namespace std;

#define THREAD_COUNT    10

pthread_spinlock_t spinlock;

void *thread_callback(void *arg) {
    int *pcount=(int*)arg;
    int i;
    for (i=0;i<100000;i++) {

        pthread_spin_lock(&spinlock);
        (*pcount)++;
        pthread_spin_unlock(&spinlock);

        usleep(1);
    }
    return nullptr;
}

int main(int argc, char *argvs[]) {
    pthread_t threadid[THREAD_COUNT] = {0};

    pthread_spin_init(&spinlock,PTHREAD_PROCESS_SHARED);

    int i;
    int count=0;
    for (i=0;i<THREAD_COUNT;i++) {
        pthread_create(&threadid[i], nullptr,thread_callback,&count);
    }
    for (i=0;i<100;i++) {
        cout<<count<< endl;
        sleep(1);
    }
    return 0;
}
