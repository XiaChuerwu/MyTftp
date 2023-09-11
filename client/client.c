#include <stdio.h>
#include <string.h>
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

#include "client.h"
#include "commen.h"

//最多接受128个文件
#define MAX_FILENUM 128
#define MAX_SIZE 255


static char service_ip[20] = "192.168.123.151";



/*
    客户端
*/



/*从服务端下载一个文件
    返回值：-1代表文件不存在
*/
int download_file(int socket_fd, const char* path, const char* file_name){
    int name_len = strlen(file_name);

    unsigned char buf[100] = {GET,name_len};
    char* p = &buf[2];
    //将文件名拷贝到buf数组中
    strncpy(p,file_name,name_len);
    unsigned char file_data[65535];
    //保存服务端发来的数据
    unsigned char service_data[65535];
    //开发板
    // unsigned char service_data[200];

    //向服务端发送请求报文，看服务端是否存在该文件
    send_one_data_package(socket_fd,buf,strlen(buf));
    //printf("buf = %s\n",buf);
    //read(scokt_fd,buf,100);

    int r = recv_one_data_package(socket_fd,service_data,16384);
    //开发板
    // int r = recv_one_data_package(socket_fd,service_data,200);
    service_data[r] = '\0';
    //不存在就返回-1
    if(service_data[0] == 0){
        return -1;
    }
    else{
        //文件存在，创建目标文件
        char file_path[100];
        sprintf(file_path,"%s/%s",path,file_name);

        // int file_fd = open(file_path,O_RDWR | O_TRUNC | O_CREAT, 0664);
        FILE* file_fd = fopen(file_path,"wb");
        while(r > 2){
            //从第三个字节开始拷贝数据。使用内存拷贝
            char* temp = &service_data[2];
            memcpy(file_data,temp,r - 2);

            //printf("file_data = %s\n",file_data);
            // write(file_fd,file_data,r - 2);
            fwrite(file_data,1,r - 2,file_fd);
            //开发板
            // r = recv_one_data_package(socket_fd,service_data,200);
            //正常使用
            r = recv_one_data_package(socket_fd,service_data,16384);
            //printf("现在r = %d\n",r);
            service_data[r] = '\0';
        }
    }

    printf("下载%s文件成功~\n",file_name);
}


/*上传一个文件至服务端
    返回值：-1代表文件不存在
*/
int up_file(int socket_fd, char* file_path){
    //printf("file_path = %s\n",file_path);
    //首先判断是否存在该文件
    FILE* file_fd = fopen(file_path,"rb");
    if(file_fd == NULL){
        //文件不存在，直接返回
        return -1;
    }

    //文件格式可能为 1.txt ./1.txt /home/sen/temp/1.txt，所以我们需要从字符串后开始找，找到第一个 / 的位置
    //文件存在，向服务端发送信息
    int path_len = strlen(file_path);
    //printf("path_len = %d\n",path_len);
    //尾指针指向字符串最后一个字符，头指针指向第一个字符
    char* tail = &file_path[path_len];
    char* head = file_path;
    int name_len = 0;
    while(tail != head && *tail-- != 47){
        name_len++;
    }
    //name_len -= 1;
    //printf("name_len = %d\n",name_len);
    //从最后一个 / 的位置开始拷贝文件名
    char file_name[100];
    tail = &file_path[path_len];
    strncpy(file_name,tail - name_len, name_len);
    file_name[name_len] = '\0';

    unsigned char buf[100] = {PUT,name_len};
    char* p = &buf[2];
    //将文件名拷贝到buf数组中
    strncpy(p,file_name,name_len);

    //向服务端发送报文，表示要上传文件了
    send_one_data_package(socket_fd,buf,strlen(buf));

    unsigned char file_data[100];
    unsigned char client_pack[100];
    int data_bits;
    do{
        client_pack[0] = 1;
        //从文件中开始读数据
        data_bits = fread(file_data,1,97,file_fd);
        client_pack[1] = data_bits;
        char* temp = &client_pack[2];
        memcpy(temp,file_data,data_bits);

        //向服务端发送信息
        send_one_data_package(socket_fd,client_pack,data_bits + 2);
    }while(data_bits);

    printf("上传文件%s成功~\n",file_name);

    fclose(file_fd);
}

 
/*从服务端获取目录下的所有文件
*/
void get_all_file(int socket_fd){
    unsigned char buf[100] = {LIST};
    char* file_name[MAX_FILENUM];
    for(int i = 0;i < MAX_FILENUM; i++){
        file_name[i]=(char *)malloc(sizeof(char)*32); //为每个指针申请开设32字符的存储空间
    }

    int max_file_len = 0;       //保存最长的哪个文件的长度
    send_one_data_package(socket_fd,buf,1);
    int r, num = 0;
    printf("服务端有如下文件：\n");
    while(1){
        r = recv_one_data_package(socket_fd,file_name[num],128);
        //求最长的文件长度
        if(max_file_len < strlen(file_name[num])){
            max_file_len = strlen(file_name[num]);
        }
        num++;
        if(r == 0)
            break;
    }
    for(int i = 0; i < num; i++){
        if(i != 0 && i % 5 == 0)
            printf("\n");
        printf("%-*s ",max_file_len,file_name[i]);
        //打印完就将指针释放掉
        free(file_name[i]);
    }
    printf("\n");
    
    
}


