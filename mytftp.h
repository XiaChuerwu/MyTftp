#ifndef __MYTFTP_H__
#define __MYTFTP_H__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>


#define HEAD 0xff
#define TAIL 0xff
#define MAX_CONNECT 3
#define PATH "/home/sen/tftpboot"

//枚举类型保存协议的信息
enum type{
    GET = 1,
    PUT,
    LIST
};

//服务端结构体
typedef struct service{
    //网络文件的描述符
    int fd;
    struct sockaddr_in client_addr;
    char work_path[100];
}Service; 

/*
    共用的
*/
int get_file_name(const char * path, char* file);

//拆包
int recv_one_data_package(int fd, unsigned char* buf,int size);

//封包
void send_one_data_package(int fd, unsigned char* p, int size);

/*
    服务端
*/

//与客户端进行交流
void* connect_client(void* arg);

void func(int argc);
//与客户端进行连接
void* communicate(void* arg);

//向客户端发送一个文件
int send_file(int fd, const char* file, const char* work_path);

int get_file(int socket_fd, const char* file_name, const char* work_path);

int send_all_file_name(int socket_fd, const char* work_path);

void service_help();

/*
    客户端
*/

int download_file(int socket_fd, const char* path, const char* file_name);

int up_file(int socket_fd, char* file_path);

/*从服务端获取目录下的所有文件
*/
void get_all_file(int socket_fd);

void client_help();

int connect_service(const char* ip);

/*
    同时下载多个文件
*/
int download_some_file(int socket_fd, const char* path, char* file_name[], int file_num);
#endif