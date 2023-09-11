#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/epoll.h>

#include "commen.h"
#include "service.h"
#include "threadpool.h"

#define _GNU_SOURCE

//最多接受128个文件
#define MAX_FILENUM 128



static const char * db_path = "/mnt/hgfs/CS2307/2阶段/4数据库/Code/test.db";
static int alive_pthread = 0;
static int flag = 1;
static int chat_room_flag = 0;      /*只创建一个线程用于聊天室的功能*/

User_Info user_info[30];
static int user_count = 0;              /*用户数*/

/*
    服务端
*/

//功能将flag标志位置0
void func(int argc){
    flag = 0;
}


/*功能：创建子线程与客户端进行连接
*/
void* connect_client(void* arg){
    //判断是否存在工作路径
    FILE* fd = fopen("/usr/mytftp/mytftp.txt","r");
    char work_path[100];
    if(fd == NULL){
        printf("啊哦~服务端工作路径有误，请检查是否成功创建文件或给予权限.或使用 -h 查看帮助\n");
        return NULL;
    }else{
        char buf[100];

        int ret = fread(buf,1,100,fd);
        int path_len = ret - 10;
        char* temp = &buf[10];

        strncpy(work_path,temp,path_len);
        work_path[path_len - 1] = '\0';
        printf("当前服务端工作路径为：%s\n",work_path);
    }


    //捕捉信号
    signal(SIGINT,func);
    int scokt_fd = *((int*)arg);
    pthread_t communicate_tid[MAX_CONNECT];
    // Service service[MAX_CONNECT];
    Service service;
    int num = 0;

    // 创建线程池
    threadpool_t *pool = creat_threadpool(4,12,6);
    
    while(flag){
        //让套接字进入监听状态
        int listen_ret = listen(scokt_fd,MAX_CONNECT);
        if(listen_ret == -1){
            perror("listen error");
            return NULL;
        }

        //等待客户端的连接请求
        struct sockaddr_in client_addr;
        int len = sizeof(client_addr);
        int accept_fd = accept(scokt_fd,(struct sockaddr*)&client_addr,&len);
        if(accept_fd == -1){
            perror("accept error");
            return NULL;
        }
        else{
            service.fd = accept_fd;
            strncpy(service.work_path,work_path,strlen(work_path));
            // //在创建线程之前进行判断
            // //如果在执行的线程数大于了MAX_CONNECT最大连接数，就得等待最开始的哪个线程执行完
            // if(alive_pthread >= MAX_CONNECT){
            //     printf("线程已满，请等待%ld执行完\n",communicate_tid[num % MAX_CONNECT]);
            // }
            // pthread_create(&communicate_tid[num % MAX_CONNECT],NULL,communicate,(void*)&service);
            // num += 1;
            // alive_pthread += 1;
            // printf("连接套接字描述符 = %d,添加新任务\n",accept_fd);
            threadpool_add_task(pool,communicate,(void*)&service);
        }

    }

    close(scokt_fd);
}


/*功能：与客户端进行通信
*/
void* communicate(void* arg) {
    Service service = *((Service*)arg);
    int fd = service.fd;
    char work_path[100];
    strncpy(work_path,service.work_path,strlen(service.work_path));

    //printf("work path = %s\n",work_path);

    //保存文件的名字
    char file_name[100];

    unsigned char reply[65535];
    int r = recv_one_data_package(fd,reply,100 - 1);
    if(r == 0)
        return NULL;
    reply[r] = '\0';

    //解析客户端发来的数据
    if(reply[0] == GET){
        //客户端想要得到一个文件，调用send_file函数
        char* p = &reply[2];
        strncpy(file_name,p,reply[1]);
        send_file(fd,file_name,work_path);
    }
    else if(reply[0] == PUT){
        //客户端想要得到一个文件，调用get_file函数
        char* p = &reply[2];
        strncpy(file_name,p,reply[1]);
        get_file(fd,file_name,work_path);
    }
    else if(reply[0] == LIST){
        send_all_file_name(fd,work_path);
    }
    else if(reply[0] == LOGIN){
        printf("client request login \n");
        sercive_login(fd,&reply[1]);
    }
    else if(reply[0] == REGIST){
        service_regist(fd,&reply[1]);
    }

    close(fd);
}


