#ifndef MYTBF_H__
#define MYTBF_H__


#define MYTBF_MAX 1024
typedef void mytbf_t;


mytbf_t* mytbf_init(int cps,int brust);

int mytbf_fetchtoken(mytbf_t* tbf,int num);

int mytbf_returntoken(mytbf_t* tbf,int num);

int mytbf_destroy(mytbf_t* tbf);




#endif