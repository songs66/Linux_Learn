#include <iostream>
#include <stdio.h>
#include<pthread.h>
#include <unistd.h>

using namespace std;

#define THREAD_COUNT    10


pthread_mutex_t mutex;


void *thread_callback(void *arg) {
    int *pcount=(int*)arg;
    int i;
    for (i=0;i<100000;i++) {

        pthread_mutex_lock(&mutex);
        (*pcount)++;
        pthread_mutex_unlock(&mutex);

        usleep(1);
    }
    return nullptr;
}



int main(int argc, char *argvs[]) {
    pthread_t threadid[THREAD_COUNT] = {0};

    pthread_mutex_init(&mutex, nullptr);

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
