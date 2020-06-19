#ifndef MY_NETWORK_H
#define MY_NETWORK_H

int my_sent(const int socketfd, const char *msg);
char *my_receive(const int socket);

#endif