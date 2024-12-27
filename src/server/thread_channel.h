#ifndef THREAD_CHANNEL_H__
#define THREAD_CHANNEL_H__

#include "medialib.h"


int thr_channel_create(struct media_list_t* media_list);
int thr_channel_destroy(struct media_list_t* media_list);
int thr_channel_destroy_all();



#endif