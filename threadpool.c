#include "threadpool.h"

void usage(){
    printf("Usage: server <port> <pool-size> <max-number-of-request>");
}

threadpool *create_threadpool(int num_threads_in_pool)
{
    if (num_threads_in_pool < 0 || num_threads_in_pool > MAXT_IN_POOL)
    {
        usage();
        return NULL;
    }
    threadpool *threadpool = malloc(sizeof(threadpool));
    if (threadpool == NULL)
    {
        perror("malloc");
        return NULL;
    }
    threadpool->num_threads = num_threads_in_pool;
    threadpool->qsize = 0;
    threadpool->qhead = NULL;
    threadpool->qtail = NULL;
    // threadpool->qlock =
    // threadpool->q_not_empty =
    // threadpool->q_empty =
    threadpool->shutdown = 0;
    threadpool->dont_accept = 0;

    return threadpool;
}