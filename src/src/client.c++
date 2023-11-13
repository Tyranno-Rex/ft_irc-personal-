#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    // 서버 설정
    const char* server_ip = "127.0.0.1";
    const int server_port = 12345;

    // 소켓 생성
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "소켓 생성 실패" << std::endl;
        exit(1);
    }

    // 서버에 연결
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "서버 연결 실패" << std::endl;
        exit(1);
    }

    std::cout << "서버에 연결되었습니다." << std::endl;

    while (true) {
        // 사용자 입력 받기
        std::cout << "메시지를 입력하세요: ";
        std::string message;
        std::getline(std::cin, message);

        // 사용자 입력을 서버로 전송
        send(client_socket, message.c_str(), message.length(), 0);

        // 서버로부터 응답 받기
        char response[1024];
        ssize_t bytes_received = recv(client_socket, response, sizeof(response), 0);
        if (bytes_received <= 0) {
            break; // 연결 종료 시 루프를 빠져나갑니다.
        }

        response[bytes_received] = '\0';
        std::cout << "서버 응답: " << response << std::endl;
    }

    // 연결 종료
    close(client_socket);

    return 0;
}
