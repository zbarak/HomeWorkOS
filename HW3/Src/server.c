//===================================================
//server.c:
#include "../Header/server.h"


// send_all:
// Sends exactly `len` bytes from `buf` to socket `fd`.
// Returns 0 on success, -1 on failure.
static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);

        // send() returning 0 means connection closed
        if (n == 0) return -1;

        // Handle errors
        if (n < 0) {
            // If interrupted by signal, retry
            if (errno == EINTR) continue;
            return -1;
        }

        sent += (size_t)n;
    }
    return 0;
}

// recv_line:
// Reads data from socket until '\n' is received or maxlen-1 bytes are read.
// The buffer is always null-terminated.
// Returns number of bytes read (excluding '\0'),
// 0 if the connection was closed, or -1 on error.
static ssize_t recv_line(int fd, char *buf, size_t maxlen) {
    size_t idx = 0;

    while (idx < maxlen - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);

        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        // Connection closed
        if (n == 0) {
            if (idx == 0) return 0;
            break;
        }

        buf[idx++] = c;
        if (c == '\n') break;
    }

    buf[idx] = '\0';
    return (ssize_t)idx;
}

// Finds a client index by name.
// Returns index if found, -1 otherwise.
static int find_client_by_name(Client clients[], const char *name) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 &&
            strcmp(clients[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {

    // 1. Validate command line arguments
    if (argc != 2) {
        printf("Usage: %s port\n", argv[0]);
        return 1;
    }

    // 2. Create listening socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    // Allow fast reuse of the port
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. Bind socket to port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(argv[1]));

    if (bind(listen_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    // 4. Start listening
    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    // 5. Initialize client list
    Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].name[0] = '\0';
        clients[i].ip[0] = '\0';
    }

    fd_set readfds;

    while (1) {
        // 6. Prepare file descriptor set for select()
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        int maxfd = listen_fd;

        // Add all active client sockets
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != -1) {
                FD_SET(clients[i].socket, &readfds);
                if (clients[i].socket > maxfd)
                    maxfd = clients[i].socket;
            }
        }

        // 7. Wait for activity on any socket
        int rc = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        // 8. New incoming connection
        if (FD_ISSET(listen_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);

            int connfd = accept(listen_fd,
                                (struct sockaddr *)&client_addr,
                                &addrlen);
            if (connfd < 0) {
                perror("accept");
            } else {
                // Find empty client slot
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].socket == -1) {
                        slot = i;
                        break;
                    }
                }

                if (slot == -1) {
                    // Server full
                    send_all(connfd, "server full\n", 12);
                    close(connfd);
                } else {
                    // Receive client name (handshake)
                    char namebuf[MAX_NAME_LEN];
                    ssize_t n = recv_line(connfd, namebuf, sizeof(namebuf));

                    if (n <= 0) {
                        close(connfd);
                    } else {
                        // Remove newline
                        namebuf[strcspn(namebuf, "\r\n")] = '\0';

                        clients[slot].socket = connfd;
                        strncpy(clients[slot].name,
                                namebuf, MAX_NAME_LEN - 1);

                        inet_ntop(AF_INET,
                                  &client_addr.sin_addr,
                                  clients[slot].ip,
                                  sizeof(clients[slot].ip));

                        printf("client %s connected from %s\n",
                               clients[slot].name,
                               clients[slot].ip);
                    }
                }
            }
        }

        // 9. Handle messages from existing clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].socket;
            if (sd == -1) continue;

            if (FD_ISSET(sd, &readfds)) {
                char buf[BUFFER_SIZE];
                ssize_t n = recv(sd, buf, sizeof(buf) - 1, 0);

                if (n <= 0) {
                    // Client disconnected
                    printf("client %s disconnected\n", clients[i].name);
                    close(sd);
                    clients[i].socket = -1;
                    clients[i].name[0] = '\0';
                    continue;
                }

                buf[n] = '\0';
                buf[strcspn(buf, "\r\n")] = '\0';

                // Create message prefix: "name: message"
                char out[BUFFER_SIZE];
                snprintf(out, sizeof(out),
                         "%s: %s\n",
                         clients[i].name, buf);

                // Whisper message (@user ...)
                if (buf[0] == '@') {
                    char *space = strchr(buf, ' ');
                    if (space) {
                        char target[MAX_NAME_LEN];
                        size_t len = space - (buf + 1);
                        strncpy(target, buf + 1, len);
                        target[len] = '\0';

                        int idx = find_client_by_name(clients, target);
                        if (idx != -1) {
                            send_all(clients[idx].socket,
                                     out, strlen(out));
                        }
                    }
                }
                // Normal broadcast message
                else {
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].socket != -1) {
                            send_all(clients[j].socket,
                                     out, strlen(out));
                        }
                    }
                }
            }
        }
    }

    close(listen_fd);
    return 0;
}
//===================================================
