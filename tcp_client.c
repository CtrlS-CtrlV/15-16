/* TCP-клієнт із коректним завершенням */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 9000
#define BUF_SIZE    1024

int main(void)
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port   = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &srv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    if (connect(sock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1) {
        perror("connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("Підключено до %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("Введіть повідомлення (Ctrl+D або 'quit' для виходу):\n");

    char buf[BUF_SIZE];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        if (strncmp(buf, "quit", 4) == 0)
            break;

        size_t len = strlen(buf);
        size_t total_written = 0;

        while (total_written < len) {
            ssize_t w = write(sock_fd, buf + total_written, len - total_written);
            if (w == -1) {
                if (errno == EINTR) continue;
                perror("write");
                break;
            }
            total_written += w;
        }

        ssize_t n = read(sock_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Відповідь: %s", buf);
        } else if (n == 0) {
            printf("Сервер закрив з'єднання.\n");
            break;
        } else {
            if (errno == EINTR) continue;
            perror("read");
            break;
        }
    }

    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);
    printf("З'єднання закрито.\n");
    return EXIT_SUCCESS;
}
