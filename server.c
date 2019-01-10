#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "threadpool.h"

/* DEFINES */

#define ERROR -1
#define ALLOC_ERROR -2
#define BUFF 4000
#define SERVER_PROTOCOL "webserver/1.1"
#define SERVER_HTTP "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

#define HTTP_400                                  \
    "<HTML>"                                      \
    "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>" \
    "<BODY>"                                      \
    "<H4>400 Bad request</H4>"                    \
    "Bad Request."                                \
    "</BODY>"                                     \
    "</HTML>"

#define HTTP_501                                    \
    "<HTML>"                                        \
    "<HEAD><TITLE>501 Not supported</TITLE></HEAD>" \
    "<BODY><H4>501 Not supported</H4>"              \
    "Method is not supported"                       \
    "</BODY>"                                       \
    "</HTML>"

#define HTTP_404                                \
    "<HTML>"                                    \
    "<HEAD><TITLE>404 Not Found</TITLE></HEAD>" \
    "<BODY><H4>404 Not Found</H4>"              \
    "File not found."                           \
    "</BODY>"                                   \
    "</HTML>"

#define HTTP_302                            \
    "<HTML>"                                \
    "<HEAD><TITLE>302 Found</TITLE></HEAD>" \
    "<BODY><H4>302 Found</H4>"              \
    "Directories must end with a slash."    \
    "</BODY>"                               \
    "</HTML>"

#define HTTP_500                                            \
    "<HTML>"                                                \
    "<HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>" \
    "<BODY><H4>500 Internal Server Error</H4>"              \
    "Some server side error."                               \
    "</BODY>"                                               \
    "</HTML>"

#define HTTP_403                                \
    "<HTML>"                                    \
    "<HEAD><TITLE>403 Forbidden</TITLE></HEAD>" \
    "<BODY><H4>403 Forbidden</H4>"              \
    "Access denied."                            \
    "</BODY>"                                   \
    "</HTML>"

/* END DEFINES */

/* Wrinting to the socket */
int write_to_socket(int sock, char *msg)
{
    int bytes = 0, sum = 0;
    const char *pointer = msg;
    size_t length = strlen(msg);
    while (true)
    {
        bytes = write(sock, pointer, length);
        sum += bytes;
        if (sum == strlen(msg))
            break;
        if (bytes < 0)
        {
            perror("write");
            return ERROR;
        }
    }
    return sum;
}

void usage_message()
{
    printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
}

int get_int(char *argv)
{
    if (strcmp(argv, "0") == 0) /* check for identical 0 */
        return 0;
    int port = atoi(argv);
    if (port == 0)
    {
        usage_message();
        return ERROR;
    }
    return port;
}

void internal_error(int socket)
{
    time_t now;
    char timebuf[128];
    memset(timebuf, 0, 128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    char response[BUFF];
    memset(response, 0, BUFF);
    snprintf(response, sizeof(response),
             "%s 500 Internal Server Error\r\n"
             "Server: %s\r\n"
             "Date: %s\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             SERVER_HTTP, SERVER_PROTOCOL,
             timebuf, strlen(HTTP_500), HTTP_500);
    if (write_to_socket(socket, response) == ERROR)
        internal_error(socket);
}

void bad_req(int socket)
{
    time_t now;
    char timebuf[128];
    memset(timebuf, 0, 128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    char response[BUFF];
    memset(response, 0, BUFF);
    snprintf(response, sizeof(response),
             "%s 400 Bad Request\r\n"
             "Server: %s\r\n"
             "Date: %s\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             SERVER_HTTP, SERVER_PROTOCOL,
             timebuf, strlen(HTTP_400), HTTP_400);
    if (write_to_socket(socket, response) == ERROR)
        internal_error(socket);
}

void not_supported(int socket)
{
    time_t now;
    char timebuf[128];
    memset(timebuf, 0, 128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    char response[BUFF];
    memset(response, 0, BUFF);
    snprintf(response, sizeof(response),
             "%s 501 Not supported\r\n"
             "Server: %s\r\n"
             "Date: %s\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             SERVER_HTTP, SERVER_PROTOCOL,
             timebuf, strlen(HTTP_501), HTTP_501);
    if (write_to_socket(socket, response) == ERROR)
        internal_error(socket);
}

/* Parsing the requast */
int parsing(char req[], char *method[], char *path[], char *version[])
{
    char *temp;
    char **parsed = malloc(BUFF * sizeof(char *));
    if (parsed == NULL)
        return ERROR;
    int position = 0;
    char *end = strstr(req, "\r\n");
    end[0] = '\0';
    temp = strtok(req, " ");
    while (temp != NULL && temp < end)
    {
        parsed[position] = temp;
        position++;
        temp = strtok(NULL, " ");
    }
    parsed[position] = NULL;
    *method = parsed[0], *path = parsed[1], *version = parsed[2];
    if (*method == NULL || *path == NULL || *version == NULL)
    {
        free(parsed);
        return ERROR;
    }
    free(parsed);
    return !ERROR;
}

/* New sockets will processed by thread in this function */
int process_request(void *arg)
{
    int newfd = *(int *)arg, bytes;
    char buffer[BUFF];
    char *method = NULL, *path = NULL, *version = NULL;
    memset(buffer, 0, BUFF);
    if ((bytes = read(newfd, buffer, sizeof(buffer))) == -1)
    {
        perror("read");
        internal_error(newfd);
        goto CLOSE;
    }
    int parse = parsing(buffer, &method, &path, &version);
    if (parse == ALLOC_ERROR)
    {
        internal_error(newfd);
        goto CLOSE;
    }
    if (parse == ERROR)
    {
        bad_req(newfd);
        goto CLOSE;
    }
    if (strcmp(method, "POST") == 0)
    {
        not_supported(newfd);
        goto CLOSE;
    }

CLOSE:
    free(arg);
    close(newfd);
    return !ERROR;
}

/* Main */
int main(int argc, char *argv[])
{
    if (argc != 4) /*Verify for right input */
    {
        usage_message();
        return EXIT_FAILURE;
    }

    struct sockaddr_in server, client;               /* used by bind() , used by accept() */
    int fd, port, poolSize, maxClients, counter = 0; /* socket descriptor , new socket descriptor , port handle ,  pool-size handle , max-clients handle */
    unsigned int cli_len = sizeof(client);
    server.sin_family = AF_INET;

    for (int i = 1; i < argc; i++)
    {
        int temp = get_int(argv[i]);
        if (temp == ERROR)
            return EXIT_FAILURE;
        if (i == 1)
            port = temp;
        else if (i == 2)
            poolSize = temp;
        else if (i == 3)
            maxClients = temp;
    }

    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("bind");
        return EXIT_FAILURE;
    }

    threadpool *threadpool = create_threadpool(poolSize);
    assert(threadpool != NULL);
    if (listen(fd, maxClients) < 0)
    {
        perror("listen");
        destroy_threadpool(threadpool);
        return EXIT_FAILURE;
    }
    printf("Server is listening on 127.0.0.1:%d\n", port);
    while (true)
    {
        if (counter >= maxClients)
        {
            //Handle ERROR <cannot accept connetion>
            break;
        }
        int *newfd = malloc(sizeof(int));
        assert(newfd != NULL);
        if ((*newfd = accept(fd, (struct sockaddr *)&client, &cli_len)) < 0)
        {
            perror("accept");
            break;
        }
        dispatch(threadpool, process_request, newfd);
        counter++;
    }

    /* destructors */
    destroy_threadpool(threadpool);
    shutdown(fd, SHUT_RDWR);
    close(fd);
    return EXIT_SUCCESS;
}
