// mini_ntyco_fixed.c  —— 修正版：ucontext + epoll 的最小协程 echo server
#define _GNU_SOURCE
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dlfcn.h>
#include <errno.h>
#include <arpa/inet.h>

enum { STACK_SIZE = 16*1024, MAX_EVENTS = 64 };
typedef void (*proc_t)(void *);

typedef struct coro {
    ucontext_t ctx;
    char       stack[STACK_SIZE];

    proc_t     entry;
    void      *arg;

    int        state;          // 0:NEW  1:RUN  2:SUSPEND  3:DEAD
    int        wait_fd;
    int        wait_event;     // EPOLLIN / EPOLLOUT
} coro;

typedef struct schedule {
    ucontext_t main;
    int        epfd;
    int        cur_id;
    int        cap;
    coro     **co;
    int        listenfd;
} schedule;

static schedule *g_sched;

/* ----------------- forward ----------------- */
static void coro_body(schedule *S);
static int coro_create(schedule *S, proc_t func, void *arg);
static void coro_resume(schedule *S, int id);
static void coro_yield(schedule *S);
static void schedule_loop(schedule *S);

/* ----------------- coroutine core ----------------- */
static void coro_body(schedule *S)
{
    int id = S->cur_id;
    coro *c = S->co[id];
    c->state = 1;
    c->entry(c->arg);
    c->state = 3;      // DEAD
    S->cur_id = -1;
}

static int coro_create(schedule *S, proc_t func, void *arg)
{
    int id = S->cap++;
    S->co = realloc(S->co, sizeof(coro*) * S->cap);
    if (!S->co) { perror("realloc"); exit(1); }
    coro *c = calloc(1, sizeof(coro));
    if (!c) { perror("calloc"); exit(1); }
    c->entry = func;
    c->arg = arg;
    c->state = 0;
    c->wait_fd = -1;
    c->wait_event = 0;
    S->co[id] = c;
    return id;
}

static void coro_resume(schedule *S, int id)
{
    if (id < 0 || id >= S->cap) return;
    coro *c = S->co[id];
    if (!c || c->state == 3) return;
    S->cur_id = id;
    if (c->state == 0) {
        getcontext(&c->ctx);
        c->ctx.uc_stack.ss_sp = c->stack;
        c->ctx.uc_stack.ss_size = STACK_SIZE;
        c->ctx.uc_link = &S->main;
        makecontext(&c->ctx, (void (*)(void))coro_body, 1, S);
    }
    swapcontext(&S->main, &c->ctx);
}

static void coro_yield(schedule *S)
{
    int id = S->cur_id;
    if (id < 0) return;
    coro *c = S->co[id];
    c->state = 2;
    S->cur_id = -1;
    swapcontext(&c->ctx, &S->main);
}

/* ----------------- syscall hooks ----------------- */
static ssize_t (*sys_read)(int, void *, size_t) = NULL;
static ssize_t (*sys_write)(int, const void *, size_t) = NULL;

/* Hooked read: 在协程中变成非阻塞并在 EAGAIN 时 yield，fd 必须在 accept 时加入 epoll */
ssize_t read(int fd, void *buf, size_t n)
{
    schedule *S = g_sched;
    int id = S->cur_id;
    if (id >= 0) {
        int fl = fcntl(fd, F_GETFL, 0);
        if (fl != -1) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        while (1) {
            ssize_t r = sys_read(fd, buf, n);
            if (r >= 0) return r;
            if (errno != EAGAIN && errno != EWOULDBLOCK) return -1;
            S->co[id]->wait_fd = fd;
            S->co[id]->wait_event = EPOLLIN;
            coro_yield(S);
        }
    }
    return sys_read(fd, buf, n);
}

ssize_t write(int fd, const void *buf, size_t n)
{
    schedule *S = g_sched;
    int id = S->cur_id;
    if (id >= 0) {
        int fl = fcntl(fd, F_GETFL, 0);
        if (fl != -1) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        while (1) {
            ssize_t w = sys_write(fd, buf, n);
            if (w >= 0) return w;
            if (errno != EAGAIN && errno != EWOULDBLOCK) return -1;
            S->co[id]->wait_fd = fd;
            S->co[id]->wait_event = EPOLLOUT;
            coro_yield(S);
        }
    }
    return sys_write(fd, buf, n);
}

