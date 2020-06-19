#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "../include/network.h"

int my_sent(const int socketfd, const char *msg)
{
    char str[24] = {0};

    if (sprintf(str, "%zu", strlen(msg) + 1) == -1)
    {
        perror("sprintf() failed");
        return -1;
    }

    if (write(socketfd, str, 24) == -1)
    {
        perror("write() failed");
        return -1;
    }

    if (write(socketfd, msg, strlen(msg) + 1) == -1)
    {
        perror("write() failed");
        return -1;
    }

    return 0;
}

char *my_receive(const int socket)
{
    long int len = 0;
    char str[24] = {0};
    char *bufffer = NULL;

    if (recv(socket, str, 24, MSG_WAITALL) == -1)
    {
        perror("read() failed");
        return NULL;
    }

    len = strtol(str, NULL, 10);
    // printf("len %ld\n", len);
    // if ((len == LONG_MIN) || (len = LONG_MAX))
    // {
    //     perror("strtol() failed");
    //     return NULL;
    // }

    if ((bufffer = malloc(len)) == NULL)
    {
        perror("malloc() failed");
        return NULL;
    }

    if (recv(socket, bufffer, len, MSG_WAITALL) == -1)
    {
        perror("read() failed");
        return NULL;
    }

    return bufffer;
}