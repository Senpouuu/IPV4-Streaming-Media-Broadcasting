# Sever端
  文件系统作为媒体库，存放每个频道传输的数据

# 用多线程并发 两个模块：
## thread_channel   
  管理每一个频道，通过socket向每一个频道发送数据

## thread_list
  管理节目单，通过socket向每一个频道发送节目单

## media_lib
  用于解析媒体文件，并将解析后的数据发送给thread_channel