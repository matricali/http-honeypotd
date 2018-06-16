/*
MIT License

Copyright (c) 2017 Jorge Matricali

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <curl/curl.h>

#define BUFSIZE 8096
#ifndef SERVER_HEADER
#define SERVER_HEADER "jorgee"
#endif

struct mime_type {
    char *extension;
    char *mimetype;
};

struct mime_type mime_types [] = {
    {"gif", "image/gif"},
    {"jpg", "image/jpg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"ico", "image/ico"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {"js", "application/x-javascript"},
    {"css", "text/css"},
    {0, 0}
};

struct http_request {
    char method[50];
    char path[1024];
    char protocol[100];
};

enum log_level {
    LOG_NONE,
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG,
    LOG_NEVER
};

FILE *logfile = NULL;

void usage(char *ex)
{
    printf("usage: %s [-p PORT]\n", ex);
}

void logger(enum log_level level, const char *format, ...) {
    if (level == LOG_NEVER || level == LOG_NONE) {
        return;
    }

    va_list arg;
    char message[1024];
    char datetime[26];
    int ret;

    time_t timer;
    struct tm* tm_info;

    if (logfile == NULL) {
        logfile = fopen("http-honeypotd.log", "a");
        if (logfile < 0) {
            logger(LOG_ERROR, "Error opening log file.\n");
        }
    }

    time(&timer);
    tm_info = localtime(&timer);
    strftime(datetime, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    memset(message, 0, 1024);

    va_start(arg, format);
    ret = vsprintf(message, format, arg);
    va_end(arg);

    if (logfile > 0) {
        fprintf(logfile, "[%s][%d] %s", datetime, getpid(), message);
    }

    if (level < LOG_WARNING) {
        fprintf(stderr, "[%s][%d] %s", datetime, getpid(), message);
    } else if (level <= LOG_DEBUG) {
        printf("[%s][%d] %s", datetime, getpid(), message);
    }
}

void oh_post_request(char *source, char *protocol, char *method, char *path)
{
    CURL *curl;
    CURLcode res;
    char post_data[2048];
    char hostname[1024];

    time_t now;
    time(&now);

    gethostname(hostname, 1023);

    char buf[sizeof "2011-10-08T07:07:09.00Z"];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S.00Z", gmtime(&now));

    sprintf(
        post_data,
        "{\"source\":\"%s\",\"target\":\"%s\",\"protocol\":\"%s\",\"method\":\"%s\",\"path\":\"%s\",\"datetime\":\"%s\"}",
        source,
        hostname,
        protocol,
        method,
        path,
        buf
    );

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://ixy8phb65c.execute-api.us-east-1.amazonaws.com/v1/log/http/request");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        // curl_easy_setopt(curl, CURLOPT_NOBODY, 1);

        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

void process_request(int socket_fd, const char *source)
{
    long i;
    int n;
    long ret;

    static char buffer[BUFSIZE + 1];
    struct http_request request = {};

    ret = read(socket_fd, buffer, BUFSIZE);

    if (ret == 0 || ret == -1) {
        (void) write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
        (void) close(socket_fd);

        logger(LOG_ERROR, "HTTP/1.1 403 Forbidden\tFailed to read browser request.\n");
    }

    if (ret > 0 && ret < BUFSIZE) {
        /* Cerrar buffer */
        buffer[ret] = 0;
    } else {
        buffer[0] = 0;
    }

    /* Parsear cabeceras */
    int h = 0;
    for (i = 0; i < ret; i++) {
        if (buffer[i] == ' ') {
            request.method[h] = 0;
            i++;
            break;
        }
        request.method[h] = buffer[i];
        h++;
    }
    for (h = 0; i < ret; i++) {
        if (buffer[i] == ' ') {
            request.path[h] = 0;
            i++;
            break;
        }
        request.path[h] = buffer[i];
        h++;
    }
    for (h = 0; i < ret-1; i++) {
        if (buffer[i] == '\r' && buffer[i+1] == '\n') {
            request.protocol[h] = 0;
            i++;
            break;
        }
        request.protocol[h] = buffer[i];
        h++;
    }

    logger(LOG_INFO, "method=%s,path=%s,protocol=%s\n---REQUEST-START---\n%s\n---REQUEST-END---\n",
    request.method, request.path, request.protocol, buffer);

    oh_post_request(source, request.protocol, request.method, request.path);

    /* No soportamos HTTP/2.0 :( */
    if (strcmp("HTTP/2.0", request.protocol) == 0) {
        logger(LOG_WARNING, "Unsupported protocol (%s) !!\n", request.protocol);

        sprintf(
            buffer,
            "HTTP/1.1 505 HTTP Version Not Supported\nServer: %s\nContent-Length: 0\nConnection: close\n\n",
            SERVER_HEADER
        );

        write(socket_fd, buffer, strlen(buffer));

        sleep(1);
        close(socket_fd);
        exit(1);
    }


    /* Si el protocolo no es HTTP/1.0 o HTTP/1.1 cierro la conexion */
    if (strcmp("HTTP/1.0", request.protocol) != 0 &&
    strcmp("HTTP/1.1", request.protocol) != 0) {
        logger(LOG_WARNING, "Unsupported protocol (%s) !!\n", request.protocol);

        close(socket_fd);
        exit(1);
    }

    /* Verificamos si el metodo esta soportado */
    if (strcmp("GET", request.method) != 0 &&
    strcmp("HEAD", request.method) != 0) {
        logger(LOG_WARNING, "Unsupported method (%s) !!\n", request.method);

        sprintf(
            buffer,
            "HTTP/1.1 405 Method Not Allowed\nServer: %s\nContent-Length: 0\nConnection: close\n\n",
            SERVER_HEADER
        );

        write(socket_fd, buffer, strlen(buffer));

        sleep(1);
        close(socket_fd);
        exit(1);
    }

    /* No permitir .. o // (directory transversal) */
    if (strstr(request.path, "..") != NULL || strstr(request.path, "//") != NULL) {
        logger(LOG_WARNING, "Possible directory transversal attemp !!\n");

        sprintf(
            buffer,
            "HTTP/1.1 400 Bad Request\nServer: %s\nContent-Length: 0\nConnection: close\n\n",
            SERVER_HEADER
        );

        write(socket_fd, buffer, strlen(buffer));

        sleep(1);
        close(socket_fd);
        exit(1);
    }


    /* Directory index */
    if (strcmp("/", request.path) == 0) {
        strcpy(request.path, "/index.html\0");
    }


    int inputfile_fd = 0;
    long content_length = 0;
    char *content_type = 0;
    int len = 0;
    int buflen = strlen(buffer);
    char filename[1024];

    if (request.path[0] == '/') {
        strcpy(filename, &request.path[1]);
    } else {
        strcpy(filename, request.path);
    }

    logger(LOG_DEBUG, "filename=%s\n", filename);

    /* Detectar mime-type */
    for (i = 0; mime_types[i].extension != 0; i++) {
        len = strlen(mime_types[i].extension);
        if (!strncmp(&filename[strlen(filename) - len], mime_types[i].extension, len)) {
            content_type = mime_types[i].mimetype;
            break;
        }
    }

    if (content_type == 0) {
        content_type = "text/plain";
    }

    /* Existe el archivo? */
    if ((inputfile_fd = open(filename, O_RDONLY)) == -1) {
        (void) sprintf(
            buffer,
            "HTTP/1.1 404 Not Found\nServer: %s\nContent-Length: 0\nConnection: close\n\n",
            SERVER_HEADER
        );
    } else {
        content_length = (long)lseek(inputfile_fd, (off_t)0, SEEK_END);
        lseek(inputfile_fd, (off_t)0, SEEK_SET);
        sprintf(
            buffer,
            "HTTP/1.1 200 OK\nServer: %s\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n",
            SERVER_HEADER,
            content_length,
            content_type
        );
        /* Enviamos los headers */
        (void) write(socket_fd, buffer, strlen(buffer));

        if (strcmp("HEAD", request.method) != 0) {
            /* Si es una peticion HEAD no enviamos el body */
            while ((ret = read(inputfile_fd, buffer, BUFSIZE)) > 0) {
                (void) write(socket_fd, buffer, ret);
            }
        }
    }

    (void) write(socket_fd, buffer, strlen(buffer));

    sleep(1);
    close(socket_fd);
    exit(1);
}

