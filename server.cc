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


int 
main(int argc, const char * argv[]) {
    if(argc != 3) {
        cout << "Usage: program ip port" << endl;
        return 1;
    }
    
    int                               port, listenfd, newfd, kq, ev_number = 0;
    char                              buffer[1024] = { 0 };
    string                            ip;
    socklen_t                         socklen;
    sockaddr_in                       peer_addr;
    struct kevent                     events[1024], revents[1024];
    client_ctx_map                    mp;
    
    
    socklen = sizeof(sockaddr_in);
    ip = argv[1];
    port = atoi(argv[2]);

    listenfd = init_socket(ip, port);
    
    kq = kqueue();
    
    EV_SET(&events[ev_number++], listenfd,  EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void *)listenfd);
    while(1) {
        int nev = kevent(kq, events, ev_number, revents, ev_number, NULL);
        if(nev < 0) {
            cout << "kqueue error" << endl;
            break;
        } else if(nev == 0) {
            cout << "kqueue timeout..." << endl;
            continue;
        }

        for(int i = 0; i < nev; ++i) {
            int                 fd;
            struct kevent       kv = revents[i];

            fd = (long long)kv.udata;
            if(fd == listenfd) {
                accept_and_add(mp, events, fd, ev_number);

            } else {
                recv_and_process(mp, events, &kv, fd, ev_number);
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
        cout << "create sockfd error" << endl;
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
        cout << "bind error" << endl;
        exit(-1);
    }
    
    ret = listen(sockfd, 5);
    if(ret < 0) {
        cout << "listen error" << endl;
        exit(-1);
    }
    
    return sockfd;
}

void 
accept_and_add(client_ctx_map &mp, struct kevent* events, int listenfd, int &ev_number) {
    socks5_client_ctx      *ctx;

    ctx = new socks5_client_ctx;
    ctx->addr = (sockaddr *)malloc(sizeof(sockaddr_in));
    ctx->addr_len = sizeof(sockaddr_in);

    ctx->sockfd = accept(listenfd, ctx->addr, &ctx->addr_len);
    ctx->ip = inet_ntoa(((sockaddr_in *)(ctx->addr))->sin_addr);

    EV_SET(&events[ev_number++], ctx->sockfd, EVFILT_READ, 
           EV_ADD | EV_ENABLE, 0, 0, (void *)ctx->sockfd);

    mp[ctx->sockfd] = ctx;

    cout << "Client " << ctx->ip
         << " connected, fd is " << ctx->sockfd << endl; 
}

void
recv_and_process(client_ctx_map &mp ,struct kevent *events, 
                 struct kevent *kv, int sockfd, int &ev_number) {
    int                            ret;
    char                           buffer[BUFFER_SIZE];
    client_status                  status;
    socks5_client_ctx             *ctx;

    ctx = mp[sockfd];
    status = ctx->status;

    ret = socks5_recv(sockfd, buffer, BUFFER_SIZE, 0);
    if(ret <= 0 ) {
        close_and_delete(mp, sockfd, kv, ev_number, ctx);
        return;
    }

    switch(status) {
    case SHAKING_HANDS: 
        {
            ret = socks5_shake_hands(sockfd, buffer, BUFFER_SIZE);
            break;
        }
    case CONNECTING:
        {
            ret = socks5_connect(sockfd, buffer, BUFFER_SIZE, ctx);                        
            break;
        }
    case WORKING:
        {

            break;
        }
    default:
        {
            cout << "Client " << ctx->ip << " status error" << endl;
            ret = -1;
        }
    }

    if(ret < 0) {
        cout << "Close client " << ctx->ip << endl;
        close_and_delete(mp, sockfd, kv, ev_number, ctx);
    }
}

int
socks5_recv(int sockfd, char *buffer, int size, int flag) {
    int        ret;

    ret = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if(ret < 0) {
        cout << "sockfd " << sockfd << " recv error" << endl;
        return -1;
    } else if(ret == 0) {
        cout << "A client disconnected, sockfd is " << sockfd << endl;
        return -1;
    }

    return ret;
}

void
close_and_delete(client_ctx_map &mp, int fd, struct kevent *kv,
                 int &ev_number, socks5_client_ctx *ctx) {
    EV_SET(kv, fd, 0, EV_DELETE | EV_DISABLE, 0, 0, NULL);
    ::close(fd);
    delete ctx;
    mp.erase(fd);
    --ev_number;
}

int
socks5_shake_hands(int sockfd, char *buffer, int size) {
    shake_hands_request_t         req;
    shake_hands_response_t        res;

    cout << "sockfd " << sockfd << " client say: " << buffer << endl;
    if(parse_shake_hands(buffer, size, req) < 0) {
        return -1;
    }

    res.version = 0x05;
    res.method = choose_authentication_method(req);
    if(res.method < 0) {
        cout << "No acceptable methods" << endl;
        res.method = 0xff;
        send(sockfd, &res, sizeof(res), 0);
        return -1;
    }
    send(sockfd, &res, sizeof(res), 0);

    if(authentication(res.method) < 0) {
        cout << "Authentic failed, fd = " << sockfd << endl;
        return -1;
    }

    return 0;
}

int
parse_shake_hands(const char *buffer, int size, shake_hands_request_t &r) {
    int            len;
    const char    *p;
    
    len = strlen(buffer);
    
    if(*p != 0x05) {
        cerr << "version error" << endl;
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
socks5_connect(int sockfd, char *buffer, int size, socks5_client_ctx *ctx) {
    connect_response_t        *res;
    
    res = &ctx->conn_res;

    res->reply = parse_connect_request(buffer, size, ctx);
    if(res->reply == 0) {
        res->reply = connect_to_upstream(ctx);
    }

    res->version = 0x05;
    res->reserved = 0x00;
    res->address_type = 0x01;



    return 0;
}

char 
parse_connect_request(char *buffer, int size, socks5_client_ctx *ctx) {
    char                         *p;
    connect_request_t            *req;

    p = buffer;
    req = &ctx->conn_req;

    req->version = *p;
    if(req->version != 0x05) {
        cout << "Client " << ctx->ip 
             << " connect request: version error" << endl;
        return 0x02;
    }

    ++p;
    req->command = *p;
    if(req->command != 0x01 ||
       req->command != 0x02 ||
       req->command != 0x03) {
        cout << "Client " << ctx->ip
             << " connect request: command error" << endl;
        return 0x07;
    }

    ++p;
    req->reserved = *p;
    if(req->reserved != 0x00) {
        cout << "Client " << ctx->ip
             << " connect request: reserved error" << endl;
        return 0x02;
    }

    ++p;
    req->address_type = *p;
    if(req->address_type != 0x01 ||
       req->address_type != 0x03 ||
       req->address_type != 0x04) {
        cout << "Client " << ctx->ip
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
                cout << "Connect to " << req->dest_address << " failed" << endl;
                return 0x04;
            }
        }
    case 0x03:
        {
            /* TODO: support IPv6 */
        }
    case 0x04:
        {
            struct hostent        *ht;
            sockaddr_in           *peer_addr;

            ht = gethostbyname(req->dest_address.c_str());
            if(ht->h_addr == NULL) {
                cout << "Hostname " << req->dest_address << " DNS parse failed" << endl;
                return 0x04;
            }

            peer_addr = (sockaddr_in *)malloc(sizeof(sockaddr_in));

            upstream->addr = (sockaddr*)peer_addr;
            upstream->addr_len = sizeof(sockaddr_in);
            upstream->sockfd = socket(AF_INET, SOCK_STREAM, 0);

            for(int i = 0; ht->h_addr_list[i]; ++i) {
                peer_addr->sin_addr.s_addr = *(in_addr_t *)ht->h_addr_list[i];

                ret = connect(upstream->sockfd, upstream->addr, upstream->addr_len);
                if(ret == 0) {
                    break;
                }
            }

            if(ret < 0) {
                cout << "Connect to " << req->dest_address << " failed" << endl;
                return 0x04;
            }
        }
    default:
        {
            /* Never run here */
        }
    }
    
    return 0;
}
