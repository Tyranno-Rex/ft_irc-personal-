// #ifndef CHANNEL_HPP
// #define CHANNEL_HPP

#include <unordered_map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <algorithm>
#include <exception>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <cstring>
#include <vector>
#include <string>
#include <ctime>
#include <map>
#include <set>


//  /mode #채널명 +i (초대한 사람만 들어올수 있게함)
//  /mode #채널명 +t (op만 TOPIC을 변경할 수 있게 함)
//  /mode #채널명 +k 비밀번호 (채널에 비밀번호를 설정)
//  /mode #채널명 +o 닉네임 (op권한을 줌)
//  /mode #채널명 +l number (number 만큼 입장가능 인원을 제한함)
#define MODE_  0;
#define MODE_I 1;
#define MODE_T 2;
#define MODE_K 3;
#define MODE_O 4;
#define MODE_L 5;

class Channel {
private:
    std::map<std::string, Client> _users;
    std::map<std::string, Client> _op_user;

    std::string _name;
    std::string _pass;
    std::string _topic;

    unsigned int _user_limit;
    unsigned int _mode; 

public:
    Channel();
    ~Channel();
    Channel(std::string &name, std::string &pass, Client &client);

    void setName(std::string &name);
    std::string getName();

    void setPassword(std::string &pass);
    std::string getPassword(); // 이거 해야하나?

    void setTopic(std::string &topic);
    std::string getTopic();

    void setMode(unsigned int mode);
    unsigned int getMode();
};

// #endif // !CHANNEL_HPP