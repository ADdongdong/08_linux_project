#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <string.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "locker.h"
#include <sys/uio.h>

#define READ_BUFFER_SIZE  2048  // 读缓冲区的大小
#define WRITE_BUFFER_SIZE  2048 // 写缓冲区的大小
#define FILENAME_LEN  200       // 文件名的最大长度

class http_conn {
public:

    /// HTTP请求方法，但我们只支持GET
    enum METHON {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};

    /*
        解析客户请求时，主状态机的状态:
        CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER:当前正在分析头部字段
        CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};

    //从状态机的三种可能状态，即, 行的读取状态，分别表示：
    //就是在解析http报文的时候，按行进行解析，每一行解析的情况
    // 1.读取到一个完成的行 LINE_OK
    // 2.行出错 LINE_BAD
    // 3.行数据尚且不完整 LINE_OPEN
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN};

    /*
        服务器处理HTTP请求的可能结果，报文解析的结果：
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDER_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求，获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};




    static int m_epollfd; // 所有的socket上的事件，全都被注册到一个epoll对象上
    static int m_user_count; // 统计用户数量

    http_conn(/* args */);
    ~http_conn();

    void process(); // 处理客户端的请求
    void init(int, const sockaddr_in&); //初始化新接受的链接
    void close_conn(); //关闭连接
    bool read(); //非阻塞读
    bool write(); //非阻塞写
    HTTP_CODE process_read(); // 解析http请求
    bool process_write(HTTP_CODE); // 响应
    HTTP_CODE parse_request_line(char* text); // 解析请求首行
    HTTP_CODE parse_headers(char* text); // 解析请求头
    HTTP_CODE parse_content(char* text); // 解析请求体

    LINE_STATUS parse_line(); // 获取一行数据
    HTTP_CODE do_request();
    
    

private:
    int m_sockfd; // 该HTTP链接的socket
    sockaddr_in m_address; // 通信的socket地址

    char m_read_buf[READ_BUFFER_SIZE]; //定义读缓冲区
    int m_read_idx; //标识读缓冲区中已经读入的客户端数据的最后一个字节的下一个位置

    int m_checked_index; // 当前正在分析的字符在读缓冲区的位置,
                         //因为要判断是否遇到了\r\n，如果遇到了，说明一行已经分析完了
    
    int m_start_line; // 当前正在解析的行的起始位置

    CHECK_STATE m_check_state;  // 主状态机当前所属的状态
    char* m_url;                //请求目标文件的文件名
    char* m_version;            // 协议版本，只支持HTTP1.1
    METHON m_method;            // 请求方法
    char* m_host;               // 主机名
    bool m_linger;              // HTTP请求是否保持连接
    int m_content_length;       // HTTP请求消息总长度
    char m_real_file[FILENAME_LEN]; // 客户请求的目标文件的完整路径，其内容等于doc_root + m_url, doc_root是网站根目录
    struct stat m_file_stat;    //目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否连续可读，并获取文件大小等信息
    char* m_file_address;       // 客户请求的目标文件被mmap到内存中的起始位置
    struct iovec m_iv[2];       // 我们将采用write来执行写操作，所以定义了下面两个成员，
    int m_iv_count;             // 其中，m_iv_count表示被写内存块的数量


    char m_write_buf[WRITE_BUFFER_SIZE]; // 定义写缓冲区
    int m_write_idx;                    // 写缓冲区中待发送的字节数

    int bytes_to_send;          // 将要发送的数据的字节数
    int bytes_have_send;        // 已经发送的字节数
    void init();                // 初始化连接其余的信息

    char* get_line() {return m_read_buf + m_start_line;}
    
    //这一组函数被process_write调用，以填充HTTP应答
    void unmap();  // 对内存映射执行munmap操作
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_content_type();
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();
};

#endif