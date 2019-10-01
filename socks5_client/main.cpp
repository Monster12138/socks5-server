//
//  main.cpp
//  socks5_client
//
//  Created by zzz on 2019/9/16.
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

#include <iostream>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace std;

int init_socket(string ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        cout << "create sockfd error" << endl;
        exit(-1);
    }
    
    sockaddr_in peer;
    peer.sin_family  = AF_INET;
    peer.sin_addr.s_addr = inet_addr(ip.c_str());
    peer.sin_port = htons(port);
    
    socklen_t socklen;
    socklen = sizeof(sockaddr_in);
    
    int ret;
    ret = connect(sockfd, (sockaddr *)&peer, socklen);
    if(ret < 0) {
        cout << "connect error" << endl;
        exit(-1);
    }
    
    return sockfd;
}

int main(int argc, const char * argv[]) {
    // insert code here...
    if(argc < 2 || argc > 3) {
        cout << "Usage: program ip port" << endl;
        return 1;
    }
    
    int         port, sockfd;
    char        buffer[1024] = { 0 };
    string      ip;
    
    if(argc == 3) {
        ip = argv[1];
        port = atoi(argv[2]);
    } else {
        ip = "127.0.0.1";
        port = atoi(argv[1]);
    }
    
    sockfd = init_socket(ip, port);
    
    cin >> buffer;
    send(sockfd, buffer, strlen(buffer), 0);
    memset(buffer, 0, strlen(buffer));
    recv(sockfd, buffer, 1024, 0);
    cout << buffer << endl;
    
    ::close(sockfd);
    return 0;
}
