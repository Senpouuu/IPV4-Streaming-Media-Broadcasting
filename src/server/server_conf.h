#ifndef _SERVER_CONF_H_
#define _SERVER_CONF_H_

#define DEAFULT_MEDIADIR "/home/gulu/桌面/medialib"
#define DEAFULT_IFNAME "lo"

enum run_mode
{
    RUN_DEAMON = 0,
    RUN_FRONT
};

struct server_default_cmd
{
    char *mtgroup;
    char *port;
    char *dir;
    char runmode;
    char *ifname;
};

extern struct server_default_cmd server_conf;


#endif