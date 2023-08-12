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
#define _GNU_SOURCE

#include "mytftp.h"


static int flag = 1;
static int alive_pthread = 0;

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
int get_file_name(const char * path, char* file){
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
            char* q = file;
            name_len = strlen(p->d_name);

            strncpy(q + file_len,p->d_name,name_len);
            file_len += name_len;
            strcpy(q + file_len,"     ");
            file_len += 5;
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

/*
    服务端
*/



/*功能：与客户端进行通信
*/
void* communicate(void* arg){
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

    close(fd);
    printf("线程%ld运行结束\n",pthread_self());
    alive_pthread--;
    pthread_exit(NULL);
}

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
    Service service[MAX_CONNECT];
    int num = 0;
    
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
            service[num % MAX_CONNECT].fd = accept_fd;
            strncpy(service[num % MAX_CONNECT].work_path,work_path,strlen(work_path));
            //在创建线程之前进行判断
            //如果在执行的线程数大于了MAX_CONNECT最大连接数，就得等待最开始的哪个线程执行完
            if(alive_pthread >= MAX_CONNECT){
                printf("线程已满，请等待%ld执行完\n",communicate_tid[num % MAX_CONNECT]);
                pthread_join(communicate_tid[num % MAX_CONNECT],NULL);
                alive_pthread--;
            }
            pthread_create(&communicate_tid[num % MAX_CONNECT],NULL,communicate,(void*)&service[num % MAX_CONNECT]);
            num += 1;
            alive_pthread += 1;
            //printf("连接套接字描述符 = %d\n",accept_fd);
        }

    }

    //销毁线程
    for(int i = 0; i < num; i++){
        pthread_join(communicate_tid[i],NULL);
    }

    close(scokt_fd);
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
    //保存服务端发来的数据
    unsigned char client_data[65535];
    
    //创建目标文件
    FILE* file_fd = fopen(file_path,"wb");

    int r = recv_one_data_package(socket_fd,client_data,100);
    client_data[r] = '\0';
    while(client_data[1] != 0){
        char* temp = &client_data[2];
        printf("data len = %d\n",r);
        memcpy(file_data,temp,r - 2);

        fwrite(file_data,1,r - 2,file_fd);
        r = recv_one_data_package(socket_fd,client_data,100);
        client_data[r] = '\0';
    }


    fclose(file_fd);

    return 0;
}


/*向客户端发送所有文件的名字
*/
int send_all_file_name(int socket_fd, const char* work_path){
    char all_file[65535];
    get_file_name(work_path,all_file);

    //向客户端发送数据
    send_one_data_package(socket_fd,all_file,strlen(all_file));

    //printf("%s\n",all_file);

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
    name_len -= 1;
    //printf("name_len = %d\n",name_len);
    //从最后一个 / 的位置开始拷贝文件名
    char file_name[100];
    tail = &file_path[path_len];
    strncpy(file_name,tail - name_len, name_len);
    file_name[name_len] = '\0';
    //printf("file name = %s\n",file_name);

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
    unsigned char all_file[65535];
    send_one_data_package(socket_fd,buf,1);

    int r = recv_one_data_package(socket_fd,all_file,65535);

    printf("服务端有如下文件：\n%s\n",all_file);

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
    printf("好耶~连接客户端成功!\n");

    return socket_fd;

}


/*从服务端中下载多个文件
*/
int download_some_file(int socket_fd, const char* path, char* file_name[], int file_num){
    printf("一共有 %d 个文件需要下载\n",file_num);
    for(int i = 0; i < file_num; i++){
        // printf("file name = %s\n",file_name[i]);
        printf("正在下载 %s\n",file_name[i]);
        int ret = download_file(socket_fd,path,file_name[i]);
        if(ret == -1){
            printf("啊哦~服务端不存在文件%s\n",file_name[i]);
            return -1;
        }
    }

    return 0;
}
