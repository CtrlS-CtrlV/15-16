/* ітеративний TCP echo-сервер */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT     9000
#define BUF_SIZE 1024
#define BACKLOG  5

static void to_upper(char *s, ssize_t len)
{
    for (ssize_t i = 0; i < len; i++)
        s[i] = toupper((unsigned char)s[i]);
}

int main(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons(PORT);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1) {
        perror("bind");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    printf("TCP echo-сервер слухає на порту %d...\n", PORT);

    for (;;) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);

        int conn_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (conn_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("[+] Клієнт: %s:%d\n", client_ip, ntohs(cli_addr.sin_port));

        char buf[BUF_SIZE];
        ssize_t n;

        while ((n = read(conn_fd, buf, sizeof(buf))) > 0) {
            to_upper(buf, n);
            if (write(conn_fd, buf, n) != n) {
                perror("write");
                break;
            }
        }

        if (n == -1) {
            perror("read");
        }

        printf("[-] Клієнт відключився.\n");
        close(conn_fd);
    }

    close(listen_fd);
    return EXIT_SUCCESS;
}
