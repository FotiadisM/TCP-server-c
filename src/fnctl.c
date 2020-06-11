#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/fnctl.h"

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

        free(wp->ip);
        clear_stringNode(wp->countries);
        free(wp);

        wp = wp->next;
    }
}

int add_worker_country(worker_ptr wp, const char *ip, const int port, const char *country)
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
                    return -1;
                }
                return 0;
            }
        }

        tmp_wp = tmp_wp->next;
    }
}