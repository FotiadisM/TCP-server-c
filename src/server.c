#define _XOPEN_SOURCE 700

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "../include/fnctl.h"
#include "../include/pipes.h"

#define SERVER_BACKLOG 10
#define SHUT_DOWN_TIME 2
#define SOCK_BUFFER 100
#define max(a, b) (((a) > (b)) ? (a) : (b))

typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;

static void *thread_run();
static int server_init(uint16_t port, const int backlog);
static int handle_query(const int socket);
static int handle_statistic(const int socket);
static void handler(int signum);

extern char *optarg;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wp_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t smpl = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
queue_ptr q = NULL;
worker_ptr work_arr = NULL;
volatile sig_atomic_t m_signal = 0;

int main(int argc, char *argv[])
{
    struct sigaction act;
    fd_set r_fds, cur_fds;
    pthread_t *pool = NULL;
    sigset_t blockset, emptyset;
    char sockets_arr[FD_SETSIZE] = {0};
    uint16_t queryPortNum = 0, statisticsPortNum = 0;
    int opt = 0, numThreads = 0, bufferSize = 0, q_sockfd = 0, s_sockfd = 0, cli_sockfd = 0, err = 0, maxfd = 0;

    if (argc != 9)
    {
        fprintf(stderr, "Usage: –q <queryPortNum> -s <statisticsPortNum> –w <numThreads> –b <bufferSize>\n");
        return -1;
    }

    while ((opt = getopt(argc, argv, "q:s:w:b:")) != -1)
    {
        switch (opt) /* condition */
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
    maxfd = max(q_sockfd, s_sockfd) + 1;

    sigemptyset(&blockset);
    sigemptyset(&emptyset);
    sigaddset(&blockset, SIGINT);
    sigprocmask(SIG_BLOCK, &blockset, NULL);

    act.sa_handler = (void *)handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    while (1)
    {
        r_fds = cur_fds;

        if (pselect(maxfd, &r_fds, NULL, NULL, NULL, &emptyset) == -1)
        {
            if (errno == EINTR)
            {
                printf("\nexiting..\n");
                if (m_signal == SIGINT)
                {
                    break;
                }
            }
            else
            {
                perror("pselect() failed");
                return -1;
            }
        }
        else
        {
            for (int i = 0; i < maxfd; i++)
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

                        pthread_mutex_lock(&smpl);
                        sockets_arr[cli_sockfd] = 'q';
                        pthread_mutex_unlock(&smpl);
                        FD_SET(cli_sockfd, &cur_fds);
                        maxfd = max(maxfd, cli_sockfd) + 1;
                    }
                    else if (i == s_sockfd)
                    {
                        if ((cli_sockfd = accept(s_sockfd, NULL, NULL)) == -1)
                        {
                            perror("accept() failed");
                            exit(EXIT_FAILURE);
                        }

                        pthread_mutex_lock(&smpl);
                        sockets_arr[cli_sockfd] = 's';
                        pthread_mutex_unlock(&smpl);
                        FD_SET(cli_sockfd, &cur_fds);
                        maxfd = max(maxfd, cli_sockfd) + 1;
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
    }

    sleep(SHUT_DOWN_TIME); // wait for threads to finish their work;
    pthread_cond_broadcast(&condition_var);

    for (int i = 0; i < numThreads; i++)
    {
        pthread_join(pool[i], NULL);
    }

    close(q_sockfd);
    close(s_sockfd);

    free(pool);
    queue_close(q);
    worker_close(work_arr);

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
            if (m_signal == SIGINT)
            {
                pthread_mutex_unlock(&mutex);
                break;
            }
            pthread_cond_wait(&condition_var, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        if (node != NULL)
        {
            pthread_mutex_lock(&smpl);
            if (node->port == 'q')
            {
                pthread_mutex_unlock(&smpl);
                handle_query(node->socket);
            }
            else if (node->port == 's')
            {
                pthread_mutex_unlock(&smpl);
                handle_statistic(node->socket);
            }
            free(node);
        }
    }

    return NULL;
}

static int handle_query(const int socket)
{
    char buffer[2000] = {0};

    read(socket, buffer, 2000);
    printf("msg: %s\n", buffer);
    // write(socket, "i am the server", 100);

    close(socket);

    return 0;
}

static int handle_statistic(const int socket)
{
    char *buffer = NULL;

    if ((buffer = decode(socket, SOCK_BUFFER)) == NULL)
    {
        fprintf(stderr, "decode() failed\n");
        return -1;
    }

    if (!strcmp(buffer, "HANDSHAKE"))
    {
        SA_IN clientaddr;
        socklen_t sa_len = sizeof(SA_IN);

        memset(&clientaddr, 0, sizeof(SA_IN));
        getsockname(socket, (SA *)&clientaddr, &sa_len);

        free(buffer);
        while (1)
        {
            if ((buffer = decode(socket, SOCK_BUFFER)) == NULL)
            {
                fprintf(stderr, "decode() failed\n");
                return -1;
            }

            if (!strcmp(buffer, "OK"))
            {
                free(buffer);
                break;
            }

            pthread_mutex_lock(&wp_mtx);
            if ((work_arr = add_worker_country(work_arr, inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), buffer)) == NULL)
            {
                fprintf(stderr, "add_worker_country() failed\n");
                return -1;
            }
            pthread_mutex_unlock(&wp_mtx);

            free(buffer);
        }
    }
    else
    {
        // handle stats
        free(buffer);
    }

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

static void handler(int signum)
{
    m_signal = signum;
}