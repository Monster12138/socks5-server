# socks5代理服务器
一个用C++实现的支持socks5协议的轻量级代理服务器，不依赖第三方库。

代理服务器目前仅支持MacOS和Linux操作系统。
当工作在MacOS上时，会使用clang++编译，IO多路复用使用[kqueue](https://www.freebsd.org/cgi/man.cgi?query=kqueue&sektion=2)。
当工作在Linux上时，会使用g++编译，IO多路复用使用[epoll](http://man7.org/linux/man-pages/man7/epoll.7.html)。

## 食用方式
本代理服务器需要配合代理客户端食用，例如Chrome浏览器的SwitchyOmega插件等。

## SwitchyOmega插件设置
![image](https://github.com/Monster12138/socks5-server/blob/master/SwitchyOmega-config.png)
**注：图中的IP和端口号需要改为代理服务器工作主机的外网IP地址+对应端口号。**

## 编译 
```
make
```

## 运行
```
./server ip port
```
或者
```
./server port
```
举个栗子: 
```
./server 0.0.0.0 1080
```
或者
```
./server 1080
```
如果在参数列表中没有提供要绑定的IP地址，程序会默认绑定到INADDR_ANY(0.0.0.0)。
端口号需要用户提供，如果使用1024以下的端口号，需要以管理员身份运行(sudo ...)。

## 存在的问题
1. 代理服务器使用单进程+多路复用的方式处理网络IO，所有的IO都是阻塞的。在连接一个未在监听状态/网络不通的上游服务器时，connect()系统调用会阻塞75秒(这个数值与系统的网络设置有关)。当发现程序阻塞在connecting xxx...时需要手动键入ctrl+c退出(或者等待75秒 - -!)。
2. 日志系统未完善，暂时不能指定日志等级
3. 只实现了不需要认证的method，未实现USERNAME/PASSWORD和GSSAPI的认证方式,详见[RFC1928](https://tools.ietf.org/html/rfc1928)
4. 不支持IPV6

## 联系我
使用过程中发生问题请联系838009062@qq.com

食用愉快~
