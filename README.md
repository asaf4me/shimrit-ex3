# shimrit-ex3 http server <br />

Author: @Asaf Joseph <br />

TCP-HTTP server side: <br />

Description:
At this exercise we have implemented an HTTP server over TCP. <br />
The server will look for files from the server root (where the binary runs) and deeper to the directories. <br />
Short flow: <br />
At evry new entry the first thing the server will look-up for is the index.html, in case of Non-index direcorie it will show the content inside it. <br />
In case of not found, an 404 not-found message will be sent. <br />
We support only GET method, in case of POST, 401 will be returned. <br />
403 as usual will handle forbiden files. <br />
500 will return in case that your requast failed due to a server internal ERROR. <br />
All the files can be sent over the TCP socket, the end of files the showed up at the mime func will also include a mime title at the html header. <br />

Usage: <br />
At the ex3.tar file you will find 3 files: <br />
- threadpool.c <br />
- server.c <br />
- README <br />

the file compiled with:
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;gcc -c server.c -o server.o -Wall -Wvla -g -lpthread  <br />
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;gcc -c threadpool.c -o threadpool.o -Wall -Wvla -g -lpthread  <br />
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;gcc threadpool.o server.o -o server -Wall -Wvla -g -lpthread  <br />

At any usage fail: the out will be: <br />
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;printf("Usage: server <port> <pool-size> <max-number-of-request>\n")

To run the server do the following: <br />
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;./server PORT NUMBER_OF_THREADS MAX_REQUAST <br />

NOTICE: <br />
At the main function, lines: 699, 714, 715 ar comment out, this lines will allow you to test the server on LAN IP, if you want to do so please: <br />
comment out the following line, add you LAN IP adrees instead of mine, and run the binary again. <br />

The project can be fount at my git: <br />
https://github.com/asaf4me/shimrit-ex3 <br />

As usual, I hope you will find this fun and usefull, for any more contact im here: asaf4me@gmail.com. <br />

FREE FOR USE