//向客户端发送一个文件
int send_file(int socket_fd, const char* file_name, const char* work_path){
    //printf("file name = %s\n",file_name);
    //首先看是否存在该文件
    char file_path[100];
    sprintf(file_path,"%s/%s",work_path,file_name);
    printf("%s\n",file_path);
    //以只读二进制的形式打开服务端的文件
    //int file_fd = open(file_path,O_RDONLY);
    FILE* file_fd = fopen(file_path,"rb");

    //使用epoll来判断客户端是否断开连接
    int service_epoll_fd = epoll_create(1);
    if(service_epoll_fd == -1){
        perror("epoll creat error");
        return -1;
    }

    struct epoll_event service_sockt_ev;
    service_sockt_ev.events = EPOLLRDHUP | EPOLLOUT;
    service_sockt_ev.data.fd = socket_fd;
    epoll_ctl(service_epoll_fd,EPOLL_CTL_ADD,socket_fd,&service_sockt_ev);

    //response保存的是向客户端发送的数据 file_data保存的是从文件中读出的数据
    unsigned char response[65535];
    unsigned char file_data[65535];
    int ret;
    int data_bits = 0;
    if(file_fd == NULL){
        //文件不存在,向客户端发送包报告
        response[0] = 0;
        send_one_data_package(socket_fd,response,100);

        return -1;
    }
    else{
        while(1){
            //response 头部信息占两个字节 所以从file文件中一次最多读出100 - '\0' - 头部长度 = 97
            response[0] = 1;
            //data_bits = read(file_fd,file_data,97);
            data_bits = fread(file_data,1,16384 - 3,file_fd);
            // data_bits = fread(file_data,1,200 - 3,file_fd);
            printf("data_bits = %d\n",data_bits);
            //sleep(2);
            response[1] = data_bits;

            char* temp = &response[2];
            //strncpy(temp,file_data,data_bits);
            memcpy(temp,file_data,data_bits);

            //再向客户端发送
            send_one_data_package(socket_fd,response,data_bits + 2);
            
            //如果读出的字节数为0就退出
            if(data_bits == 0){
                break;
            }
            ret = epoll_wait(service_epoll_fd,&service_sockt_ev,1,30000);
            if(ret > 0){
                if(service_sockt_ev.events & EPOLLRDHUP){
                    return 0;
                }
                else if(service_sockt_ev.events & EPOLLOUT){
                    continue;
                }
            }
        }
    }

    //使用epoll来判断客户端是否断开连接,并且是否需要下载多个文件
    int epoll_fd = epoll_create(1);
    if(epoll_fd == -1){
        perror("epoll creat error");
        return -1;
    }

    struct epoll_event sockt_ev;
    sockt_ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    sockt_ev.data.fd = socket_fd;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,socket_fd,&sockt_ev);
    ret = epoll_wait(epoll_fd,&sockt_ev,1,30000);
    if(ret == 0){
        printf("timeout\n");
    }
    else if(ret < 0){
        perror("epoll_wait error");
        return 0;
    }
    else{
        if(sockt_ev.events & EPOLLRDHUP){
            return 0;
        }
        else{
            unsigned char reply[65535];
            int r = recv_one_data_package(socket_fd,reply,100 - 1);
            if(r == 0)
                return -1;
            reply[r] = '\0';
            char* p = &reply[2];
            char temp_name[100];
            strncpy(temp_name,p,reply[1]);
            send_file(socket_fd,temp_name,work_path);
        }
    }

}


