#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

int main(void) {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(0),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0) {
        perror("bind/listen");
        close(listener);
        return 1;
    }

    socklen_t length = sizeof(address);
    if (getsockname(listener, (struct sockaddr *)&address, &length) != 0) {
        perror("getsockname");
        close(listener);
        return 1;
    }
    printf("PORT %u\n", ntohs(address.sin_port));
    fflush(stdout);

    int connection = accept(listener, NULL, NULL);
    close(listener);
    if (connection < 0) {
        perror("accept");
        return 1;
    }

    char request;
    if (recv(connection, &request, 1, 0) != 1 || request != 'q') {
        fprintf(stderr, "did not receive the expected request byte\n");
        close(connection);
        return 1;
    }
    if (send(connection, "r", 1, 0) != 1) {
        perror("send reply");
        close(connection);
        return 1;
    }

    // The peer sends an extra byte only after receiving the reply. Wait for
    // that byte without consuming it from this socket's receive queue.
    struct pollfd pfd = {.fd = connection, .events = POLLIN};
    int ready = poll(&pfd, 1, 5000);
    if (ready != 1 || !(pfd.revents & POLLIN) ||
        (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
        fprintf(stderr, "poll failed: result=%d revents=%d errno=%d\n",
                ready, pfd.revents, errno);
        close(connection);
        return 1;
    }

    // Give the peer time to enter its final read before closing. The byte
    // remains unread in this socket throughout the wait.
    struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000};
    nanosleep(&delay, NULL);

    puts("closing with the extra byte unread");
    fflush(stdout);
    if (close(connection) != 0) {
        perror("close");
        return 1;
    }
    return 0;
}
