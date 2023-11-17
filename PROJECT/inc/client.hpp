// #ifndef CLIENT_HPP
// #define CLIENT_HPP
#include "channel.hpp"
#include "server.hpp"

class Client {
private:
    int         _socket;
    std::string _buffer;
    std::string _servername;
    std::string _realname;
    std::string _nickname;
    std::string _username;
    std::string _hostname;
    std::map<std::string, Channel> _channel;

public:
    Client();
    ~Client();
    Client(int socket);

    void setSocket(int socket);
    int getSocket();

    void setNickname(std::string &name);
    std::string getNickname();
    void setServername(std::string &name);
    std::string getServername();
    void setRealname(std::string &name);
    std::string getRealname();
    void setUsername(std::string &name);
    std::string getUsername();
    void setHostname(std::string &name);
    std::string getHostname();

    void joinChannel(Channel *channel);
};

// #endif // !CLIENT_HPP