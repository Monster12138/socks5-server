# socks5-server
A proxy server for socks5 protocol with c++

This proxy server currently only supports MacOS/FreeBSD operating system.
Linux support will be updated in the future.

## use
This proxy server needs to be used with the proxy client, such as the Chrome browser plug-in SwitchyOmega.

### SwitchyOmega config
![image](https://github.com/Monster12138/socks5-server/SwitchyOmega-config.png)

## Build 
```
make
```

## Run
```
./server ip port
```
or
```
./server port
```
E.g: 
```
./server 0.0.0.0 1080
```
or 
```
./server 1080
```
If the bound ip address is not provided in the command line argument, the program will bind INADDR_ANY (0.0.0.0) by default.

## Existing problem
Currently all IO operations are blocked, and IO events are handled in a single process in a multiplexed manner.
The connect() system call blocks for 75 seconds when connecting to an upstream server that is not listening.

## E_mail
838009062@qq.com

All done.
Have a nice ... day(night)
Enjoy it:)
