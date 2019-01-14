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
#define TIME_BUFF 128
#define PATH_MAX 4096
#define ENTITY_LINE 500
#define SUCCESS 0
#define FILE 1
#define DIRECTORY 2
#define ALLOC_ERROR -2
#define BUFF 4000
#define LOCATION_BUFF 20
#define SERVER_PROTOCOL "webserver/1.1"
#define SERVER_HTTP "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

#define HTML_PAGE                    \
    "<HTML>"                         \
    "<HEAD><TITLE>%s</TITLE></HEAD>" \
    "<BODY><H4>%s</H4>"              \
    "%s."                            \
    "</BODY>"                        \
    "</HTML>"

#define HTTP_HEADER                       \
    "%s %s\r\n"                           \
    "Server: %s\r\n"                      \
    "Date: %s\r\n"                        \
    "%s /%s\\\r\n"                        \
    "Content-Type: %s; charset=utf-8\r\n" \
    "Content-Length: %ld\r\n"             \
    "%s %s\r\n"                           \
    "Connection: close\r\n\r\n"           \
    "%s"

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

/* Usage message */
void usage_message()
{
    printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
}

/* Handle all the other HTTP response */
void server_response(int socket, const char *title, const char *body, char *path)
{
    char http[BUFF], timebuf[TIME_BUFF], response[BUFF + PATH_MAX];
    memset(timebuf, 0, TIME_BUFF);
    memset(response, 0, BUFF + PATH_MAX);
    memset(http, 0, BUFF);
    snprintf(http, sizeof(http), HTML_PAGE, title, title, body);
    time_t now;
    int length = 0;
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    if (strlen(path) > 0)
        length = snprintf(response, sizeof(response), HTTP_HEADER, SERVER_HTTP, title, SERVER_PROTOCOL, timebuf, "Location: ", path, "text/html", strlen(http), "", "", http);
    else
        length = snprintf(response, sizeof(response), HTTP_HEADER, SERVER_HTTP, title, SERVER_PROTOCOL, timebuf, "", "", "text/html", strlen(http), "", "", http);
    write_to_socket(socket, response, length);
}

/* Convert string to int */
int get_int(char *argv)
{
    if (strcmp(argv, "0") == 0)
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
        close(filefd);
        fprintf(stderr, "unknown mime: %s.\n", file);
        return ERROR;
    }
    char response[BUFF], timebuf[TIME_BUFF];
    memset(response, 0, BUFF);
    memset(timebuf, 0, TIME_BUFF);
    time_t now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    int textLength = snprintf(response, sizeof(response), HTTP_HEADER, SERVER_HTTP, "200 OK", SERVER_PROTOCOL, timebuf, "", "", mime, length, "", "", "");
    if (write_to_socket(newfd, response, textLength) == ERROR)
        server_response(newfd, "500 Internal Server Error", "Some server side error", "");
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

/* Get the file list of directory */
char *get_dir_content(const char *path, struct stat st)
{
    int size = BUFF + strlen(path), len = 0;
    char *html = malloc(size + 1);
    char entity[ENTITY_LINE];
    if (html == NULL)
        return NULL;
    memset(entity, 0, ENTITY_LINE);
    memset(html, 0, size);
    html[size] = '\0';
    snprintf(html, BUFF, "<HTML>"
                         "<HEAD><TITLE>Index of %s</TITLE></HEAD>"
                         "<BODY>"
                         "<H4>Index of %s</H4>"
                         "<table CELLSPACING=8>"
                         "<tr>"
                         "<th>Name</th><th>Last Modified</th><th>Size</th>",
             path, path);
    DIR *directory = NULL;
    if (strcmp(path, "/") == 0)
    {
        if ((directory = opendir("./")) == NULL)
        {
            free(html);
            perror("opendir");
            return NULL;
        }
    }
    else
    {
        if ((directory = opendir(path)) == NULL)
        {
            free(html);
            perror("opendir");
            return NULL;
        }
    }
    struct dirent *entry = NULL;
    struct stat sd;
    while ((entry = readdir(directory)) != NULL)
    {
        if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
            continue;
        if (stat(path, &sd) == ERROR)
        {
            perror("stat");
            free(html);
            closedir(directory);
            return NULL;
        }
        if (S_ISDIR(sd.st_mode)) /* Identify for a directory */
            len = snprintf(entity, ENTITY_LINE, "<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>%s</td></tr>", entry->d_name, entry->d_name, ctime(&sd.st_mtime), "");
        else if (S_ISREG(sd.st_mode)) /* Identify for a regular file */
            len = snprintf(entity, ENTITY_LINE, "<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>%ld</td></tr>", entry->d_name, entry->d_name, ctime(&sd.st_mtime), sd.st_size);
        strncat(html, entity, ENTITY_LINE);
        if (len > ENTITY_LINE)
            fprintf(stderr, "assumption of 500 entity line");
    }
    strncat(html, "</table><HR><ADDRESS>webserver/1.1</ADDRESS></BODY></HTML>", ENTITY_LINE);
    closedir(directory);
    return html;
}

