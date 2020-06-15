#define _XOPEN_SOURCE 700

#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;

static void *thread_run(void *val);

int servPort;
pthread_t *pool;
char *servIP = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

int main(int argc, char *argv[])
{
    size_t len = 0;
    FILE *filePtr = NULL;
    int numThreads = 0, err = 0;
    char *queryFile = NULL, **query_arr = NULL;

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
            if ((servPort = atoi(argv[++i])) == 0)
            {
                fprintf(stderr, "Invalid value: %s\n", argv[i]);
                return -1;
            }
        }
        else if (!strcmp("-sip", argv[i]))
        {
            i++;
            if ((servIP = strdup(argv[i])) == 0)
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

    if ((query_arr = malloc(numThreads * sizeof(char *))) == NULL)
    {
        perror("malloc");
        return -1;
    }

    for (int i = 0; i < numThreads; i++)
    {
        query_arr[i] = NULL;
    }

    if ((filePtr = fopen(queryFile, "r")) == NULL)
    {
        perror("fopen");
        fprintf(stdout, "Unable to open file: %s", queryFile);
        return -1;
    }

    while (1)
    {
        int count = 0;

        for (int i = 0; i < numThreads; i++)
        {
            if (getline(&query_arr[i], &len, filePtr) == -1)
            {
                break;
            }
            else
            {
                count++;
                if ((err = pthread_create(&pool[i], NULL, thread_run, (void *)query_arr[i])) != 0)
                {
                    fprintf(stderr, "pthread_create() failed: %s\n", strerror(err));
                    return -1;
                }
            }
        }

        if (count == 0)
        {
            break;
        }

        sleep(1); // wait for threads
        pthread_cond_broadcast(&condition_var);

        for (int i = 0; i < count; i++)
        {
            pthread_join(pool[i], NULL);
        }

        printf("\n");
    }

    for (int i = 0; i < numThreads; i++)
    {
        free(query_arr[i]);
    }

    fclose(filePtr);

    free(pool);
    free(queryFile);
    free(servIP);
    free(query_arr);

    return 0;
}

static void *thread_run(void *val)
{
    int sockfd;
    SA_IN servaddr;
    char *query = NULL;
    char buffer[100] = {0};

    query = (char *)val;

    printf("que %s", query);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket() failed");
        return NULL;
    }

    memset(&servaddr, 0, sizeof(SA_IN));

    pthread_mutex_lock(&mutex);
    servaddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, servIP, &servaddr.sin_addr) != 1)
    {
        perror("inet_pton");
        return NULL;
    }
    servaddr.sin_port = htons(servPort);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&condition_var, &mutex);
    pthread_mutex_unlock(&mutex);

    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("connect() failed");
        return NULL;
    }

    write(sockfd, "i am the client", 100);
    read(sockfd, buffer, 100);
    printf("buf: %s\n", buffer);

    close(sockfd);

    return NULL;
}