int main(int argc, char **argv)
{
    int opt = 0;
    int port = 8080;
    int listenfd = 0;
    int socketfd = 0;
    int pid = 0;
    int iEnabled = 1;

    static struct sockaddr_in serv_addr;
    static struct sockaddr_in cli_addr;
    socklen_t len = 0;

    while ((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;

            case 'h':
                printf("http-honeypotd v0.1a\n(c) 2017 Jorge Matricali\n\n");

            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    printf("Starting http-honeypotd on port %d...\n", port);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        logger(LOG_FATAL, "Error opening listen socket.\n");
        return EXIT_FAILURE;
    }

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*) &iEnabled,
    sizeof(iEnabled));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        logger(LOG_FATAL, "Cannot bind port\n");
        return EXIT_FAILURE;
    }

    if (listen(listenfd, 64) < 0) {
        logger(LOG_FATAL, "Cannot listen on port\n");
        return EXIT_FAILURE;
    }

    if (chdir("./public/") == -1) {
        logger(LOG_FATAL, "Cannot change directory\n");
        return EXIT_FAILURE;
    }

    for (;;) {
        len = sizeof(cli_addr);

        socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &len);

        char client_address[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(cli_addr.sin_addr), client_address, INET_ADDRSTRLEN);

        if (socketfd < 0) {
            logger(LOG_FATAL, "Cannot accept incoming connection from %s\n", client_address);
            (void) close(socketfd);
            return EXIT_FAILURE;
        }

        logger(LOG_INFO, "Incoming connection from %s\n", client_address);

        pid = fork();
        if (pid < 0) {
            logger(LOG_FATAL, "Cannot fork!\n");
            return EXIT_FAILURE;
        }

        if (pid == 0) {
            (void) close(listenfd);
            process_request(socketfd, client_address);
        } else {
            (void) close(socketfd);
        }
    }
}
