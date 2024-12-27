#ifndef MEDIALIB_H__
#define MEDIALIB_H__

#include <stdlib.h>

#include "../include/site_type.h"

struct media_list_t 
{
    chnid_t chnid;
    char* desc;
};

int midlib_getchnlist(struct media_list_t** chnlist, int* count);

void midlib_freechnlist(struct media_list_t** chnlist);

ssize_t midlib_readchn(chnid_t chnid, void* buf, ssize_t count);


#endif