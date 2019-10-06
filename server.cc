//
//  main.cpp
//  sock5_server
//
//  Created by zzz on 2019/9/15.
//  Copyright © 2019 zzz. All rights reserved.
//

#if 0
1. 客户端发起握手
+----+----------+----------+
|VER | NMETHODS | METHODS  |
+----+----------+----------+
| 1  |    1     | 1 to 255 |
+----+----------+----------+
2. 服务器回应握手
+----+--------+
|VER | METHOD |
+----+--------+
| 1  |   1    |
+----+--------+
3. 客户端发起连接
+----+-----+-------+------+----------+----------+
|VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
+----+-----+-------+------+----------+----------+
| 1  |  1  | X'00' |  1   | Variable |    2     |
+----+-----+-------+------+----------+----------+
4. 服务器回应连接
+----+-----+-------+------+----------+----------+
|VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
+----+-----+-------+------+----------+----------+
| 1  |  1  | X'00' |  1   | Variable |    2     |
+----+-----+-------+------+----------+----------+
#endif

#include "server.h"


using namespace std;


int                               ev_number = 0;
struct kevent                     events[1024];

int 
main(int argc, const char * argv[]) {
    if(argc != 2 && argc != 3) {
        cout << "Usage: program ip port" << endl;
        return 1;
    }
    
    int                               port, listenfd, newfd, kq;
    char                              buffer[1024] = { 0 };
    string                            ip;
    socklen_t                         socklen;
    sockaddr_in                       peer_addr;
    struct kevent                     revents[1024];
    client_ctx_map                    mp;
    
    
    socklen = sizeof(sockaddr_in);
    if(argc == 3) {
        ip = argv[1];
        port = atoi(argv[2]);

    } else {
        port = atoi(argv[1]);
    }

    listenfd = init_socket(ip, port);
    
    kq = kqueue();
    
    EV_SET(&events[ev_number++], listenfd,  EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void *)listenfd);
    while(1) {
        cout << "[debug] kqueue wait..." << endl;
        int nev = kevent(kq, events, ev_number, revents, ev_number, NULL);
        if(nev < 0) {
            cout << "[error] kqueue error" << endl;
            break;
        } else if(nev == 0) {
            cout << "[debug] kqueue timeout..." << endl;
            continue;
        }

        for(int i = 0; i < nev; ++i) {
            int                 fd;
            struct kevent       kv = revents[i];

            fd = (long long)kv.udata;
            if(fd == listenfd) {
                accept_and_add(mp, fd);

            } else {
                recv_and_process(mp, &kv, fd);
            }
        }
    }

    ::close(listenfd);
    return 0;
}

int 
init_socket(string ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        cout << "[error] create sockfd error" << endl;
        exit(-1);
    }
    
    sockaddr_in local;
    local.sin_family  = AF_INET;
    if(ip == "") {
        local.sin_addr.s_addr = INADDR_ANY;
    } else {
        local.sin_addr.s_addr = inet_addr(ip.c_str());
    }
    local.sin_port = htons(port);
    
    socklen_t socklen;
    socklen = sizeof(sockaddr_in);
    
    int ret;
    ret = ::bind(sockfd, (sockaddr *)&local, socklen);
    if(ret < 0) {
        cout << "[error] bind error" << endl;
        exit(-1);
    }
    
    ret = listen(sockfd, 5);
    if(ret < 0) {
        cout << "[error] listen error" << endl;
        exit(-1);
    }
    
    return sockfd;
}

void 
accept_and_add(client_ctx_map &mp, int listenfd) {
    socks5_client_ctx      *ctx;

    ctx = new socks5_client_ctx;
    ctx->addr = (sockaddr *)malloc(sizeof(sockaddr_in));
    ctx->addr_len = sizeof(sockaddr_in);

    ctx->sockfd = accept(listenfd, ctx->addr, &ctx->addr_len);
    ctx->ip = inet_ntoa(((sockaddr_in *)(ctx->addr))->sin_addr);

    EV_SET(&events[ev_number++], ctx->sockfd, EVFILT_READ, 
           EV_ADD | EV_ENABLE, 0, 0, (void *)ctx->sockfd);

    mp[ctx->sockfd] = ctx;

    cout << "[debug] Client " << ctx->ip
         << " connected, fd is " << ctx->sockfd << endl; 
}

