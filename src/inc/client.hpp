#include "channel.hpp"


class Client {
private:
    std::string realname;
    std::string nickname;

public:
    Client();
    ~Client();
    std::string getname();
};

