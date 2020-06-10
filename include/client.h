#ifndef CLIENT_H
#define CLIENT_H

typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;

typedef struct thread_info
{
    int port;
    char *serverIP;
    char *line;
} thread_info;

#endif