//===================================================
//client.c:
#include "client.h"

static int send_all(int fd, const char *buf, size_t len){ // Sends exactly `len` bytes from `buf` over socket `fd`.
                                                          // Returns 0 on success (all bytes sent), -1 on error.
    size_t sent = 0;
    while(sent < len){                                    //loops on all Bytes
        ssize_t n = send(fd, buf + sent, len - sent, 0);
    if(n == 0) return -1;
    if (n < 0) {                              //if send() returns an error
            if (errno == EINTR) continue;                 //EINTR means the send was interrupted by a signal
                                                          //This is NOT a real error — we should just retry
            return -1;                                    //Any other error must be stopped.
        }
    sent += (size_t)n;                                    //update sent to next bytes.
    }
return 0;
}
//Connect_To_Server tries to connect a TCP client socket to a server given an address
//(like "127.0.0.1" or "chat.example.com") and a port (like "5555"). 
//If it succeeds, it returns a connected socket file descriptor, otherwise it returns -1.
static int connect_to_server(const char *addr, const char *port){
    struct addrinfo hints, *res = NULL, *p = NULL; //addrinfo hints are the setting to give getaddrinfo in its unique format
    int sockfd = -1; //will hold the sockfd descriptor, initilized to -1
    int rc; //will hold the returning code from getaddrinfo
    memset(&hints, 0, sizeof(hints)); //sets all bytes of hints to 0 to start clean with no grabage.
    hints.ai_family   = AF_UNSPEC;     // allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;   // TCP (Not UDP)
    rc = getaddrinfo(addr, port, &hints, &res); //this function asks OS to give addr and port
    if (rc != 0) { //if not zero was retuned - error, report it
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) { //Loops over every address candidate in the linked list.
                                               //p->ai_next moves to the next result.
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol); //this creates the endpoint to connect with
        if (sockfd < 0) continue; // try next address

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            // success
            freeaddrinfo(res); //we got a socket saved in sockfd, free res and return sockfd.
            return sockfd;
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);  //everything failed. free res and return -1 indicator of failure.
    return -1;
}


int main(int argc, char *argv[]) {
    // 1. Validate command line syntax: hw3client addr port name
    if (argc != 4) {
        printf("Usage: %s addr port name\n", argv[0]);
        return 1;
    }

    const char *addr = argv[1];
    const char *port = argv[2];
    const char *name = argv[3];

    // 2. Connect to the server
    int sockfd = connect_to_server(addr, port); //sockd has now relevent socket info
    if (sockfd < 0) {
        perror("connect_to_server");
        return 1;
    }

    // 3. Phase 1 Handshake: Notify server of the name
    // "details up to you" => we send "name\n" once.
    char hello[BUFFER_SIZE];
    snprintf(hello, sizeof(hello), "%s\n", name);

    if (send_all(sockfd, hello, strlen(hello)) < 0) { //sends 'hello' over the connection
        perror("send_all(name)");
        close(sockfd);
        return 1;
    }

    while (1) {
        // 4. Use select() to monitor BOTH stdin (0) and the server socket
        fd_set readfds;               //This code initializes a file-descriptor set and adds stdin and the connected socket to it-
        FD_ZERO(&readfds);            // so the program can monitor both for incoming data. -    
        FD_SET(STDIN_FILENO, &readfds);//It prepares the set to be passed to select()-
        FD_SET(sockfd, &readfds);       //which will block until either the user types something or the server sends a message. 

        int maxfd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

        int rc = select(maxfd + 1, &readfds, NULL, NULL, NULL);
//This line blocks indefinitely until at least one file descriptor in readfds (either stdin or the socket) becomes readable-
//where maxfd + 1 specifies the range of descriptors to check-
//and the NULL arguments indicate no write, exception, or timeout monitoring.

        if (rc < 0) {
            if (errno == EINTR) continue; // interrupted -> retry
            perror("select");
            break;
        }

        // 5. If server socket is ready: message received
        if (FD_ISSET(sockfd, &readfds)) {
            char buf[BUFFER_SIZE];
            ssize_t n = recv(sockfd, buf, sizeof(buf) - 1, 0);

            if (n < 0) {
                if (errno == EINTR) continue;
                perror("recv");
                break;
            }

            if (n == 0) {
                // Server closed the connection
                printf("server disconnected\n");
                break;
            }

            // Print "as is" (but ensure it's null-terminated for printf)
            buf[n] = '\0';
            fputs(buf, stdout);
            fflush(stdout);
        }

        // 6. If stdin is ready: user is typing
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char line[BUFFER_SIZE];

            // fgets includes the newline if it fits in the buffer
            if (fgets(line, sizeof(line), stdin) == NULL) {
                // EOF (Ctrl+D) or error -> exit gracefully
                printf("client exiting\n");
                break;
            }

            // Send the line to the server (normal or whisper are both just "a line")
            if (send_all(sockfd, line, strlen(line)) < 0) {
                perror("send_all(line)");
                break;
            }

            // "!exit" is sent like a normal message, then client exits
            // We accept "!exit\n" too, because fgets keeps newline.
            if (strcmp(line, "!exit\n") == 0 || strcmp(line, "!exit") == 0) {
                printf("client exiting\n");
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
//=========================================================================================================