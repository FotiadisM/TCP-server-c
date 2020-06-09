#define _XOPEN_SOURCE 700

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "../include/client.h"

static void *thread_run(void *val);

pthread_t *pool;

int main(int argc, char *argv[])
{
    thread_info info;
    char *queryFile = NULL;
    int numThreads = 0, err = 0;

    if (argc != 9)
    {
        fprintf(stderr, "Usage: –q <queryFile> -w <numThreads> –sp <servPort> –sip <servIP>\n");
        return -1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp("-q", argv[i]))
        {
            i++;
            if ((queryFile = strdup(argv[i])) == NULL)
            {
                perror("strdup failed()");
                return -1;
            }
        }
        else if (!strcmp("-w", argv[i]))
        {
            if ((numThreads = atoi(argv[++i])) == 0)
            {
                fprintf(stderr, "Invalid value: %s\n", argv[i]);
                return -1;
            }
        }
        else if (!strcmp("-sp", argv[i]))
        {
            if ((info.port = atoi(argv[++i])) == 0)
            {
                fprintf(stderr, "Invalid value: %s\n", argv[i]);
                return -1;
            }
        }
        else if (!strcmp("-sip", argv[i]))
        {
            i++;
            if ((info.serverIP = strdup(argv[i])) == 0)
            {
                fprintf(stderr, "Invalid value: %s\n", argv[i]);
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "Usage: –q <queryFile> -w <numThreads> –sp <servPort> –sip <servIP>\n");
            return -1;
        }
    }

    if ((pool = malloc(numThreads * sizeof(pthread_t))) == NULL)
    {
        perror("malloc");
        return -1;
    }

    for (int i = 0; i < numThreads; i++)
    {
        if ((err = pthread_create(&pool[i], NULL, thread_run, &info)) != 0)
        {
            fprintf(stderr, "pthread_create() failed: %s\n", strerror(err));
            return -1;
        }
    }

    for (int i = 0; i < numThreads; i++)
    {
        pthread_join(pool[i], NULL);
    }

    free(pool);
    free(queryFile);
    free(info.serverIP);

    return 0;
}

static void *thread_run(void *val)
{
    int sockfd;
    SA_IN servaddr;
    thread_info info;
    char buffer[100] = {0};

    memcpy(&info, val, sizeof(servaddr));

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket() failed");
        return NULL;
    }

    memset(&servaddr, 0, sizeof(SA_IN));

    servaddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, info.serverIP, &servaddr.sin_addr) != 1)
    {
        perror("inet_pton");
        return NULL;
    }
    servaddr.sin_port = htons(info.port);

    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("connect() failed");
        return NULL;
    }

    write(sockfd, "i am the client", 100);
    read(sockfd, buffer, 100);
    printf("buf: %s\n", buffer);

    return NULL;
}