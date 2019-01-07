#include "threadpool.h"
#define THREAD_SIZE 10

int dispatch_function(void *arg)
{
    printf("in dispatch function, came from thread: %d\n", arg);
    return 0;
}

int main(int argc, char const *argv[])
{
    threadpool *threadpool = NULL;
    threadpool = create_threadpool(THREAD_SIZE);
    if (threadpool == NULL)
        return EXIT_FAILURE;
    for (int i = 0; i < THREAD_SIZE*1000; i++)
        dispatch(threadpool, dispatch_function, (void *)(long)i);
    destroy_threadpool(threadpool);
    return EXIT_SUCCESS;
}
