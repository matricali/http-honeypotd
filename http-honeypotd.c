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
    int socketfd = 0;
    int pid = 0;

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

    for (;;) {
		len = sizeof(cli_addr);

        socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &len);

        char client_address[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(cli_addr.sin_addr), client_address, INET_ADDRSTRLEN);

		if (socketfd < 0) {
            fprintf(stderr, "Cannot accept incoming connection from %s\n", client_address);
            (void) close(socketfd);
            continue;
        }

        printf("Incoming connection from %s", client_address);

        pid = fork();
		if (pid < 0) {
			fprintf(stderr, "Cannot fork!\n");
            return EXIT_FAILURE;
		}

        if (pid == 0) {
			(void) close(listenfd);
			/* Aca tendriamos que manejar la peticion */
		} else {
			(void) close(socketfd);
		}
	}
}
