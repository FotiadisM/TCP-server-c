#include <stdio.h>
#include <stdlib.h>

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