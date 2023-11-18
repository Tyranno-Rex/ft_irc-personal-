#include "../inc/Server.hpp"
#include "../inc/Client.hpp"
#include "../inc/Channel.hpp"

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		std::cerr << "Invalid Arguments Number" << std::endl;
		return 1;
	}

	int port = atoi(argv[1]);
	std::string password(argv[2]);


	// 유효 port 범위 입력
	if (port < 1024 || port > 49151)
	{
		std::cerr << "Bad port number" << std::endl;
		return 1;
	}


	// 비밀번호 프로토콜 1. 비어있으면 안된다. 2. 길이가 10글자 이상이면 안된다.
	if (password.empty())
	{
		std::cerr << "Password empty" << std::endl;
		return 1;
	}

	if (password.length() > 10)
	{
		std::cerr << "Password too long" << std::endl;
		return 1;
	}



	// server에는 port 번호와 password만 전송함
	// 초기화 이후에 실행
	Server server(port, password);
	try
	{
		server.init();
		server.run();
	}
	catch(const std::exception& e)
	{
		server.deleteChannel();
		std::cerr << e.what() << '\n';
	}

}