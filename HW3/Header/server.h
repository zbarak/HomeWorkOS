//===================================================
//server.h:
#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 16
#define MAX_NAME_LEN 256
#define MAX_MSG_LEN 256
#define BUFFER_SIZE 2048

typedef struct {
    int socket;
    char name[MAX_NAME_LEN];
    char ip[INET_ADDRSTRLEN];
} Client;   

#endif

//===================================================

