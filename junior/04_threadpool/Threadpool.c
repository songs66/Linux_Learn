//编译指令为：gcc Threadpool.c -o Threadpool -lpthread
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


#define LIST_INSERT_HEAD(item, list) do {       \
    (item)->prev = NULL;                        \
    (item)->next = (list);                      \
    if ((list) != NULL)                         \
        (list)->prev = (item);                  \
    (list) = (item);                            \
} while (0)

#define LIST_INSERT_TAIL(item, list) do {       \
    (item)->next = NULL;                        \
    (item)->prev = NULL;                        \
    if ((list) == NULL)                         \
        (list) = (item);                        \
    else {                                      \
        typeof(item) tail_ = (list);            \
        while (tail_->next) tail_ = tail_->next;\
        tail_->next = (item);                   \
        (item)->prev = tail_;                   \
    }                                           \
} while (0)

#define LIST_REMOVE(item, list) do {            \
    if ((item)->prev != NULL)                   \
        (item)->prev->next = (item)->next;      \
    if ((item)->next != NULL)                   \
        (item)->next->prev = (item)->prev;      \
    if ((list) == (item))                       \
        (list) = (item)->next;                  \
    (item)->prev = (item)->next = NULL;         \
} while (0)

//任务节点
struct nTask {
    void (*task_func)(void *arg);

    void *user_data;

    struct nTask *prev;
    struct nTask *next;
};

//工作节点
typedef struct nManager ThreadPool;
struct nWorker {
    pthread_t threadid;
    int terminate;//是否终止的标识
    ThreadPool *manager;//typedef struct nManager ThreadPool;共享线程池

    struct nWorker *prev;
    struct nWorker *next;
};

//管理组件(线程池)
typedef struct nManager {
    struct nTask *tasks;
    struct nWorker *workers;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ThreadPool;

//回调函数（不等于任务）
static void* nThreadPoolCallback(void *arg) {
    struct nWorker *worker=arg;

    while (1) {

        pthread_mutex_lock(&worker->manager->mutex);//lock
        while (worker->manager->tasks==NULL) {
            if (worker->terminate) {//终止信号
                break;
            }
            pthread_cond_wait(&worker->manager->cond,&worker->manager->mutex);//wait
        }
        if (worker->terminate) {//终止信号
            pthread_mutex_unlock(&worker->manager->mutex);//unlock
            break;
        }
        struct nTask *task=worker->manager->tasks;//取任务
        LIST_REMOVE(task,worker->manager->tasks);//被取出则删

        pthread_mutex_unlock(&worker->manager->mutex);//unlock

        task->task_func(task);//做任务
    }
    free(worker);
    return NULL;
}

//线程池创建
int nThreadPoolCreate(ThreadPool *pool, int numWorkers) {
    //校验参数
    if (pool == NULL) {
        return -1;
    }
    if (numWorkers < 1) {
        numWorkers = 1;
    }
    //初始化*pool
    memset(pool,0,sizeof(ThreadPool));
    //.初始化cond
    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
    memcpy(&pool->cond, &blank_cond, sizeof(pthread_cond_t));
    //.初始化mutex
    pthread_mutex_init(&pool->mutex,NULL);
    //.初始化workers
    int i = 0;
    for (i = 0; i < numWorkers; i++) {
        struct nWorker *worker=malloc(sizeof(struct nWorker));
        if (worker==NULL) {
            perror("malloc");
            return -2;
        }
        memset(worker,0,sizeof(struct nWorker));
        worker->manager=pool;//共享线程池
        //.开始创建线程
        //第三个参数传的是函数地址，函数名会被编译器自动识别为函数地址
        int ret=pthread_create(&worker->threadid,NULL,&nThreadPoolCallback,worker);//创建成功返回0，不成功返回非0
        if (ret) {
            perror("pthread_create error");
            free(worker);
            return -3;
        }
        LIST_INSERT_TAIL(worker,pool->workers);//插入节点
    }
    return 0;
}

//线程池销毁
void nThreadPoolDestroy(ThreadPool *pool, int numWorkers) {
    struct nWorker *worker=NULL;
    for (worker=pool->workers;worker!=NULL;worker=worker->next) {
        worker->terminate=1;
    }

    pthread_mutex_lock(&pool->mutex);

    pthread_cond_broadcast(&pool->cond);//广播(唤醒所有)

    pthread_mutex_unlock(&pool->mutex);

    pool->tasks=NULL;
    pool->workers=NULL;

}

//push任务进入线程池
int nThreadPoolPushTask(ThreadPool *pool, struct nTask *task) {
    pthread_mutex_lock(&pool->mutex);

    LIST_INSERT_TAIL(task,pool->tasks);

    pthread_cond_signal(&pool->cond);//唤醒一个线程

    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

//---------------------------------------------------------------------------
#if 1

#define THREADPOOL_INIT_COUNT   10
#define TASK_INIT_SIZE          10

void task_entry(void *arg) {
    struct nTask *task=arg;
    int idx=*(int *)task->user_data;

    printf("idx:%d\n",idx);

    free(task->user_data);
    free(task);
}

void task_entry_1(void *arg) {
    struct nTask *task=arg;
    int idx=*(int *)task->user_data;

    printf("第%d线程运行中......\n",idx);
    sleep(2);
    printf("第%d线程运行完成\n",idx);
    sleep(2);

    free(task->user_data);
    free(task);
}

int main(const int argc, char *argv[]) {
    if (argc<2) {
        perror("parm error\n");
        return -1;
    }
    ThreadPool pool;

    nThreadPoolCreate(&pool,THREADPOOL_INIT_COUNT);

    int i;
    for (i=0;i<atoi(argv[1]);i++) {
        struct nTask *task=malloc(sizeof(struct nTask));
        if (task==NULL) {
            perror("malloc error");
            exit(1);
        }
        memset(task,0,sizeof(struct nTask));

        task->task_func=task_entry_1;//可变函数
        task->user_data=malloc(sizeof(int));
        *(int*)task->user_data=i;

        nThreadPoolPushTask(&pool,task);
        printf("第%d个任务添加成功\n",i+1);
    }

    getchar();

    nThreadPoolDestroy(&pool,THREADPOOL_INIT_COUNT);

    return 0;
}

#endif














