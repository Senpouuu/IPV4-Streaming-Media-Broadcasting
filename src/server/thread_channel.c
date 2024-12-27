#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../include/proto.h"
#include "thread_channel.h"
#include "server.h"

struct thr_channel_t 
{
    pthread_t tid;
    chnid_t chnid;
};

static struct thr_channel_t thr_channel_list[CHN_NUM];
static int pos_next = 0;


static void* thr_channel_sender(void* arg)
{
    struct chn_msg_st* send_buf;
    int len;
    
    send_buf = malloc(MSG_MAX_DATA);
    if(send_buf == NULL)
    {
        syslog(LOG_ERR, "malloc error");
        exit(1);
    }
    while(1)
    {
        send_buf->chn_id = ((struct media_list_t *)arg)->chnid;
        len = midlib_readchn(send_buf->chn_id, send_buf->data, 320*1024/8); // 320kbit/s
        // printf("send data len %d\n", len);
        if (sendto(sfd, send_buf, len + sizeof(chnid_t), 0, (void *)&sndaddr, sizeof(sndaddr)) < 0)
        {
            syslog(LOG_ERR, "sendto error");
            exit(1);
        }
        sched_yield();
    }
    pthread_exit(NULL);
}

int thr_channel_create(struct media_list_t *media_list)
{
    int err = pthread_create(&thr_channel_list[pos_next].tid, NULL, thr_channel_sender, (void *)media_list);
    if (err)
    {
        syslog(LOG_ERR, "pthread_create error: %d", err);
        exit(1);
    }

    // printf("create channel %d\n,pos_next %d", media_list->chnid, pos_next);
    thr_channel_list[pos_next].chnid = media_list->chnid;

    pos_next++;

    return 0;
}


int thr_channel_destroy(struct media_list_t *media_list)
{
    for(int i = 0; i < CHN_NUM; i++)
    {
        if(thr_channel_list[i].chnid == media_list->chnid)
        {
            if(pthread_cancel(thr_channel_list[i].tid) < 0)
            {
                syslog(LOG_ERR, "pthread_cancel error");
                exit(1);
            }
            
            pthread_join(thr_channel_list[i].tid, NULL);
            thr_channel_list[i].chnid = -1;
            return 0;
        }
    }
}

int thr_channel_destroy_all()
{
    for(int i = 0; i < CHN_NUM; i++)
    {
        if (pthread_cancel(thr_channel_list[i].tid) < 0)
        {
            syslog(LOG_ERR, "pthread_cancel error");
            exit(1);
        }
        pthread_join(thr_channel_list[i].tid, NULL);
        thr_channel_list[i].chnid = -1;
    }
    return 0;
}