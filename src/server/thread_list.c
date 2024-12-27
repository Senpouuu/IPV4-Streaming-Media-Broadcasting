#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "thread_list.h"
#include "medialib.h"
#include "../include/proto.h"
#include "server.h"


#define LISTCHNID 0 


static pthread_t tid_list;
static int nr_list_ent;
static struct media_list_t* media_list;


static void *thr_list(void *ptr)
{
    int size;
    int i,ret;
    int totalsize;
    struct msg_list_entry_st* msg_entp;
    struct chn_list_st* msg_ent_listp;

    totalsize = sizeof(chnid_t);

    for(i=0; i<nr_list_ent; i++)
        totalsize += sizeof(struct msg_list_entry_st) + strlen(media_list[i].desc);
    
    msg_ent_listp = malloc(totalsize);
    printf("size of msg_ent_listp: %d\n",totalsize);

    if(msg_entp == NULL)
    {
        syslog(LOG_ERR, "malloc failed for entryp");
        exit(1);
    }

    msg_ent_listp->chn_id = LISTCHNID;
    msg_entp = msg_ent_listp->msg_list;
    


    // 填msg_list中的每一个entry
    for(i=0; i<nr_list_ent; i++)
    {
        size = sizeof(struct msg_list_entry_st) + strlen(media_list[i].desc);
        msg_entp->chn_id = media_list[i].chnid;
        msg_entp->len = htons(size);
        strcpy(msg_entp->desc, media_list[i].desc);
        // 移动指针到下一个entry
        msg_entp = (void*)((char*)msg_entp + size); 

    }

    // int s = 25;
    // msg_entp = msg_ent_listp->msg_list;
    // for(i=0; i<nr_list_ent; i++)
    // {
    //     printf("written entry: %d %d %s\n",msg_entp->chn_id,ntohs(msg_entp->len),msg_entp->desc);
    //     // 移动指针到下一个entry
    //     msg_entp = (void*)((char*)msg_entp + s); 
    // }

    while (1)
    {
        ret = sendto(sfd, msg_ent_listp, totalsize, 0, (void *)&sndaddr, sizeof(sndaddr));
        if (ret < 0)
            syslog(LOG_DEBUG, "sendto failed");
        else
        {
            printf("Channel %d: %s to client\n", msg_ent_listp->chn_id, msg_ent_listp->msg_list->desc);
        }

        sleep(1);
    }
}

int thr_list_create(struct media_list_t *list, int num)
{
    int err;

    media_list = list;
    nr_list_ent = num;

    err = pthread_create(&tid_list, NULL, thr_list, NULL);
    if (err)
    {
        syslog(LOG_ERR, "pthread_create failed for list thread");
        return -1;
    }

    return 0;
}

int thr_list_destroy()
{
    free(media_list);
    media_list = NULL;
    nr_list_ent = 0;
    pthread_cancel(tid_list);
    pthread_join(tid_list, NULL);
    return 0;
}
