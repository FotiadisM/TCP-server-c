#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int Pipe_Init(const char *path, const int pid, const int flags)
{
    int fd = 0;
    char str[12] = {'\0'};
    char path_f[24] = {'\0'};

    if (sprintf(str, "%d", pid) == -1)
    {
        perror("sprintf");
        return -1;
    }

    if (strcpy(path_f, path) == NULL)
    {
        perror("strcpy");
        return -1;
    }

    strcat(path_f, str);

    if (mkfifo(path_f, 0640) == -1)
    {
        if (errno != EEXIST)
        {
            perror("mkfifo");
            return -1;
        }
    }

    if ((fd = open(path_f, flags)) == -1)
    {
        perror("open");
        return -1;
    }

    return fd;
}

int encode(const int fd, const char *buffer, const size_t bufferSize)
{
    char str[24] = {0};
    size_t str_size = sizeof(str);
    size_t buffer_len = strlen(buffer) + 1;

    sprintf(str, "%zu", buffer_len);

    for (size_t i = 0; i < str_size / bufferSize; i++)
    {
        if (write(fd, str + (i * bufferSize), bufferSize) == -1)
        {
            perror("write");
            return -1;
        }
    }

    if (str_size % bufferSize)
    {
        if (write(fd, str + (str_size - str_size % bufferSize), str_size % bufferSize) == -1)
        {
            perror("write");
            return -1;
        }
    }

    for (size_t i = 0; i < buffer_len / bufferSize; i++)
    {
        if (write(fd, buffer + (i * bufferSize), bufferSize) == -1)
        {
            perror("write");
            return -1;
        }
    }

    if (buffer_len % bufferSize)
    {
        if (write(fd, buffer + (buffer_len - buffer_len % bufferSize), buffer_len % bufferSize) == -1)
        {
            perror("write");
            return -1;
        }
    }

    return 0;
}

char *decode(const int fd, const size_t bufferSize)
{
    char *r_buffer = NULL, *buffer = NULL;
    char str[24] = {0};
    size_t str_size = sizeof(str);
    size_t r_buffer_size = 0;

    if ((buffer = malloc(bufferSize)) == NULL)
    {
        perror("malloc");
        return NULL;
    }

    for (size_t i = 0; i < str_size / bufferSize; i++)
    {
        if (read(fd, buffer, bufferSize) == -1)
        {
            perror("write");
            return NULL;
        }
        memcpy(str + (i * bufferSize), buffer, bufferSize);
    }

    if (str_size % bufferSize)
    {
        if (read(fd, buffer, str_size % bufferSize) == -1)
        {
            perror("write");
            return NULL;
        }
        memcpy(str + (str_size / bufferSize) * bufferSize, buffer, str_size - (str_size / bufferSize) * bufferSize);
    }

    r_buffer_size = (size_t)strtol(str, NULL, 10);

    if ((r_buffer = malloc(r_buffer_size)) == NULL)
    {
        perror("malloc");
        return NULL;
    }

    for (size_t i = 0; i < r_buffer_size / bufferSize; i++)
    {
        if (read(fd, buffer, bufferSize) == -1)
        {
            perror("write");
            return NULL;
        }
        memcpy(r_buffer + (i * bufferSize), buffer, bufferSize);
    }

    if (r_buffer_size % bufferSize)
    {
        if (read(fd, buffer, r_buffer_size % bufferSize) == -1)
        {
            perror("write");
            return NULL;
        }
        memcpy(r_buffer + (r_buffer_size / bufferSize) * bufferSize, buffer, r_buffer_size - (r_buffer_size / bufferSize) * bufferSize);
    }

    free(buffer);

    return r_buffer;
}