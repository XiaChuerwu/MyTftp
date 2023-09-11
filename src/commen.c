#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>


#define _GNU_SOURCE

#include "commen.h"


/*客户端与服务端的协议：
    第一个字节为                GET or PUT ...
    第二个字节为               文件的长度n
    第三个字节到n+3个字节为     文件名
*/  

/*服务端与客户端的协议：
    第一个字节为                文件是否存在
    第二个字节为               该文件传输是否已经结束
*/  

/*
    共用的
*/

//获取一个目录中普通文件的数目和总大小
//path 目录的路径
//返回普通文件的数目
int get_file_name(const char * path, char* file_path[]){
    DIR * dirp = opendir(path);
    if(dirp == NULL){
        perror("opendir error");
        return 0;
    }
    int count = 0;//记录普通文件的个数

    struct dirent *p;
    struct stat sb;
    int r;
    int name_len = 0;
    int file_len = 0;
    while(1){
        p = readdir(dirp);
        if(p == NULL)
            break;
        
        //分配足够的内存用来保存路径名
        char pathname[strlen(path)+strlen(p->d_name)+1+1];//路径和名字之间有一个 '/' ，最后还要有一个'\0'
        sprintf(pathname,"%s/%s",path,p->d_name);
        //printf("%s\n",pathname);

        //获取这个文件的属性信息
        r = lstat(pathname,&sb);
        if(r==-1){
            perror("lstat error");
            continue;
        }

        if(S_ISREG(sb.st_mode)){
            //说明该文件为普通文件
            //将普通文件拷贝进file数组中
            file_path[count] = p->d_name;
            count++;
        }
    }

    closedir(dirp);
    return count;
}


/*对数据进行拆包处理
*/
int recv_one_data_package(int fd, unsigned char* buf,int size){
    unsigned char temp;
    //先找到包头
    do{
        read(fd,&temp,1);
    } while (temp != HEAD);
    
    int i = 0;

    while(1){
        int r = read(fd,&temp,1);
        if(r == 0)
            return 0;
        if(temp == 0xfd){
            //再判断下一个字符是什么
            int r = read(fd,&temp,1);
            if(r == 0)
                break;
            if(temp == 0xfd){
                buf[i++] = temp;
            }
            else if(temp == 0xff){
                buf[i++] = temp;
            }
        }
        else if(temp == TAIL || i == size - 1){
            break;
        }
        else{
            buf[i++] = temp;
        }
    }
    return i;
}


/*对数据进行封包处理
*/
void send_one_data_package(int fd, unsigned char* p, int size){
    //定义用于转换的字符串大小为极端情况下两倍的size
    unsigned char* dispose = (unsigned char*)malloc(2*size);

    //开始转换
    int len_dispose = 0;
    for(int i = 0; i < size; i++){
        if(p[i] == 0xff){
            dispose[len_dispose++] = 0xfd;
            dispose[len_dispose++] = 0xff;
        }
        else if(p[i] == 0xfd){
            dispose[len_dispose++] = 0xfd;
            dispose[len_dispose++] = 0xfd;
        }
        else{
            dispose[len_dispose++] = p[i];
        }
    }
    // for(int i = 0; i < len_dispose; i++){
    //     printf("0x%x ",dispose[i]);
    // }
    // printf("\n");
    //定义用于发送信息的字符串大小为转换后的字符串大小再+2
    unsigned char* message = (unsigned char*)malloc(len_dispose + 2);
    
    //加包头
    message[0] = HEAD;
    //内存拷贝转换后的字符串
    memcpy(message + 1,dispose,len_dispose);
    //加包尾
    message[len_dispose + 1] = TAIL;
    //向套接字文件描述符写入信息
    write(fd,message,len_dispose + 2);
    //printf("%s\n",message);


    free(message);
    free(dispose);
}