/*从客户端得到一共文件
*/
int get_file(int socket_fd, const char* file_name, const char* work_path){
    unsigned char file_path[100];
    //将文件名和PATH路径拷贝到file_path数组中
    sprintf(file_path,"%s/%s",work_path,file_name);
    unsigned char file_data[100];
    printf("%s\n",file_path);
    //保存服务端发来的数据
    unsigned char client_data[65535];
    
    //创建目标文件
    FILE* file_fd = fopen(file_path,"wb");

    //创建epoll文件监听客户端是否断开连接
    int epoll_fd = epoll_create(1);
    if(epoll_fd == -1){
        perror("epoll creat error");
        return -1;
    }

    //创建epoll事件结构体
    struct epoll_event client_sockt_ev;
    client_sockt_ev.events = EPOLLRDHUP | EPOLLIN;
    client_sockt_ev.data.fd = socket_fd;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,socket_fd,&client_sockt_ev);

    int r = recv_one_data_package(socket_fd,client_data,100);
    int ret;
    client_data[r] = '\0';
    while(client_data[1] != 0){
        char* temp = &client_data[2];
        printf("data len = %d\n",r);
        memcpy(file_data,temp,r - 2);

        fwrite(file_data,1,r - 2,file_fd);

        ret = epoll_wait(epoll_fd,&client_sockt_ev,1,30000);
        if(ret == 0){
            printf("timeout\n");
        }
        else if(ret < 0){
            perror("epoll_wait error");
            return 0;
        }
        else{
            if(client_sockt_ev.events & EPOLLRDHUP){
                return 0;
            }
        }
        r = recv_one_data_package(socket_fd,client_data,100);
        client_data[r] = '\0';
    }


    fclose(file_fd);

    return 0;
}


/*向客户端发送所有文件的名字
*/
int send_all_file_name(int socket_fd, const char* work_path){
    char* file_name[1024];
    int num;
    num = get_file_name(work_path,file_name);
    //向客户端发送数据
    for(int i = 0; i < num; i++){
        // if(i % 5 == 0 && i != 0){
        //     printf("\n");
        // }
        send_one_data_package(socket_fd,file_name[i],strlen(file_name[i]));
        // printf("%-20s",file_name[i]);

    }
    printf("客户端进行获取文件名操作\n");
    

    return 0;
}


/*服务端帮助手册
*/
void service_help(){
    printf("《------------------以下是运行服务端帮助手册------------------》\n");
    printf("\n");
    printf("1.创建目录\n");
    printf("请在 /usr目录下创建目录 mytftp 并给予权限 0665\n");
    printf("例如:sudo mkdir -m 0665 /usr/mytftp\n");
    printf("\n");
    printf("2.创建文件\n");
    printf("进入mytftp目录并创建文件 mytftp.txt 给予权限 0664\n");
    printf("例如:sudo vim mytftp.txt\n");
    printf("     chmod +0664 mytftp.txt\n");
    printf("\n");
    printf("3.文件编辑\n");
    printf("最后在文件第一行编辑如下内容 work_path=你的服务端工作目录\n");
    printf("例如:work_path=/home/sen/tftpboot\n");
    printf("\n");
}


/*查找数据
*/
int select_data(sqlite3* pdb, const char* username, const char* password){
    char select_sql[100] = "select username,password from userinfo where username = :user;";
    //printf("sql = %s\n",select_sql);

    sqlite3_stmt* pstmt;

    //创建一个 SQL 语句对象
    int ret = sqlite3_prepare_v2(pdb,select_sql,-1,&pstmt,NULL);
    if(ret != SQLITE_OK){
        printf("创建 SQL 语句对象失败！错误码：%d\n",ret);
        
        return -1;
    }

    //绑定参数并获取参数的下标
    int index = sqlite3_bind_parameter_index(pstmt,":user");
    sqlite3_bind_text(pstmt,index,username,strlen(username),NULL);

    int flag = 1;
    const char * column_username;
    const char * column_password;
    while(1){
        ret = sqlite3_step(pstmt);
        if(ret == SQLITE_ROW){

            //获取各个字段的类型
            column_username = sqlite3_column_text(pstmt,0);
            column_password = sqlite3_column_text(pstmt,1);

            //判断用户名是否匹配
            if(strcmp(column_username,username) == 0){      /*存在并且是对的*/
                if(strcmp(column_password,password) == 0){  /*密码也是对的*/
                    return LOGIN_SUCC;
                }
                else{
                    return PASS_ERROR;
                }
            }
            
        }
        else if(ret == SQLITE_DONE){
            return NOT_EXIST;
            break;
        }
        else{
            printf("错误码：%d\n",ret);
            return LOGIN_ERROR;
        }
    }
    //销毁sql语句对象
    sqlite3_finalize(pstmt);
}


