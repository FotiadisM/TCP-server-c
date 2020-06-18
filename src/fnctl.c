#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/fnctl.h"
#include "../include/pipes.h"

queue_ptr queue_init(const int bufferSize)
{
    queue_ptr q = NULL;

    if ((q = malloc(sizeof(queue))) == NULL)
    {
        fprintf(stderr, "malloc failed\n");
        return NULL;
    }

    q->len = 0;
    q->max = bufferSize;
    q->head = NULL;
    q->tail = NULL;

    return q;
}

void queue_close(queue_ptr q)
{
    queue_node_ptr node = q->head;

    while (q->head != NULL)
    {
        node = q->head;
        free(node);
        q->head = q->head->next;
    }

    free(q);
}

int enqueue(queue_ptr q, const int value, const char port)
{
    queue_node_ptr node = NULL;

    if ((node = malloc(sizeof(queue_node))) == NULL)
    {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    node->socket = value;
    node->port = port;
    node->next = NULL;

    if (q->len < q->max)
    {
        if (q->tail == NULL)
        {
            q->head = node;
            q->tail = node;
            q->len++;

            return 0;
        }

        q->tail->next = node;
        q->tail = node;
        q->len++;

        return 0;
    }

    return -2;
}

queue_node_ptr dequeue(queue_ptr q)
{
    queue_node_ptr node = q->head;

    if (node == NULL)
    {
        return NULL;
    }

    q->head = q->head->next;

    if (q->head == NULL)
    {
        q->tail = NULL;
    }

    q->len--;

    return node;
}

worker_ptr add_worker(worker_ptr wp, const char *ip, const int port)
{
    worker_ptr new_wp = NULL;

    if ((new_wp = malloc(sizeof(worker_i))) == NULL)
    {
        perror("malloc");
        return NULL;
    }

    if ((new_wp->ip = strdup(ip)) == NULL)
    {
        perror("strdup");
        return NULL;
    }
    new_wp->port = port;
    new_wp->countries = NULL;
    new_wp->next = wp;

    return new_wp;
}

void worker_close(worker_ptr wp)
{
    worker_ptr tmp_wp = wp;

    while (wp != NULL)
    {
        tmp_wp = wp;
        wp = wp->next;

        free(tmp_wp->ip);
        clear_stringNode(tmp_wp->countries);
        free(tmp_wp);
    }
}

worker_ptr add_worker_country(worker_ptr wp, const char *ip, const int port, const char *country)
{
    worker_ptr tmp_wp = wp;

    while (tmp_wp != NULL)
    {
        if (tmp_wp->port == port)
        {
            if (!strcmp(tmp_wp->ip, ip))
            {
                if ((tmp_wp->countries = add_stringNode(tmp_wp->countries, country)) == NULL)
                {
                    fprintf(stderr, "add_stringNode() failed");
                    return NULL;
                }
                return wp;
            }
        }

        tmp_wp = tmp_wp->next;
    }

    if ((tmp_wp = add_worker(wp, ip, port)) == NULL)
    {
        fprintf(stderr, "add_worker() failed\n");
        return NULL;
    }
    if ((tmp_wp->countries = add_stringNode(tmp_wp->countries, country)) == NULL)
    {
        fprintf(stderr, "add_stringNode failed\n");
        return NULL;
    }

    return tmp_wp;
}

worker_ptr getWorker(const worker_ptr wp, const char *str)
{
    worker_ptr worker = wp;
    string_nodePtr node = NULL;

    if (str != NULL)
    {
        while (worker != NULL)
        {
            node = worker->countries;

            while (node != NULL)
            {
                if (!strcmp(node->str, str))
                {
                    return worker;
                }
                node = node->next;
            }
            worker = worker->next;
        }
    }

    return NULL;
}

int diseaseFrequency(const worker_ptr wp, const char *str, const wordexp_t *p, const size_t bufferSize, char **answ)
{
    fd_set fdset;
    char *tmp = NULL;
    worker_ptr worker = NULL;
    int socketfd = 0, count = 0;

    if (p->we_wordc == 4 || p->we_wordc == 5)
    {
        if ((worker = getWorker(wp, p->we_wordv[4])) == NULL)
        {
            worker = wp;
            while (worker != NULL)
            {
                if ((socketfd = send_to(worker->ip, worker->port, str, bufferSize)) == -1)
                {
                    fprintf(stderr, "send_to() failed");
                    return -1;
                }

                // FD_ZERO(&fdset);
                // FD_SET(socketfd, &fdset);

                // if (pselect(socketfd + 1, &fdset, NULL, NULL, NULL, NULL) == -1)
                // {
                //     perror("pselect");
                // }

                // sleep(1);
                tmp = malloc(1000);
                read(socketfd, tmp, 1000);
                // if ((tmp = decode(socketfd, bufferSize)) == NULL)
                // {
                //     fprintf(stderr, "decode() failed");
                //     return -1;
                // }
                printf("res: %s\n", tmp);

                count += strtol(tmp, NULL, 10);

                free(tmp);
                close(socketfd);

                worker = worker->next;
            }

            *answ = malloc(100);
            sprintf(*answ, "%d", count);
            printf("multiple answ: %s\n", *answ);
        }
        else
        {
            if ((socketfd = send_to(worker->ip, worker->port, str, bufferSize)) == -1)
            {
                fprintf(stderr, "send_to() failed");
                return -1;
            }

            FD_ZERO(&fdset);
            FD_SET(socketfd, &fdset);

            if (pselect(socketfd + 1, &fdset, NULL, NULL, NULL, NULL) == -1)
            {
                perror("pselect");
            }

            if ((*answ = decode(socketfd, bufferSize)) == NULL)
            {
                fprintf(stderr, "decode() failed");
                return -1;
            }
            printf("single answ: %s\n", *answ);

            close(socketfd);
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int numFunction(const worker_ptr wp, const char *str, const wordexp_t *p, const size_t bufferSize, char **answ)
{
    int socketfd = 0;
    char *tmp = NULL;
    worker_ptr worker = NULL;
    string_nodePtr snode = NULL;

    if (p->we_wordc == 4 || p->we_wordc == 5)
    {
        if (p->we_wordc == 5)
        {
            if ((worker = getWorker(wp, p->we_wordv[4])) == NULL)
            {
                return 2;
            }
            else
            {
                if ((socketfd = send_to(worker->ip, worker->port, str, bufferSize)) == -1)
                {
                    fprintf(stderr, "send_to() failed");
                    return -1;
                }

                if ((*answ = decode(socketfd, bufferSize)) == NULL)
                {
                    fprintf(stderr, "decode() failed");
                    return -1;
                }

                close(socketfd);
            }
        }
        else
        {
            worker = wp;

            while (worker != NULL)
            {
                if ((socketfd = send_to(worker->ip, worker->port, str, bufferSize)) == -1)
                {
                    fprintf(stderr, "send_to() failed");
                    return -1;
                }

                while (1)
                {
                    if ((tmp = decode(socketfd, bufferSize)) == NULL)
                    {
                        fprintf(stderr, "decode() failed");
                        return -1;
                    }

                    if (!strcmp(tmp, "OK"))
                    {
                        free(tmp);
                        break;
                    }
                    add_stringNode(snode, tmp);

                    free(tmp);
                }

                close(socketfd);

                worker = worker->next;
            }

            clear_stringNode(snode);
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int topk_AgeRanges(const worker_ptr wp, const char *str, const wordexp_t *p, const size_t bufferSize, char **answ)
{
    int socketfd = 0;
    worker_ptr worker = NULL;
    string_nodePtr snode = NULL;

    if (p->we_wordc == 6)
    {
        if ((worker = getWorker(wp, p->we_wordv[2])) == NULL)
        {
            return 2;
        }
        else
        {
            char *tmp = NULL;

            if ((socketfd = send_to(worker->ip, worker->port, str, bufferSize)) == -1)
            {
                fprintf(stderr, "send_to() failed");
                return -1;
            }

            while (1)
            {
                if ((tmp = decode(socketfd, bufferSize)) == NULL)
                {
                    fprintf(stderr, "decode() failed");
                    return -1;
                }

                if (!strcmp(tmp, "OK"))
                {
                    free(tmp);
                    break;
                }

                add_stringNode(snode, tmp);
            }

            close(socketfd);
        }
    }
    else
    {
        return 1;
    }

    clear_stringNode(snode);

    return 0;
}

int searchPatientRecord(const worker_ptr wp, const char *str, const wordexp_t *p, const size_t bufferSize, char **answ)
{
    int socketfd = 0;
    char *tmp = NULL;
    worker_ptr worker = wp;
    string_nodePtr snode = NULL;

    if (p->we_wordc == 2)
    {
        while (worker != NULL)
        {
            if ((socketfd = send_to(worker->ip, worker->port, str, bufferSize)) == -1)
            {
                fprintf(stderr, "send_to() failed");
                return -1;
            }

            if ((tmp = decode(socketfd, bufferSize)) == NULL)
            {
                fprintf(stderr, "decode() failed");
                return -1;
            }

            if (strcmp(tmp, "OK"))
            {
                add_stringNode(snode, tmp);
            }

            free(tmp);
            close(socketfd);

            worker = worker->next;
        }
    }
    else
    {
        return 1;
    }

    clear_stringNode(snode);

    return 0;
}

int send_to(const char *ip, const int port, const char *str, const size_t bufferSize)
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
    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) != 1)
    {
        perror("inet_pton");
        return -1;
    }
    servaddr.sin_port = htons(port);

    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("connect() failed");
        return -1;
    }

    if (encode(sockfd, str, bufferSize) == -1)
    {
        fprintf(stderr, "encode() failed");
        return -1;
    }

    return sockfd;
}