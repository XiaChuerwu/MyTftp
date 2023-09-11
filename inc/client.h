/*
 * @creater: XiaChuerwu 1206575349@qq.com
 * @since: 2023-09-09 15:09:46
 * @lastTime: 2023-09-10 14:17:06
 * @LastAuthor: XiaChuerwu 1206575349@qq.com
 */
#ifndef __CLIENT_H__
#define __CLIENT_H__

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

/*登录
*/
int client_login(int fd, const char* username, const char* password);

/*注册
*/
int client_regist(int fd, const char* username, const char* password);

void login();

int client_chat(const char * username);

int tools();

#endif