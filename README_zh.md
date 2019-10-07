# socks5代理服务器
一个用C++实现的支持socks5协议的轻量级代理服务器。

这个代理服务器目前仅支持MacOS/FreeBSD系列操作系统，后续会更新对Linux操作系统的支持。

## 食用方式
本代理服务器需要配合代理客户端食用，例如Chrome浏览器的SwitchyOmega插件等。

## SwitchyOmega插件设置
![image](https://github.com/Monster12138/socks5-server/SwitchyOmega-config.png)

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
本程序使用单进程+多路复用的方式处理网络IO，所有的IO都是阻塞的。
所以在连接一个未在监听状态的上游服务器时，connect()系统调用会阻塞75s(这个数值与系统的网络设置有关)。

## 联系我
838009062@qq.com

就这些
祝您食用愉快~
