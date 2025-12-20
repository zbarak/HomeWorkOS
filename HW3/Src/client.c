//===================================================
//client.c
#include "client.h"
static int send_all(int fd, const char *buf, size_t len){ // Sends exactly `len` bytes from `buf` over socket `fd`.
                                                          // Returns 0 on success (all bytes sent), -1 on error.
    size_t sent = 0;
    while(sent < len){                                    //loops on all Bytes
        ssize_t n = send(fd, buf + sent, len - sent, 0);
    
    if (n < 0) {                                          //if send() returns an error
            if (errno == EINTR) continue;                 //EINTR means the send was interrupted by a signal
                                                          //This is NOT a real error — we should just retry
            return -1;                                    //Any other error must be stopped.
        }
    sent += (size_t)n;                                    //update sent to next bytes.
    }
return 0;
}

static int connect_to_server(const char *addr, const char *port){
    struct addrinfo hints, *res = NULL, *p = NULL;
    memset()
}


int main(int argc, char *argv[]) {
    // 1. Validate command line syntax: hw3client addr port name [cite: 11]
    if (argc != 4) {
        printf("Usage: %s addr port name\n", argv[0]);
        return 1;
    }

    // 2. Connect to the server [cite: 11, 27]
    // Use socket() and connect()

    // 3. Phase 1 Handshake: Notify server of the name [cite: 27]
    // send(sockfd, argv[3], strlen(argv[3]), 0);

    while (1) {
        // 4. Use select() to monitor BOTH stdin (0) and the server socket 

        // 5. If server socket is ready: message received [cite: 28]
        // - recv() and print the message to the screen "as is" [cite: 28]

        // 6. If stdin is ready: user is typing [cite: 29, 30]
        // - Read line from user (fgets)
        
        // - Check for !exit: 
        //   - If "!exit", send to server, print "client exiting", and close.
        
        // - Check for Whisper (@friend msg) or Normal message: [cite: 31, 32]
        //   - send() the string to the server.
    }

    return 0;
}
//===================================================