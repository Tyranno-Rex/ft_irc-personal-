#include "../inc/client.hpp"


Channel::Channel(){

};

Channel::~Channel(){

};

// Channel::Channel(std::string &name, )
Channel::Channel(std::string &name, std::string &pass, Client &client)
{
    std::string name = client.getNickname();
    this->_op_user[name] = client;
    this->_users[name] = client;

    this->_name = name;
    if (!pass.empty()){
        this->_pass = pass;
    }
    this->_topic == "";
    this->_user_limit = 25; // 서브젝트에 최대 얼마라고 했는지 모르겠음
    this->_mode = MODE_;
};

void Channel::setName(std::string &name){
    this->_name = name;
}

std::string Channel::getName(){
    return this->_name;
}

void Channel::setPassword(std::string &pass){
    this->_pass = pass;
}

std::string Channel::getPassword(){
    return this->_pass;
}

void Channel::setTopic(std::string &topic){
    this->_topic = topic;
}

std::string Channel::getTopic(){
    return this->_topic;
}




// void join(const std::string& username) {
//     users.push_back(username);
// }

// void kick(const std::string& kicker, const std::string& target) {
//     auto targetIt = std::find(users.begin(), users.end(), target);

//     if (targetIt != users.end()) {
//         users.erase(targetIt);
//         std::cout << target << " has been kicked from the channel" << std::endl;
//     } else {
//         std::cout << target << " is not in the channel" << std::endl;
//     }
// }

// const std::vector<std::string>& getUsers() const {
//     return users;
// }
// void Channel::join(const std::string& username) {
//     this->users.push_back(username);
// };






















int main() {
    IRCServer server;

    // 예시 사용법
    server.joinChannel("user1", "#channel1");
    server.joinChannel("user2", "#channel1");

    server.kickUser("user1", "user2", "#channel1");

    server.sendMessage("user1", "#channel1", "Hello, everyone!");

    server.inviteUser("user1", "user3", "#channel1");

    return 0;
}
