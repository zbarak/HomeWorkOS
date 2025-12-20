//===================================================
//server.c
#include "../Header/server.h"


int main(int argc, char *argv[]) {
    // 1. Validate command line syntax: hw3server port [cite: 8, 9]
    if (argc != 2) {
        printf("Usage: %s port\n", argv[0]);
        return 1;
    }

    // 2. Initialize Master Socket (bind and listen) [cite: 16]
    // Use socket(), bind(), and listen()

    // 3. Initialize client tracking structure [cite: 18]
    Client clients[MAX_CLIENTS];
    // Set all client sockets to -1 initially

    while (1) {
        // 4. Use select() to monitor the listening socket AND all active client sockets [cite: 18]
        
        // 5. If listening socket is ready: accept() new connection [cite: 17]
        // - Receive the name from the client (Phase 1 Handshake) [cite: 27]
        // - Print: "client [name] connected from [address]" [cite: 17]

        // 6. If a client socket is ready: recv() message [cite: 18]
        // - Handle disconnection (recv returns 0): Print "client [name] disconnected" [cite: 25]
        
        // - Handle Incoming Message:
        //   - If it starts with '@': Parse as whisper [cite: 21, 22]
        //   - Else: Treat as normal message [cite: 19]
        
        // - Route Message:
        //   - Add prefix "sourcename: " [cite: 23]
        //   - Send to destination(s) using send() [cite: 19, 20]
    }

    return 0;
}
//===================================================