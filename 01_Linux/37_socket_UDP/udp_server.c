#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int main(int argc, char const *argv[])
{
    //1.创建一个socket
    int fd = socket(PF_INET, SOCK_DGRAM,  0);

    if (fd == -1){
        perror("socket");
        return -1;
    }

    //2.绑定
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9999);
    
    int ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1){
        perror("bind");
        return -1;
    }

    //3.通信
    while (1) {
        //接收数据
        char recvbuf[128];
        char ipbuf[16];

        struct sockaddr_in cliaddr;
        int len = sizeof(cliaddr);
        int num = recvfrom(fd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&cliaddr, &len);
        if (num == -1) {
            perror("recvfrom");
            return -1;
        }

        printf("client IP: %s, Port: %d\n",
                inet_ntop(AF_INET, &cliaddr.sin_addr.s_addr, ipbuf, sizeof(ipbuf)),
                ntohs(cliaddr.sin_port));

        printf("client say: %s\n", recvbuf);


        //发送数据
        sendto(fd, recvbuf, strlen(recvbuf) + 1, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));

    }
    
    close(fd);
    return 0;
}
