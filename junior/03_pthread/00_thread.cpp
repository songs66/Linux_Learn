#include <iostream>
#include <stdio.h>
#include<pthread.h>
#include <unistd.h>

#define THREAD_COUNT    10


void *thread_callback(void *arg) {
    int *pcount=(int*)arg;
    int i;
    for (i=0;i<100000;i++) {
        (*pcount)++;
        usleep(1);
    }
    return nullptr;
}



int main(int argc, char *argvs[]) {
    pthread_t threadid[THREAD_COUNT] = {0};

    int i;
    int count=0;
    for (i=0;i<THREAD_COUNT;i++) {
        pthread_create(&threadid[i],NULL,thread_callback,&count);
    }
    for (i=0;i<100;i++) {
        std::cout<<count<< std::endl;
        sleep(1);
    }
    return 0;
}
