# shimrit-ex3 http server <br />

This is the implement of tcp http server using threadpool to serve the clients. <br />
Compiled with: <br />
gcc -c server.c -o server.o -Wall -Wvla -g -lpthread <br />
gcc -c threadpool.c -o threadpool.o -Wall -Wvla -g -lpthread <br />
gcc threadpool.o server.o -o server -Wall -Wvla -g -lpthread <br />

Usage: <br />
./server PORT NUMBER_OF_THREADS MAX_REQUAST <br />

FREE FOR USE
