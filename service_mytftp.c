#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mytftp.h"

//创建目录 sudo mkdir -m 0665 /usr/mytftp 并给权限
//创建文件 sudo vim mytftp.txt  在第一行编辑 work_path=你服务端的工作路径  并给权限 chmod +0664 mytftp.txt   

int main(int argc, char* argv[]){

    if(argc > 1){
        if(strcmp(argv[1],"-h") == 0){
            service_help();

            return 0;
        }
    }

    //创建socket套接字
    int socket_fd = socket(AF_INET,SOCK_STREAM,0);
    if(socket_fd == -1){
        perror("open socket_fd fail");
        return -1;
    }

    //设置套接字允许本地端口重用
    int val = 1;
    setsockopt(socket_fd,SOL_SOCKET,SO_REUSEPORT,&val,sizeof(val));

    //定义服务端的网络地址
    struct sockaddr_in service_addr;
    service_addr.sin_family = AF_INET;//指定协议族
    service_addr.sin_port = htons(1688);//指定端口号
    memset(service_addr.sin_zero,0,8);//清空填充数组 sin_zero
    service_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    //service_addr.sin_addr.s_addr = inet_addr("192.168.3.81");

    //绑定服务端网络地址
    int bind_ret = bind(socket_fd,(struct sockaddr*)&service_addr,sizeof(service_addr));
    if(bind_ret == -1){
        perror("bind error");
        return -1;
    }

    //创建线程id用来监听客户端的链接
    pthread_t listen_tid;

    //创建监听线程
    pthread_create(&listen_tid,NULL,connect_client,&socket_fd);

    //
    pthread_join(listen_tid,NULL);

    //关闭socket描述符
    close(socket_fd);

    return 0;
}