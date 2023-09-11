#ifndef __COMMEN_H__
#define __COMMEN_H__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sqlite3.h>

#define HEAD 0xff
#define TAIL 0xff
#define MAX_CONNECT 3
#define PATH "/home/sen/tftpboot"
#define LOGIN_SUCC      1       /*登录成功*/
#define LOGIN_ERROR     2       /*登录出错*/
#define NOT_EXIST       3       /*账号不存在*/
#define EXIST           4       /*账号已存在*/
#define PASS_ERROR      5       /*密码错误*/
#define USER_ERROR      6       /*账号错误*/
#define REGIST_SUCC     7       /*注册成功*/
#define REGIST_ERROR    8       /*注册出错*/

//枚举类型保存协议的信息
enum type{
    GET = 1,
    PUT,
    LIST,
    LOGIN,
    REGIST,
    CHAT
};

/*
    共用的
*/
int get_file_name(const char * path, char* file_path[]);

//拆包
int recv_one_data_package(int fd, unsigned char* buf,int size);

//封包
void send_one_data_package(int fd, unsigned char* p, int size);

#endif