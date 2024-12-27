#ifndef CLIENT_H__
#define CLIENT_H__

#define DECODER_CMD "/usr/bin/mpg123 -   > /dev/null"

/*
*    -M --mtgroup    指定多播组IP
*    -P --port       指定端口
*    -S --server     指定服务器IP
*    -D --decoder    指定解码器
*    -h --help       显示帮助
*/

struct client_default_cmd
{
    char *mtgroup;
    char *port;
    char *server;
    char *decoder;
};


extern struct client_default_cmd clietn_conf;



#endif 