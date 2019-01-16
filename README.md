# shimrit-ex3 http server

This is the implement of tcp http server using threadpool to serve the clients.
Compiled with: 
gcc -c server.c -o server.o -Wall -Wvla -g -lpthread 
gcc -c threadpool.c -o threadpool.o -Wall -Wvla -g -lpthread
gcc threadpool.o server.o -o server -Wall -Wvla -g -lpthread

Usage: 
./server PORT NUMBER_OF_THREADS MAX_REQUAST

FREE FOR USE
