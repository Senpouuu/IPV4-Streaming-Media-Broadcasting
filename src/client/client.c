#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <net/if.h>

#include "client.h"
#include "../include/proto.h"

#define BUFSIZE 320*1024/8*3

#define MGIP_SIZE 1000
#define MGPORT_SIZE 1000
#define SERVERIP_SIZE 1000
#define NAMDEC_SIZE 1000

/*
*    -M --mtgroup    指定多播组IP
*    -P --port       指定端口
*    -S --server     指定服务器IP
*    -D --decoder    指定解码器
*    -h --help       显示帮助
*/

struct client_default_cmd client_conf= {
    .mtgroup = DEFAULT_MGIP,
    .port = DEFAULT_MGPORT,
    .server = "0.0.0.0",
    .decoder = "/usr/bin/mpg123" 
};


ssize_t writen(int fd, const void * buf, size_t count)
{
    int ret,pos = 0;
    size_t len = count;
    while(len > 0)
    {
        ret = write(fd, (char *)buf + pos, len);
        // printf("writen: ret = %d, pos = %d, len = %ld\n", ret, pos, len);

        if(ret == 0)
            return 0;

        if (ret < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            perror("write");
            return -1;
        }
        len -= ret;
        pos += ret;
    }

}

int main(int argc, char *argv[])
{
    int c;
    static int opt_index = 0;
    static struct option long_options[] = {
        {"mtgroup", required_argument, 0, 'M'},
        {"port", required_argument, 0, 'P'},
        {"server", required_argument, 0, 'S'},
        {"decoder", required_argument, 0, 'D'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    while (1)
    {
        c = getopt_long(argc, argv, "M:P:S:D:h", long_options, &opt_index);
        if (c < 0)
            break;

        switch (c)
        {
        case 0:
            if (strcmp(long_options[opt_index].name, "mtgroup"))
                strcpy(client_conf.mtgroup, optarg);

            if (strcmp(long_options[opt_index].name, "port"))
                strcpy(client_conf.port, optarg);

            if (strcmp(long_options[opt_index].name, "server"))
                strcpy(client_conf.server, optarg);

            if (strcmp(long_options[opt_index].name, "decoder"))
                strcpy(client_conf.decoder, optarg);

            if (strcmp(long_options[opt_index].name, "help"))
            {
                printf("-M --mtgroup    指定多播组IP\n-P --port       指定端口\n-S --server     指定服务器IP\n-D --decoder    指定解码器\n-h --help       显示帮助\n");
                exit(0);
            }
            break;

        case 'M':
            strcpy(optarg, client_conf.mtgroup);
            break;

        case 'P':
            strcpy(optarg, client_conf.port);
            break;

        case 'S':
            strcpy(optarg, client_conf.server);
            break;

        case 'D':
            strcpy(optarg, client_conf.decoder);
            break;

        case 'h':
            printf("-M --mtgroup    指定多播组IP\n-P --port       指定端口\n-S --server     指定服务器IP\n-D --decoder    指定解码器\n-h --help       显示帮助\n");
            exit(0);
            break;

        case '?':
            printf("Unknown option: %c\n", c);
            abort();
            break;

        default:
            break;
        }
    }

    int sfd; // 频道socket
    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd < 0)
    {
        perror("socket");
        exit(1);
    }

    struct ip_mreqn mreqn;
    memset(&mreqn, 0, sizeof(mreqn));
    inet_pton(AF_INET, DEFAULT_MGIP, &mreqn.imr_multiaddr.s_addr);
    inet_pton(AF_INET, "0.0.0.0", &mreqn.imr_address.s_addr);
    // mreqn.imr_ifindex = if_nametoindex("lo");
    mreqn.imr_ifindex = 0; 

    if (setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    int flag = 1;
    if (setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_LOOP, &flag, sizeof(flag)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in clinet_addr;
    memset(&clinet_addr, 0, sizeof(clinet_addr));
    clinet_addr.sin_family = AF_INET;
    clinet_addr.sin_port = htons(atoi(client_conf.port));
    inet_pton(AF_INET, "0.0.0.0", &clinet_addr.sin_addr);
    
    if (bind(sfd, (void *)&clinet_addr, sizeof(clinet_addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    printf("client started, listening on %s:%s\n", client_conf.mtgroup, client_conf.port);

    int pfd[2];
    if (pipe(pfd) < 0)
    {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
    }
    /*父进程从网络上收包 ，收到发给子进程*/
    if (pid > 0) // parent process
    {
        close(pfd[0]);
        
        // 收节目单
        struct chn_list_st *chn_list;
        chn_list = malloc(MSG_MAX_ENTRY);
        if (chn_list == NULL)
        {
            perror("malloc");
            exit(1);
        }

        ssize_t len;
        struct sockaddr_in server_addr_play;
        char server_ip[40];
        memset(&server_addr_play, 0, sizeof(server_addr_play));
        socklen_t server_addr_play_len = sizeof(server_addr_play);

        printf("receiving ip and port from server...\n");
        while (1)
        {
            len = recvfrom(sfd, chn_list, MSG_MAX_ENTRY, 0, (void *)&server_addr_play,&server_addr_play_len); 
            if (len < sizeof(struct chn_list_st))
            {
                fprintf(stderr, "recvfrom: %s\n", strerror(errno));
                continue;
            }
            if (chn_list->chn_id != 0)
            {
                fprintf(stderr, "recvfrom: chn_id is not 0\n");
                continue;
            }
            break;
        }
        inet_ntop(AF_INET, &server_addr_play.sin_addr, server_ip, sizeof(server_ip));

        // 打印节目单，选择节目
        struct msg_list_entry_st *pos = chn_list->msg_list;
        
        printf("size is %d",ntohs(chn_list->msg_list->len));

        for (pos; (char *)pos < ((char *)chn_list + len); pos = (void *)(((char *)pos) + ntohs(pos->len)))
        {
            printf("ID:%d, Desc:%s\n", pos->chn_id, pos->desc);
        }

        free(chn_list);

        chnid_t select_id = 1;
        // while (1)
        // {
        //     printf("请输入频道 ID (0-255)，或输入 -1 退出: ");
        //     int ret = scanf("%hhu", &select_id); // 使用 %hhu 读取 unsigned char
        //     if (ret == 1)
        //     {
        //         printf("您输入的频道 ID 是: %u\n", select_id);
        //         break;
        //     }
        // }

        // 收频道包发给子进程
        struct chn_msg_st *msg_channel;
        msg_channel = malloc(MSG_CHN_MAX);
        if (msg_channel == NULL)
        {
            perror("malloc");
            exit(1);
        }

        struct sockaddr_in server_addr_msg;
        memset(&server_addr_msg, 0, sizeof(server_addr_msg));
        socklen_t server_addr_msg_len = sizeof(server_addr_msg);
        
        char rcvbuf[BUFSIZE];
        uint32_t offset = 0;
        memset(rcvbuf, 0, BUFSIZE);
        int bufct = 0; // buffer count

        while (1)
        {
            len = recvfrom(sfd, msg_channel, MSG_MAX_ENTRY, 0, (void *)&
            server_addr_msg, &server_addr_msg_len);
            

            // IP地址和端口号必须匹配
            if (server_addr_msg.sin_addr.s_addr != server_addr_play.sin_addr.s_addr || server_addr_msg.sin_port != server_addr_play.sin_port)
            {
                fprintf(stderr, "recvfrom: not from server\n");
                continue;
            }

            // 包不对
            if (len < sizeof(struct chn_msg_st))
            {
                fprintf(stderr, "recvfrom: %s\n", strerror(errno));
                continue;
            }

            // 节目ID不匹配
            if (msg_channel->chn_id != select_id)
                continue;

            if (msg_channel->chn_id == select_id)
            {
                ssize_t s;
                memcpy(rcvbuf + offset, msg_channel->data, len - sizeof(chnid_t));
                offset += len - sizeof(chnid_t);
                if (bufct++ % 2 == 0)
                {
                    s = writen(pfd[1], rcvbuf, offset);
                    printf("write %ld bytes\n", s);
                    if (len < 0)
                        exit(1);
                    offset = 0;
                }
            }
        }

        free(msg_channel);
        close(pfd[1]);
        close(sfd);
        exit(0);
    }
    /* 子进程调解码器解包，播放.. */
    if (pid == 0) // child process
    {
        close(sfd);
        close(pfd[1]);
        dup2(pfd[0], 0);

        close(pfd[0]);
        execl("/bin/sh", "sh", "-c", DECODER_CMD, NULL);
        perror("execl");
    }

    exit(0);
}
