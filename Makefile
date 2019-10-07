all:server
server:server.cc
	clang++ $^ -o $@
clean:
	rm -rf server
