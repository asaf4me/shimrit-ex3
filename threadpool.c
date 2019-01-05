#include "threadpool.h"

void usage()
{
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
    threadpool->threads = (pthread_t *)malloc(threadpool->num_threads * sizeof(pthread_t));
    if (threadpool == NULL)
    {
        perror("malloc");
        return NULL;
    }
    for (int i = 0; i < threadpool->num_threads; i++)
    {
        if (pthread_create(&threadpool->threads[i], NULL, do_work, (void *)dispatch))
        {
            printf("ERROR\n");
            exit(-1);
        }
    }
    threadpool->qhead = NULL;
    threadpool->qtail = NULL;
    // threadpool->qlock =
    // threadpool->q_not_empty =
    // threadpool->q_empty =
    threadpool->shutdown = 0;
    threadpool->dont_accept = 0;

    return threadpool;
}

void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg)
{
}

void *do_work(void *p)
{
}

void destroy_threadpool(threadpool *destroyme)
{
}