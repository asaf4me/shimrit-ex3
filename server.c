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
#include <signal.h>
#include "threadpool.h"

/* DEFINES */

typedef enum
{
    false,
    true
} bool;

#define ERROR -1
#define PATH_MAX 4096
#define ENTITY_PAGE 500
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

#define DIRERCTOY_CONTENTS                             \
    "<HTML>"                                           \
    "<HEAD><TITLE>Index of %s</TITLE></HEAD>"          \
    "<BODY>"                                           \
    "<H4>Index of %s</H4>"                             \
    "<table CELLSPACING=8>"                            \
    "<tr>"                                             \
    "<th>Name</th><th>Last Modified</th><th>Size</th>" \
    "</tr>"                                            \
    "%s"                                               \
    "</table><HR>"                                     \
    "<ADDRESS>%s</ADDRESS>"                            \
    "</BODY>"                                          \
    "</HTML>"

#define DIR_CONTENTS_TEMPLATE \
    "<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>%s</td></tr>"

/* END DEFINES */

/* Wrinting to the socket */
int write_to_socket(int sock, char *msg, size_t length)
{
    int bytes = 0, sum = 0;
    const char *pointer = msg;
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

/* Sending ERROR 500 */
void internal_error(int socket)
{
    time_t now;
    char timebuf[128];
    memset(timebuf, 0, 128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    char response[BUFF];
    memset(response, 0, BUFF);
    int length = snprintf(response, sizeof(response),
             "%s 500 Internal Server Error\r\n"
             "Server: %s\r\n"
             "Date: %s\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             SERVER_HTTP, SERVER_PROTOCOL,
             timebuf, strlen(HTTP_500), HTTP_500);
    write_to_socket(socket, response, length);
}

/* Handle all the other HTTP response */
void server_response(int socket, const char *msg, const char *http)
{
    time_t now;
    char timebuf[128];
    memset(timebuf, 0, 128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    char response[BUFF];
    memset(response, 0, BUFF);
    int length = snprintf(response, sizeof(response),
             "%s %s\r\n"
             "Server: %s\r\n"
             "Date: %s\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             SERVER_HTTP, msg, SERVER_PROTOCOL,
             timebuf, strlen(http), http);
    if (write_to_socket(socket, response, length) == ERROR)
        internal_error(socket);
}

/* Convert string to int */
int get_int(char *argv)
{
    if (strcmp(argv, "0") == 0) /* check for identical 0 */
        return 0;
    int num = atoi(argv);
    if (num == 0)
    {
        usage_message();
        return ERROR;
    }
    return num;
}

/* Get the mime type of a file */
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

/* Get the size of the file using lseek */
off_t get_size(int file)
{
    off_t currentPos = lseek(file, (size_t)0, SEEK_CUR);
    off_t length = lseek(file, (size_t)0, SEEK_END);
    if (currentPos == (off_t)-1 || length == (off_t)-1)
    {
        close(file);
        return (off_t)ERROR;
    }
    if (lseek(file, currentPos, SEEK_SET) == -1)
    {
        close(file);
        return (off_t)ERROR;
    }
    return length;
}

/* Transfer file via socket */
int send_file_via_socket(int newfd, char *file)
{
    int filefd, bytes;
    if (file[0] == '/')
        ++file;
    if ((filefd = open(file, O_RDONLY)) == ERROR)
    {
        perror("open");
        return ERROR;
    }
    off_t length = get_size(filefd);
    if (length == ERROR)
    {
        close(filefd);
        return ERROR;
    }
    char *mime = get_mime_type(file);
    if (mime == NULL)
    {
        fprintf(stderr, "unknown mime: %s.\n", file);
        return ERROR;
    }
    char response[BUFF];
    memset(response, 0, BUFF);
    time_t now;
    char timebuf[128];
    memset(timebuf, 0, 128);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    int textLength = snprintf(response, sizeof(response),
             "%s 200 \r\n"
             "Server: %s\r\n"
             "Date: %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n",
             SERVER_HTTP, SERVER_PROTOCOL,
             timebuf, mime, length);
    if (write_to_socket(newfd, response, textLength) == ERROR)
        internal_error(newfd);
    char *indexHtml = malloc(length * sizeof(char) + 1);
    if (indexHtml == NULL)
    {
        close(filefd);
        return ERROR;
    }
    indexHtml[length] = '\0';
    while ((bytes = read(filefd, indexHtml, length)) > 0)
    {
        if (write_to_socket(newfd, indexHtml, length) == ERROR)
        {
            perror("write");
            free(indexHtml);
            close(filefd);
            return ERROR;
        }
    }
    free(indexHtml);
    close(filefd);
    return SUCCESS;
}

bool has_permission(char *file)
{
    if (file[0] == '/')
        ++file;
    int fileFd;
    if ((fileFd = open(file, O_RDONLY)) >= 0)
    {
        close(fileFd);
        return true;
    }
    close(fileFd);
    return false;
}

/* Getting all the files within a directory */
char *get_dir_content(char *path, char *file)
{
    
    return NULL;
}

/* Searching for the index.html within a directory */
char *get_index(char *path, char *file)
{
    DIR *directory;
    if (strcmp(path, "/") == 0)
    {
        if ((directory = opendir("./")) == NULL)
        {
            perror("opendir");
            return NULL;
        }
    }
    else
    {
        if ((directory = opendir(path)) == NULL)
        {
            perror("opendir");
            return NULL;
        }
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
    {
        if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
            continue;
        if (strcmp(entry->d_name, file) == 0)
        {
            closedir(directory);
            return strncat(path, file, PATH_MAX + strlen(file));
        }
    }
    closedir(directory);
    return NULL;
}

/* Handle all the path proccess logic */
int path_proccesor(char *path, int newfd)
{
    char *index = NULL;
    if (is_exist(++path) == false && strcmp(--path, "/") != 0) /* Return error -> 404 not found */
    {
        server_response(newfd, "404 Not Found", HTTP_404);
        return SUCCESS;
    }
    if (is_directory(path) == true) /* If path is a directory */
    {
        if (path[strlen(path) - 1] != '/')
        {
            server_response(newfd, "302 Found", HTTP_302);
            return SUCCESS;
        }
        if ((index = get_index(path, "index.html")) != NULL) /* Return index.html within the folder */
        {
            if (send_file_via_socket(newfd, index) == ERROR)
                internal_error(newfd);
            return SUCCESS;
        }
        /* Return the content dir */

    }
    if (is_file(path) == true) /* If path is a file */
    {
        if (has_permission(path) == false)
        {
            server_response(newfd, "403 Forbidden", HTTP_403);
            return SUCCESS;
        }
        if (send_file_via_socket(newfd, path) == ERROR)
            internal_error(newfd);
        return SUCCESS;
    }
    if (is_file(path) == false || has_permission(path) == false) /* If path is not a regular file or file has no read permission */
    {
        server_response(newfd, "403 Forbidden", HTTP_403);
        return SUCCESS;
    }
    return SUCCESS;
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
    if (strlen(*path) == 0)
        *path = "/";
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
        server_response(newfd, "400 Bad Request", HTTP_400);
        goto CLOSE;
    }
    if (strcmp(method, "POST") == 0) /* Only support the get method */
    {
        server_response(newfd, "501 Not supported", HTTP_501);
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
    signal(SIGPIPE, SIG_IGN); /* Prevent SIG_PIPE */
    struct sockaddr_in server, client;               /* Used by bind() , Used by accept() */
    int fd, port, poolSize, maxClients, counter = 0; /* Socket descriptor , New socket descriptor , Port handle ,  Pool-size handle , Max-clients handle */
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

    /* Destructors */
    destroy_threadpool(threadpool);
    shutdown(fd, SHUT_RDWR);
    close(fd);
    return EXIT_SUCCESS;
}
