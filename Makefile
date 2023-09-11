TARGEAT:=service_test

CC:=gcc

INC:=-I ./inc

SRCPATH:=./src
SERVICE_DIR:=./sercive

#根据路径得到 .o 文件列表
SRCNAME:=$(wildcard $(SRCPATH)/*.c)
SERVICENAME:=$(wildcard $(SERVICE_DIR)/*.c)

SERVICEOBJ:=$(patsubst %.c,%.o,$(SERVICENAME))
SRCOBJ:=$(patsubst %.c,%.o,$(SRCNAME))

$(TARGEAT):$(SRCOBJ) $(SERVICEOBJ)
	$(CC) $(SRCOBJ) $(SERVICEOBJ) -o $(TARGEAT) -pthread -l sqlite3

%.o : %.c
	$(CC) $(INC) -c $^ -o $@

clear:
	rm $(SRCOBJ) $(SERVICEOBJ)