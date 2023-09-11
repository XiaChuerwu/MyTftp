#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "threadpool.h"

/* 创建线程池 */
threadpool_t *creat_threadpool(int min_thr_num, int max_thr_num, int queue_max_size) {
    // 为线程池分配空间
    threadpool_t *pool = (threadpool_t *)malloc(sizeof(threadpool_t));
    if (pool == NULL) {
        threadpool_free(pool);
        return NULL;
    }

    // 初始化线程池的信息
    pool->min_thread_num = min_thr_num;
    pool->max_thread_num = max_thr_num;
    pool->live_thread_num = min_thr_num;
    pool->busy_thread_num = 0;
    pool->wait_exit_num = 0;

    // 初始化队列的信息
    pool->queue_front = 0;
    pool->queue_rear = 0;
    pool->queue_size = 0;

    pool->queue_max_size = queue_max_size;

    pool->shutdown = 0;

    // 初始化互斥锁和条件变量
    if (pthread_mutex_init(&pool->pool_lock, NULL) != 0 || pthread_mutex_init(&pool->work_lock, NULL) != 0 ||
        pthread_cond_init(&pool->queue_not_full, NULL) != 0 || pthread_cond_init(&pool->queue_not_empty, NULL) != 0) {
        printf("init lock or cond false;\n");
        threadpool_free(pool);
        return NULL;
    }

    // 创建线程ID数组并初始化
    pool->tid = (pthread_t *)malloc(sizeof(pthread_t) * max_thr_num);
    if (pool->tid == NULL) {
        printf("malloc threads false;\n");
        threadpool_free(pool);
        return NULL;
    }

    // 为任务队列开辟空间
    pool->task_queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_max_size);
    if (pool->task_queue == NULL) {
        printf("malloc task queue false;\n");
        threadpool_free(pool);
        return NULL;
    }
    memset(pool->task_queue, 0, sizeof(threadpool_task_t)*queue_max_size);

    // 开启最少个线程
    for (int i = 0; i < min_thr_num; i++) {
        // 往线程池丢线程 所有的线程都在线程池等任务
        pthread_create(&pool->tid[i], NULL, threadpool_thread, (void *)pool);
    }

    /* 管理者线程 admin_thread函数在后面讲解 */
    pthread_create(&(pool->admin_tid), NULL, admin_thread, (void *)pool);

    return pool;
}

/* 工作线程池 */
void *threadpool_thread(void *threadpool) {
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;

    while (1) {
        // 先上锁
        pthread_mutex_lock(&pool->pool_lock);

        // 如果没有任务并且没有下班就阻塞,有任务就跳出
        while (pool->queue_size == 0 && !pool->shutdown) {
            printf("thread 0x%x is waiting \n", (unsigned int)pthread_self());
            pthread_cond_wait(&pool->queue_not_empty, &pool->pool_lock);

            // 判断有没有线程需要自杀
            if (pool->wait_exit_num > 0) {
                pool->wait_exit_num--;

                // 判断线程池中的线程数是否大于最小线程数，是则结束当前线程
                if (pool->live_thread_num > pool->min_thread_num) {
                    printf("thread 0x%x is exiting \n", (unsigned int)pthread_self());
                    pool->live_thread_num--;
                    // 解锁并自杀
                    pthread_mutex_unlock(&pool->pool_lock);
                    pthread_exit(NULL);
                }
            }
        }
        // 判断线程池的状态
        if (pool->shutdown) {
            printf("thread 0x%x is exiting \n", (unsigned int)pthread_self());
            // 解锁并自杀
            pthread_mutex_unlock(&pool->pool_lock);
            pthread_exit(NULL);
        }

        // 否则该线程可以拿出任务
        task.func = pool->task_queue[pool->queue_front].func;
        task.arg = pool->task_queue[pool->queue_front].arg;

        pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size; // 环型结构
        pool->queue_size--;

        // 通知队列可以加任务了
        pthread_cond_broadcast(&pool->queue_not_full);

        // 释放互斥锁并开始做任务
        pthread_mutex_unlock(&pool->pool_lock);
        printf("thread 0x%x start working \n", (unsigned int)pthread_self());
        // 锁住忙线程/正在工作的线程 变量
        pthread_mutex_lock(&pool->work_lock);
        pool->busy_thread_num++;
        pthread_mutex_unlock(&pool->work_lock);

        // 做任务
        (*(task.func))(task.arg);

        // 任务结束处理
        printf("thread 0x%x end working \n", (unsigned int)pthread_self());
        pthread_mutex_lock(&pool->work_lock);
        pool->busy_thread_num--;
        pthread_mutex_unlock(&pool->work_lock);
    }

    pthread_exit(NULL);
}

