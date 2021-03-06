#ifndef STATS_H
#define STATS_H

typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;

typedef struct ageInfo
{
    int ag1;
    int ag2;
    int ag3;
    int ag4;
} ageInfo;

typedef ageInfo *ageInfoPtr;

typedef struct stats
{
    char *disease;
    ageInfoPtr ag;
    struct stats *next;
} stats;

typedef stats *statsPtr;

int Worker_sendStatistics(const statsPtr st, const int serverPort, const char *serverIP, const char *country, const char *file);
ageInfoPtr ageInfo_Init();
void ageInfo_add(ageInfoPtr ag, const char *age_str);
statsPtr stats_Init();
statsPtr stats_add(statsPtr st, const char *disease, const char *age_str);
void stats_close(statsPtr st);

#endif