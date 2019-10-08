OS_NAME = $(shell uname)
LC_OS_NAME = $(shell echo $(OS_NAME) | tr '[A-Z]' '[a-z]')

ifeq ($(LC_OS_NAME), linux)
CC = "g++"
CUR_OS = "current os is linux, use g++"
else
CC = "clang++"
CUR_OS = "current os is Macos, use clang++"
endif

all:checkos server

checkos:
	echo $(CUR_OS)

server:server.cc
	$(CC) $^ -o $@

clean:
	rm -rf server
