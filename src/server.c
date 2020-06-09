#define _XOPEN_SOURCE 700

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/fnctl.h"

#define SERVER_BACKLOG 10

typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;

static void *thread_run();
static int server_init(uint16_t port, const int backlog);
static int handle_query(const int socket);
static int handle_statistic(const int socket);

extern char *optarg;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
queue_ptr q = NULL;

int main(int argc, char *argv[])
{
    char sockets_arr[FD_SETSIZE] = {0};
    fd_set r_fds, cur_fds;
    pthread_t *pool = NULL;
    uint16_t queryPortNum = 0, statisticsPortNum = 0;
    int opt = 0, numThreads = 0, bufferSize = 0, q_sockfd = 0, s_sockfd = 0, cli_sockfd = 0, err = 0;

    if (argc != 9)
    {
        fprintf(stderr, "Usage: –q <queryPortNum> -s <statisticsPortNum> –w <numThreads> –b <bufferSize>\n");
        return -1;
    }

    while ((opt = getopt(argc, argv, "q:s:w:b:")) != -1)
    {
        switch (opt)
        {
        case 'q':
            if ((queryPortNum = atoi(optarg)) == 0)
            {
                fprintf(stderr, "Invalid value: %s\n", optarg);
            }
            break;
        case 'b':
            if ((bufferSize = atoi(optarg)) == 0)
            {
                fprintf(stderr, "Invalid value: %s\n", optarg);
            }
            break;
        case 's':
            if ((statisticsPortNum = atoi(optarg)) == 0)
            {
                fprintf(stderr, "Invalid value: %s\n", optarg);
            }
            break;
        case 'w':
            if ((numThreads = atoi(optarg)) == 0)
            {
                fprintf(stderr, "Invalid value: %s\n", optarg);
            }
            if (numThreads > 20)
            {
                printf("wow a lot of threads, hope your machine can handle it\n");
            }
            break;
        case '?':
            fprintf(stderr, "Usage: –q <queryPortNum> -s <statisticsPortNum> –w <numThreads> –b <bufferSize>\n");
            return -1;
        }
    }

    if (queryPortNum == statisticsPortNum)
    {
        fprintf(stderr, "queryPortNum and statisticsPortNum must differ\n");
        return -1;
    }

    if ((q = queue_init(bufferSize)) == NULL)
    {
        fprintf(stderr, "queue_init() failed\n");
        return -1;
    }

    if ((pool = malloc(numThreads * sizeof(pthread_t))) == NULL)
    {
        perror("malloc() failed");
        return -1;
    }

    for (int i = 0; i < numThreads; i++)
    {
        if ((err = pthread_create(&pool[i], NULL, thread_run, NULL)) != 0)
        {
            fprintf(stderr, "pthread_create() failed: %s\n", strerror(err));
            return -1;
        }
    }

    if ((q_sockfd = server_init(queryPortNum, SERVER_BACKLOG)) == -1)
    {
        fprintf(stderr, "server_init() failed\n");
        return -1;
    }

    if ((s_sockfd = server_init(statisticsPortNum, SERVER_BACKLOG)) == -1)
    {
        fprintf(stderr, "server_init() failed\n");
        return -1;
    }

    FD_ZERO(&cur_fds);
    FD_SET(q_sockfd, &cur_fds);
    FD_SET(s_sockfd, &cur_fds);

    while (1)
    {
        r_fds = cur_fds;

        if (pselect(FD_SETSIZE, &r_fds, NULL, NULL, NULL, NULL) == -1)
        {
            perror("pselect() failed");
            return -1;
        }

        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (FD_ISSET(i, &r_fds))
            {
                if (i == q_sockfd)
                {
                    if ((cli_sockfd = accept(q_sockfd, NULL, NULL)) == -1)
                    {
                        perror("accept() failed");
                        exit(EXIT_FAILURE);
                    }
                    printf("new connection: %d\n", cli_sockfd);
                    sockets_arr[cli_sockfd] = 'q';
                    FD_SET(cli_sockfd, &cur_fds);
                }
                else if (i == s_sockfd)
                {
                    if ((cli_sockfd = accept(s_sockfd, NULL, NULL)) == -1)
                    {
                        perror("accept() failed");
                        exit(EXIT_FAILURE);
                    }

                    sockets_arr[cli_sockfd] = 's';
                    FD_SET(cli_sockfd, &cur_fds);
                }
                else
                {
                    int val = 0;

                    pthread_mutex_lock(&mutex);
                    val = enqueue(q, i, sockets_arr[i]);

                    if (val != -1 && val != -2)
                    {
                        sockets_arr[i] = 0;
                        FD_CLR(i, &cur_fds);
                        pthread_cond_signal(&condition_var);
                    }

                    pthread_mutex_unlock(&mutex);
                }
            }
        }
    }

    for (int i = 0; i < numThreads; i++)
    {
        pthread_join(pool[i], NULL);
    }

    free(pool);

    close(q_sockfd);
    close(s_sockfd);

    return 0;
}

static void *thread_run()
{
    while (1)
    {
        queue_node_ptr node = NULL;

        pthread_mutex_lock(&mutex);

        if ((node = dequeue(q)) == NULL)
        {
            pthread_cond_wait(&condition_var, &mutex);
        }

        pthread_mutex_unlock(&mutex);

        if (node != NULL)
        {
            printf("about to handle a conn\n");
            if (node->port == 'q')
            {
                handle_query(node->socket);
            }
            else if (node->port == 's')
            {
                handle_statistic(node->socket);
            }
            free(node);
        }
    }

    return NULL;
}

static int handle_query(const int socket)
{
    char buffer[100] = {0};

    printf("reading..\n");
    read(socket, buffer, 100);
    printf("msg: %s\n", buffer);
    write(socket, "i am the server", 100);
    printf("done\n");

    close(socket);

    return 0;
}

static int handle_statistic(const int socket)
{
    char buffer[100] = {0};

    read(socket, buffer, 100);
    printf("STATISTIc: %s", buffer);

    close(socket);

    return 0;
}

static int server_init(uint16_t port, const int backlog)
{
    int sockfd = 0;
    SA_IN servaddr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket() failed");
        return -1;
    }

    memset(&servaddr, 0, sizeof(SA_IN));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (SA *)&servaddr, sizeof(SA_IN)) == -1)
    {
        perror("bind() failed");
        return -1;
    }

    if (listen(sockfd, backlog) == -1)
    {
        perror("listen() failed");
        return -1;
    }

    printf("Server listening on port: %d\n", port);

    return sockfd;
}