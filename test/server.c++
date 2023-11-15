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
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "소켓 생성 실패" << std::endl;
        exit(1);
    }

    // 주소 설정
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    // 소켓과 주소를 바인딩
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "바인딩 실패" << std::endl;
        exit(1);
    }

    // 연결 대기
    if (listen(server_socket, 5) == -1) {
        std::cerr << "연결 대기 실패" << std::endl;
        exit(1);
    }

    std::cout << "서버가 " << server_ip << ":" << server_port << "에서 대기 중..." << std::endl;

    // 클라이언트 연결 대기
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_socket == -1) {
        std::cerr << "클라이언트 연결 실패" << std::endl;
        exit(1);
    }

    std::cout << "클라이언트가 연결되었습니다." << std::endl;

    while (true) {
        char buffer[1024];
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            break; // 연결 종료 시 루프를 빠져나갑니다.
        }

        // 수신된 데이터 출력
        buffer[bytes_received] = '\0';
        std::cout << "클라이언트로부터 받은 데이터: " << buffer << std::endl;

        // 클라이언트에 응답 보내기
        const char* response = "서버가 데이터를 받았습니다.";
        send(client_socket, response, strlen(response), 0);
    }

    // 연결 종료
    close(client_socket);
    close(server_socket);

    return 0;
}
