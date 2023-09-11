
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "service.h"



//  gcc -I ./ commen.c ./sercive/service_main.c sercive/service.c -o service_test -pthread -l sqlite3
// gcc -I ../inc service_main.c service.c -o service_test ../src/commen.c ../src/threadpool.c -pthread -l sqlite3
int main(int argc, char* argv[]){

    if(argc > 1){
        if(strcmp(argv[1],"-h") == 0){
            service_help();

            return 0;
        }
    }

    //创建socket套接
    int socket_fd = socket(AF_INET,SOCK_STREAM,0);
    if(socket_fd == -1){
        perror("open socket_fd fail");
        return -1;
    }

    //设置套接字允许本地端口重用
    int val = 1;
    setsockopt(socket_fd,SOL_SOCKET,SO_REUSEPORT,&val,sizeof(val));

    //定义服务端的传输文件网络地址
    struct sockaddr_in service_addr;
    service_addr.sin_family = AF_INET;//指定协议族
    service_addr.sin_port = htons(1688);//指定端口号
    memset(service_addr.sin_zero,0,8);//清空填充数组 sin_zero
    service_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    //service_addr.sin_addr.s_addr = inet_addr("192.168.3.81");

    //绑定服务端传输文件网络地址
    int bind_ret = bind(socket_fd,(struct sockaddr*)&service_addr,sizeof(service_addr));
    if(bind_ret == -1){
        perror("bind error");
        return -1;
    }

    //创建线程id用来监听客户端的链接
    pthread_t listen_tid;
    pthread_t chat_tid;

    //创建监听线程
    pthread_create(&listen_tid,NULL,connect_client,&socket_fd);

    //创建socket套接字
    int chat_fd = socket(AF_INET,SOCK_DGRAM,0);
    if(chat_fd == -1){
        perror("open chat_fd fail");
        return -1;
    }

    //设置套接字允许本地端口重用
    setsockopt(chat_fd,SOL_SOCKET,SO_REUSEPORT,&val,sizeof(val));
    //定义服务端的传输文件网络地址
    struct sockaddr_in chat_addr;
    chat_addr.sin_family = AF_INET;//指定协议族
    chat_addr.sin_port = htons(1788);//指定端口号
    memset(chat_addr.sin_zero,0,8);//清空填充数组 sin_zero
    chat_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    //chat_addr.sin_addr.s_addr = inet_addr("192.168.3.81");

    //绑定服务端传输文件网络地址
    bind_ret = bind(chat_fd,(struct sockaddr*)&chat_addr,sizeof(chat_addr));
    if(bind_ret == -1){
        perror("bind error");
        return -1;
    }
    //创建聊天室线程
    pthread_create(&chat_tid,NULL,chat_room,&chat_fd);

    //
    pthread_join(listen_tid,NULL);
    pthread_join(chat_tid,NULL);

    //关闭socket描述符
    close(socket_fd);

    return 0;
}