/* Getting all the files within a directory */
int dir_content(const char *path, int newfd)
{
    struct stat st;
    if (stat(path, &st) == ERROR)
        return ERROR;
    char response[BUFF], timebuf[TIME_BUFF];
    memset(response, 0, BUFF);
    memset(timebuf, 0, TIME_BUFF);
    char *contents = NULL;
    time_t now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    contents = get_dir_content(path, st);
    if (contents == NULL)
        return ERROR;
    int length = snprintf(response, sizeof(response), HTTP_HEADER, SERVER_HTTP, "200 OK", SERVER_PROTOCOL, timebuf, "", "", "text/html", strlen(contents), "Last-Modified: ", ctime(&st.st_mtime), contents);
    if (write_to_socket(newfd, response, length) == ERROR) /* Send the header + html */
    {
        free(contents);
        return ERROR;
    }
    free(contents);
    return !ERROR;
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
        server_response(newfd, "404 Not Found", "File not found", "");
    }
    else if (is_directory(path) == true) /* If path is a directory */
    {
        if (path[strlen(path) - 1] != '/')
        {
            server_response(newfd, "302 Found", "Directories must end with a slash", path);
        }
        else if ((index = get_index(path, "index.html")) != NULL) /* Return index.html within the folder */
        {
            if (send_file_via_socket(newfd, index) == ERROR)
                server_response(newfd, "500 Internal Server Error", "Some server side error", "");
        }
        else if (dir_content(path, newfd) == ERROR) /* Return the content dir */
            server_response(newfd, "500 Internal Server Error", "Some server side error", "");
    }
    else if (is_file(path) == true) /* If path is a file */
    {
        if (has_permission(path) == false)
        {
            server_response(newfd, "403 Forbidden", "Access denied", "");
        }
        else if (send_file_via_socket(newfd, path) == ERROR)
            server_response(newfd, "500 Internal Server Error", "Some server side error", "");
    }
    else if (is_file(path) == false || has_permission(path) == false) /* If path is not a regular file or file has no read permission */
    {
        server_response(newfd, "403 Forbidden", "Access denied", "");
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

/* Free the new fd and close the socket */
void clean(int newfd, void *arg)
{
    if (arg != NULL)
        free(arg);
    close(newfd);
}

/* New sockets will processed by thread in this function */
int process_request(void *arg)
{
    int newfd = *(int *)arg, bytes;
    char buffer[BUFF];
    memset(buffer, 0, BUFF);
    char *method = NULL, *path = NULL, *version = NULL;
    while (true)
    {
        if ((bytes = read(newfd, buffer, sizeof(buffer))) == -1) /* Read from socket */
        {
            perror("read");
            server_response(newfd, "500 Internal Server Error", "Some server side error", "");
            clean(newfd, arg);
            return ERROR;
        }
        if (strchr(buffer, '\n') != NULL) /* Identify the end of the first line */
            break;
    }
    int parse = parsing(buffer, &method, &path, &version);
    if (parse == ALLOC_ERROR)
    {
        server_response(newfd, "500 Internal Server Error", "Some server side error", "");
        clean(newfd, arg);
        return ERROR;
    }
    if (parse == ERROR) /* Bad requast -> HTTP_400 */
    {
        server_response(newfd, "400 Bad Request", "Bad Request", "");
        clean(newfd, arg);
        return ERROR;
    }
    if (strcmp(method, "POST") == 0) /* Only support the get method */
    {
        server_response(newfd, "501 Not supported", "Method is not supported", "");
        clean(newfd, arg);
        return ERROR;
    }
    path_proccesor(path, newfd);
    clean(newfd, arg);
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
    signal(SIGPIPE, SIG_IGN);                        /* Prevent SIG_PIPE */
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
    printf("Server is listening on %d:%d\n", server.sin_addr.s_addr, port);
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