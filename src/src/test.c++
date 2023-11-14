#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_map>

class Channel {
private:
    std::vector<std::string> users;

public:
    void join(const std::string& username) {
        users.push_back(username);
    }

    void kick(const std::string& kicker, const std::string& target) {
        auto targetIt = std::find(users.begin(), users.end(), target);

        if (targetIt != users.end()) {
            users.erase(targetIt);
            std::cout << target << " has been kicked from the channel" << std::endl;
        } else {
            std::cout << target << " is not in the channel" << std::endl;
        }
    }

    const std::vector<std::string>& getUsers() const {
        return users;
    }
};

class IRCServer {
private:
    std::unordered_map<std::string, Channel> channels;
    std::unordered_map<std::string, std::string> users;

public:
    void joinChannel(const std::string& username, const std::string& channelName) {
        channels[channelName].join(username);
    }

    void kickUser(const std::string& kicker, const std::string& target, const std::string& channelName) {
        auto& channel = channels[channelName];
        channel.kick(kicker, target);
    }

    void sendMessage(const std::string& sender, const std::string& target, const std::string& message) {
        std::cout << sender << " sends to " << target << ": " << message << std::endl;
    }

    void inviteUser(const std::string& inviter, const std::string& target, const std::string& channelName) {
        auto& channel = channels[channelName];
        const auto& users = channel.getUsers();
        auto targetIt = std::find(users.begin(), users.end(), target);

        if (targetIt != users.end()) {
            std::cout << target << " is already in the channel" << std::endl;
        } else {
            std::cout << inviter << " invites " << target << " to the channel" << std::endl;
        }
    }
};

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
