#ifndef PROTO_H__
#define PROTO_H__

#define DEFAULT_MGIP "224.2.2.2"
#define DEFAULT_MGPORT "1025"

/* 频道号定义 */
#define CHN_NUM 3           // 允许的频道总数 
#define CHN_LIST_NUM 0      // 节目单频道号
#define MIN_CHN_NUM 1       // 起始频道号
#define MAX_CHN_NUM (MIN_CHN_NUM + CHANNEL_NUM - 1)  // 结束频道号

/* udp包最大长度 */
#define MSG_CHN_MAX (65536 - 20 - 8)                // 推荐长度 - ipv4头部 - udp头部 
#define MSG_MAX_DATA (MSG_CHN_MAX - sizeof(chnid_t))  // 剩余长度 - 频道id长度

/* 节目单包最大长度 */
#define MSG_MAX_ENTRY (65536 - 20 - 8 - sizeof(chnid_t))     


#include "site_type.h"

/*
1. MUSIC xxxxxxxxx简介
2. SPORTS xxxxxxxxx简介
3. MOVIE xxxxxxxxx简介
4. TV xxxxxxxxx简介
    ....
*/
struct msg_list_entry_st
{
    chnid_t chn_id;        // 节目单ID
    uint16_t len;           // 总体大小 
    uint8_t desc[1];        // 节目描述
}__attribute__((packed));


// 节目单包
struct chn_list_st
{
    chnid_t chn_id;         // 必须为 0 频道ID 
    struct msg_list_entry_st msg_list[1];
}__attribute__((packed));  // 节目单结构体



// 消息包
struct chn_msg_st
{
    chnid_t chn_id;         // 0 - 100  频道ID 
    uint8_t data[1];
}__attribute__((packed));  // 传输数据结构体



#endif