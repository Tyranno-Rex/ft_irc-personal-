#include "../inc/server.hpp"
#include "../inc/client.hpp"
#include "../inc/channel.hpp"/


int main() {
    // 서버 정보 설정
    const char* server_ip = "127.0.0.1";  // 서버 IP 주소
    const int server_port = 12345;         // 서버 포트 번호

    // 소켓 생성
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return -1;
    }

    // 서버 정보 구조체 초기화
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return -1;
    }

    // 서버에 연결
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Connection failed" << std::endl;
        return -1;
    }

    // 서버로 메시지 전송
    const char* message = "Hello, server!";
    send(client_socket, message, strlen(message), 0);

    // 서버에서 메시지 수신
    char buffer[1024] = {0};
    recv(client_socket, buffer, sizeof(buffer), 0);
    std::cout << "Server response: " << buffer << std::endl;

    // 소켓 닫기
    close(client_socket);

    return 0;
}