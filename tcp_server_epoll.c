/* мультиплексований TCP-сервер з epoll + broadcast */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT        9000
#define MAX_EVENTS  64
#define MAX_CLIENTS 128
#define BUF_SIZE    1024

static int client_fds[MAX_CLIENTS];
static int client_count = 0;

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void add_client(int fd)
{
    if (client_count < MAX_CLIENTS) {
        client_fds[client_count++] = fd;
    }
}

static void remove_client(int fd)
{
    for (int i = 0; i < client_count; i++) {
        if (client_fds[i] == fd) {
            client_fds[i] = client_fds[client_count - 1];
            client_count--;
            break;
        }
    }
    close(fd);
}

static void broadcast(int sender_fd, const char *msg, size_t len)
{
    for (int i = 0; i < client_count; i++) {
        if (client_fds[i] != sender_fd) {
            write(client_fds[i], msg, len);
        }
    }
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

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
    if (listen(listen_fd, SOMAXCONN) == -1) {
        perror("listen"); close(listen_fd); return 1;
    }

    set_nonblocking(listen_fd);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) { perror("epoll_create1"); close(listen_fd); return 1; }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl listen_fd"); close(listen_fd); close(epoll_fd); return 1;
    }

    struct epoll_event events[MAX_EVENTS];
    printf("Epoll-сервер (broadcast) на порту %d\n", PORT);

    for (;;) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait"); break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                while (1) {
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int cfd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
                    if (cfd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept"); break;
                    }
                    set_nonblocking(cfd);
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = cfd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cfd, &ev);
                    add_client(cfd);
                    printf("[+] Новий клієнт на сокеті %d\n", cfd);
                }
            } else if (events[i].events & EPOLLIN) {
                char buf[BUF_SIZE];
                int closed = 0;
                while (1) {
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        closed = 1; break;
                    } else if (n == 0) {
                        closed = 1; break;
                    } else {
                        broadcast(fd, buf, n);
                    }
                }
                if (closed) {
                    printf("[-] Клієнт на сокеті %d відключився\n", fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    remove_client(fd);
                }
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return EXIT_SUCCESS;
}