/* 向线程池的任务队列中添加一个任务 */
int threadpool_add_task(threadpool_t *pool, void *(*function)(void *arg), void *arg) {
    // 先锁住整个线程池
    pthread_mutex_lock(&pool->pool_lock);

    // 如果队列满了就阻塞住
    while (pool->queue_size == pool->queue_max_size && !pool->shutdown) {
        pthread_cond_wait(&pool->queue_not_full,&pool->pool_lock);
    }

    // 如果线程池关了
    if (pool->shutdown) {
        // 解锁并退出
        pthread_mutex_unlock(&pool->pool_lock);
        return -1;
    }

    // 清空工作线程的回调函数的参数arg
    if (pool->task_queue[pool->queue_rear].arg != NULL) {
        free(pool->task_queue[pool->queue_rear].arg);
        pool->task_queue[pool->queue_rear].arg = NULL;
    }

    // 添加任务到任务队列
    pool->task_queue[pool->queue_rear].func = function;
    pool->task_queue[pool->queue_rear].arg = arg;
    pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;   // 逻辑环
    pool->queue_size++;

    // 添加完任务后,队列就不为空了,唤醒线程池中的一个线程
    pthread_cond_signal(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->pool_lock);
    return 0;
}

/* 管理线程 */
void *admin_thread(void *threadpool) {
    // 说到底，它就是一个单独的线程，定时的去检查，根据我们的一个维持平衡算法去增删线程；
    threadpool_t *pool = (threadpool_t *)threadpool;
    while (!pool->shutdown) {
        printf("----------------- admin thread is working! -----------------\n");
        sleep(DEFAULT_TIME);
        // 锁住线程池 取出任务数量与存活的线程数
        pthread_mutex_lock(&pool->pool_lock);
        int queue_size = pool->queue_size;
        int live_num = pool->live_thread_num;
        int max_num = pool->max_thread_num;
        pthread_mutex_unlock(&pool->pool_lock);

        // 锁住忙线程 取出正在工作的线程数
        pthread_mutex_lock(&pool->work_lock);
        int work_num = pool->busy_thread_num;
        pthread_mutex_unlock(&pool->work_lock);

        // 创建新线程 实际任务数量大于 最小正在等待的任务数量，存活线程数小于最大线程数
        if (queue_size >= MIN_WAIT_TASK_NUM  && live_num <= pool->max_thread_num) {
            printf("admin add-----------\n");
            pthread_mutex_lock(&pool->pool_lock);

            int add = 0;
            // 一次增加 DEFAULT_THREAD_NUM 个线程
            for (int i = 0; i < max_num && add < DEFAULT_THREAD_NUM && pool->live_thread_num < max_num; i++) {
                if (pool->tid[i] == 0 || !is_thread_alive(pool->tid[i])) {
                    pthread_create(&(pool->tid[i]), NULL, threadpool_thread, (void *)pool);
                    add++;
                    pool->live_thread_num++;
                    printf("create new thread -----------------------\n");
                }
            }
            pthread_mutex_unlock(&pool->pool_lock);
        }

        // 销毁多余的线程 忙线程x2都 小于 存活线程，并且存活的 大于 最小线程数
        if ((work_num * 2) < live_num && live_num > pool->min_thread_num) {
            // 一次销毁DEFAULT_THREAD_NUM个线程
            pthread_mutex_lock(&(pool->pool_lock));
            pool->wait_exit_num = DEFAULT_THREAD_NUM;
            pthread_mutex_unlock(&(pool->pool_lock));

            for (int i = 0; i < DEFAULT_THREAD_NUM; i++) {
                // 通知正在处于空闲的线程，自杀
                pthread_cond_signal(&pool->queue_not_empty);
                printf("----- admin clear -----\n");
            }
        }

    }
    return NULL;
}

/* 线程是否存活 */
int is_thread_alive(pthread_t tid) {
    // 发送0号信号，测试是否存活
    int kill_rc = pthread_kill(tid, 0);
    // 线程不存在
    if (kill_rc == ESRCH) {
        return 0;
    }
    return 1;
}

/* 释放线程池 */
int threadpool_free(threadpool_t *pool) {
    if (pool == NULL) {
        return -1;
    }

    if (pool->task_queue) {
        free(pool->task_queue);
    }

    if (pool->tid) {
        // 先上锁再销毁 为了确保在销毁线程池时的数据一致性和线程安全性。
        pthread_mutex_lock(&pool->pool_lock);
        pthread_mutex_destroy(&pool->pool_lock);
        pthread_mutex_lock(&pool->work_lock);
        pthread_mutex_destroy(&pool->work_lock);
        // 销毁条件变量
        pthread_cond_destroy(&pool->queue_not_empty);
        pthread_cond_destroy(&pool->queue_not_full);
    }
    free(pool);
    pool = NULL;

    return 0;
}

/* 销毁线程池 */
int threadpool_destroy(threadpool_t *pool) {
    if (pool == NULL) {
        return -1;
    }

    pool->shutdown = 1;

    // 销毁管理者线程
    pthread_join(pool->admin_tid,NULL);

    //通知所有线程去自杀(在自己领任务的过程中)
    for (int i = 0; i < pool->live_thread_num; i++) {
        pthread_cond_broadcast(&pool->queue_not_empty);
    }

    // 等待线程结束
    for (int i = 0; i < pool->live_thread_num; i++) {
        pthread_join(pool->tid[i],NULL);
    }

    threadpool_free(pool);
    return 0;
}
