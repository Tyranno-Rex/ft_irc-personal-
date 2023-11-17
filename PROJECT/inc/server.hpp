// #ifndef SERVER_HPP
// #define SERVER_HPP

/*
https://hyeonski.tistory.com/9

kevent()는 인자 kq로 전달된 kqueue에 새로 모니터링할 이벤트를 등록하고, 발생하여 아직 처리되지 않은(pending 상태인) 이벤트의 개수를 return한다. 
changelist는 kevent 구조체의 배열로, changelist 배열에 저장된 kevent 구조체(이벤트)들은 kqueue에 등록된다. nchanges는 등록할 이벤트의 개수이다.

struct kevent {
    uintptr_t ident;            identifier for this event
    int16_t   filter;           filter for event 
    uint16_t  flags;            action flags for kqueue 
    uint32_t  fflags;           filter flag value 
    intptr_t  data;             filter data value 
    void      *udata;           opaque user data identifier 
};

ident: event에 대한 식별자로, fd 번호가 입력된다.

filter: 커널이 event를 핸들링할 때 이용되는 필터이다. filter 또한 event의 식별자로 사용되며, 주로 이용되는 filter들은 다음과 같다.
    EVFILT_READ: ident의 fd를 모니터링하고, 읽을 수 있는 data가 있다면(read가 가능한 경우) event는 return된다. file descriptor의 종류에 따라 조금씩 다른 동작을 한다(socket, vnodes, fifo, pipe 등 - man page 참고).
    EVFILT_WRITE: ident의 fd에 write가 가능한 경우 event가 return된다.
    이 외 EVFILT_VNODE, EVFILT_SIGNAL 등 특수한 filter들이 있으며, man page에 자세한 설명이 있다.

flags: event를 적용시키거나, event가 return되었을 때 사용되는 flag이다.
    EV_ADD: kqueue에 event를 등록한다. 이미 등록된 event, 즉 식별자(ident, filter)가 같은 event를 재등록하는 경우 새로 만들지 않고 덮어씌워진다. 등록된 event는 자동으로 활성화된다.
    EV_ENABLE: kevent()가 event를 return할 수 있도록 활성화한다.
    EV_DISABLE: event를 비활성화한다. kevent()가 event를 return하지 않게 된다.
    EV_DELETE: kqueue에서 event를 삭제한다. fd를 close()한 경우 관련 event는 자동으로 삭제된다.
    etc...

fflags: filter에 따라 다르게 적용되는 flag이다.

data: filter에 따라 다르게 적용되는 data값이다. EVFILT_READ의 경우 event가 return되었을 때 data에는 read가 가능한 바이트 수가 기록된다.

udata: event와 함께 등록하여 event return시 사용할 수 있는 user-data이다. udata 또한 event의 식별자로 사용될 수 있다(optional - kevent64() 및 kevent_qos()는 인자 flags로 udata를 식별자로 사용할지 말지 결정할 수 있다).
*/

// void joinChannel(const std::string& username, const std::string& channelName) {
//     channels[channelName].join(username);
// }

// void kickUser(const std::string& kicker, const std::string& target, const std::string& channelName) {
//     auto& channel = channels[channelName];
//     channel.kick(kicker, target);
// }

// void sendMessage(const std::string& sender, const std::string& target, const std::string& message) {
//     std::cout << sender << " sends to " << target << ": " << message << std::endl;
// }

// void inviteUser(const std::string& inviter, const std::string& target, const std::string& channelName) {
//     auto& channel = channels[channelName];
//     const auto& users = channel.getUsers();
//     auto targetIt = std::find(users.begin(), users.end(), target);

//     if (targetIt != users.end()) {
//         std::cout << target << " is already in the channel" << std::endl;
//     } else {
//         std::cout << inviter << " invites " << target << " to the channel" << std::endl;
//     }
// }

// #endif // !SERVER_HPP

#include "channel.hpp"
#include "client.hpp"

class Server {
private:
    unsigned int            _port;  			// 서버가 listen할 포트 번호
    std::string             _password;  		// 서버에 접속하기 위한 비밀번호
    int                     _server_socket;  	// 서버 소켓의 파일 디스크립터
    std::string             _servername;  		// 서버의 이름
    std::map<int, Client>   _clients;  		    // 현재 서버에 접속한 클라이언트 목록
    std::vector<struct kevent> _change_list;    // kqueue에 등록된 변경 이벤트 리스트
    std::map<std::string, Channel *> _channels; // 서버에 등록된 채널 목록 (채팅방 등)
    // std::map<int, std::string> send_data;  	// 클라이언트에게 보낼 데이터 목록 

public:
    Server();
    ~Server();
    Server(unsigned int port, std::string password);

    void run();

    void setPort(unsigned int port);
    unsigned int getPort();
    void setServerSocket(int socket);
    int getServerSocket();
    void setServername(std::string &name);
    std::string getServername();
    std::map<int, Client> getClient();
    std::map<std::string, Channel*> getchannel();
    
    std::string func_pass(Client &client, std::stringstream &buffer_stream);
    std::string func_nick(Client &client, std::stringstream &buffer_stream);
    std::string func_user(Client &client, std::stringstream &buffer_stream);
    std::string func_quit(Client &client, std::stringstream &buffer_stream);
    std::string func_join(Client &client, std::stringstream &buffer_stream);
    std::string func_topic(Client &client, std::stringstream &buffer_stream);
    std::string func_kick(Client &client, std::stringstream &buffer_stream);
    std::string func_part(Client &client, std::stringstream &buffer_stream);
    std::string func_mode(Client &client, std::stringstream &buffer_stream);
    std::string func_name(Client &client, std::stringstream &buffer_stream);
    std::string func_privmsg(Client &client, std::stringstream &buffer_stream);
};
