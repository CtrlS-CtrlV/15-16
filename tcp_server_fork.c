/* багатоклієнтний TCP-сервер з fork() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT     9000
#define BUF_SIZE 1024
#define BACKLOG  10

static void sigchld_handler(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

static void handle_client(int conn_fd, struct sockaddr_in *cli_addr)
{
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("[PID %d] Обслуговую клієнта %s:%d\n",
           getpid(), client_ip, ntohs(cli_addr->sin_port));

    char buf[BUF_SIZE];
    ssize_t n;

    while ((n = read(conn_fd, buf, sizeof(buf))) > 0) {
        if (write(conn_fd, buf, n) != n) {
            break;
        }
    }

    printf("[PID %d] Клієнт відключився.\n", getpid());
    close(conn_fd);
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons(PORT);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1) {
        perror("bind"); close(listen_fd); return 1;
    }
    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen"); close(listen_fd); return 1;
    }

    printf("Fork-сервер слухає на порту %d...\n", PORT);

    for (;;) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int conn_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);

        if (conn_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(conn_fd);
            continue;
        }

        if (pid == 0) {
            close(listen_fd);
            handle_client(conn_fd, &cli_addr);
            _exit(0);
        } else {
            close(conn_fd);
        }
    }

    close(listen_fd);
    return EXIT_SUCCESS;
}
