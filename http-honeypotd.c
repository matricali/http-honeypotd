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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>

#define BUFSIZE 8096
#define SERVER_HEADER "jorgee"

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

void usage(char *ex)
{
    printf("usage: %s [-p PORT]\n", ex);
}

void process_request(int socket_fd)
{
	long i;
    int n;
    long ret;
	static char buffer[BUFSIZE + 1];

	ret = read(socket_fd, buffer, BUFSIZE);

	if (ret == 0 || ret == -1) {
        (void) write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
        (void) close(socket_fd);

        fprintf(stderr, "%s\tHTTP/1.1 403 Forbidden\tFailed to read browser request.\n", "asdsd");
	}

	if (ret > 0 && ret < BUFSIZE) {
        /* Cerrar buffer */
		buffer[ret] = 0;
    } else {
        buffer[0] = 0;
    }

	for (i = 0; i < ret; i++) {
        /* Eliminar CR LF */
		if (buffer[i] == '\r' || buffer[i] == '\n') {
			buffer[i] = '*';
        }
    }

    printf("REQUEST >> %s\n", buffer);

    if (strncmp(buffer, "GET ", 4) == 0 || strncmp(buffer, "get ", 4) == 0) {
        /* HTTP 1.x GET */
        for (i = 4; i < BUFSIZE; i++) {
    		if (buffer[i] == ' ') {
    			buffer[i] = 0;
    			break;
    		}
    	}

        /* No permitir .. */
        for (n = 0; n < i - 1; n++) {
    		if (buffer[n] == '.' && buffer[n + 1] == '.') {
                (void) sprintf(
                    buffer,
                    "HTTP/1.1 400 Bad Request\nServer: %s\nContent-Length: 0\nConnection: close\n\n",
                    SERVER_HEADER
                );

                (void) write(socket_fd, buffer, strlen(buffer));

            	sleep(1);
            	close(socket_fd);
            	exit(1);
    		}
        }

        /* index */
        if (strcmp("/", buffer+4) == 0) {
            strcpy(buffer+4, "/index.html\0");
        }


        int inputfile_fd = 0;
        long content_length = 0;
        char *content_type = 0;
        int len = 0;
        int buflen = strlen(buffer);


        /* Detectar mime-type */
        for (i = 0; mime_types[i].extension != 0; i++) {
    		len = strlen(mime_types[i].extension);
    		if (!strncmp(&buffer[buflen - len], mime_types[i].extension, len)) {
    			content_type = mime_types[i].mimetype;
    			break;
    		}
    	}

        if (content_type == 0) {
            content_type = "text/plain";
        }

        /* Existe el archivo? */
        if ((inputfile_fd = open(&buffer[5], O_RDONLY)) == -1) {
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

            while ((ret = read(inputfile_fd, buffer, BUFSIZE)) > 0) {
        		(void) write(socket_fd, buffer, ret);
        	}
        }
    } else {
        /* Unsupported method */
        (void) sprintf(
            buffer,
            "HTTP/1.1 405 Method Not Allowed\nServer: %s\nContent-Length: 0\nConnection: close\n\n",
            SERVER_HEADER
        );
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
		fprintf(stderr, "Error opening listen socket.\n");
        return EXIT_FAILURE;
    }

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*) &iEnabled,
        sizeof(iEnabled));

    serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "Cannot bind port\n");
        return EXIT_FAILURE;
    }

	if (listen(listenfd, 64) < 0) {
        fprintf(stderr, "Cannot listen on port\n");
        return EXIT_FAILURE;
    }

    if (chdir("./public/") == -1) {
        fprintf(stderr, "Cannot change directory\n");
        return EXIT_FAILURE;
    }

    for (;;) {
		len = sizeof(cli_addr);

        socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &len);

        char client_address[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(cli_addr.sin_addr), client_address, INET_ADDRSTRLEN);

		if (socketfd < 0) {
            fprintf(stderr, "Cannot accept incoming connection from %s\n", client_address);
            (void) close(socketfd);
            return EXIT_FAILURE;
        }

        printf("Incoming connection from %s\n", client_address);

        pid = fork();
		if (pid < 0) {
			fprintf(stderr, "Cannot fork!\n");
            return EXIT_FAILURE;
		}

        if (pid == 0) {
			(void) close(listenfd);
			process_request(socketfd);
		} else {
			(void) close(socketfd);
		}
	}
}
