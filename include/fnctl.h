#ifndef FNCTL_h
#define FNCTL_H

#include "../include/list.h"

typedef struct queue_node
{
    int socket;
    char port;
    struct queue_node *next;
} queue_node;

typedef queue_node *queue_node_ptr;

typedef struct queue
{
    int len;
    int max;
    queue_node_ptr head;
    queue_node_ptr tail;
} queue;

typedef queue *queue_ptr;

typedef struct worker_i
{
    char *ip;
    int port;
    string_nodePtr countries;
    struct worker_i *next;
} worker_i;

typedef worker_i *worker_ptr;

queue_ptr queue_init(const int bufferSize);
void queue_close(queue_ptr q);

int enqueue(queue_ptr q, const int value, const char port);
queue_node_ptr dequeue(queue_ptr q);

worker_ptr add_worker(worker_ptr wp, const char *ip, const int port);
void worker_close(worker_ptr wp);

worker_ptr add_worker_country(worker_ptr wp, const char *ip, const int port, const char *country);

#endif