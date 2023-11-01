#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 512

void error_handling(char* message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int main(int argc, char* argv[])
{
    int serv_sock;
    int clnt_sock;

    // sockaddr은 주소정보를 담는 기본형태이다.
    // 주소체계에 따라 sockaddr_in, sockaddr_un, sockaddr_in6 등을 sockaddr로 형변환해서 사용하면 편리하다.
    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;

    // TCP연결, IPv4 도메인을 위한 소켓 생성
    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket error");
    
    //서버의 주소 정보 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; // 주소패밀리
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 서버의 ip주소
    serv_addr.sin_port = htons(atoi(argv[1])); // 서버 프로그램의 포트
    //htonl, htos - 빅엔디안 타입으로 변환

    // 소켓과 주소정보를 결합
    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind error");
    
    // 연결요청 대기
    if (listen(serv_sock, 5) == -1)
        error_handling("listen error");
    
    // 연결 수락
    clnt_addr_size = sizeof(clnt_addr);
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
    if (clnt_sock == -1)
        error_handling("accept error");

    while (1)
    {
        char* buf[BUF_SIZE];

        // 클라이언트 데이터 읽기
        if (read(clnt_sock, buf, BUF_SIZE) <= 0)
        {
            close(clnt_sock);
            printf("client disconnected\n");
            break;
        }
        // 클라이언트에게 데이터 전송
        else
        {
            write(clnt_sock, buf, strlen((const char*)buf));
        }
        
        memset(buf, 0, BUF_SIZE);
    }
    close(serv_sock);

    return 0;
}