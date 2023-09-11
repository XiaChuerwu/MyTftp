/*
 * @creater: XiaChuerwu 1206575349@qq.com
 * @since: 2023-09-09 15:09:46
 * @lastTime: 2023-09-10 11:17:38
 * @LastAuthor: XiaChuerwu 1206575349@qq.com
 */
#ifndef __SERVICE_H__
#define __SERVICE_H__

#include "commen.h"

// 服务端结构体
typedef struct service {
    // 网络文件的描述符
    int fd;
    // struct sockaddr_in client_addr;
    char work_path[100];
} Service;

// 客户端用户信息结构体
typedef struct user_info {
    int user_id;    /*用户ID*/
    char addr[30];  /*用户的ip地址*/
    unsigned short port;  /*用户的端口号*/
    char name[50];  /*用户名*/
} User_Info;

/*
    服务端
*/

// 与客户端进行交流
void *connect_client(void *arg);

void func(int argc);
// 与客户端进行连接
void *communicate(void *arg);

// 向客户端发送一个文件
int send_file(int fd, const char *file, const char *work_path);

int get_file(int socket_fd, const char *file_name, const char *work_path);

int send_all_file_name(int socket_fd, const char *work_path);

void service_help();

/*查找数据
 */
int select_data(sqlite3 *pdb, const char *username, const char *password);

/*向数据库插入一个数据
 */
int insert_data(sqlite3 *pdb, const char *username, const char *password);

/*服务端判断用户登录
 */
int sercive_login(int fd, char *client_data);

/*服务端判断客户端的注册信息是否匹配
 */
int service_regist(int fd, char *client_data);

void *chat_room(void *arg);

/* 判断这名用户是否在聊天室中 */
int is_inChat(unsigned short client_port);

#endif