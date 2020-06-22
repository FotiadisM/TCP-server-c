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

static void *thread_run();
static int server_init(uint16_t port, const int backlog);
static int handle_query(const int socket);
static int handle_statistic(const int socket);
static void handler(int signum);

queue_ptr q = NULL;
worker_ptr wp = NULL;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wp_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t smpl = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

extern char *optarg;
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

                        pthread_mutex_lock(&mtx);
                        val = enqueue(q, i, sockets_arr[i]);

                        if (val == -1)
                        {
                            fprintf(stderr, "enqueue() failed");
                        }
                        else if (val == -2)
                        {
                            if (encode(i, "Server is busy", SOCK_BUFFER) == -1)
                            {
                                fprintf(stderr, "encode() failed");
                            }
                            close(i);
                            FD_CLR(i, &cur_fds);
                        }
                        else
                        {
                            sockets_arr[i] = 0;
                            FD_CLR(i, &cur_fds);
                            pthread_cond_signal(&condition_var);
                        }

                        pthread_mutex_unlock(&mtx);
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
    worker_close(wp);

    return 0;
}

static void *thread_run()
{
    while (1)
    {
        queue_node_ptr node = NULL;

        pthread_mutex_lock(&mtx);
        if ((node = dequeue(q)) == NULL)
        {
            if (m_signal == SIGINT)
            {
                pthread_mutex_unlock(&mtx);
                break;
            }
            pthread_cond_wait(&condition_var, &mtx);
        }
        pthread_mutex_unlock(&mtx);

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
    int flag = 0;
    char *buffer = NULL, *str = NULL;
    wordexp_t p;

    if ((buffer = decode(socket, SOCK_BUFFER)) == NULL)
    {
        fprintf(stderr, "decode() failed\n");
        return -1;
    }

    if (wordexp(buffer, &p, 0))
    {
        fprintf(stderr, "wordexp() failed\n");
        return -1;
    }

    if (!strcmp(p.we_wordv[0], "/diseaseFrequency"))
    {
        flag = diseaseFrequency(wp, buffer, &p, SOCK_BUFFER, &str);
    }
    else if (!strcmp(p.we_wordv[0], "/topk-AgeRanges"))
    {
        flag = topk_AgeRanges(wp, buffer, &p, SOCK_BUFFER, &str);
    }
    else if (!strcmp(p.we_wordv[0], "/searchPatientRecord"))
    {
        flag = searchPatientRecord(wp, buffer, &p, SOCK_BUFFER, &str);
    }
    else if (!strcmp(p.we_wordv[0], "/numPatientAdmissions"))
    {
        flag = numFunction(wp, buffer, &p, SOCK_BUFFER, &str);
    }
    else if (!strcmp(p.we_wordv[0], "/numPatientDischarges"))
    {
        flag = numFunction(wp, buffer, &p, SOCK_BUFFER, &str);
    }
    else
    {
        flag = 1;
    }

    switch (flag)
    {
    case -1:
        if (encode(socket, "Error 500: Internal Server Error", SOCK_BUFFER) == -1)
        {
            fprintf(stderr, "encode() failed\n");
            return -1;
        }
        return -1;
    case 0:
        if (encode(socket, str, SOCK_BUFFER) == -1)
        {
            fprintf(stderr, "encode() failed\n");
            return -1;
        }

        free(str);
        break;
    case 1:
        if (encode(socket, "Error 400: Bad Request", SOCK_BUFFER) == -1)
        {
            fprintf(stderr, "encode() failed\n");
            return -1;
        }
        break;
    case 2:
        if (encode(socket, "Error 501: Country Not Registered", SOCK_BUFFER) == -1)
        {
            fprintf(stderr, "encode() failed\n");
            return -1;
        }
        break;
    }

    free(buffer);
    wordfree(&p);

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
        int port;
        char *ip = NULL;
        SA_IN waddr;
        socklen_t len = sizeof(SA_IN);

        if (getsockname(socket, (SA *)&waddr, &len) == -1)
        {
            perror("getsockname() failed");
            return -1;
        }

        if ((ip = strdup(inet_ntoa(waddr.sin_addr))) == NULL)
        {
            perror("strdup or inet_ntoa faile");
            return -1;
        }

        free(buffer);
        if ((buffer = decode(socket, SOCK_BUFFER)) == NULL)
        {
            fprintf(stderr, "decode() failed\n");
            return -1;
        }
        port = strtol(buffer, NULL, 10);

        free(buffer);
        printf("worker --> ip:%s, port: %d\n", ip, port);

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
            if ((wp = add_worker_country(wp, ip, port, buffer)) == NULL)
            {
                fprintf(stderr, "add_worker_country() failed\n");
                return -1;
            }
            pthread_mutex_unlock(&wp_mtx);

            free(buffer);
        }

        free(ip);
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

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1)
    {
        fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");
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