/*客户端的帮助
*/
void client_help(){
    printf("《------------------输入  -g   查看从服务端下载一个文件的帮助-------------》\n");
    printf("《------------------输入  -ng  查看从服务端下载多个文件的帮助-------------》\n");
    printf("《------------------输入  -p   查看上传文件至服务端的帮助-----------------》\n");
    printf("《------------------输入  -l   查看获取服务端目录下所有文件的帮助---------》\n");
    char buf[10];
    while (1){
        scanf("%s",buf);
        if(strcmp(buf,"-g") && strcmp(buf,"-ng") && strcmp(buf,"-l") && strcmp(buf,"-p") ){
            printf("输入有误请重新输入\n");
            continue;
        }
        else{
            break;
        }
    }

    //判断是需要什么的帮助
    if(strcmp(buf,"-g") == 0){
        printf("输入格式如下：\n");
        printf("./client_mytftp -g 服务端目录下的文件名 服务端ip地址\n");
        printf("eg：\n");
        printf("./client_mytftp -g photo_bg.jpg 192.168.3.81\n");
    }
    else if(strcmp(buf,"-ng") == 0){
        printf("输入格式如下：\n");
        printf("./client_mytftp -ng 服务端目录下的文件名列表 ip:服务端ip地址\n");
        printf("eg：\n");
        printf("./client_mytftp -ng photo_bg.jpg 1.txt 3.txt ip:192.168.3.81\n");
        printf("\n");
        printf("\n");
        printf("\t\t\t\t注意：使用-ng命令参数时，服务端ip地址前面必须有后缀ip:以用来和文件列表区分，且文件名列表最大为5\n");
        printf("\n");
        printf("\n");
    }
    else if(strcmp(buf,"-l") == 0){
        printf("输入格式如下：\n");
        printf("./client_mytftp -l 服务端ip地址\n");
        printf("eg：\n");
        printf("./client_mytftp -l 192.168.3.81\n");
    }
    else if(strcmp(buf,"-p") == 0){
        printf("输入格式如下：\n");
        printf("./client_mytftp -p 本地文件的绝对路径 服务端ip地址\n");
        printf("eg：\n");
        printf("./client_mytftp -p /home/sen/temp/test.txt 192.168.3.81\n");
    }
    
}


/*与服务端建立连接
*/
int connect_service(const char* ip){
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1){
        perror("open socket_fd fail");
        return -1;
    }
    //获取套接字接收缓冲区的大小
    int val;
    int len=sizeof(val);
    getsockopt(socket_fd,SOL_SOCKET,SO_RCVBUF,&val,&len);
    //printf("该套接字接收缓冲区的大小为:%d\n",val);

    //定义服务端网络地址
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;//指定协议族
    addr.sin_port = htons(1688);//指定端口号
    memset(addr.sin_zero,0,8);//清空填充数组 sin_zero
    addr.sin_addr.s_addr = inet_addr(ip);



    //链接服务端
    int connect_ret = connect(socket_fd,(struct sockaddr*)&addr,sizeof(struct sockaddr_in));
    if(connect_ret == -1){
        //perror("啊哦~");
        printf("啊哦~服务端IP地址不正确！\n");
        close(socket_fd);
        return -1;
    }
    printf("好耶~连接服务端成功!\n");

    return socket_fd;

}


