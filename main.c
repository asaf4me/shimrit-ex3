#include "threadpool.h"
#define THREAD_SIZE 5

int dispatch_function(void *arg)
{
    printf("in dispatch function, came from thread: %d\n",arg);
    return 0;
}

int main(int argc, char const *argv[])
{
    threadpool *threadpool = create_threadpool(THREAD_SIZE);
    if (threadpool == NULL)
    {
        //Destroy;
        return EXIT_FAILURE;
    }
    for (int i = 0; i < THREAD_SIZE; i++)
        dispatch(threadpool, dispatch_function, (void *)(long)i);
    do_work(threadpool);
    destroy_threadpool(threadpool);
    return EXIT_SUCCESS;
}
