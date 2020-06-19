#ifndef FNCTL_h
#define FNCTL_H

#include <wordexp.h>

#include "../include/list.h"

typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;

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

worker_ptr getWorker(const worker_ptr wp, const char *str);
int searchPatientRecord(const worker_ptr wp, const char *str, const wordexp_t *p, const size_t bufferSize, char **answ);
int topk_AgeRanges(const worker_ptr wp, const char *str, const wordexp_t *p, const size_t bufferSize, char **answ);
int numFunction(const worker_ptr wp, const char *str, const wordexp_t *p, const size_t bufferSize, char **answ);
int diseaseFrequency(const worker_ptr wp, const char *str, const wordexp_t *p, const size_t bufferSize, char **answ);

int send_to(const char *ip, const int port, const char *str, const size_t bufferSize);
char *combine(string_nodePtr snode);

#endif