void
recv_and_process(client_ctx_map &mp , struct kevent *kv, int sockfd) {
    int                            ret;
    client_status                  status;
    socks5_client_ctx             *ctx;

    ctx = mp[sockfd];
    status = ctx->status;
    
    ret = socks5_recv(sockfd, 0, ctx);
    if(ret <= 0 ) {
        close_and_delete(mp, sockfd, kv, ctx);
        return;
    }

    switch(status) {
    case SHAKING_HANDS: 
        {
            ret = socks5_shake_hands(sockfd, ctx);
            ctx->status = CONNECTING;
            break;
        }
    case CONNECTING:
        {
            ret = socks5_connect(sockfd, ctx, mp);
            ctx->status = WORKING;
            break;
        }
    case WORKING:
        {
            if(sockfd == ctx->sockfd) {
                ret = socks5_forward(ctx->upstream.sockfd, ctx->sockfd, ctx);
            } else {
                ret = socks5_forward(ctx->sockfd, ctx->upstream.sockfd, ctx);
            }
            break;
        }
    default:
        {
            cout << "[error] Client " << ctx->ip << " status error" << endl;
            ret = -1;
        }
    }

    if(ret < 0) {
        cout << "[debug] Close client " << ctx->ip << endl;
        close_and_delete(mp, sockfd, kv, ctx);
    }
}

int
socks5_send(int sockfd, void *buffer, int size, int flag) {
    int        ret;

    ret = send(sockfd, buffer, size, 0);
    if(ret < 0) {
        cout << "[error] sockfd " << sockfd << " send error" << endl;
        return -1;
    } else {
        cout << "[debug] fd " << sockfd << " send " << ret << " bytes data" << endl;
    }

    return ret;
}

int
socks5_recv(int sockfd, int flag, socks5_client_ctx *ctx) {
    int        ret, *size, *capacity;
    char     **buffer;

    buffer = &ctx->buffer;
    size = &ctx->size;
    capacity = &ctx->capacity;


    while(1) {
        if(*size == *capacity) {
            *buffer = buffer_expansion(*buffer, *capacity);
            if(*buffer == NULL) {
                return -1;
            }
            *capacity *= 2;
        }

        ret = recv(srcfd, *buffer + size, *capacity - *size, 0);
        if(ret < 0) {
            cout << "[error] sockfd " << sockfd << " recv error" << endl;
            return -1;
        } else if(ret == 0) {
            cout << "[debug] A client disconnected, sockfd is " << sockfd << endl;
            return -1;
        } else {
            cout << "[debug] fd " << sockfd << " recv " << ret << " bytes data" << endl;
        }

        *size += ret;
        if(*size != *capacity) {
            break;
        }
    }

    return *size;
}

void
close_and_delete(client_ctx_map &mp, int fd, struct kevent *kv,
                 socks5_client_ctx *ctx) {
    EV_SET(kv, fd, 0, EV_DELETE | EV_DISABLE, 0, 0, NULL);
    if(ctx->upstream.connected) {
        ::close(ctx->upstream.sockfd);
        mp.erase(ctx->upstream.sockfd);
        EV_SET(kv, ctx->upstream.sockfd, 0, EV_DELETE | EV_DISABLE, 0, 0, NULL);
    }
    ::close(fd);
    mp.erase(fd);

    delete ctx;
    --ev_number;
}

int
socks5_shake_hands(int sockfd, socks5_client_ctx *ctx) {
    int                           ret;
    char                         *buffer;
    shake_hands_request_t         req;
    shake_hands_response_t        res;

    buffer = ctx->buffer;

    cout << "[debug] shake hands: ";
    for(int i = 0; i < 3; ++i) {
        printf("%#X ", *(buffer + i));
    }
    cout << endl;

    if(parse_shake_hands(ctx,  req) < 0) {
        return -1;
    }
    ctx->size = 0;

    res.version = 0x05;
    res.method = choose_authentication_method(req);
    if(res.method < 0) {
        cout << "[debug] No acceptable methods" << endl;
        res.method = 0xff;
        ret = socks5_send(sockfd, &res, sizeof(res), 0);
        if(ret < 0) {
            return -1;
        }
        return -1;
    }
    ret = socks5_send(sockfd, &res, sizeof(res), 0);
    if(ret < 0) {
        return -1;
    }

    if(authentication(res.method) < 0) {
        cout << "[error] Authentic failed, fd = " << sockfd << endl;
        return -1;
    }

    return 0;
}

