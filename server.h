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
    
    socks5_upstream():
        addr(NULL)
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
    
    socks5_client_ctx(): 
        addr(NULL),
        status(SHAKING_HANDS)
    {}

    ~socks5_client_ctx() {
        if(addr != NULL) {
            free(addr);
        }
    }
};

typedef    std::map<int, socks5_client_ctx*>   client_ctx_map;

int        init_socket(std::string ip, int port);
void       accept_and_add(client_ctx_map &mp, struct kevent* events,
                         int listenfd, int &ev_number);
void       recv_and_process(client_ctx_map &mp ,struct kevent *events, 
                            struct kevent *kv, int sockfd, int &ev_number);
int        socks5_shake_hands(int sockfd, char *buffer, int length);
int        socks5_recv(int sockfd, char *buffer, int size, int flag);
void       close_and_delete(client_ctx_map &mp, int fd, struct kevent *kv,
                            int &ev_number, socks5_client_ctx *ctx);
int        parse_shake_hands(const char *buffer, int size, 
                             shake_hands_request_t &r);
char       choose_authentication_method(const shake_hands_request_t &r);
int        authentication(char method);
int        socks5_connect(int sockfd, char *buffer, int size,
                         socks5_client_ctx *ctx);
char       parse_connect_request(char *buffer, int size, 
                                 socks5_client_ctx *ctx);
int        connect_to_upstream(socks5_client_ctx *ctx);


#endif

