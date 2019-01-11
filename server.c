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
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "threadpool.h"

/* DEFINES */

// typedef enum {false,true} bool;

#define ERROR -1
#define SUCCESS 0
#define FILE 1
#define DIRECTORY 2
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
}

void server_response(int socket,const char *msg, const char* http)
{
    time_t now;
    char timebuf[128];
    memset(timebuf, 0, 128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    char response[BUFF];
    memset(response, 0, BUFF);
    snprintf(response, sizeof(response),
             "%s %s\r\n"
             "Server: %s\r\n"
             "Date: %s\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             SERVER_HTTP,msg, SERVER_PROTOCOL,
             timebuf, strlen(http), http);
    if (write_to_socket(socket, response) == ERROR)
        internal_error(socket);
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

char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');
    if (!ext)
        return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".au") == 0)
        return "audio/basic";
    if (strcmp(ext, ".wav") == 0)
        return "audio/wav";
    if (strcmp(ext, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0)
        return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0)
        return "audio/mpeg";
    return NULL;
}

/* Checking for Directory */
bool is_directory(const char *path)
{
    struct stat stats;
    stat(path, &stats);
    if (S_ISDIR(stats.st_mode))
        return true;
    return false;
}

/* Checking for file */
bool is_file(const char *path)
{
    struct stat stats;
    stat(path, &stats);
    if (S_ISREG(stats.st_mode))
        return true;
    return false;
}

/* Checking for path existent */
bool is_exist(const char *path)
{
    struct stat st;
    if (stat(path, &st) == -1)
        return false;
    return true;
}

int get_dir_content(const char *path, int newfd)
{
    DIR *directory = opendir(path);
    if(directory == NULL)
    {
        perror("opendir");
        internal_error(newfd);
        return ERROR;
    }
    struct dirent *dp;
    while((dp = readdir(directory)) != NULL)
    {
        
    }
    return SUCCESS;
}

/* Handle all the path proccess logic */
int path_proccesor(char *path, int newfd)
{
    if (is_exist(++path) == false && strcmp(path,"/") != 0) /* Return error -> 404 not found */
    {
        server_response(newfd,"404 Not Found",HTTP_404);
        return SUCCESS;
    }
    if (is_directory(path) == true) /* If path is a directory */
    {
        if (path[strlen(path) - 1] != '/')
        {
            server_response(newfd,"302 Found",HTTP_302);
            return SUCCESS;
        }
        if (get_dir_content(path, newfd) != SUCCESS)
        {
            /* Return contents of directory */
            internal_error(newfd);
            return ERROR;
        }  
    }
    if (is_file(path) == true) /* If path is a file */
    {
    }

    return !ERROR;
}

/* Parsing the requast */
int parsing(char req[], char *method[], char *path[], char *version[])
{
    char *temp;
    char **parsed = malloc(BUFF * sizeof(char *));
    if (parsed == NULL)
        return ALLOC_ERROR;
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
    if (parse == ERROR) /* Bad requast -> HTTP_400 */
    {
        server_response(newfd,"400 Bad Request",HTTP_400);
        goto CLOSE;
    }
    if (strcmp(method, "POST") == 0) /* Only support the get method */
    {
        server_response(newfd,"501 Not supported",HTTP_501);
        goto CLOSE;
    }
    path_proccesor(path, newfd);
    goto CLOSE;
CLOSE:
    free(arg);
    close(newfd);
    return !ERROR;
}

/* Main */
int main(int argc, char *argv[])
{
    if (argc != 4) /* Verify for right input */
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
