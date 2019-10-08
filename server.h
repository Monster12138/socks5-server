#ifndef SERVER_H
#define SERVER_H

#ifdef _WIN32 // note the underscore: without it, it's not msdn official!
    // Windows (x64 and x86)
#elif __unix__ // all unices
    // Unix
#elif __posix__
    // POSIX
#elif __linux__
    // linux
#elif __APPLE__
    // Mac OS, not sure if this is covered by __posix__ and/or __unix__ though...
#endif

#include <iostream>
#include <string>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef __APPLE__
#include <sys/event.h>
#elif __linux__
#include <sys/epoll.h>
#endif

#define        INIT_BUFFER_SIZE         1024

#define        SOCKS5_VERSION           0x05
#define        SOCKS5_RESERVED          0x00

/*
o  X'00' NO AUTHENTICATION REQUIRED
o  X'01' GSSAPI
o  X'02' USERNAME/PASSWORD
o  X'03' to X'7F' IANA ASSIGNED
o  X'80' to X'FE' RESERVED FOR PRIVATE METHODS
o  X'FF' NO ACCEPTABLE METHODS;
*/
#define        METHOD_NO_AUTHEN_REQUIRED         0x00
#define        METHOD_GSSAPI                     0x01
#define        METHOD_USERNAME_PASSWORD          0x02
#define        METHOD_NO_ACCEPTABLE              0xff

#define        CMD_CONNECT                       0x01
#define        CMD_BIND                          0x02
#define        CMD_UDP_ASSOCIATE                 0x03

#define        ATYP_IPV4                         0x01
#define        ATYP_DOMANNAME                    0x03
#define        ATYP_IPV6                         0x04

#define        REP_SUCCEEDED                     0x00
#define        REP_SOCKS_SERVER_FAILURE          0x01
#define        REP_CONNECT_NOT_ALLOWED           0x02
#define        REP_NETWORK_NOREACHABLE           0x03
#define        REP_HOST_NOREACHABLE              0x04
#define        REP_CONNECTION_REFUSED            0x05
#define        REP_TTL_EXPIRED                   0x06
#define        REP_COMMAND_NOT_SUPPORTED         0x07
#define        REP_ATYP_NOT_SUPPORTED            0x08



typedef        unsigned char        u_char;
/*
 *+----+----------+----------+
 *|VER | NMETHODS | METHODS  |
 *+----+----------+----------+
 *| 1  |    1     | 1 to 255 |
 *+----+----------+----------+
 */
struct shake_hands_request_t {
    u_char        version;
    u_char        nmethods;
    u_char        methods[8];
};

/*
 *+----+--------+
 *|VER | METHOD |
 *+----+--------+
 *| 1  |   1    |
 *+----+--------+
 */
struct shake_hands_response_t {
    u_char        version;
    u_char        method;
};

/*
 *+----+-----+-------+------+----------+----------+
 *|VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
 *+----+-----+-------+------+----------+----------+
 *| 1  |  1  | X'00' |  1   | Variable |    2     |
 *+----+-----+-------+------+----------+----------+
 */
struct connect_request_t {
    u_char               version;
    u_char               command;
    u_char               reserved;                     /* must be 0x00 */
    u_char               address_type;
    std::string          dest_address;
    short                dest_port;
};

/*
 *+----+-----+-------+------+----------+----------+
 *|VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
 *+----+-----+-------+------+----------+----------+
 *| 1  |  1  | X'00' |  1   | Variable |    2     |
 *+----+-----+-------+------+----------+----------+
 */
struct connect_response_t {
    u_char               version;
    u_char               reply;
    u_char               reserved;                     /* must be 0x00 */
    u_char               address_type;
    std::string          bind_address;
    short                bind_port;
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
    u_char                    *buffer;
    
    socks5_client_ctx(): 
        addr(NULL),
        status(SHAKING_HANDS),
        capacity(INIT_BUFFER_SIZE),
        size(0),
        buffer((u_char *)malloc(capacity))
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
int        socks5_event_init();
int        socks5_event_add(int evfd, int fd, void *data);
int        socks5_event_del(int evfd, int fd);
void       accept_and_add(int evfd, client_ctx_map &mp, int listenfd);
void       recv_and_process(int evfd, client_ctx_map &mp , int sockfd);
int        socks5_shake_hands(int sockfd, socks5_client_ctx *ctx);
int        socks5_send(int sockfd, void *buffer, int size, int flag);
int        socks5_recv(int sockfd, int flag, socks5_client_ctx *ctx);
void       close_and_delete(client_ctx_map &mp, socks5_client_ctx *ctx);
int        parse_shake_hands(socks5_client_ctx *ctx, shake_hands_request_t &r);
u_char     choose_authentication_method(const shake_hands_request_t &r);
int        authentication(u_char method);
int        socks5_connect(int evfd, int sockfd, socks5_client_ctx *ctx,
                          client_ctx_map &mp);
int        connect_response_serialization(connect_response_t *res, 
                                          socks5_client_ctx *ctx);
u_char     parse_connect_request(socks5_client_ctx *ctx);
int        connect_to_upstream(socks5_client_ctx *ctx);
int        socks5_forward(int dstfd, int srcfd, socks5_client_ctx *ctx);
u_char*    buffer_expansion(u_char *buffer, int capacity);


#endif

