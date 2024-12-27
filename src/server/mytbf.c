#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>
#include "mytbf.h"

struct mytbf_st
{
	int cps;
	int burst;	
	int token;
	int pos;
    pthread_mutex_t tbf_mux;
    pthread_cond_t tbf_cond;
};

static struct mytbf_st* job[MYTBF_MAX];
static pthread_mutex_t job_mux = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t job_once = PTHREAD_ONCE_INIT;
static pthread_t alrm_tid;
static void moudle_unload();

static void *alrm_func(void *args)
{
    int i;
    while (1)
    {
        pthread_mutex_lock(&job_mux);
        for (i = 0; i < MYTBF_MAX; i++)
        {
            if (job[i] == NULL)
                continue;

            if (job[i]->token + job[i]->cps <= job[i]->burst)
                job[i]->token += job[i]->cps;
            else
                job[i]->token = job[i]->burst;

            pthread_cond_signal(&job[i]->tbf_cond);
        }
        pthread_mutex_unlock(&job_mux);
        sleep(1);
    }
}

static int find_pos()
{
    for (int i = 0; i < MYTBF_MAX; i++)
        if (job[i] == NULL)
            return i;
    return -1;
}

static void moudle_load()
{
    alrm_tid = pthread_create(&alrm_tid, NULL, alrm_func, NULL);
    atexit(moudle_unload);
}

static void moudle_unload()
{
    pthread_mutex_destroy(&job_mux);
    pthread_exit(&alrm_tid);
    pthread_join(alrm_tid, NULL);

    for (int i = 0; i < MYTBF_MAX; i++)
        free(job[i]);
}

mytbf_t *mytbf_init(int cps, int brust)
{

    pthread_once(&job_once, moudle_load);

    struct mytbf_st *tbf;
    int pos;
    tbf = malloc(sizeof(struct mytbf_st));
    if (tbf == NULL)
        return NULL;

    tbf->cps = cps;
    tbf->burst = brust;
    tbf->token = brust;
    pthread_mutex_init(&tbf->tbf_mux, NULL);
    pthread_cond_init(&tbf->tbf_cond, NULL);

    pthread_mutex_lock(&job_mux);
    pos = find_pos();
    if (pos < 0)
    {
        pthread_mutex_unlock(&job_mux);
        free(tbf);
        return NULL;
    }

    job[pos] = tbf;
    pthread_mutex_unlock(&job_mux);

    return tbf;
}

int mytbf_fetchtoken(mytbf_t *tbf, int num)
{
    struct mytbf_st *ptr = (struct mytbf_st *)tbf;
    int fetch_num = num;

    pthread_mutex_lock(&ptr->tbf_mux);
    // 等待令牌
    while (ptr->token <= 0)
        pthread_cond_wait(&ptr->tbf_cond, &ptr->tbf_mux);

    if (ptr->token < fetch_num)
    {
        fetch_num = ptr->token;
        ptr->token = 0;
    }

    if (ptr->token >= fetch_num)
        ptr->token -= fetch_num;
    pthread_mutex_unlock(&ptr->tbf_mux);

    return fetch_num;
}

int mytbf_returntoken(mytbf_t *tbf, int num)
{
    struct mytbf_st *ptr = (struct mytbf_st *)tbf;
    int return_num = num;

    pthread_mutex_lock(&ptr->tbf_mux);

    if (ptr->token + return_num >= ptr->burst)
    {
        return_num = ptr->burst - ptr->token;
        ptr->token = ptr->burst;
    }
    else
        ptr->token += return_num;

    pthread_cond_signal(&ptr->tbf_cond);

    pthread_mutex_unlock(&ptr->tbf_mux);

    return return_num;
}

int mytbf_destroy(mytbf_t *tbf)
{
    struct mytbf_st *ptr = (struct mytbf_st *)tbf;

    pthread_mutex_lock(&job_mux);
    job[ptr->pos] = NULL;
    pthread_mutex_unlock(&job_mux);

    pthread_mutex_destroy(&ptr->tbf_mux);
    pthread_cond_destroy(&ptr->tbf_cond);

    free(tbf);
    return 0;
}