int
parse_shake_hands(socks5_client_ctx *ctx, shake_hands_request_t &r) {
    const char    *p;
    
    if(ctx->size < 3) {
        return -1;
    }

    p = ctx->buffer;
    
    if(*p != 0x05) {
        printf("[error] client %s:%d version error(%#x)\n",
               ctx->ip.c_str(), ctx->port, *p);
        return -1;
    }

    r.version = *p;
    ++p;
    
    r.nmethods = *p;
    ++p;

    for(char i = 0x0; i < r.nmethods; ++i) {
        r.methods[i] = *(p + i);
    }

    return 0;
}

char
choose_authentication_method(const shake_hands_request_t &r) {
    return 0x00;
}

int
authentication(char method) {
    return 0;
}

int
socks5_connect(int sockfd, socks5_client_ctx *ctx, client_ctx_map &mp) {
    int                        len, ret;
    char                       send_buffer[BUFFER_SIZE];
    connect_response_t        *res;
    
    res = &ctx->conn_res;

    res->reply = parse_connect_request(ctx);
    if(res->reply == 0) {
        res->reply = connect_to_upstream(ctx);
    }
    ctx->size = 0;

    if(res->reply == 0) {
        ctx->upstream.connected = true;
        mp[ctx->upstream.sockfd] = ctx;
        EV_SET(&events[ev_number++], ctx->upstream.sockfd, EVFILT_READ, 
               EV_ADD | EV_ENABLE, 0, 0, (void *)ctx->upstream.sockfd);
    }

    res->version = 0x05;
    res->reserved = 0x00;
    res->address_type = 0x01;
    res->bind_address = "\0\0\0\0";
    res->bind_port = 0x0000;

    len = connect_response_serialization(res, send_buffer, BUFFER_SIZE);
    
    ret = socks5_send(sockfd, send_buffer, len, 0);
    if(ret < 0) {
        return -1;
    }
    
    return 0;
}

int
connect_response_serialization(connect_response_t *res, char *buffer, int size) {
    int        byte_count;
    char      *p;

    byte_count = 0;
    p = buffer;

    memcpy(p, res, 4);
    p += 4;
    byte_count += 4;

    switch(res->address_type) {
    case 0x01:
        {
            //memcpy(p, res->bind_address.c_str(), 4);
            memset(p, 0, 4);
            p += 4;
            byte_count += 4;
            break;
        }
    case 0x03:
        {
            memcpy(p, res->bind_address.c_str(), res->bind_address.size());
            p += res->bind_address.size();
            byte_count += res->bind_address.size();
            break;
        }
    case 0x04:
        {
            //memcpy(p, res->bind_address.c_str(), 16);
            memset(p, 0, 16);
            p += 16;
            byte_count += 16;
            break;
        }
    default:
        {
            /* Never run here */
        }
    }
    
    memcpy(p, &res->bind_port, 2);
    byte_count += 2;

    return byte_count;
}

