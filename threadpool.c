#include "threadpool.h"

threadpool *create_threadpool(int num_threads_in_pool)
{
    if (num_threads_in_pool < 0 || num_threads_in_pool > MAXT_IN_POOL)
        return NULL;
    threadpool *pool = (threadpool *)malloc(sizeof(threadpool));
    if (pool == NULL)
        return NULL;
    pool->num_threads = num_threads_in_pool;
    pool->qsize = 0;
    pool->threads = (pthread_t *)malloc(num_threads_in_pool * sizeof(pthread_t));
    if (pool == NULL)
    {
        destroy_threadpool(pool);
        return NULL;
    }
    pool->qhead = NULL;
    pool->qtail = NULL;
    pthread_mutex_init(&pool->qlock, NULL);
    pthread_cond_init(&pool->q_empty, NULL);
    pthread_cond_init(&pool->q_not_empty, NULL);
    for (int i = 0; i < pool->num_threads; i++)
    {
        if (pthread_create(&pool->threads[i], NULL, do_work, (void *)pool))
        {
            destroy_threadpool(pool);
            return NULL;
        }
    }
    pool->shutdown = pool->dont_accept = 0;
    return pool;
}

void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg)
{
    pthread_mutex_lock(&from_me->qlock);
    if (from_me->dont_accept == 1)
    {
        pthread_mutex_unlock(&(from_me->qlock));
        return;
    }
    work_t *work = (work_t *)malloc(sizeof(work_t));
    if (work == NULL)
    {
        pthread_mutex_unlock(&(from_me->qlock));
        return;
    }
    from_me->qsize++;
    work->arg = arg;
    work->routine = dispatch_to_here;
    work->next = NULL;
    if (from_me->qhead == NULL)
    {
        from_me->qhead = from_me->qtail = work;
        pthread_cond_signal(&from_me->q_not_empty);
        pthread_mutex_unlock(&from_me->qlock);
        return;
    }
    from_me->qtail->next = work;
    from_me->qtail = work;
    pthread_cond_signal(&from_me->q_not_empty);
    pthread_mutex_unlock(&from_me->qlock);
}

work_t *dequeue(threadpool *pool)
{
    if (pool == NULL || pool->qhead == NULL) /* if the pool is not init, or the queue is empty, return NULL */
        return NULL;
    work_t *temp = pool->qhead;      /* store the head */
    pool->qhead = pool->qhead->next; /* move the head down */
    if (pool->qhead == NULL)         /* if the head is NULL, then the tail also need to be NULL */
        pool->qtail = NULL;
    return temp;
}

void *do_work(void *p)
{
    threadpool *pool = (threadpool *)p;
    while (true)
    {
        pthread_mutex_lock(&(pool->qlock));
        if (pool->shutdown == 1)
        {
            pthread_mutex_unlock(&(pool->qlock));
            pthread_exit(NULL);
            break;
        }
        if (pool->qsize == 0 && pool->dont_accept == 0)
            pthread_cond_wait(&(pool->q_not_empty), &(pool->qlock));

        if (pool->shutdown == 1)
        {
            pthread_mutex_unlock(&(pool->qlock));
            pthread_exit(NULL);
            break;
        }

        work_t *worker = dequeue(pool);
        if (worker != NULL)
        {
            (worker->routine)(worker->arg);
            pool->qsize--;
            free(worker);
        }

        if (worker != NULL && pool->qsize == 0 && pool->dont_accept == 1)
        {
            pthread_mutex_unlock(&(pool->qlock));
            pthread_cond_signal(&(pool->q_empty));
            break;
        }
        pthread_mutex_unlock(&(pool->qlock));
    }
    return NULL;
}

void destroy_threadpool(threadpool *destroyme) // Debug and test with valgring
{
    if (destroyme == NULL)
        return;
    pthread_mutex_lock(&(destroyme->qlock));
    destroyme->dont_accept = 1; /* rise up the dont accept flag */
    if (destroyme->qsize > 0)   /* wait for work queue to get empty */
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    destroyme->shutdown = 1;
    pthread_mutex_unlock(&(destroyme->qlock));
    pthread_cond_broadcast(&(destroyme->q_not_empty)); /* wake up all worker threads */
    for (int i = 0; i < destroyme->num_threads; i++)   /* Join all worker thread */
        pthread_join(destroyme->threads[i], NULL);
    if (destroyme->threads)
        free(destroyme->threads);
    pthread_cond_destroy(&(destroyme->q_empty));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    pthread_mutex_destroy(&(destroyme->qlock));
    free(destroyme);
}
