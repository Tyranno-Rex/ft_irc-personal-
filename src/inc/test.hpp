

// class IRCServer {
// private:
//     std::unordered_map<std::string, Channel> channels;
//     std::unordered_map<std::string, std::string> users;

// public:
//     void joinChannel(const std::string& username, const std::string& channelName) {
//         channels[channelName].join(username);
//     }

//     void kickUser(const std::string& kicker, const std::string& target, const std::string& channelName) {
//         auto& channel = channels[channelName];
//         channel.kick(kicker, target);
//     }

//     void sendMessage(const std::string& sender, const std::string& target, const std::string& message) {
//         std::cout << sender << " sends to " << target << ": " << message << std::endl;
//     }

//     void inviteUser(const std::string& inviter, const std::string& target, const std::string& channelName) {
//         auto& channel = channels[channelName];
//         const auto& users = channel.getUsers();
//         auto targetIt = std::find(users.begin(), users.end(), target);

//         if (targetIt != users.end()) {
//             std::cout << target << " is already in the channel" << std::endl;
//         } else {
//             std::cout << inviter << " invites " << target << " to the channel" << std::endl;
//         }
//     }
// };