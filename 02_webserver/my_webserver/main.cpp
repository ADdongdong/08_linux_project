#include <stdio.h>
#include <iostream>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "http_conn.h"
#include "threadpool.h"

#define MAX_FD 65535 //最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 一次监听的最大的事件数量


//添加信号捕捉
void addsig(int sig, void(handler)(int)){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);//设置临时阻塞信号集
    sigaction(sig, &sa, NULL);
}

// 添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);

// 从epoll中删除文件描述符
extern int removefd(int epollfd, int fd);

// 修改文件描述符
extern void modfd(int epollfd, int fd, int ev);

int main(int argc, char const *argv[]) {
    
    if (argc <= 1) {
        printf("请按照如下格式运行：%s port_number\n", basename(argv[0]));
        exit(-1);
    }

    // 获取端口号
    int port = atoi(argv[1]);

    // 对SGPIE信号进行处理
    addsig(SIGPIPE, SIG_IGN);

    // 创建线程池，初始化线程池
    threadpool<http_conn>* pool = NULL;
    try{
        pool = new threadpool<http_conn>;
        //因为我们在threadpool中只定义了一个带默认参数的构造函数
        //所以，这样子就相当于直接调用了带参的默认构造函数，编译器就会找到那个构造函数并执行
    } catch(...) {
        exit(-1);
    }

    // 创建一个数组，用于保存所有的客户端信息
    http_conn * users = new http_conn[ MAX_FD ];

    // 创建监听的套接字
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    // 设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd, (struct sockaddr*)&address, sizeof(address));

    // 监听
    listen(listenfd, 5);

    // epoll代码
    // 创建epoll对象，事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);

    // 将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while (true) {
        // 这里的返回值num表示：检测到了几个事件发生了
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if ((num < 0) && (errno != EINTR)) {
            std::cout << "epoll failure" << std::endl;
            break;
        }

        //循环遍历事件数组
        for (int i = 0; i < num; i++) {
            int sockfd = events[i].data.fd;
            
            //有客户端连接进来
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlen);

                if (http_conn::m_user_count >= MAX_FD) {
                // 说明目前链接数满了，已经大于最大支持的链接数
                    // 给客户端写一个信息：服务器内部正忙。
                    close(connfd);
                    continue;
                }

                // 将新的客户的数据初始化，放到数组中
                users[connfd].init(connfd, client_address);

            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //对方异常断开或者错误等事件发生了
                users[sockfd].close_conn(); //关闭连接
            } else if (events[i].events & EPOLLIN) {
                //判断事件中是否有读事件发生
                if ( users[sockfd].read() ) {
                    //调用我们自己定义的read函数，一次性将数据全部读出来
                    pool->append(users + sockfd);
                } else {
                    users[sockfd].close_conn();
                }
            } else if(events[i].events & EPOLLOUT) {
                if ( !users[sockfd].write() ) {
                //一次性写完所有数据
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;

    return 0;
}