/*向数据库插入一个数据
*/
int insert_data(sqlite3* pdb, const char* username, const char* password){
    char insert_sql[100] = "insert into 'userinfo' (username,password) values";
    sprintf(insert_sql,"%s ('%s','%s');",insert_sql,username,password);
    printf("sql = %s\n",insert_sql);

    sqlite3_stmt* pstmt;

    //创建一个 SQL 语句对象
    int ret = sqlite3_prepare_v2(pdb,insert_sql,-1,&pstmt,NULL);
    if(ret != SQLITE_OK){
        printf("创建 SQL 语句对象失败！错误码：%d\n",ret);
        
        return -1;
    }

    //执行 SQL 语句对象
    ret = sqlite3_step(pstmt);
    if(ret != SQLITE_DONE){
        printf("执行 SQL 语句失败！错误码：%d\n",ret);

        return REGIST_ERROR;
    }
    else{
        printf("SQL语句执行成功！\n");

        return REGIST_SUCC;
    }

    //销毁sql语句对象
    sqlite3_finalize(pstmt);
}


/*服务端判断用户登录
*/
int sercive_login(int fd, char* client_data){
    //printf("client_data = %s\n",client_data);
    //client_data 的第一个元素保存的是长度
    char username[50];
    char password[50];

    //取出用户名和密码
    char* p = &client_data[1];
    strncpy(username,p,client_data[0]);
    username[client_data[0]] = '\0';

    //密码的数据保存在 用户名的长度，元素和密码的长度后 如 4 root 3 123
    p = &client_data[client_data[0] + 2];       /* client_data[0] 保存的是用户名的长度*/
                                                /*所以 client_data[client_data[0] + 2] 就是密码的数据*/
    strncpy(password,p,client_data[client_data[0] + 1]);
    password[client_data[client_data[0] + 1]] = '\0';

    //定义sqlite3指针
    sqlite3* pdb;
    
    //打开或创建一个sqlite3数据库文件
    int ret = sqlite3_open(db_path,&pdb);
    if(ret != SQLITE_OK){
        printf("打开或数据库文件失败\n");

        return -1;
    }
    // printf("user = %s\n",username);
    // printf("pass = %s\n",password);
    //判断查找数据的返回值是什么
    ret = select_data(pdb,username,password);
    unsigned char response[100];
    if(ret == LOGIN_SUCC){
        response[0] = LOGIN_SUCC;
        send_one_data_package(fd,response,strlen(response));
        printf("客户端登录成功！\n");
    }
    else if(ret == PASS_ERROR){
        response[0] = PASS_ERROR;
        send_one_data_package(fd,response,strlen(response));
        printf("客户端密码错误！\n");
    }
    else if(ret == NOT_EXIST){
        response[0] = NOT_EXIST;
        send_one_data_package(fd,response,strlen(response));
        printf("客户端账号不存在！\n");
    }


    //关闭数据库文件
    sqlite3_close(pdb);
}


