#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// #include <fcntl.h>


void usage(char *ex)
{
    printf("usage: %s [-p PORT]\n", ex);
}

int main(int argc, char **argv)
{
    int opt = 0;
    int port = 8080;
    int listenfd = 0;

    static struct sockaddr_in serv_addr;

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

    serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
}