char 
parse_connect_request(socks5_client_ctx *ctx) {
    const char                   *p;
    connect_request_t            *req;

    p = ctx->buffer;
    req = &ctx->conn_req;

    cout << "[debug] connect request: ";
    for(int i = 0; i < 4; ++i) {
        printf("%#X ", *(buffer + i));
    }
    cout << endl;

    req->version = *p;
    if(req->version != 0x05) {
        cout << "[error] Client " << ctx->ip 
             << " connect request: version error" << endl;
        return 0x02;
    }

    ++p;
    req->command = *p;
    if(req->command != 0x01 &&
       req->command != 0x02 &&
       req->command != 0x03) {
        cout << "[error] Client " << ctx->ip
             << " connect request: command error" << endl;
        return 0x07;
    }

    ++p;
    req->reserved = *p;
    if(req->reserved != 0x00) {
        cout << "[error] Client " << ctx->ip
             << " connect request: reserved error" << endl;
        return 0x02;
    }

    ++p;
    req->address_type = *p;
    if(req->address_type != 0x01 &&
       req->address_type != 0x03 &&
       req->address_type != 0x04) {
        cout << "[error] Client " << ctx->ip
             << " connect request: address type error" << endl;
        return 0x08;
    }

    ++p;
    switch(req->address_type) {
    case 0x01:
        {
            char ip[INET_ADDRSTRLEN] = {0};
            req->dest_address = inet_ntop(AF_INET, p, ip, INET_ADDRSTRLEN);
            p += 4;
             
            break;
        }
    case 0x03:
        {
            int len = *p;
            ++p;
            req->dest_address.assign(string(p, len));
            p += len;
            break;
        }
    case 0x04:
        {
            char        ip[INET6_ADDRSTRLEN] = {0};
            req->dest_address = inet_ntop(AF_INET6, p, ip, INET6_ADDRSTRLEN);
            p += 16;
            break;
        }
    default:
        {
            /* Never run here */
        }
    }

    req->dest_port = *(short *)p;

    return 0x00;
}

int
connect_to_upstream(socks5_client_ctx *ctx) {
    int                       ret;
    socks5_upstream          *upstream;
    connect_request_t        *req;

    upstream = &ctx->upstream;
    req = &ctx->conn_req;
    
    switch(req->address_type) {
    case 0x01:
        {
            sockaddr_in           *peer_addr;

            peer_addr = (sockaddr_in *)malloc(sizeof(sockaddr_in));
            upstream->addr = (sockaddr*)peer_addr;
            upstream->addr_len = sizeof(sockaddr_in);

            peer_addr->sin_addr.s_addr = inet_addr(req->dest_address.c_str());
            
            upstream->sockfd = socket(AF_INET, SOCK_STREAM, 0);
            ret = connect(upstream->sockfd, upstream->addr, upstream->addr_len);
            if(ret < 0) {
                cout << "[error] Connect to " << req->dest_address << " failed" << endl;
                return 0x04;
            }
            break;
        }
    case 0x03:
        {
            char                  *ip;
            struct hostent        *ht;
            sockaddr_in           *peer_addr;


            ht = gethostbyname(req->dest_address.c_str());
            if(ht->h_addr == NULL) {
                cout << "[error] Hostname " << req->dest_address << " DNS parse failed" << endl;
                return 0x04;
            }

            peer_addr = (sockaddr_in *)malloc(sizeof(sockaddr_in));

            upstream->addr = (sockaddr*)peer_addr;
            upstream->addr_len = sizeof(sockaddr_in);
            upstream->sockfd = socket(AF_INET, SOCK_STREAM, 0);

            for(int i = 0; ht->h_addr_list[i]; ++i) {
                ip = inet_ntoa(*(in_addr*)ht->h_addr_list[i]);
                cout << "[debug] connecting " << ip << "..." << endl;
                peer_addr->sin_family = AF_INET;
                peer_addr->sin_addr.s_addr = inet_addr(ip);
                peer_addr->sin_port = htons(80);

                ret = connect(upstream->sockfd, upstream->addr, upstream->addr_len);
                if(ret == 0) {
                    break;
                }
                cout << "[error] connect " << ip << " failed" << endl;
            }

            if(ret < 0) {
                cout << "[error] Connect to " << req->dest_address << " failed" << endl;
                return 0x04;
            }
            break;
        }
    case 0x04:
        {
            /* TODO: support IPv6 */
            break;
        }
    default:
        {
            /* Never run here */
        }
    }
    
    return 0;
}

int
socks5_forward(int dstfd, int srcfd, socks5_client_ctx *ctx) {
    int          capacity, size, ret;
    char        *buffer;

    cout << "[debug] forward: fd " << srcfd << "->" << " fd " << dstfd << endl;
    
    capacity = ctx->capacity;
    size = ctx->size;
    buffer = ctx->buffer;

    ret = socks5_send(dstfd, buffer, size, 0);
    if(ret < 0) {
        return -1;
    }
    
    ctx->size = 0;

    return 0;
}

char *
buffer_expansion(char *buffer, int capacity) {
    int        new_capacity;

    new_capacity = 2 * capacity;
    buffer = (char *)reallocf(buffer, new_capacity);

    return buffer;
}
