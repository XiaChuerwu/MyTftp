TARGEAT:=client_test

CC:=gcc

INC:=-I ../inc

SRCPATH:=../src
CLIENT_DIR:=./
#根据路径得到 .o 文件列表
SRCNAME:=$(wildcard $(SRCPATH)/*.c)
CLIENTNAME:=$(wildcard $(CLIENT_DIR)/*.c)

CLIENTOBJ:=$(patsubst %.c,%.o,$(CLIENTNAME))
SRCOBJ:=$(patsubst %.c,%.o,$(SRCNAME))

#需要库的
$(TARGEAT):$(SRCOBJ) $(CLIENTOBJ)
	$(CC) $(SRCOBJ) $(CLIENTOBJ) -o $(TARGEAT) -pthread -l sqlite3

%.o : %.c
	$(CC) $(INC) -c $^ -o $@

clear:
	rm $(SRCOBJ) $(CLIENTOBJ)