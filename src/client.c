#define _XOPEN_SOURCE 700

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

#define SERVER_BACKLOG 10

typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;

pthread_t *pool;

int main(int argc, char *argv[])
{
    SA_IN servaddr;
    char *queryFile = NULL, *serverIP = NULL;
    int numThreads = 0, serverPort = 0, sockfd = 0;

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
            if ((serverPort = atoi(argv[++i])) == 0)
            {
                fprintf(stderr, "Invalid value: %s\n", argv[i]);
                return -1;
            }
        }
        else if (!strcmp("-sip", argv[i]))
        {
            i++;
            if ((serverIP = strdup(argv[i])) == 0)
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

    printf("%s %s %d %d\n", queryFile, serverIP, serverPort, numThreads);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket() failed");
        return -1;
    }

    memset(&servaddr, 0, sizeof(SA_IN));

    servaddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, serverIP, &servaddr.sin_addr) != 1)
    {
        perror("inet_pton");
        return -1;
    }
    servaddr.sin_port = htons(serverPort);

    free(serverIP);

    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("connect() failed");
        return -1;
    }

    char buffer[100] = {0};
    write(sockfd, "i am the client", 100);
    read(sockfd, buffer, 100);
    printf("buf: %s\n", buffer);

    printf("all good my boy\n");

    free(pool);
    free(queryFile);

    return 0;
}