#ifndef FNCTL_h
#define FNCTL_H

typedef struct queue_node
{
    int socket;
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

queue_ptr queue_init(const int bufferSize);
void queue_close(queue_ptr q);

int enqueue(queue_ptr q, const int value);
int *dequeue(queue_ptr q);

#endif