/* ----------------- echo coroutine ----------------- */
/* arg: pointer to heap-allocated int (client fd). echo_client 在结束时会 free(arg). */
static void echo_client(void *arg)
{
    int *pfd = (int *)arg;
    int fd = *pfd;
    free(pfd);   // 复制了 fd，释放堆内存
    char buf[512];

    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == 0) { /* peer closed */ break; }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("read client");
            break;
        }
        ssize_t s = 0;
        while (s < n) {
            ssize_t w = write(fd, buf + s, n - s);
            if (w <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                perror("write client");
                goto out;
            }
            s += w;
        }
    }

out:
    close(fd);
}

/* ----------------- scheduler: epoll-based loop ----------------- */
static void schedule_loop(schedule *S)
{
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int nfds = epoll_wait(S->epfd, events, MAX_EVENTS, -1); // 阻塞直到事件
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            int mask = events[i].events;

            if (fd == S->listenfd) {
                /* listenfd 就绪：接受所有连接（非阻塞 accept 循环） */
                while (1) {
                    struct sockaddr_in cli;
                    socklen_t len = sizeof(cli);
                    int cfd = accept(S->listenfd, (struct sockaddr*)&cli, &len);
                    if (cfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }
                    /* 设置非阻塞 */
                    int fl = fcntl(cfd, F_GETFL, 0);
                    if (fl != -1) fcntl(cfd, F_SETFL, fl | O_NONBLOCK);

                    /* 把 client fd 加到 epoll 关注读事件（level-trigger） */
                    struct epoll_event ev;
                    ev.data.fd = cfd;
                    ev.events = EPOLLIN; // level triggered
                    if (epoll_ctl(S->epfd, EPOLL_CTL_ADD, cfd, &ev) < 0) {
                        perror("epoll_ctl add client");
                        close(cfd);
                        continue;
                    }

                    /* 为协程准备堆内存的 fd 拷贝（避免使用栈地址） */
                    int *pfd = malloc(sizeof(int));
                    *pfd = cfd;
                    coro_create(S, echo_client, pfd);
                }
                continue;
            }

            /* 普通 fd：寻找等待该 fd 的协程并唤醒 */
            for (int k = 0; k < S->cap; k++) {
                coro *c = S->co[k];
                if (!c) continue;
                if (c->state == 2 && c->wait_fd == fd &&
                    (c->wait_event & mask)) {
                    /* 唤醒前不要删除 fd（仍由 epoll 管理）*/
                    c->wait_fd = -1;
                    c->wait_event = 0;
                    coro_resume(S, k);
                    break;
                }
            }
        }

        /* 如果没有正在运行的协程，挑一个非 DEAD 的跑一下（可选） */
        if (S->cur_id == -1) {
            for (int k = 0; k < S->cap; k++) {
                if (S->co[k] && S->co[k]->state != 3) {
                    coro_resume(S, k);
                    break;
                }
            }
        }
    }
}

/* ----------------- main ----------------- */
int main()
{
    schedule S;
    memset(&S, 0, sizeof(S));
    g_sched = &S;
    S.cur_id = -1;   // 重要：表示当前没有协程在运行

    S.epfd = epoll_create1(0);
    if (S.epfd < 0) { perror("epoll_create1"); return 1; }

    sys_read  = dlsym(RTLD_NEXT, "read");
    sys_write = dlsym(RTLD_NEXT, "write");
    if (!sys_read || !sys_write) {
        fprintf(stderr, "dlsym failed\n");
        return 1;
    }

    /* 监听端口 */
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8888);

    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(listenfd); return 1;
    }
    if (listen(listenfd, 128) < 0) { perror("listen"); close(listenfd); return 1; }

    /* 将 listenfd 设置为非阻塞并加入 epoll */
    int fl = fcntl(listenfd, F_GETFL, 0);
    if (fl != -1) fcntl(listenfd, F_SETFL, fl | O_NONBLOCK);

    struct epoll_event ev;
    ev.data.fd = listenfd;
    ev.events = EPOLLIN;
    if (epoll_ctl(S.epfd, EPOLL_CTL_ADD, listenfd, &ev) < 0) {
        perror("epoll_ctl add listenfd"); close(listenfd); return 1;
    }

    S.listenfd = listenfd;

    printf("mini_ntyco_fixed running, listening on 0.0.0.0:8888\n");

    /* 进入调度循环（会同时处理 accept + client events） */
    schedule_loop(&S);

    /* cleanup */
    close(listenfd);
    close(S.epfd);
    return 0;
}
