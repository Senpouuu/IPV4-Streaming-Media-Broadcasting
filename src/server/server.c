#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
 #include <net/if.h>
#include <arpa/inet.h>  
#include <unistd.h>  
#include <pthread.h>  
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>
#include <signal.h>

#include "server_conf.h"
#include "../include/proto.h"
#include "medialib.h"
#include "thread_list.h"
#include "thread_channel.h"


/*
*    -M 指定多播组
*    -P 指定端口
*    -F 指定守护进程前台运行
*    -I 指定网卡
*    -H 显示帮助
*    -D 指定媒体库位置
*/


struct server_default_cmd server_conf= {
    .mtgroup = DEFAULT_MGIP,
    .port = DEFAULT_MGPORT,
    .runmode = RUN_DEAMON,
    .ifname = DEAFULT_IFNAME,
    .dir= "/home/gulu/桌面/medialib/*"
};

static struct sigaction old_act_TERM, old_act_INT, old_act_QUIT;

int sfd;
struct sockaddr_in sndaddr;
static struct media_list_t* chn_list;


// 创建一个守护进程
static int daemonize()
{
	int fd;
	pid_t pid = fork();
	
	if(pid < 0)
	{
		syslog(LOG_ERR,"fork error");
        exit(1);
    }

	if(pid > 0)
		exit(0);
	else
	{
		fd = open("/dev/null",O_RDWR);
		if(fd < 0)
        {
            syslog(LOG_WARNING,"open /dev/null error");
            exit(1);
        }


		// 守护进程不需要012三个流，重定向至垃圾堆
		dup2(fd,0);
		dup2(fd,1);
		dup2(fd,2);

		if(fd > 2)
            // 创建一个新组，父进程为init进程，此进程变为守护进程		
		    setsid();
        
        chdir("./");
        umask(0);
        return 0;
    }
}

static void socket_init()
{
    sfd = socket(AF_INET, SOCK_DGRAM, 0);   
    if(sfd < 0)
    {
        syslog(LOG_ERR,"create socket error");
        exit(1);
    }

    // bind();
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &sfd, sizeof(sfd));
    setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &sfd, sizeof(sfd));
    
    sndaddr.sin_family = AF_INET;
    sndaddr.sin_port = htons(atoi(server_conf.port));
    inet_pton(AF_INET, server_conf.mtgroup, &sndaddr.sin_addr);
    // inet_pton(AF_INET, "0.0.0.0", &sndaddr.sin_addr.s_addr);


    struct ip_mreqn mreq;
    inet_pton(AF_INET, server_conf.mtgroup, &mreq.imr_multiaddr);
    inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
    mreq.imr_ifindex = if_nametoindex(server_conf.ifname);
    if(setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        syslog(LOG_ERR,"setsockopt error");
        exit(1);
    }
}




static void deamon_exit(int sig)
{

    sigaction(SIGTERM, &old_act_TERM, NULL);
    sigaction(SIGINT, &old_act_INT, NULL);
    sigaction(SIGQUIT, &old_act_QUIT, NULL);

    thr_list_destroy();
    thr_channel_destroy_all();
    midlib_freechnlist(&chn_list);

    syslog(LOG_WARNING,"signal %d received, exit", sig);

    closelog();

    exit(0);
}



int main(int argc, char *argv[])
{
    struct sigaction act;  
    memset(&act, 0, sizeof(act));
    act.sa_handler = deamon_exit;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGTERM);
    sigaddset(&act.sa_mask, SIGINT);
    sigaddset(&act.sa_mask, SIGQUIT);
    act.sa_flags = 0;
    
    sigaction(SIGTERM, &act, &old_act_TERM);
    sigaction(SIGINT, &act, &old_act_INT);
    sigaction(SIGQUIT, &act, &old_act_QUIT);
    
    int c;
    static int opt_index = 0;
    static struct option long_options[] = {
        {"mtgroup", required_argument, 0, 'M'},
        {"port", required_argument, 0, 'P'},
        {"dir", required_argument, 0, 'D'},
        {"ifname", required_argument, 0, 'I'},
        {"front", no_argument, 0, 'F'},
        {"help", no_argument, 0, 'H'},
        {0, 0, 0, 0}};

    openlog("netradio",LOG_PID,LOG_ERR);

    while (1)
    {
        c = getopt_long(argc, argv, "M:P:D:I:FH", long_options, &opt_index);
        if (c < 0)
            break;

        switch (c)
        {
        case 0:
            if (strcmp(long_options[opt_index].name, "mtgroup"))
                strcpy(server_conf.mtgroup, optarg);

            if (strcmp(long_options[opt_index].name, "port"))
                strcpy(server_conf.port, optarg);

            if (strcmp(long_options[opt_index].name, "dir"))
                strcpy(server_conf.dir, optarg);
            
            if (strcmp(long_options[opt_index].name, "ifname"))
                strcpy(server_conf.ifname, optarg);

            if(strcmp(long_options[opt_index].name, "front"))
                server_conf.runmode = RUN_FRONT;

            if (strcmp(long_options[opt_index].name, "help"))
            {
                printf("-M 指定多播组\n -P 指定端口\n -F 指定守护进程前台运行\n -I 指定网卡\n -H 显示帮助\n -D 指定媒体库位置\n");
                exit(0);
            }
            break;

        case 'M':
            strcpy(optarg, server_conf.mtgroup);
            break;

        case 'P':
            strcpy(optarg, server_conf.port);
            break;

        case 'D':
            strcpy(optarg, server_conf.dir);
            break;

        case 'I':
            strcpy(optarg, server_conf.ifname);
            break;

        case 'F':
            server_conf.runmode = RUN_FRONT;
            break;
    
        case 'H':
            printf("-M 指定多播组\n -P 指定端口\n -F 指定守护进程前台运行\n -I 指定网卡\n -H 显示帮助\n -D 指定媒体库位置\n");
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

    /*守护进程*/
    if(server_conf.runmode == RUN_DEAMON)
        if(daemonize() != 0)
            exit(1);

    else if(server_conf.runmode == RUN_FRONT)
        printf("run in front\n");
    else
    {
        syslog(LOG_ERR,"run mode error");
        exit(0);   
    } 
    
    
    /*SOCKET 初始化*/
    socket_init();    

    /*获取频道信息*/
    int num_list;
    int ret = midlib_getchnlist(&chn_list,&num_list);
    /*if err*/
    if(ret < 0)
    {
        fprintf(stderr,"get channel list error\n");
        exit(1);
    }

    /*创建节目单线程*/
    thr_list_create(chn_list,num_list);


    /*创建频道线程*/
    int i, err;
    for(i = 0;i<CHN_NUM;i++)
    {
        // printf("create channel thread %d\n",i);
        err = thr_channel_create(&chn_list[i]);
        if(err)
        {
            fprintf(stderr,"create channel thread %d error\n",i);
            exit(1);    
        }
        syslog(LOG_DEBUG,"%d channel created",i);
    }

    
    while(1)
        pause();
        
}