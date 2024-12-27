#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>

#include "medialib.h"
#include "mytbf.h"
#include "server_conf.h"
#include "../include/proto.h"

#define PATHSIZE 1024
#define LINESIZE 1024
#define MP3_BITRATE 320 * 1024 // 采样率

struct channel_context_st
{
    chnid_t chnid;          // 频道ID
    char* desc;             // 频道描述
    glob_t mp3glob;         // 解析到的目录下所有MP3文件
    int pos;                // 当前播放第n个MP3
    int fd;                 // 当前播放的MP3的文件描述符
    off_t offset;           // 当前播放的MP3的偏移量
    mytbf_t *tbf;           // 流量控制
};

static struct channel_context_st channels[CHN_NUM+1];

static struct channel_context_st* path2entry(const char* path)
{
    char pathstr[PATHSIZE];             // 存路径
    char linebuf[LINESIZE];             // 存Decription
    FILE *fp;

    struct channel_context_st *context_ptr;
    static chnid_t curr_id = 0;

    strncpy(pathstr,path,PATHSIZE-1);
    strncat(pathstr,"/desc.txt",PATHSIZE-1);

    fp = fopen(pathstr,"r");
    if(fp == NULL)
    {
        syslog(LOG_INFO, "open %s failed:%s", pathstr, strerror(errno));
        free(context_ptr);
        return NULL;
    }

    if(fgets(linebuf, LINESIZE, fp) == NULL)
    {   
        syslog(LOG_INFO, "read desc.txt failed");
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    context_ptr = malloc(sizeof(*context_ptr));
    if(context_ptr == NULL)
    {
        syslog(LOG_INFO, "malloc failed");
        free(context_ptr);
        return NULL;
    }

    context_ptr->desc = strdup(linebuf);
    strncpy(pathstr,path,PATHSIZE-1);
    strncat(pathstr,"/*.mp3",PATHSIZE-1);

    if(glob(pathstr,0,NULL,&context_ptr->mp3glob) < 0)
    {
        syslog(LOG_INFO, "glob %s failed:%s", pathstr, strerror(errno));
        free(context_ptr);
        return NULL;
    }
    
    
    context_ptr->tbf = mytbf_init(MP3_BITRATE/8,MP3_BITRATE/8*5);
    if(context_ptr->tbf == NULL)
    {
        syslog(LOG_INFO, "mytbf_init failed");
        free(context_ptr);
        return NULL;    
    }

    context_ptr->pos = 0;
    context_ptr->offset = 0;
    context_ptr->fd = open(context_ptr->mp3glob.gl_pathv[context_ptr->pos], O_RDONLY);

    if(context_ptr->fd < 0)
    {
        syslog(LOG_INFO, "open %s failed:%s", context_ptr->mp3glob.gl_pathv[context_ptr->pos], strerror(errno));
        free(context_ptr);
        return NULL;    
    }

    context_ptr->chnid = curr_id;
    curr_id++;    

    return context_ptr;

}


int midlib_getchnlist(struct media_list_t **chnlist, int *count)
{
    int i,num = 0;
    char path[1024];
    glob_t globres;
    struct media_list_t *ptr_list;
    struct channel_context_st *ptr_cxt;


    snprintf(path, sizeof(path),"%s",server_conf.dir);
    

    for(int i=0; i<CHN_NUM+1; i++)
        channels[i].chnid = -1;
    if(glob(path,0,NULL,&globres) < 0)
        return -errno; 



    ptr_list = malloc(sizeof(struct media_list_t) * globres.gl_pathc);
    if(ptr_list == NULL)
    {
        syslog(LOG_ERR, "malloc failed");
        exit(1);    
    }

    for(i = 0; i < globres.gl_pathc; i++)
    {
        ptr_cxt = path2entry(globres.gl_pathv[i]);
        if(ptr_cxt != NULL)
        {
            syslog(LOG_INFO, "chnid:%d desc:%s", ptr_cxt->chnid,ptr_cxt->desc);
            ptr_list[i].chnid = ptr_cxt->chnid;
            ptr_list[i].desc = strdup(ptr_cxt->desc);
            memcpy(&channels[i],ptr_cxt,sizeof(*ptr_cxt));
        }
        num++;
    }

    *count = num;
    *chnlist = realloc(ptr_list,sizeof(struct media_list_t) * num);
    
    return 0;

}

void midlib_freechnlist(struct media_list_t **chnlist)
{
    free(*chnlist);
}

static void read_next(chnid_t chnid)
{
    close(channels[chnid].fd);

    if(channels[chnid].pos >= channels[chnid].mp3glob.gl_pathc)
        channels[chnid].pos = 0;

    channels[chnid].pos++;
    channels[chnid].fd = open(channels[chnid].mp3glob.gl_pathv[channels[chnid].pos], O_RDONLY);
    
    /*如果open失败，下面的While会再次调用该函数*/
    
    return;
}



ssize_t midlib_readchn(chnid_t chnid, void *buf, ssize_t count)
{
    int tbf_size;
    ssize_t ret;
    
    tbf_size = mytbf_fetchtoken(channels[chnid].tbf, count);
    
    // printf("chnid:%d tbf_size:%d\n", chnid, tbf_size);

    while(1)
    {
        ret = pread(channels[chnid].fd, buf, tbf_size, channels[chnid].offset);
        if (ret < 0)
        {
            syslog(LOG_INFO, "pread failed:%s", strerror(errno));
            read_next(chnid);
        }
        else if (ret == 0)
        {
            syslog(LOG_INFO, "read %s end", channels[chnid].mp3glob.gl_pathv[channels[chnid].pos]);
            read_next(chnid);
        }
        else
            break;
    }
    
    if(tbf_size - ret > 0)
        mytbf_returntoken(channels[chnid].tbf, tbf_size - ret);

    channels[chnid].offset += ret;
    return ret;
}
