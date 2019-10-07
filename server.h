#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <string>
#include <map>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

#define    BUFFER_SIZE     1024

/*
 *+----+----------+----------+
 *|VER | NMETHODS | METHODS  |
 *+----+----------+----------+
 *| 1  |    1     | 1 to 255 |
 *+----+----------+----------+
 */
struct shake_hands_request_t {
    char        version;
    char        nmethods;
    char        methods[8];
};

/*
 *+----+--------+
 *|VER | METHOD |
 *+----+--------+
 *| 1  |   1    |
 *+----+--------+
 */
struct shake_hands_response_t {
    char        version;
    char        method;
};

/*
 *+----+-----+-------+------+----------+----------+
 *|VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
 *+----+-----+-------+------+----------+----------+
 *| 1  |  1  | X'00' |  1   | Variable |    2     |
 *+----+-----+-------+------+----------+----------+
 */
struct connect_request_t {
    char               version;
    char               command;
    char               reserved;                     /* must be 0x00 */
    char               address_type;
    std::string        dest_address;
    int                dest_port;
};

/*
 *+----+-----+-------+------+----------+----------+
 *|VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
 *+----+-----+-------+------+----------+----------+
 *| 1  |  1  | X'00' |  1   | Variable |    2     |
 *+----+-----+-------+------+----------+----------+
 */
struct connect_response_t {
    char               version;
    char               reply;
    char               reserved;                     /* must be 0x00 */
    char               address_type;
    std::string        bind_address;
    int                bind_port;
};

class socks5_upstream {
public:
    int                sockfd;
    sockaddr          *addr;
    socklen_t          addr_len;
    bool               connected;
    
    socks5_upstream():
        addr(NULL),
        connected(false)
    {}

    ~socks5_upstream() {
        if(addr != NULL) {
            free(addr);
        }
    }
};

enum client_status {
    SHAKING_HANDS,
    CONNECTING,
    WORKING
};

class socks5_client_ctx {
public:
    std::string                ip;
    int                        port;
    int                        sockfd;
    sockaddr                  *addr;
    socklen_t                  addr_len;

    int                        bind_fd;
    connect_request_t          conn_req;
    connect_response_t         conn_res;
    client_status              status;

    socks5_upstream            upstream;

    int                        capacity;;
    int                        size;
    char                      *buffer;
    
    socks5_client_ctx(): 
        addr(NULL),
        status(SHAKING_HANDS),
        capacity(BUFFER_SIZE),
        size(0),
        buffer((char *)malloc(capacity))
    {}

    ~socks5_client_ctx() {
        if(addr != NULL) {
            free(addr);
        }
        
        if(buffer != NULL) {
            free(buffer);
            printf("[debug] free buffer %p\n", buffer);
        }
    }
};

typedef    std::map<int, socks5_client_ctx*>   client_ctx_map;

int        init_socket(std::string ip, int port);
int        kqueue_register(int kq, int fd, void *data);
void       accept_and_add(int kq, client_ctx_map &mp, int listenfd);
void       recv_and_process(int kq, client_ctx_map &mp , struct kevent *kv, int sockfd);
int        socks5_shake_hands(int sockfd, socks5_client_ctx *ctx);
int        socks5_send(int sockfd, void *buffer, int size, int flag);
int        socks5_recv(int sockfd, int flag, socks5_client_ctx *ctx);
void       close_and_delete(client_ctx_map &mp, socks5_client_ctx *ctx);
int        parse_shake_hands(socks5_client_ctx *ctx, shake_hands_request_t &r);
char       choose_authentication_method(const shake_hands_request_t &r);
int        authentication(char method);
int        socks5_connect(int kq, int sockfd, socks5_client_ctx *ctx,
                          client_ctx_map &mp);
int        connect_response_serialization(connect_response_t *res,
                                          char *buffer, int size);
char       parse_connect_request(socks5_client_ctx *ctx);
int        connect_to_upstream(socks5_client_ctx *ctx);
int        socks5_forward(int dstfd, int srcfd, socks5_client_ctx *ctx);
char*      buffer_expansion(char *buffer, int capacity);


#endif