/*从服务端中下载多个文件
*/
int download_some_file(int socket_fd, const char* path, char* file_name[], int file_num){
    int repeat_flag = 0;            //判断是否存在重复文件
    int actual_num = 0;             //保存实际应该下载的文件数目
    int i, j;
    for(i = 0; i < file_num; i++){
        //每次都跟前面所有的字符串进行比较看看是否存在相同的
        for(j = 0; j < actual_num; j++){
            if(strcmp(file_name[i],file_name[j]) == 0){
                break;
            }
        }
        //说明没有重复的
        if(j == actual_num){
            file_name[actual_num++] = file_name[i];
            repeat_flag = 1;
        }
    }
    if(repeat_flag == 0)
        printf("一共有 %d 个文件需要下载\n",actual_num);
    else
        printf("存在重复的文件，经判断共有 %d 个文件需要下载\n",actual_num);
    for(int i = 0; i < actual_num; i++){
        printf("正在下载 %s\n",file_name[i]);
        int ret = download_file(socket_fd,path,file_name[i]);
        if(ret == -1){
            printf("啊哦~服务端不存在文件%s\n",file_name[i]);
            return -1;
        }
    }

    return 0;
}


void login(){
    printf("《------------------输入  lg  来进行登录账号-------------》\n");
    printf("《------------------输入  rg  来进行注册账号-------------》\n");

    char type[5];
    char username[50];
    char password[50];
    scanf("%s",type);

    if(strcmp(type,"lg") && strcmp(type,"rg")){     /*登录*/
        printf("输入有误！\n");

        return ;
    }

    printf("请输入您的用户名：");
    scanf("%s",username);
    printf("请输入您的密码：");
    scanf("%s",password);

    char ip[100] = "192.168.123.151";
    //printf("请输入服务端的ip地址：");
    //scanf("%s",ip);
    int socket_fd = connect_service(service_ip);

    //判断是登录还是注册
    if(strcmp(type,"lg") == 0){     /*登录*/
        client_login(socket_fd,username,password);
    }
    else if(strcmp(type,"rg") == 0){    /*注册*/
        client_regist(socket_fd,username,password);
    }
}


/*登录
*/
int client_login(int fd, const char* username, const char* password){
    unsigned char response[100];
    char* p = &response[2];
    
    //保存用户名和密码的长度
    int user_len = strlen(username);
    int pass_len = strlen(password);

    response[0] = LOGIN;
    response[1] = user_len;
    //向response中赋值用户名    LOGIN 长度 root 
    strncpy(p,username,user_len);

    //response数组的第 user_len + 2 位是密码的长度  user_len + 2 是密码的值
    response[user_len + 2] = pass_len;
    p = &response[user_len + 3];
    strncpy(p,password,pass_len);

    //补 '\0'
    response[user_len + pass_len + 2 + 1] = '\0';

    //printf("cd = %ld\n",strlen(response));

    //向服务端发包
    send_one_data_package(fd,response,strlen(response));

    //等待服务端发送数据
    unsigned char service_data[100];
    recv_one_data_package(fd,service_data,100);
    if(service_data[0] == LOGIN_SUCC){
        printf("好耶~登录成功！\n");
        client_chat(username);
    }
    else if(service_data[0] == PASS_ERROR){
        printf("啊哦~密码好像错了！\n");
    }
    else if(service_data[0] == NOT_EXIST){
        printf("啊哦~好像不存该账号！\n");
    }
}


