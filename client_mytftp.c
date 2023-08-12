#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mytftp.h"

#define MAX_SIZE 255

//static char* path = "/home/sen/temp";
//static char* path = "./";

// ./client_mytftp -g photo_bg.jpg 192.168.3.81
// ./client_mytftp -ng photo_bg.jpg 1.txt 3.txt ip:192.168.3.81
// ./client_mytftp -p /home/sen/temp/test.txt 192.168.3.81
// ./client_mytftp -l 192.168.3.81

int main(int argc, char* argv[]){
    char path[MAX_SIZE];
    getcwd(path,sizeof(path));
    //puts(path);
    if(argc < 2){
        printf("啊哦~输入的命令有误，请输入 -h 查看帮助\n");

        return -1;
    }
    if(strcmp(argv[1],"-l") && strcmp(argv[1],"-g") && strcmp(argv[1],"-p") && strcmp(argv[1],"-h") && strcmp(argv[1],"-ng")){
        printf("啊哦~输入的命令有误，请输入 -h 查看帮助\n");

        return -1;
    }
    if(strcmp(argv[1],"-h") == 0){
        client_help();

        return 0;
    }
    int socket_fd;
    //创建套接字的文件描述符
    if(strcmp(argv[1],"-ng") == 0){
        // -ng 命令的ip地址最后是以 ip:结尾
        char ip[100];
        char* p = argv[argc - 1] + 3;
        strcpy(ip,p);

        socket_fd = connect_service(ip);
    }
    else{
        socket_fd = connect_service(argv[argc - 1]);
    }
    if(socket_fd == -1){
        return -1;
    }


    int ret;
    if(strcmp(argv[1],"-g") == 0){
        ret = download_file(socket_fd,path,argv[2]);
        if(ret == -1){
            //文件不存在
            printf("啊哦~服务端不存在该文件！\n");
            close(socket_fd);

            return -1;
        }
    }
    else if(strcmp(argv[1],"-p") == 0){
        ret = up_file(socket_fd,argv[2]);
        if(ret == -1){
            //文件不存在
            printf("啊哦~本地不存在该文件！\n");
            close(socket_fd);
            return -1;
        }
    }
    else if(strcmp(argv[1],"-l") == 0){
        get_all_file(socket_fd);
    }
    
    else if(strcmp(argv[1],"-ng") == 0){
        //定义字符指针数组保存所有的文件名
        char *arr[5];
        int len = 0;
        for(int i = 2; i < argc - 1; i++){
            arr[len++] = argv[i];
        }
        download_some_file(socket_fd,path,arr,len);
    }


    close(socket_fd);

    return 0;
}