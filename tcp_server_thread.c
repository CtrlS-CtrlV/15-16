/* багатоклієнтний TCP-сервер з потоками */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <time.h>

#define PORT     9000
#define BUF_SIZE 1024
#define BACKLOG  10

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_fp = NULL;

typedef struct {
    int         conn_fd;
    char        client_ip[INET_ADDRSTRLEN];
    uint16_t    client_port;
} ClientInfo;

static void log_event(const char *fmt, ...)
{
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    pthread_mutex_lock(&log_mutex);
    va_list args;
    va_start(args, fmt);
    fprintf(log_fp, "[%s] ", time_str);
    vfprintf(log_fp, fmt, args);
    fprintf(log_fp, "\n");
    fflush(log_fp);
    va_end(args);
    pthread_mutex_unlock(&log_mutex);
}

static void *client_handler(void *arg)
{
    ClientInfo *ci = (ClientInfo *)arg;

    log_event("Підключення: %s:%d (fd=%d)", ci->client_ip, ci->client_port, ci->conn_fd);

    char buf[BUF_SIZE];
    ssize_t n;

    while ((n = read(ci->conn_fd, buf, sizeof(buf))) > 0) {
        if (write(ci->conn_fd, buf, n) != n) {
            break;
        }
    }

    log_event("Відключення: %s:%d", ci->client_ip, ci->client_port);

    close(ci->conn_fd);
    free(ci);
    return NULL;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    log_fp = fopen("server.log", "a");
    if (!log_fp) { perror("fopen"); return 1; }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) { perror("socket"); fclose(log_fp); return 1; }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons(PORT);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1) {
        perror("bind"); close(listen_fd); fclose(log_fp); return 1;
    }
    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen"); close(listen_fd); fclose(log_fp); return 1;
    }

    printf("Thread-сервер слухає на порту %d...\n", PORT);

    for (;;) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int conn_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);

        if (conn_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        ClientInfo *ci = malloc(sizeof(ClientInfo));
        if (!ci) {
            close(conn_fd);
            continue;
        }
        ci->conn_fd = conn_fd;
        inet_ntop(AF_INET, &cli_addr.sin_addr, ci->client_ip, sizeof(ci->client_ip));
        ci->client_port = ntohs(cli_addr.sin_port);

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, ci) != 0) {
            perror("pthread_create");
            close(conn_fd);
            free(ci);
            continue;
        }
        pthread_detach(tid);
    }

    fclose(log_fp);
    close(listen_fd);
    return EXIT_SUCCESS;
}