/*注册
*/
int client_regist(int fd, const char* username, const char* password){
    unsigned char response[100];
    char* p = &response[2];
    
    //保存用户名和密码的长度
    int user_len = strlen(username);
    int pass_len = strlen(password);

    response[0] = REGIST;
    response[1] = user_len;
    //向response中赋值用户名    LOGIN 长度 root 
    strncpy(p,username,user_len);

    //response数组的第 user_len + 2 位是密码的长度  user_len + 2 是密码的值
    response[user_len + 2] = pass_len;
    p = &response[user_len + 3];
    strncpy(p,password,pass_len);

    //补 '\0'
    response[user_len + pass_len + 2 + 1] = '\0';

    //printf("cd = %ld\n",strlen(response));

    //向服务端发包
    send_one_data_package(fd,response,strlen(response));

    //等待服务端发送数据
    unsigned char service_data[100];
    recv_one_data_package(fd,service_data,100);
    if(service_data[0] == REGIST_SUCC){
        printf("好耶~注册成功！\n");
        // printf("输入 ng 进行登录！\n");
        // char temp[10];
        // scanf("%s",temp);
        // if(strcmp(temp,"ng") == 0){
        //     client_login(fd,username,password);
        // }
        // else{
        //     printf("输入有误！\n");
        // }
    }
    else if(service_data[0] == EXIST){
        printf("啊哦~该账号已存在，不需要重新注册！\n");
    }
    else if(service_data[0] == REGIST_ERROR){
        printf("啊哦~注册失败，请重新注册！\n");
    }
}


int client_chat(const char * username){
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_fd == -1){
        perror("open socket_fd fail");
        return -1;
    }

    //定义服务端聊天室的网络地址
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;//指定协议族
    addr.sin_port = htons(1788);//指定端口号
    memset(addr.sin_zero,0,8);//清空填充数组 sin_zero
    addr.sin_addr.s_addr = inet_addr(service_ip);

    unsigned char response[100];
    printf("《------------------欢迎来到闲聊室，可以尽情聊天啦-------------》\n");
    unsigned char user_data[2048] = "";
    unsigned char recv_buf[2048] = "";
    unsigned char stdin_buf[2000] = "";
    // 使用 epoll 来在发消息同时也能接受消息
    // 多路复用
    int epoll_fd = epoll_create(1);
    if(epoll_fd == -1){
        perror("epoll creat error");
        close(socket_fd);
        return -1;
    }
    struct epoll_event in_ev;
    in_ev.events = EPOLLIN;
    in_ev.data.fd = fileno(stdin);
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fileno(stdin),&in_ev);

    struct epoll_event sockt_ev;
    sockt_ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    sockt_ev.data.fd = socket_fd;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,socket_fd,&sockt_ev);

    int flag = 1;
    int ret = 0;
    while(flag){
        //设置
        struct epoll_event ev[2];
        ret = epoll_wait(epoll_fd,ev,2,30000);
        if(ret == 0){
            // printf("timeout\n");
        }
        else if(ret < 0){
            perror("epoll_wait error");
            break;
        }
        else{
            //就绪了，假设只有一个文件描述符就绪，那么这个文件描述符的就绪信息保存在
            // ev[0]元素中，假设有2个文件描述符就绪，那么2个文件描述符的就绪信息分别保存在
            //ev[0]和ev[1]中
            for(int i = 0; i < ret; i++){
                //判断是谁可读了
                if(ev[i].events & EPOLLIN){
                    if(ev[i].data.fd == fileno(stdin)){
                        //说明键盘输入了
                        //说明标准输入可读了
                        // 每次输入的时候清空
                        memset(user_data, 0, sizeof(user_data));
                        // 将用户名拷进数组中，告诉客户端是哪一个用户发的
                        int name_len = strlen(username);
                        user_data[0] = name_len;
                        char *pud = &user_data[1];
                        strncpy(pud,username, name_len);

                        // 从键盘输入数据保存进 stdin_buf 数组中
                        fgets(stdin_buf, 512, stdin);
                        if (stdin_buf[0] == '\n') {
                            continue;
                        }
                        if (strcmp(stdin_buf,"-help\n") == 0 || strcmp(stdin_buf,"-h\n") == 0) {
                            client_help();
                            continue;
                        }
                        if (strcmp(stdin_buf,"-tools\n") == 0 || strcmp(stdin_buf,"-t\n") == 0) {
                            tools();
                            continue;
                        }
                        printf("您发送了:%s",stdin_buf);

                        // 对数据进行拼接
                        pud = &user_data[name_len + 1];
                        strcpy(pud,stdin_buf);
                        sendto(socket_fd, user_data, strlen(user_data), 0, (struct sockaddr *)&addr, sizeof(addr));
                    }
                    else if(ev[i].data.fd == socket_fd){
                        //说明 fd可读了
                        // printf("fd可读了\n");
                        memset(recv_buf, 0, sizeof(recv_buf));
                        int r = recv(socket_fd,recv_buf,2048,0);
                        recv_buf[r] = '\0';
                        // 过滤掉空消息
                        if (r == recv_buf[0] + 1) {
                            continue;
                        }
                        // 取出用户名
                        char name[64];
                        memset(name, 0, sizeof(name));
                        strncpy(name,&recv_buf[1],recv_buf[0]);
                        // 如果是自己发送的就过滤
                        if (strcmp(name,username) == 0) {
                            continue;
                        }
                        char *p = &recv_buf[strlen(name) + 1];
                        printf("%s发来了消息:%s",name,p);
                    }
                }
                if(ev[i].events & EPOLLRDHUP){
                    //说明对端断开连接了
                    flag = 0;
                    break;
                }
            }
        }
        
    }
    
    close(socket_fd);
    return 0;
}


