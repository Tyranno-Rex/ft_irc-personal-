#include "../inc/client.hpp"

Client::Client(){

}

Client::~Client(){

}

Client::Client(int socket) : _socket(socket){
    this->_buffer       = "";
    this->_servername   = "root";
    this->_realname     = "root";
    this->_nickname     = "client" + std::to_string(socket);
    this->_username     = "root";
    this->_hostname     = "127.0.0.1";
}

void Client::setSocket(int socket){
    this->_socket = socket;
}

int Client::getSocket(){
    return this->_socket;
}


//--------------------------------------//
void Client::setNickname(std::string &name){
    this->_nickname = name;
}
std::string Client::getNickname(){
    return this->_nickname;
}

//--------------------------------------//
void Client::setServername(std::string &name){
    this->_servername = name;
}
std::string Client::getServername(){
    return this->_servername;
}

//--------------------------------------//
void Client::setRealname(std::string &name){
    this->_realname = name;
}
std::string Client::getRealname(){
    return this->_realname;
}

//--------------------------------------//
void Client::setUsername(std::string &name){
    this->_username = name;
}
std::string Client::getUsername(){
    return this->_username;
}
//--------------------------------------//
void Client::setHostname(std::string &name){
    this->_hostname = name;
}
std::string Client::getHostname(){
    return this->_hostname;
}

void Client::joinChannel(Channel *channel){
    this->_channel[channel->getName()] = *channel;
}