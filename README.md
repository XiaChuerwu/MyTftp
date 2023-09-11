# 基于Socket编程的多功能聊天室
基于Socket编程的多功能聊天室，在与他人聊天的同时还能获取服务端的文件列表，以及上传文件到服务端或从服务端下载文件列表。

### 技术栈

- 使用**线程池**，大大减少了内存的开销，降低**内存碎片化**，大幅**提升**了服务端的**执行速度**
- 使用**维持平衡算法**，动态管理线程池中的空闲线程与增加新线程
- 使用**IO多路复用**，减少了**内存开销**和上下文切换的**CPU开销**
- 基于**TFTP**简单文件传输协议，与**Socket套接字编程**来实现文件的**下载**与**上传**
- 基于**Makefile**的多文件编译

### 项目中的文件

```
.
├── client
│   ├── client.c
│   ├── client_main.c
│   ├── client_test
│   └── Makefile
├── inc
│   ├── client.h
│   ├── commen.h
│   ├── service.h
│   └── threadpool.h
├── Makefile
├── README.md
├── sercive
│   ├── service.c
│   └── service_main.c
├── service_test
└── src
    ├── commen.c
    └── threadpool.c
```

4 directories, 15 files



### 项目运行方式

#### 服务端

在源文件根目录直接使用

```
Make
```

会生成 service_test 可执行二进制文件，接着

```
./service_test
```

#### 客户端

客户端需要cd进入client文件夹再 Make 同样也会在该目录生成可执行文件

### 文件上传与下载命令

-g 命令从服务端下载单个文件到运行目录下，格式如下

```
-g 文件名 服务端ip地址			// -g test.txt 192.168.1.1
```

-ng 命令从服务端下载多个文件到运行目录下(最多同时下载五个文件)，格式如下

```
-ng 文件列表 ip:服务端ip地址		// -ng 2.txt 1.txt 3.txt ip:192.168.1.1
```

-p 命令上传指定目录下的文件到服务端，格式如下

```
-p 指定目录下的文件名 服务端ip地址//-p /home/xxx/xxx/test.txt 192.168.1.1
```

-p 命令获取服务端运行目录下的文件列表，格式如下

```
-l 服务端ip地址				// -l 192.168.1.1
```