// -g photo_bg.jpg 192.168.123.151
// -ng photo_bg.jpg 1.txt 3.txt ip:192.168.123.151
// -p /home/sen/temp/test.txt 192.168.123.151
// -l 192.168.123.151
int tools() {
    printf("\n");
    printf("《------------------ 欢迎使用文件下载功能 ------------------》\n");
    // 获取工作目录
    char path[MAX_SIZE];
    getcwd(path,sizeof(path));

    // 获取并保存命令 -g photo_bg.jpg 192.168.3.81
    char command[MAX_SIZE];
    fgets(command,MAX_SIZE,stdin);

    // 保存拆开的命令
    char* argv[4];
    int argc = 0;
    char* token = strtok(command, " ");

    // 以空格的形式拆开命令保存进argv中
    while (token != NULL) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    if(argc < 2){
        printf("啊哦~输入的命令有误，请输入 -h 查看帮助\n");

        return -1;
    }
    if(strcmp(argv[0],"-l") && strcmp(argv[0],"-g") && strcmp(argv[0],"-p") && strcmp(argv[0],"-h") && strcmp(argv[0],"-ng")){
        printf("啊哦~输入的命令有误，请输入 -h 查看帮助\n");

        return -1;
    }
    if(strcmp(argv[0],"-h") == 0){
        client_help();

        return 0;
    }
    int socket_fd;
    //创建套接字的文件描述符
    if(strcmp(argv[0],"-ng") == 0){
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
    if(strcmp(argv[0],"-g") == 0){
        ret = download_file(socket_fd,path,argv[1]);
        if(ret == -1){
            //文件不存在
            printf("啊哦~服务端不存在该文件！\n");
            close(socket_fd);

            return -1;
        }
    }
    else if(strcmp(argv[0],"-p") == 0){
        ret = up_file(socket_fd,argv[1]);
        if(ret == -1){
            //文件不存在
            printf("啊哦~本地不存在该文件！\n");
            close(socket_fd);
            return -1;
        }
    }
    else if(strcmp(argv[0],"-l") == 0){
        get_all_file(socket_fd);
    }
    
    else if(strcmp(argv[0],"-ng") == 0){
        //定义字符指针数组保存所有的文件名
        char *arr[5];
        int len = 0;
        //一次下载如果超过五个文件就退出
        if(argc > 7){
            printf("啊哦~一次下载的最大文件数目不能超过五个！\n");

            close(socket_fd);
            return -1;
        }
        for(int i = 1; i < argc - 1; i++){
            arr[len++] = argv[i];
        }
        download_some_file(socket_fd,path,arr,len);
    }


    close(socket_fd);

    return 0;

    // for (int i = 0; i < argc; i++) {
    //     printf("%s\n",argv[i]);
    // }

}