/*服务端判断客户端的注册信息是否匹配
*/
int service_regist(int fd, char* client_data){
    //printf("client_data = %s\n",client_data);
    //client_data 的第一个元素保存的是长度
    char username[50];
    char password[50];

    //取出用户名和密码
    char* p = &client_data[1];
    strncpy(username,p,client_data[0]);
    username[client_data[0]] = '\0';

    //密码的数据保存在 用户名的长度，元素和密码的长度后 如 4 root 3 123
    p = &client_data[client_data[0] + 2];       /* client_data[0] 保存的是用户名的长度*/
                                                /*所以 client_data[client_data[0] + 2] 就是密码的数据*/
    strncpy(password,p,client_data[client_data[0] + 1]);
    password[client_data[client_data[0] + 1]] = '\0';

    //定义sqlite3指针
    sqlite3* pdb;
    
    //打开或创建一个sqlite3数据库文件
    int ret = sqlite3_open(db_path,&pdb);
    if(ret != SQLITE_OK){
        printf("打开或数据库文件失败\n");

        return -1;
    }
    // printf("user = %s\n",username);
    // printf("pass = %s\n",password);
    ret = select_data(pdb,username,password);
    unsigned char response[100];
    if(ret == NOT_EXIST){
        //数据库不存在该用户名，就去插入一个
        ret = insert_data(pdb,username,password);
        if(ret == REGIST_SUCC){
            response[0] = REGIST_SUCC;
            send_one_data_package(fd,response,strlen(response));
            printf("客户端账号注册成功！\n"); 
        }
        else{
            response[0] = REGIST_ERROR;
            send_one_data_package(fd,response,strlen(response));
            printf("客户端账号注册失败！\n"); 
        }
    }
    else{
        //否则就是存在
        response[0] = EXIST;
        send_one_data_package(fd,response,strlen(response));
        printf("客户端账号已存在，不需要注册！\n");
    }

    //关闭数据库文件
    sqlite3_close(pdb);
}


/*聊天室线程专门用于监听客户发来的信息
*/
void* chat_room(void* arg){
    int socket_fd = *((int*)arg);
    
    //定义客户的网络地址结构体
    struct sockaddr_in client_addr;
    int len = sizeof(client_addr);

    //定义临时的客户网络地址结构体
    struct sockaddr_in temp_client_addr;
    temp_client_addr.sin_family = AF_INET;//指定协议族
    
    
    //定义接受数据的数组并初始化
    unsigned char recv_buf[2048];
    memset(recv_buf, 0, sizeof(recv_buf));

    // 创建发送数据的数组并初始化
    unsigned char send_buf[2048];
    memset(send_buf, 0, sizeof(send_buf));
    printf("创建聊天室成功\n");
    int recv_bites;
    while(1){
        memset(recv_buf, 0, sizeof(recv_buf));
        recv_bites = recvfrom(socket_fd, recv_buf, sizeof(recv_buf), 0,(struct sockaddr *)&client_addr, &len);
        if(recv_bites > 0){

            // 判断该用户在不在聊天室中
            // char client_ip[32];
            // inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            unsigned short client_port = ntohs(client_addr.sin_port);

            // 如果不在聊天室就保存该用户信息
            int user_id = is_inChat(client_port);
            if (user_id == -1) {
                // 保存用户的信息
                user_info[user_count].user_id = user_count;
                // 用户的IP地址及端口
                inet_ntop(AF_INET, &(client_addr.sin_addr), user_info[user_count].addr, INET_ADDRSTRLEN);
                user_info[user_count].port = ntohs(client_addr.sin_port);
                user_count++;
            }
            printf("客户端IP地址：%s端口号：%d发来了消息%s", user_info[user_id].addr,client_port,recv_buf);
            // 向当前在线的用户广播发送该消息，除了该用户
            for (int i = 0; i < user_count; i++) {
                if (i != user_id) {
                    // 清空数组并赋值
                    memset(send_buf, 0, sizeof(send_buf));
                    strncpy(send_buf,recv_buf,recv_bites);
                    unsigned short port = user_info[i].port;
                    printf("向 port = %u发送信息\n",port);
                    temp_client_addr.sin_port = htons(port);//指定端口号
                    memset(temp_client_addr.sin_zero,0,8);//清空填充数组 sin_zero
                    temp_client_addr.sin_addr.s_addr = inet_addr(user_info[i].addr);
                    
                    sendto(socket_fd, send_buf, strlen(send_buf), 0, (struct sockaddr *)&temp_client_addr, sizeof(temp_client_addr));
                }
            }
        }
    }

}

/* 判断这名用户是否在聊天室中,返回用户的用户id */
int is_inChat(unsigned short client_port) {
    for (int i = 0; i < user_count; i++) {
        // printf("userinfo = %u,client = %u\n", user_info[i].port,client_port);
        if (user_info[i].port == client_port) {
            return i;
        }
    }
    return -1;
}