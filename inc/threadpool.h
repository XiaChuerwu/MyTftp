/*
 * @creater: XiaChuerwu 1206575349@qq.com
 * @since: 2023-09-09 15:20:42
 * @lastTime: 2023-09-10 10:51:25
 * @LastAuthor: XiaChuerwu 1206575349@qq.com
 */
#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <pthread.h>

#define DEFAULT_TIME 20
#define MIN_WAIT_TASK_NUM 5
#define DEFAULT_THREAD_NUM 3

// 任务结构体存在以下两个元素：
typedef struct {
    //​	第一个是函数指针，也就是要做的任务
    void *(*func)(void*);
    // 该任务的参数
    void *arg;
}threadpool_task_t;


// 线程池管理结构体：
typedef struct {
    
    pthread_mutex_t pool_lock;      // 线程互斥锁 目的是锁住整个线程池
    pthread_mutex_t work_lock;      // 互斥锁 用于锁住忙线程时的锁
    pthread_cond_t queue_not_full;  // 条件变量 任务队列不为满
    pthread_cond_t queue_not_empty; // 条件变量 任务队列不为空
    
    pthread_t *tid;                 // 线程id数组
    pthread_t admin_tid;            // 管理者线程的id
    threadpool_task_t *task_queue;  // 任务队列 这里的任务队列也是线性的

	/* 线程池的信息 */
    int min_thread_num;             // 线程池中最小的线程数量
    int max_thread_num;             // 线程池中最大的线程数量
    int live_thread_num;            // 线程中存活的线程数量
    int busy_thread_num;            // 正在工作的线程，也就是忙线程
    int wait_exit_num;              // 等待/需要销毁的线程

    /* 任务队列的信息 */
    int queue_front;                // 队头
    int queue_rear;                 // 队尾
    int queue_size;                 // 队的大小

    /* 存在的任务数量 */
    int queue_max_size;             // 任务队列能容纳的最大任务数

    /* 线程池的状态 */
    int shutdown;                   // 关闭还是工作 1 关机 0 工作

}threadpool_t;

/* 创建线程池 */
threadpool_t *creat_threadpool(int min_thr_num, int max_thr_num, int queue_max_size);

/* 工作线程 */
void *threadpool_thread(void *threadpool);

/*向线程池的任务队列中添加一个任务*/
int threadpool_add_task(threadpool_t *pool, void *(*function)(void *arg), void *arg);

/*管理线程*/
void *admin_thread(void *threadpool);

/*释放线程池*/
int threadpool_free(threadpool_t *pool);

/*销毁线程池*/
int threadpool_destroy(threadpool_t *pool);

int is_thread_alive(pthread_t tid);
#endif // __THREADPOOL_H__