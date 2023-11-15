#include "../inc/server.hpp"

Server::Server(){};
Server::~Server(){};

Server::Server(unsigned int port, std::string password){
    this->_port = port;
    this->_password = password;
    this->_servername = "minjineunseong_IRC";
}

void Server::run() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        int client_socket = accept(_server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        // 클라이언트를 생성하고 소켓을 설정
        Client new_client(client_socket);

        // 클라이언트를 서버 목록에 추가
        _clients[client_socket] = new_client;

        // 여기에 클라이언트와의 통신 로직을 추가

        // 예시로 클라이언트에게 환영 메시지 보내기
        std::string welcome_message = "Welcome to the server!";
        send(client_socket, welcome_message.c_str(), welcome_message.size(), 0);
    }
}