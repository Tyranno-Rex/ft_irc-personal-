#include "../inc/Server.hpp"

Server::Server()
{
}

Server::~Server()
{
}

Server::Server(int port, std::string password) : port(port), password(password), servername("happyirc")
{
}

void Server::init()
{
	/*
	1번째 매개변수:	 주소 체계(Address Family) 	/ AF_INET 
					AF_INET는 IPv4 주소를 사용하는 것을 나타냄
	2번째 매개변수:  소켓의 타입 ()				/ SOCK_STEAM   
					SOCK_STREAM은 연결 지향형 소켓, 이는 TCP를 사용하는 소켓
	3번째 매개변수:  프로토콜을 의미함, 
					0은 주어진 주소 체계와 타입에 따라 시스템이 적절한 프로토콜을 선택하도록 하는 것을 의미함.
	*/
    this->server_socket = socket(AF_INET, SOCK_STREAM, 0); // 서버 소켓 생성


    // 소켓 옵션 설정 (주로 소켓 재사용을 위해 설정)
    const int value = 1;

	// 소켓옵션선택함/   설정하는 소켓   /  소켓레벨 / 소켓재사용옵션 / 
    if (setsockopt(this->server_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) != 0)
        throw std::runtime_error("set server socket error");

    // 서버 주소 구조체 초기화 및 설정
    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(this->port);

	/*
    소켓에 주소 할당 (바인드)
	sockfd : 앞서 socket 함수로 생성된 endpoint 소켓의 식별 번호
	my_addr : IP 주소와 port 번호를 저장하기 위한 변수가 있는 구조체
	namelen : 두 번째 인자의 데이터 크기
	반환값 : 성공하면 0, 실패하면 -1
	*/

    if (bind(this->server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        close(server_socket);
        throw bindError();
    }

    // 소켓을 수동 대기 모드로 전환하여 클라이언트 연결을 대기
    if (listen(server_socket, 15) == -1)
    {
        close(server_socket);
        throw listenError();
    }

    // 소켓을 비블록 모드로 설정 -> 효율적인 다중 처리를 위한 함수
    fcntl(this->server_socket, F_SETFL, O_NONBLOCK);

    // kqueue를 생성하여 이벤트 처리를 위한 파일 디스크립터를 얻음
    this->kq = kqueue();

    // kqueue 생성 실패 시 예외 처리
    if (kq == -1)
    {
        close(server_socket);
        throw kqueueError();
    }

    // 서버 소켓의 read 이벤트를 큐에 등록
    changeEvent(change_list, this->server_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
}


// change_list 에 새 이벤트 추가
void Server::changeEvent(std::vector<struct kevent> &change_list, uintptr_t ident, int16_t filter, uint16_t flags, uint32_t fflags, intptr_t data, void *udata)
{
    // kevent 구조체를 생성하고 초기화
    struct kevent temp_event;

    // EV_SET 매크로를 사용하여 kevent 구조체에 이벤트 정보를 설정
    // EV_SET(kev, ident, filter, flags, fflags, data, udata)
    // kev: kevent 구조체, ident: 식별자(파일 디스크립터 또는 식별 번호), filter: 이벤트 필터, flags: 이벤트 플래그, fflags: 이벤트 플래그의 추가 정보, data: 필터에 대한 데이터, udata: 사용자 데이터
    EV_SET(&temp_event, ident, filter, flags, fflags, data, udata);

    // 설정한 이벤트를 kevent 배열에 추가
    // change_list: kevent 배열, temp_event: 추가할 이벤트
    this->change_list.push_back(temp_event);
}


void Server::run()
{
	int new_events;
	struct kevent *curr_event;
	while (1)
	{
		// change_list 에 있는 이벤트들을 kqueue에 등록
		// change_list = 큐에 등록할 이벤트들이 담겨있는 배열
		// event_list = 발생할 이벤트들이 리턴될 배열
		new_events = kevent(kq, &change_list[0], change_list.size(), event_list, 8, NULL);
		if (new_events == -1)
		{
			closeClient();
			close(server_socket);
			throw keventError();
		}

		// 큐에 다 담았으니 change_list 초기화
		change_list.clear();

		// 리턴된 이벤트를 체크
		for (int i = 0; i < new_events; ++i)
		{
			// 하나씩 돌면서 확인
			curr_event = &event_list[i];

			// 이벤트 리턴값이 error인 경우 (이벤틑 처리 과정에서 에러 발생)
			if (curr_event->flags & EV_ERROR)
			{
				// 서버에서 에러가 난 경우 -> 서버 포트 닫고, 에러 던지고 프로그램 종료
				if (curr_event->ident == server_socket)
				{
					closeClient();
					close(server_socket);					
					throw std::runtime_error("server socket error");
				}
				// 클라이언트에서 에러가 난 경우 -> 해당 클라이언트 소켓 닫기 (관련된 이미 등록된 이벤트는 큐에서 삭제됨)
				else
				{
					std::cerr << "client socket error" << std::endl;
					disconnectClient(curr_event->ident);
				}
			}
			// read 가 가능한 경우
			else if (curr_event->filter == EVFILT_READ)
			{
				// 서버인 경우 (클라이언트가 새로 접속한 경우)
				if (curr_event->ident == server_socket)
				{
					int client_socket;
					if ((client_socket = accept(server_socket, NULL, NULL)) == -1)
						throw acceptError();
					std::cout << "accept new client: " << client_socket << std::endl;
					fcntl(client_socket, F_SETFL, O_NONBLOCK);

					// 새로 등록된 경우 클라이언트의 read와 write 이벤트 모두 등록
					changeEvent(change_list, client_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
					changeEvent(change_list, client_socket, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
					// 클라이언트 목록에 추가
					clients[client_socket] = Client(client_socket);
				}
				// 이미 연결된 클라이언트의 read 가 가능한 경우
				else if (clients.find(curr_event->ident) != clients.end())
				{
					char buf[1024];
					// 해당 클라이언트의 데이터 읽기
					int n = recv(curr_event->ident, buf, sizeof(buf), 0);

					// 에러 발생 시 클라이언트 연결 끊기
                    if (n <= 0)
                    {
                        if (n < 0)
                            std::cerr << "client read error!" << std::endl;
                        disconnectClient(curr_event->ident);
                    }
                    else
                    {
                        buf[n] = '\0';
                        clients[curr_event->ident].addBuffer(buf);
                        std::cout << "received data from " << curr_event->ident << ": " << clients[curr_event->ident].getBuffer() << std::endl;
						// 읽은 데이터 파싱해서 write할 데이터 클라이언트 배열의 버퍼에 넣기
						// 여기로 들어가면 여러함수들 실행됨
						parseData(clients[curr_event->ident]);
						// 버퍼가 비어있지 않은 경우에만 write 이벤트로 전환
						if (!send_data[curr_event->ident].empty())
						{
							// read 이벤트 리턴 x -> 발생해도 큐에서 처리 x
							changeEvent(change_list, curr_event->ident, EVFILT_READ, EV_DISABLE, 0, 0, curr_event->udata);
							// write 이벤트 등록
							changeEvent(change_list, curr_event->ident, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, curr_event->udata);
						}
					}
				}
			}
			// write 가 가능한 경우
			else if (curr_event->filter == EVFILT_WRITE)
			{
				std::map<int, Client>::iterator it = clients.find(curr_event->ident);
				if (it != clients.end())
				{ // 버퍼가 비어있는 경우 전송 x
					// 버퍼에 문자가 있으면 전송
					if (!send_data[curr_event->ident].empty())
					{
						int n;
						std::cout << "send data from " << curr_event->ident << ": " << this->send_data[curr_event->ident] << std::endl;
						if ((n = send(curr_event->ident, this->send_data[curr_event->ident].c_str(),
										this->send_data[curr_event->ident].size(), 0) == -1))
						{
							// 전송하다 에러난 경우 연결 끊기
							std::cerr << "client write error!" << std::endl;
							disconnectClient(curr_event->ident);
						}
						// 전송 성공한 경우
						// 버퍼 비우기
						else
						{
							this->send_data[curr_event->ident].clear();
							if (clients[curr_event->ident].getClose())
							{
								disconnectClient(curr_event->ident);
								continue;
							}
							// write 이벤트 리턴 x -> 발생해도 큐에서 처리 x
							changeEvent(change_list, curr_event->ident, EVFILT_WRITE, EV_DISABLE, 0, 0, curr_event->udata);
							// read 이벤트 등록
							changeEvent(change_list, curr_event->ident, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, curr_event->udata);
						}
					}
				}
			}
		}
	}
}

std::string Server::handleNick(Client &client, std::stringstream &buffer_stream)
{
	// 사용자의 새로운 닉네임을 저장할 변수
	std::string name;

	// 이전 닉네임과 프리픽스를 저장할 변수
	std::string before_nick;
	std::string before_prefix;

	// 응답 메시지를 초기화
	std::string response = "";

	// NICK 뒤에 파라미터가 들어오지 않은 경우
	if (!(buffer_stream >> name))
		response = ERR_NONICKNAMEGIVEN(client.getNickname());
	// 닉네임이 주어진 경우
	else
	{
		// 닉네임 중복 체크
		if (this->isClient(name))
		{
			// 중복된 닉네임일 경우 에러 응답 생성
			response = ERR_NICKNAMEINUSE(name);
		}
		else
		{
			// 닉네임 변경 전의 정보 저장
			before_nick = client.getNickname();
			before_prefix = client.getPrefix();

			// 사용자의 닉네임 변경
			client.setNickname(name);

			// 사용자가 속한 채널의 닉네임도 변경
			changeChannelNick(client, before_nick, before_prefix);
		}
	}

	// 처리 결과에 따른 응답 반환
	return response;
}


std::string Server::handleUser(Client &client, std::stringstream &buffer_stream)
{
	// 사용자 정보를 저장할 변수들
	std::string line;
	std::string name[4];
	int cnt = 0;

	// 사용자 정보 파싱
	while (cnt < 4)
	{
		// 파라미터 부족 시 에러 응답
		if (!(buffer_stream >> line))
			return ERR_NEEDMOREPARAMS(client.getNickname(), "USER");

		// 파라미터를 배열에 저장
		name[cnt] = line;
		cnt++;
	}

	// 사용자의 정보 설정
	client.setUsername(name[0]);
	client.setHostname(name[1]);
	client.setServername(name[2]);

	// 실제 이름 (Realname) 처리
	if (name[3][0] == ':')
	{
		// ':'로 시작하면 실제 이름이 여러 단어로 이루어진 경우
		name[3] = line.substr(1);
		while (buffer_stream >> line)
		{
			name[3] += " " + line;
		}

		// 사용자의 실제 이름 설정
		client.setRealname(name[3]);
	}

	// 처리 결과에 따른 환영 메시지 반환
	return RPL_WELCOME(client.getNickname());
}

std::string Server::handlePass(Client &client, std::stringstream &buffer_stream)
{
	// 이미 register된 클라이언트인 경우
	if (client.getRegister())
		return ERR_ALREADYREGISTRED(client.getNickname());

	int cnt = 0;
	std::string line;

	// 입력 스트림에서 password를 읽어옴
	buffer_stream >> line;

	// PASS 뒤에 파라미터가 없는 경우
	if (line.empty())
	{
		// 클라이언트 연결 종료 및 에러 메시지 반환
		client.setClose(true);
		return ERR_NEEDMOREPARAMS(client.getNickname(), "PASS");
	}

	// 입력된 password가 서버의 설정과 다른 경우
	if (this->password != line)
	{
		// 클라이언트 연결 종료 및 에러 메시지 반환
		client.setClose(true);
		return ERR_PASSWDMISMATCH(client.getNickname());
	}

	// 성공적으로 인증된 경우, 클라이언트를 register로 표시
	client.setRegister(true);

	// 성공적으로 처리되었으므로 빈 문자열 반환
	return "";
}


std::string Server::handleQuit(Client &client, std::stringstream &buffer_stream)
{
	// 사용자의 나가기 메시지를 저장할 변수
	std::string line;
	std::string message;

	// 입력 스트림에서 첫 번째 토큰을 읽어옴
	buffer_stream >> line;

	// 나가기 메시지가 비어있는 경우 기본 메시지 설정
	if (line.empty())
		line = ":leaving";

	// 나가기 메시지 설정 (':'를 제외한 나머지 부분을 사용)
	message = line.substr(1);

	// 추가적인 나가기 메시지가 있는 경우 계속해서 읽어옴
	while (1)
	{
		// 입력 스트림에서 다음 토큰을 읽어옴
		if (!(buffer_stream >> line))
			break;

		// 나가기 메시지에 추가
		message += " " + line;
	}

	// 나가기 메시지 반환
	return message;
}


std::string Server::handleWho(Client &client, std::stringstream &buffer_stream)
{
	// WHO 명령에 대한 파라미터를 저장할 변수들
	std::string name;
	std::string ch_name;
	std::string option = "H";
	std::string reply;
	std::string response = "";
	// 입력 스트림에서 첫 번째 토큰을 읽어옴
	buffer_stream >> name;
	// 파라미터가 비어있지 않은 경우에만 처리
	if (!name.empty())
	{
		// 채널 이름이 주어진 경우
		if (name[0] == '#')
		{
			// 모든 채널에 대해 반복
			for (std::map<std::string, Channel *>::iterator m_it = this->channels.begin(); m_it != this->channels.end(); m_it++)
			{
				// 주어진 채널명과 일치하며, 해당 채널이 invite-only가 아닌 경우
				ch_name = m_it->second->getName();
				if (name == ch_name && !m_it->second->findMode('i'))
				{
					// 채널의 사용자 목록을 가져와 반복
					std::map<std::string, Client> users = m_it->second->getUsers();
					for (std::map<std::string, Client>::iterator u_it = users.begin(); u_it != users.end(); u_it++)
					{
						// 사용자의 옵션 설정
						option = "H";
						if (m_it->second->isOperator(u_it->second))
							option += "@";
						// WHO 응답 메시지 생성
						reply = RPL_WHOREPLY(client.getNickname(), ch_name, u_it->second.getUsername(), u_it->second.getHostname(),
											u_it->second.getServername(), u_it->second.getNickname(), option, u_it->second.getRealname());
						
						// 응답 메시지를 response에 추가
						response += makeCRLF(reply);
					}
				}
			}
		}
		// 사용자 이름이 주어진 경우
		else
		{
			// 모든 클라이언트에 대해 반복
			for (std::map<int, Client>::iterator u_it = this->clients.begin(); u_it != this->clients.end(); u_it++)
			{
				// 주어진 사용자명과 일치하는 경우
				if (name == u_it->second.getNickname())
				{
					// 사용자가 속한 채널이 없는 경우 '*'로 표시
					if (u_it->second.getChannels().empty())
					{
						ch_name = "*";
					}
					// 사용자가 속한 채널이 있는 경우, 첫 번째 채널을 사용
					else
					{
						ch_name = u_it->second.getChannels().begin()->first;
						Channel *ch = this->channels[ch_name];
						if (ch->isOperator(u_it->second))
							option += "@";
					}
					// WHO 응답 메시지 생성
					reply = RPL_WHOREPLY(client.getNickname(), ch_name, u_it->second.getUsername(), u_it->second.getHostname(),
										u_it->second.getServername(), u_it->second.getNickname(), option, u_it->second.getRealname());
					// 응답 메시지를 response에 추가
					response += makeCRLF(reply);
				}
			}
		}
	}
	// WHO 처리 종료를 나타내는 응답 메시지를 response에 추가
	response += RPL_ENDOFWHO(client.getNickname(), name);
	// 최종적으로 생성된 응답 메시지 반환
	return response;
}


std::string Server::handlePingpong(Client &client, std::stringstream &buffer_stream)
{
	// PING 명령에 대한 응답 메시지를 저장할 변수
	std::string response;

	// 입력 스트림에서 PING 명령의 파라미터를 읽어옴
	std::string ping;
	buffer_stream >> ping;

	// PING 명령에 파라미터가 없는 경우
	if (ping.empty())
		response = ERR_NOORIGIN(client.getNickname());
	else
		// PONG 응답 메시지 생성
		response = RPL_PONG(client.getPrefix(), ping);

	// 최종적으로 생성된 응답 메시지 반환
	return response;
}


std::string Server::handleJoin(Client &client, std::stringstream &buffer_stream)
{
	// JOIN 명령에 대한 응답 메시지를 저장할 변수
	std::string response;

	// 채널 이름과 키를 저장할 변수
	std::string ch_name;
	std::string key;

	// 입력 스트림에서 채널 이름과 키를 읽어옴
	buffer_stream >> ch_name;
	buffer_stream >> key;

	// 채널 이름이 없는 경우, NEEDMOREPARAMS 에러 응답 생성
	if (ch_name.empty())
	{
		response += ERR_NEEDMOREPARAMS(client.getNickname(), "JOIN");
		return response;
	}
	// 클라이언트가 이미 최대 채널 수에 가입한 경우, TOOMANYCHANNELS 에러 응답 생성
	if (client.getChannels().size() > CLIENT_CHANLIMIT)
	{
		response += ERR_TOOMANYCHANNELS(client.getNickname(), ch_name);
		return response;
	}
	// 채널 이름이 '#'로 시작하지 않는 경우, '#'를 추가하여 채널 이름 설정
	if (ch_name[0] != '#')
        ch_name = "#" + ch_name;
	// 채널과 키를 ','로 분리하여 각각의 벡터에 저장
	std::vector<std::string> channels;
	std::stringstream channel_stream(ch_name);
	std::string channel;
	while (std::getline(channel_stream, channel, ','))
	{
		channel.erase(std::remove(channel.begin(), channel.end(), '\r'));
		channel.erase(std::remove(channel.begin(), channel.end(), '\n'));
		channels.push_back(channel);
	}
	std::vector<std::string> keys;
	std::stringstream key_stream(key);
	std::string channel_key;
	while (std::getline(key_stream, channel_key, ','))
	{
		channel_key.erase(std::remove(channel_key.begin(), channel_key.end(), '\r'));
		channel_key.erase(std::remove(channel_key.begin(), channel_key.end(), '\n'));
		keys.push_back(channel_key);
	}
	int i = 0;
	// 모든 채널에 대해 가입 처리를 수행
	for (std::vector<std::string>::iterator it = channels.begin(); it != channels.end(); it++)
	{
		// 키가 존재하면 해당 키를 사용, 아니면 빈 문자열
		if (i < keys.size())
			key = keys[i];
		else
			key = "";
		// 가입 처리를 수행하고 응답 메시지를 response에 추가
		response += clientJoinChannel(client, *it, key);
		i++;
	}
	// 최종적으로 생성된 응답 메시지 반환
	return response;
}


std::string Server::clientJoinChannel(Client &client, std::string &ch_name, std::string &key)
{
	// 가입 결과를 저장할 응답 메시지 변수
	std::string response;

	// 주어진 채널명으로부터 해당 채널 객체를 가져오거나, 채널이 없으면 생성
	Channel *p_channel;
	if (this->channels.find(ch_name) != this->channels.end())
		p_channel = this->channels[ch_name];
	else
		p_channel = createChannel(ch_name, key, client);

	// 채널에 인원이 가득 찼을 경우 에러 응답 생성
	if (p_channel->getUsers().size() + 1 > p_channel->getUserLimit())
	{
		response += ERR_CHANNELISFULL(client.getNickname(), ch_name);
		return response;
	}

	// 채널에 비밀번호가 설정되어 있을 때, 적절한 처리
	if ((!p_channel->getPassword().empty() && key.empty()) || (!p_channel->getPassword().empty() && key != p_channel->getPassword()) || (p_channel->getPassword().empty() && !key.empty()))
	{
		// channel 비밀번호가 존재하는데 request에 비밀번호가 없을 경우
		// channel 비밀번호가 존재하는데 request 비밀번호와 다를 경우
		// channel 비밀번호가 존재하지 않는데 request 비밀번호가 존재할 경우
		response += ERR_BADCHANNELKEY(client.getNickname(), ch_name);
		return response;
	}

	// 채널에 초대 모드가 설정되어 있고, 클라이언트가 초대되지 않았을 경우 에러 응답 생성
	if (p_channel->getInviteMode() && !p_channel->isInvite(client))
	{
		response += ERR_INVITEONLYCHAN(client.getNickname(), ch_name);
		return response;
	}

	try
	{
		// 채널에 가입한 클라이언트의 목록 문자열
		std::string s_users = "";

		// 채널에 속한 사용자 목록을 가져옴
		std::map<std::string, Client> users = p_channel->getUsers();

		// 클라이언트가 채널에 가입하지 않았을 경우에만 가입 처리
		if (users.find(client.getNickname()) == users.end())
			p_channel->joinClient(client, COMMON);
		
		// 클라이언트가 채널에 가입되었음을 표시
		client.joinChannel(p_channel);

		// 채널에 속한 사용자들에 대한 정보를 문자열로 구성
		for (std::map<std::string, Client>::iterator it = users.begin(); it != users.end(); it++)
		{
			// 채널 오퍼레이터인 경우 '@'를 추가
			if (p_channel->isOperator(it->second))
				s_users.append("@");
			s_users.append(it->first + " ");
		}

		// 채널 가입에 대한 브로드캐스트 및 응답 메시지 생성
		broadcast(ch_name, RPL_JOIN(client.getPrefix(), ch_name));

		// 채널에 토픽이 설정되어 있으면 토픽 관련 응답 메시지 생성
		if (!p_channel->getTopic().empty())
		{
			response += makeCRLF(RPL_TOPIC(client.getUsername(), ch_name, p_channel->getTopic()));
			response += makeCRLF(RPL_TOPICWHOTIME(client.getUsername(), ch_name, p_channel->getTopicUser(), p_channel->getTopicTime()));
		}

		// 채널에 속한 사용자 목록에 대한 응답 메시지 생성
		response += makeCRLF(RPL_NAMREPLY(client.getNickname(), '=', ch_name, s_users));
		response += makeCRLF(RPL_ENDOFNAMES(client.getNickname(), ch_name));
	}
	catch (const std::exception &e)
	{
		// 채널에서 사용자가 밴 처리된 경우 에러 응답 생성
		response += makeCRLF(ERR_BANNEDFROMCHAN(client.getNickname(), ch_name));
	}

	// 최종적으로 생성된 응답 메시지 반환
	return response;
}


std::string Server::handlePrivmsg(Client &client, std::stringstream &buffer_stream)
{
	// PRIVMSG 메시지에 대한 응답 메시지를 저장할 변수
	std::string response;

	// 메시지를 전송할 대상과 메시지 내용을 저장할 변수
	std::string target;
	std::string msg;

	// 입력 스트림에서 대상과 메시지 내용을 읽어옴
	buffer_stream >> target;
	std::getline(buffer_stream, msg);

	// 메시지 내용에서 '\r' 및 '\n' 문자 제거
	msg.erase(std::remove(msg.begin(), msg.end(), '\r'));
	msg.erase(std::remove(msg.begin(), msg.end(), '\n'));

	// 대상이 채널인 경우, 서버에 메시지 전송
	if (target[0] == '#')
	{
		response += msgToServer(client, target, msg);
	}
	// 대상이 사용자인 경우, 사용자에게 메시지 전송
	else
	{
		response += msgToUser(client, target, msg);
	}

	// 최종적으로 생성된 응답 메시지 반환
	return response;
}

std::string Server::msgToServer(Client &client, std::string &target, std::string &msg)
{
	// 응답 메시지를 저장할 변수
	std::string response;

	// 대상 채널이 존재하는지 확인
	if (this->channels.find(target) == this->channels.end())
	{
		// 채널이 존재하지 않는 경우, 에러 응답 생성
		response += makeCRLF(ERR_NOSUCHCHANNEL(client.getNickname(), target));
	}
	else
	{
		// 대상 채널이 존재하는 경우
		Channel *channel = this->channels[target];
		std::map<std::string, Client> users = channel->getUsers();

		// 해당 사용자가 채널에 속하지 않고, 'n' 모드가 설정된 경우
		if ((users.find(client.getNickname()) == users.end()) && (channel->findMode('n')))
		{
			response += makeCRLF(ERR_CANNOTSENDTOCHAN(client.getNickname(), target));
		}
		else
		{
			// 해당 채널에 속한 모든 사용자에게 메시지를 브로드캐스트 (본인 제외)
			broadcastNotSelf(target, RPL_PRIVMSG(client.getPrefix(), target, msg), client.getSocket());
		}
	}

	// 최종적으로 생성된 응답 메시지 반환
	return response;
}


std::string Server::msgToUser(Client &client, std::string &target, std::string &msg)
{
	// 대상 사용자에게 직접 메시지를 전송
	for (std::map<int, Client>::iterator c_it = this->clients.begin(); c_it != this->clients.end(); c_it++)
	{
		// 현재 반복 중인 클라이언트의 닉네임을 가져옴
		std::string name = c_it->second.getNickname();

		// 대상 사용자를 찾은 경우
		if (target == name)
		{
			// 해당 사용자에게 메시지 전송
			directMsg(c_it->second, RPL_PRIVMSG(client.getPrefix(), target, msg));
			return "";
		}
	}

	// 대상 사용자를 찾지 못한 경우, 에러 응답 생성
	return makeCRLF(ERR_NOSUCHNICK(client.getNickname(), target));
}


void Server::directMsg(Client &to, const std::string &msg)
{
	// 특정 클라이언트에게 직접 메시지를 전송하는 함수

	// 대상 클라이언트의 소켓을 통해 메시지를 전송 큐에 추가
	this->send_data[to.getSocket()] += makeCRLF(msg);

	// 대상 클라이언트의 소켓에 대한 이벤트를 쓰기 가능으로 변경
	changeEvent(change_list, to.getSocket(), EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
}

void Server::broadcast(std::string &channel_name, const std::string &msg)
{
	// 특정 채널에 속한 모든 사용자에게 메시지를 브로드캐스트하는 함수

	// 주어진 채널 이름으로 채널 객체를 가져옴
	Channel *channel = this->channels[channel_name];

	// 채널에 속한 사용자들의 목록을 가져옴
	std::map<std::string, Client> users = channel->getUsers();

	// 각 사용자에게 메시지를 전송 큐에 추가하고, 이벤트를 쓰기 가능으로 변경
	for (std::map<std::string, Client>::iterator u_it = users.begin(); u_it != users.end(); u_it++)
	{
		int c_socket = u_it->second.getSocket();
		this->send_data[c_socket] += makeCRLF(msg);
		changeEvent(change_list, c_socket, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
	}
}


void Server::broadcastNotSelf(std::string &channel_name, const std::string &msg, int self)
{
	Channel *channel = this->channels[channel_name];

	std::map<std::string, Client> users = channel->getUsers();
	for (std::map<std::string, Client>::iterator u_it = users.begin(); u_it != users.end(); u_it++)
	{
		int c_socket = u_it->second.getSocket();
		if (c_socket != self)
		{
			this->send_data[c_socket] += makeCRLF(msg);
			changeEvent(change_list, c_socket, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
		}
	}
}

Channel *Server::createChannel(std::string &channel_name, std::string &key, Client &client)
{
	// 새로운 채널을 생성하고 반환하는 함수

	// 주어진 채널 이름, 키, 클라이언트 정보를 사용하여 채널 객체 생성
	Channel *channel = new Channel(channel_name, key, client);

	// 채널을 서버의 채널 맵에 추가
	this->channels[channel_name] = channel;

	// 생성된 채널 객체 반환
	return channel;
}

std::string Server::makeCRLF(const std::string &cmd)
{
	// 주어진 명령어에 CRLF(Carriage Return, Line Feed)를 추가하여 반환하는 함수
	return cmd + "\r\n";
}

std::string Server::handleList(Client &client, std::stringstream &buffer_stream)
{
	// LIST 명령어를 처리하는 함수

	// 응답 메시지를 저장할 변수
	std::string response;

	// 채널 이름을 저장할 변수
	std::string ch_name;

	// 버퍼 스트림에서 채널 이름을 읽어옴
	buffer_stream >> ch_name;

	// 채널 이름이 비어있는 경우
	if (ch_name.empty())
	{
		// 모든 채널의 정보를 나열하는 응답 생성

		// RPL_LISTSTART 응답 생성
		response += makeCRLF(RPL_LISTSTART(client.getNickname()));

		// 서버에 존재하는 모든 채널에 대해 반복
		for (std::map<std::string, Channel *>::iterator it = this->channels.begin(); it != this->channels.end(); it++)
		{
			// 각 채널에 대한 정보를 RPL_LIST 응답으로 생성하여 추가
			response += makeCRLF(RPL_LIST(client.getNickname(), it->first, std::to_string(it->second->getUsers().size()), "[" + it->second->getModeString() + "]", it->second->getTopic()));
		}

		// RPL_LISTEND 응답 생성
		response += makeCRLF(RPL_LISTEND(client.getNickname()));

		// 완성된 응답 반환
		return response;
	}

	// 특정 채널에 대한 정보를 나열하는 응답 생성

	// RPL_LISTSTART 응답 생성
	response += makeCRLF(RPL_LISTSTART(client.getNickname()));

	// 주어진 채널 이름에 해당하는 채널 객체를 가져옴
	Channel *ch_po = this->channels[ch_name];

	// 해당 채널이 존재하는 경우
	if (ch_po)
	{
		// RPL_LIST 응답으로 해당 채널의 정보를 생성하여 추가
		response += makeCRLF(RPL_LIST(client.getNickname(), ch_name, std::to_string(ch_po->getUsers().size()), "[" + ch_po->getModeString() + "]", ch_po->getTopic()));
	}

	// RPL_LISTEND 응답 생성
	response += makeCRLF(RPL_LISTEND(client.getNickname()));

	// 완성된 응답 반환
	return response;
}


std::string Server::handleTopic(Client &client, std::stringstream &buffer_stream)
{
	// Topic 명령어를 처리하는 함수

	// 응답 메시지를 저장할 변수
	std::string response;

	// 채널과 토픽을 저장할 변수
	std::string channel;
	std::string topic;

	// 입력 스트림에서 채널과 토픽을 읽어옴
	buffer_stream >> channel;
	buffer_stream >> topic;

	// 채널이 비어있는 경우, 461 에러 응답 생성 (NEEDMOREPARAMS)
	if (channel.empty())
	{
		response += makeCRLF(ERR_NEEDMOREPARAMS(client.getNickname(), "TOPIC"));
		return response;
	}

	// 주어진 채널이 존재하지 않는 경우, 403 에러 응답 생성 (NOSUCHCHANNEL)
	if (this->channels.find(channel) == this->channels.end())
	{
		response += makeCRLF(ERR_NOSUCHCHANNEL(client.getNickname(), channel));
		return response;
	}

	// 주어진 채널이 존재하는 경우
	Channel *ch_po = this->channels[channel];

	// 토픽이 비어있는 경우
	if (topic.empty())
	{
		// 채널에 토픽이 없는 경우, 331 응답 생성 (RPL_NOTOPIC)
		if (ch_po->getTopic().empty())
		{
			response += makeCRLF(RPL_NOTOPIC(client.getNickname(), channel));
			return response;
		}

		// 채널에 토픽이 있는 경우, 332 및 333 응답 생성 (RPL_TOPIC, RPL_TOPICWHOTIME)
		response += makeCRLF(RPL_TOPIC(client.getNickname(), channel, ch_po->getTopic()));
		response += makeCRLF(RPL_TOPICWHOTIME(client.getNickname(), channel, ch_po->getTopicUser(), ch_po->getTopicTime()));
		return response;
	}

	// 주어진 토픽을 설정하기 위한 처리
	std::map<std::string, Client> users = ch_po->getUsers();

	// 채널에 유저가 없는 경우, 442 에러 응답 생성 (NOTONCHANNEL)
	if (users.find(client.getNickname()) == users.end())
	{
		response += makeCRLF(ERR_NOTONCHANNEL(client.getNickname(), channel));
		return response;
	}

	// 채널의 오퍼레이터가 아니면서 채널이 't' 모드로 설정된 경우, 482 에러 응답 생성 (CHANOPRIVSNEEDED)
	if (!ch_po->isOperator(client) && ch_po->findMode('t'))
	{
		response += makeCRLF(ERR_CHANOPRIVSNEEDED(client.getNickname(), channel));
		return response;
	}

	// 채널 토픽을 설정하고, 변경된 토픽을 채널에 속한 모든 사용자에게 브로드캐스트
	ch_po->setTopic(client, topic);
	broadcast(channel, makeCRLF(RPL_MY_TOPIC(client.getPrefix(), channel, topic)));

	// 빈 문자열 반환 (성공적인 처리를 나타냄)
	return "";
}


std::string Server::handlePart(Client &client, std::stringstream &buffer_stream)
{
	// PART 명령어를 처리하는 함수

	// 응답 메시지를 저장할 변수
	std::string response;

	// 채널 목록을 저장할 문자열
	std::string channel_line;

	// 버퍼 스트림에서 채널 목록을 읽어옴
	buffer_stream >> channel_line;

	// 채널 목록을 저장할 문자열 벡터
	std::vector<std::string> v_channels;

	// 문자열 스트림을 사용하여 채널 목록을 ','로 구분하여 벡터에 추가
	std::stringstream new_stream(channel_line);
	std::string channel;
	while (std::getline(new_stream, channel, ','))
	{
		channel.erase(std::remove(channel.begin(), channel.end(), '\r'));
		channel.erase(std::remove(channel.begin(), channel.end(), '\n'));
		v_channels.push_back(channel);
	}

	// 벡터에 저장된 각 채널에 대해 반복
	for (std::vector<std::string>::iterator it = v_channels.begin(); it != v_channels.end(); it++)
	{
		// 채널이 서버의 채널 맵에 존재하지 않는 경우
		if (this->channels.find(*it) == this->channels.end())
		{
			// ERR_NOSUCHCHANNEL 에러 메시지 생성 및 응답에 추가
			response += makeCRLF(ERR_NOSUCHCHANNEL(client.getNickname(), *it));
			return response;
		}

		// 주어진 채널 이름에 해당하는 채널 객체를 가져옴
		Channel *ch_po = this->channels[*it];

		// 해당 채널에 속한 사용자 목록을 가져옴
		std::map<std::string, Client> users = ch_po->getUsers();

		// 클라이언트가 해당 채널에 속해있지 않은 경우
		if (users.find(client.getNickname()) == users.end())
		{
			// ERR_NOTONCHANNEL 에러 메시지 생성 및 응답에 추가
			response += makeCRLF(ERR_NOTONCHANNEL(client.getNickname(), *it));
			return response;
		}

		// 클라이언트를 채널에서 나가도록 처리하는 함수 호출
		clientLeaveChannel(client, ch_po);
	}

	// 처리된 응답 반환
	return response;
}


std::string Server::handleKick(Client &client, std::stringstream &buffer_stream)
{
	// KICK 명령어를 처리하는 함수

	// 응답 메시지를 저장할 변수
	std::string response;

	// 채널 이름과 강제 퇴장시킬 닉네임 목록을 저장할 변수
	std::string ch_name;
	std::string nickname_line;

	// 버퍼 스트림에서 채널 이름과 닉네임 목록을 읽어옴
	buffer_stream >> ch_name;
	buffer_stream >> nickname_line;

	// 강제 퇴장시킬 닉네임 목록을 저장할 문자열 벡터
	std::vector<std::string> nicknames;

	// 문자열 스트림을 사용하여 닉네임 목록을 ','로 구분하여 벡터에 추가
	std::stringstream nick_stream(nickname_line);
	std::string nickname;
	while (std::getline(nick_stream, nickname, ','))
	{
		nickname.erase(std::remove(nickname.begin(), nickname.end(), '\r'));
		nickname.erase(std::remove(nickname.begin(), nickname.end(), '\n'));
		nicknames.push_back(nickname);
	}

	// 각 닉네임에 대해 반복
	for (std::vector<std::string>::iterator n_it = nicknames.begin(); n_it != nicknames.end(); n_it++)
	{
		// 채널이 존재하지 않는 경우
		if (this->channels.find(ch_name) == this->channels.end())
		{
			response += makeCRLF(ERR_NOSUCHCHANNEL(client.getNickname(), ch_name));
			continue;
		}

		// 사용자가 존재하지 않는 경우
		if (!this->isClient(nickname))
		{
			response += makeCRLF(ERR_NOSUCHNICK(client.getNickname(), nickname));
			continue;
		}

		// 채널 객체를 가져옴
		Channel *channel = this->channels[ch_name];

		// 채널에 속한 사용자 목록을 가져옴
		std::map<std::string, Client> users = channel->getUsers();

		// 클라이언트가 채널에 속해 있지 않은 경우
		if (users.find(client.getNickname()) == users.end())
		{
			response += makeCRLF(ERR_NOTONCHANNEL(client.getNickname(), ch_name));
			continue;
		}

		// 클라이언트가 채널의 운영자가 아닌 경우
		if (!channel->isOperator(client))
		{
			bool op = false;

			// 채널의 사용자 권한을 가져와서 운영자인지 확인
			std::map<std::string, int> auths = channel->getAuth();
			for (std::map<std::string, int>::iterator auth_it = auths.begin(); auth_it != auths.end(); auth_it++)
			{
				if (auth_it->second <= 2)
				{
					op = true;
					break;
				}
			}

			// 운영자가 없는 경우 에러 메시지 추가 및 다음 반복으로 이동
			if (!op)
			{
				response += makeCRLF(ERR_CHANOPRIVSNEEDED2(client.getNickname(), ch_name));
				continue;
			}

			// 운영자가 아닌 경우 에러 메시지 추가 및 다음 반복으로 이동
			response += makeCRLF(ERR_CHANOPRIVSNEEDED(client.getNickname(), ch_name));
			continue;
		}

		// 채널에서 강제 퇴장 처리 함수 호출
		clientKickedChannel(client, *n_it, channel);
	}

	// 처리된 응답 반환
	return response;
}


void Server::clientKickedChannel(Client &from, std::string& to_nick, Channel *channel)
{
	// 특정 클라이언트를 채널에서 강제 퇴장시키는 함수

	// 채널의 이름을 가져옴
	std::string ch_name = channel->getName();

	// 퇴장시킬 클라이언트의 정보를 가져옴
	Client to = channel->getUsers()[to_nick];

	// 퇴장 메시지를 모든 채널 사용자에게 전파
	broadcast(ch_name, RPL_KICK(from.getPrefix(), ch_name, to.getNickname()));

	// 채널에서 클라이언트를 퇴장시키고 클라이언트도 채널에서 나가도록 처리
	to.leaveChannel(channel);
	channel->deleteClient(to.getNickname());

	// 채널 사용자가 더 이상 없는 경우 채널 삭제
	if (channel->getUsers().size() == 0)
	{
		this->channels.erase(ch_name);
		delete channel;
		channel = 0;
	}
}


std::string Server::handleInvite(Client &client, std::stringstream &buffer_stream)
{
	std::string response;
	std::string nickname;
	std::string ch_name;

	buffer_stream >> nickname;
	buffer_stream >> ch_name;
	
	if (this->channels.find(ch_name) == this->channels.end())
	{
		// channel이 존재하지 않을 경우
		return makeCRLF(ERR_NOSUCHCHANNEL(client.getNickname(), ch_name));
	}
	if (!this->isClient(nickname))
	{
		// user가 존재하지 않을 경우
		return makeCRLF(ERR_NOSUCHNICK(client.getNickname(), nickname));
	}
	Channel *channel = this->channels[ch_name];
	std::map<std::string, Client> users = channel->getUsers();
	if (users.find(client.getNickname()) == users.end())
	{
		//채널에 없는 유저가 보냈을 경우
		return makeCRLF(ERR_NOTONCHANNEL(client.getNickname(), ch_name));
	}
	if (users.find(nickname) != users.end())
	{
		// 이미 채널에 있는 유저일 경우
		return makeCRLF(ERR_USERONCHANNEL(client.getNickname(), nickname, ch_name));
	}
	if (!channel->isOperator(client))
	{
		// operator 아닐 경우
		return makeCRLF(ERR_CHANOPRIVSNEEDED(client.getNickname(), ch_name));
	}
	Client to;
	to = getClientByName(to, nickname);
	channel->addInvited(to);
	response = makeCRLF(RPL_INVITING(client.getNickname(), nickname, ch_name));
	directMsg(to, RPL_INVITE(client.getPrefix(), nickname, ch_name));
	return response;
}

void Server::clientLeaveChannel(Client &client, Channel *channel)
{
    // 채널명과 메시지 생성에 사용할 변수 초기화
    std::string ch_name = channel->getName();

    // 클라이언트가 채널에서 나가는 메시지를 브로드캐스트
    broadcast(ch_name, RPL_PART(client.getPrefix(), ch_name));

    // 클라이언트를 채널에서 나가게 하고, 채널에서 클라이언트 제거
    client.leaveChannel(channel);
    channel->deleteClient(client.getNickname());

    // 채널 내에 남아있는 사용자가 없으면 채널 삭제
    if (channel->getUsers().size() == 0)
    {
        this->channels.erase(ch_name);
        delete channel;
        channel = 0;
    }
}

std::string Server::handleMode(Client &client, std::stringstream &buffer_stream)
{
    std::string response;
    std::string ch_name;
    std::string modes;

    // 버퍼에서 채널명과 모드 정보를 추출
    buffer_stream >> ch_name;
    buffer_stream >> modes;

    // 채널명이 잘못된 경우(유저를 찾는 경우) 401 에러
    if (ch_name[0] != '#')
        return response;

    // 존재하지 않는 채널인 경우 403 에러
    if (channels.find(ch_name) == channels.end())
    {
        response = ERR_NOSUCHCHANNEL(client.getNickname(), ch_name);
        return response;
    }

    // 채널 객체를 가져옴
    Channel* p_channel = channels[ch_name];

    // 채널명만 들어온 경우 해당 채널의 모드 반환
    if (modes.empty())
    {
        response = getChannelModeResponse(client, p_channel);
        return response;
    }
    if (modes == "b")
    {
        // TODO: 모드 'b'에 대한 처리 추가
        response = RPL_ENDOFBANLIST(client.getNickname(), ch_name);
        return response;
    }

    int flag = 1;
    int pre_flag = 0;
    std::string ch_modes = "";
    std::string ch_params = "";

    // i t k l o
    for (int i = 0; i < modes.length(); i++)
    {
        if (modes[i] == '+')
            flag = 1;
        else if (modes[i] == '-')
            flag = -1;
        else if (modes[i] == 'i' || modes[i] == 't' || modes[i] == 'n')
        {
            // op 에러
            if (!p_channel->isOperator(client))
                response += makeCRLF(ERR_CHANOPRIVSNEEDEDMODE(client.getNickname(), ch_name, modes[i]));
            else if (flag == 1)
            {
                // 이미 설정된 모드인 경우 무시
                if (p_channel->findMode(modes[i]))
                    continue;

                // 모드 추가 및 변경된 모드 정보 저장
                p_channel->addMode(modes[i]);

                if (pre_flag == -1 || pre_flag == 0)
                    ch_modes += "+";
                ch_modes += modes[i];
                pre_flag = 1;
            }
            else if (flag == -1)
            {
                // 이미 설정되지 않은 모드인 경우 무시
                if (!p_channel->findMode(modes[i]))
                    continue;

                // 모드 삭제 및 변경된 모드 정보 저장
                p_channel->eraseMode(modes[i]);

                if (pre_flag == 1 || pre_flag == 0)
                    ch_modes += "-";
                ch_modes += modes[i];
                pre_flag = -1;
            }
        }
        else if (modes[i] == 'k')
        {
            std::string param;
            buffer_stream >> param;

            // 파라미터 에러
            if (param.empty())
                response += makeCRLF(ERR_NOOPPARAM(client.getNickname(), ch_name, modes[i], "key", "key"));
            // op 에러
            else if (!p_channel->isOperator(client))
                response += makeCRLF(ERR_CHANOPRIVSNEEDEDMODE(client.getNickname(), ch_name, modes[i]));
            // 패스워드 길이 초과
            else if (param.length() > 20)
                response += makeCRLF(ERR_LONGPWD(client.getNickname(), ch_name));
            // +k
            else if (flag == 1)
            {
                // 이미 설정된 모드인 경우 무시
                if (p_channel->findMode(modes[i]))
                    continue;

                // 패스워드 설정 및 변경된 모드 정보 저장
                p_channel->setPassword(param);
                p_channel->addMode(modes[i]);
                if (pre_flag == -1 || pre_flag == 0)
                    ch_modes += "+";
                ch_modes += modes[i];
                ch_params += " " + param;
                pre_flag = 1;
            }
            // -k
            else if (flag == -1)
            {
                // 이미 설정되지 않은 모드인 경우 무시
                if (!p_channel->findMode(modes[i]))
                    continue;
                // 입력된 패스워드가 일치하지 않는 경우 무시
                if (param != p_channel->getPassword())
                    continue;

                // 패스워드 초기화 및 변경된 모드 정보 저장
                std::string pw = "";
                p_channel->setPassword(pw);
                p_channel->eraseMode(modes[i]);
                if (pre_flag == 1 || pre_flag == 0)
                    ch_modes += "-";
                ch_modes += modes[i];
                ch_params += " " + param;
                pre_flag = -1;
            }
        }
        else if (modes[i] == 'l')
        {
            std::string param;
            buffer_stream >> param;

            // op 에러
            if (!p_channel->isOperator(client))
                response += makeCRLF(ERR_CHANOPRIVSNEEDEDMODE(client.getNickname(), ch_name, modes[i]));
            // -l
            else if (flag == -1)
            {
                // 이미 설정되지 않은 모드인 경우 무시
                if (!p_channel->findMode(modes[i]))
                    continue;

                // 사용자 제한 초기화 및 변경된 모드 정보 저장
                p_channel->setUserLimit(999999999);
                p_channel->eraseMode(modes[i]);
                if (pre_flag == 1 || pre_flag == 0)
                    ch_modes += "-";
                ch_modes += modes[i];
                pre_flag = -1;
            }
            // +l
            else if (flag == 1)
            {
                // 파라미터 에러
                if (param.empty())
                    response += makeCRLF(ERR_NOOPPARAM(client.getNickname(), ch_name, modes[i], "limit", "limit"));

                // 사용자 제한 길이 초과 시 기본값 0으로 설정
                if (param.length() > 18)
                    param = "0";

                // 사용자 제한 값 설정
                long long user_limit = atoll(param.c_str());

                // 현재 사용자 수보다 작은 값으로 설정할 경우 무시
                if (user_limit < p_channel->getUsers().size())
                    continue;

                // 이미 설정된 모드이고 변경된 값이 현재 값과 동일할 경우 무시
                if (p_channel->findMode(modes[i]) && user_limit == p_channel->getUserLimit())
                    continue;

                // 사용자 제한 설정 및 변경된 모드 정보 저장
                p_channel->setUserLimit(user_limit);
                p_channel->addMode(modes[i]);
                if (pre_flag == -1 || pre_flag == 0)
                    ch_modes += "+";
                ch_modes += modes[i];
                ch_params += " " + param;
                pre_flag = 1;
            }
        }
        else if (modes[i] == 'o')
        {
            std::string param;
            std::map<std::string, Client> ch_users = p_channel->getUsers();
            buffer_stream >> param;

            // 파라미터 에러
            if (param.empty())
                response += makeCRLF(ERR_NOOPPARAM(client.getNickname(), ch_name, modes[i], "op", "nick"));
            // op 에러
            else if (!p_channel->isOperator(client))
                response += makeCRLF(ERR_CHANOPRIVSNEEDEDMODE(client.getNickname(), ch_name, modes[i]));
            // 채널 내에 해당 유저가 없는 경우 에러
            else if (ch_users.find(param) == ch_users.end())
            {
                response += makeCRLF(ERR_NOSUCHNICK(client.getNickname(), param));
            }
            // +o
            else if (flag == 1)
            {
                Client to;
                to = getClientByName(to, param);
                // 이미 오퍼레이터인 경우 무시
                if (p_channel->isOperator(to))
                    continue;

                // 오퍼레이터 설정 및 변경된 모드 정보 저장
                p_channel->setOperator(to);
                if (pre_flag == -1 || pre_flag == 0)
                    ch_modes += "+";
                ch_modes += modes[i];
                ch_params += " " + param;
                pre_flag = 1;
            }
            // -o
            else if (flag == -1)
            {
                Client to;
                to = getClientByName(to, param);
                // 이미 오퍼레이터가 아닌 경우 무시
                if (!p_channel->isOperator(to))
                    continue;

                // 권한 변경 및 변경된 모드 정보 저장
                p_channel->changeAuth(COMMON, to);
                if (pre_flag == 1 || pre_flag == 0)
                    ch_modes += "-";
                ch_modes += modes[i];
                ch_params += " " + param;
                pre_flag = -1;
            }
        }
        else if (modes[i] == 'p' || modes[i] == 's' || modes[i] == 'v' || modes[i] == 'm' || modes[i] == 'b')
            continue;
        else
            response += makeCRLF(ERR_UNKNOWNMODE(client.getNickname(), modes[i]));
    }

    // 변경된 모드 정보를 브로드캐스트
    if (ch_params.empty() && !ch_modes.empty())
        ch_modes.insert(0, ":");
    else
    {
        std::string::size_type tmp = ch_params.rfind(" ");
        ch_params.insert(tmp + 1, ":");
    }

    if (!ch_modes.empty())
        broadcast(ch_name, RPL_MODE(client.getPrefix(), ch_name, ch_modes, ch_params));

    return response;
}

std::string Server::getChannelModeResponse(Client& client, Channel* p_channel)
{
    // 응답과 모드 정보를 저장할 변수 초기화
    std::string reply;
    std::string response;
    std::string ch_modes = "+";
    std::string ch_params = "";
    std::string ch_name = p_channel->getName();

    // 모드 카운터 초기화 및 모드 종류 확인을 위한 플래그 초기화
    int cnt = 0;
    int key = 0;

    // 채널이 키 또는 유저 리미트 모드를 가지고 있는지 확인하고 플래그 갱신
    if (p_channel->findMode('k'))
        key++;
    if (p_channel->findMode('l'))
        key++;

    // 채널의 모드 종류 확인
    std::set<char> modes = p_channel->getModes();

    // 키 또는 유저 리미트 모드가 없으면 모든 모드가 '+'로 설정
    if (key == 0)
        ch_modes = ":+";

    // 채널의 각 모드에 대한 정보를 확인하고 응답 메시지 구성
    for (std::set<char>::iterator m_it = modes.begin(); m_it != modes.end(); m_it++)
    {
        if (*m_it == 'k')
        {
            // 키 모드일 경우
            if (cnt == key - 1)
                ch_params += " :";
            else
                ch_params += " ";
            ch_params += p_channel->getPassword();
            cnt++;
        }
        else if (*m_it == 'l')
        {
            // 유저 리미트 모드일 경우
            if (cnt == key - 1)
                ch_params += " :";
            else
                ch_params += " ";
            ch_params += std::to_string(p_channel->getUserLimit());
            cnt++;
        }

        // 모드 문자열에 모드 추가
        ch_modes += *m_it;
    }

    // 채널 모드에 대한 응답 메시지 구성
    response = makeCRLF(RPL_CHANNELMODEIS(client.getNickname(), ch_name, ch_modes, ch_params));

    // 채널 생성 시간에 대한 응답 메시지 구성
    response += makeCRLF(RPL_CHANNELCREATETIME(client.getNickname(), ch_name, std::to_string(channels[ch_name]->getCreateTime())));

    return response;
}

// 클라이언트로부터 수신한 데이터를 파싱하는 함수
void Server::parseData(Client &client)
{
    std::string buffer = client.getBuffer(); // 클라이언트의 수신 버퍼를 가져옴

    size_t pos = 0;

    // 수신 버퍼에서 \r\n을 찾아가며 메시지 파싱 반복
    while (1)
    {
        if (client.getClose())
            break;

        std::string line;

        if (buffer.find("\r\n") != std::string::npos)
        {
            pos = buffer.find("\r\n");
            line = buffer.substr(0, pos + 1);
            std::cout << "line : " << line << std::endl;
        }
        else
        {
            std::string left_line = buffer;
            client.clearBuffer();
            if (!left_line.empty())
                client.addBuffer(left_line);
            break;
        }

        std::string method;
        std::string pre_method;
        std::string message;
        std::string response;
        std::stringstream buffer_stream(line);
        std::stringstream pre_stream;

        buffer_stream >> method;

        // 클라이언트가 아직 등록되지 않은 경우 (인증 과정)
        if (!client.getRegister())
        {
            pre_stream.str(client.getPreCmd());
            pre_stream >> pre_method;
        }

        // 클라이언트가 PASS 명령어를 처리 중이거나 인증되지 않은 경우
        if (method != "CAP" && !client.getRegister())
        {
            if (method == "PASS")
            {
                // 다음 버퍼까지 확인해서 마지막 PASS일 때 인증 과정 수행
                client.setPreCmd(line);
                this->send_data[client.getSocket()] += makeCRLF(response);
                buffer = buffer.substr(pos + 2);
                continue;
            }
            // 마지막 PASS일 때 인증 과정 수행
            else if (pre_method == "PASS")
            {
                std::string tmp("");
                client.setPreCmd(tmp);
                buffer.insert(0, makeCRLF(line));
                response = handlePass(client, pre_stream);
            }
            else
            {
                // PASS 명령어가 처리되지 않은 경우
                response = ERR_PASSWDMISMATCH(client.getNickname());
                this->send_data[client.getSocket()] = makeCRLF(response);
                client.clearBuffer();
                client.setClose(true);
                return;
            }
        }
        // PASS 명령어가 처리되었거나 이미 인증된 경우
        else if (method == "PASS")
        {
            response = handlePass(client, buffer_stream);
        }
        else if (method == "NICK")
        {
            response = handleNick(client, buffer_stream);
        }
        else if (method == "USER")
        {
            response = handleUser(client, buffer_stream);
        }
        else if (method == "PING")
        {
            response = handlePingpong(client, buffer_stream);
        }
        else if (method == "QUIT")
        {
            // QUIT 명령어 처리 후 메시지 받아옴
            message = handleQuit(client, buffer_stream);

            // 에러 응답 생성 및 전송
            response = ERR_QUIT(client.getPrefix(), message);
            this->send_data[client.getSocket()] += makeCRLF(response);

            // 응답 메시지 생성
            response = RPL_QUIT(client.getPrefix(), message);

            // 클라이언트가 속한 모든 채널에 브로드캐스팅
            std::map<std::string, Channel> channels = client.getChannels();
            for (std::map<std::string, Channel>::iterator m_it = channels.begin(); m_it != channels.end(); m_it++)
            {
                std::string ch_name = m_it->second.getName();
                this->broadcastNotSelf(ch_name, response, client.getSocket());
            }

            client.setClose(true); // 클라이언트 연결 종료 플래그 설정
            break;
        }
        else if (method == "JOIN")
        {
            response = handleJoin(client, buffer_stream);
        }
        else if (method == "PRIVMSG")
        {
            response = handlePrivmsg(client, buffer_stream);
        }
        else if (method == "LIST")
        {
            response = handleList(client, buffer_stream);
        }
        else if (method == "WHO")
        {
            response = handleWho(client, buffer_stream);
        }
        else if (method == "TOPIC")
        {
            response = handleTopic(client, buffer_stream);
        }
        else if (method == "PART")
        {
            response = handlePart(client, buffer_stream);
        }
        else if (method == "KICK")
        {
            response = handleKick(client, buffer_stream);
        }
        else if (method == "INVITE")
        {
            response = handleInvite(client, buffer_stream);
        }
        else if (method == "MODE")
        {
            response = handleMode(client, buffer_stream);
        }

        // 응답을 클라이언트의 전송 데이터에 추가
        this->send_data[client.getSocket()] += makeCRLF(response);

        // 버퍼에서 처리한 부분 제거
        buffer = buffer.substr(pos + 2, std::string::npos);
    }
}

void Server::changeChannelNick(Client& client, const std::string& before, const std::string& before_prefix)
{
	// 클라이언트의 닉네임이 변경될 때 채널 내에서의 닉네임 변경 및 관련 이벤트 처리 함수

	// 변경 전 닉네임과 변경 전 접두사를 저장하는 변수
	std::string ch_name;
	std::map<std::string, Channel> channels = client.getChannels();
	std::set<int> c_sockets; // 이벤트 변경을 필요로 하는 클라이언트 소켓의 집합
	int auth;

	// 클라이언트가 속한 모든 채널에 대해 반복
	for (std::map<std::string, Channel>::iterator m_it = channels.begin(); m_it != channels.end(); m_it++)
	{
		// 채널의 이름을 가져옴
		ch_name = m_it->second.getName();

		// 변경 전 닉네임에 해당하는 사용자 권한을 가져옴
		auth = this->channels[ch_name]->getAuth()[before];

		// 변경 전 닉네임으로 채널에서 클라이언트 삭제
		this->channels[ch_name]->deleteClient(before);

		// 변경 후 닉네임으로 채널에 클라이언트 추가 (이전 권한을 유지)
		this->channels[ch_name]->joinClient(client, auth);

		// 채널에 속한 모든 사용자의 소켓을 가져와서 이벤트 변경이 필요한 클라이언트 소켓 집합에 추가
		std::map<std::string, Client> users = this->channels[ch_name]->getUsers();
		for (std::map<std::string, Client>::iterator u_it = users.begin(); u_it != users.end(); u_it++)
		{
			c_sockets.insert(u_it->second.getSocket());
		}
	}

	// 변경된 닉네임에 대한 RPL_NICK 메시지를 채널에 속한 모든 사용자에게 전송하고, 이벤트 변경을 요청
	for (std::set<int>::iterator s_it = c_sockets.begin(); s_it != c_sockets.end(); s_it++)
	{
		// RPL_NICK 메시지를 생성하고 해당 소켓에 전송
		this->send_data[*s_it] += makeCRLF(RPL_NICK(before_prefix, client.getNickname()));
		// 소켓에 대한 이벤트 변경을 요청
		changeEvent(change_list, *s_it, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
	}
}


// 클라이언트 연결 해제 함수
void Server::disconnectClient(int client_fd)
{
    std::string ch_name;
    std::string nickname = clients[client_fd].getNickname();
    std::map<std::string, Channel> channels = this->clients[client_fd].getChannels();

    // 클라이언트가 속한 모든 채널에서 클라이언트 삭제 및 채널 정리
    for (std::map<std::string, Channel>::iterator m_it = channels.begin(); m_it != channels.end(); m_it++)
    {
        ch_name = m_it->second.getName();
        this->channels[ch_name]->deleteClient(nickname);

        // 채널 내에 유저가 없다면 채널 삭제 및 메모리 해제
        if (this->channels[ch_name]->getUsers().size() == 0)
        {
            delete this->channels[ch_name];
            this->channels[ch_name] = 0;
            this->channels.erase(ch_name);
        }
    }

    // 클라이언트와 관련된 데이터 정리
    this->send_data.erase(client_fd);
    this->clients.erase(client_fd);

    // 클라이언트 소켓 닫기
    std::cout << "close client " << client_fd << std::endl;
    close(client_fd);
}

void Server::setPort(int port)
{
	this->port = port;
}

int Server::getPort() const
{
	return this->port;
}

void Server::setPassword(std::string password)
{
	this->password = password;
}

std::string Server::getPassword() const
{
	return this->password;
}

std::map<std::string, Channel *> Server::getChannels() const
{
	return this->channels;
}

std::map<int, Client> Server::getClients() const
{
	return this->clients;
}

// 클라이언트가 존재하는지 확인하는 함수
bool Server::isClient(const std::string& nickname)
{
    // 클라이언트 맵을 순회하며 닉네임 비교
    for(std::map<int, Client>::iterator c_it = this->clients.begin(); c_it != this->clients.end(); c_it++)
    {
        if (c_it->second.getNickname() == nickname)
            return true;
    }
    // 해당 닉네임을 가진 클라이언트가 없음
    return false;
}

// 클라이언트 닉네임에 해당하는 클라이언트를 찾는 함수
Client& Server::getClientByName(Client& client, const std::string& nickname)
{
    // 클라이언트 맵을 순회하며 닉네임 비교
    for(std::map<int, Client>::iterator c_it = this->clients.begin(); c_it != this->clients.end(); c_it++)
    {
        if (c_it->second.getNickname() == nickname)
        {
            // 해당 닉네임을 가진 클라이언트를 찾으면 client에 저장하고 반환
            client = c_it->second;
            return client;
        }
    }
    // 해당 닉네임을 가진 클라이언트가 없으면 입력된 client를 그대로 반환
    return client;
}

// 모든 채널을 삭제하는 함수
void Server::deleteChannel()
{
    if (!this->channels.empty())
    {
        // 채널 맵을 순회하며 채널 삭제 및 메모리 해제
        for(std::map<std::string, Channel *>::iterator ch_it = this->channels.begin(); ch_it != this->channels.end(); ch_it++)
        {
            Channel *channel = ch_it->second;
            delete channel;
            channel = 0;
            // 채널 맵에서 삭제
            this->channels.erase(ch_it->first);
        }
    }
}

// 모든 클라이언트 소켓을 닫는 함수
void Server::closeClient()
{
    // 현재 서버의 클라이언트 맵을 복사
    std::map<int, Client> clients = clients;

    // 클라이언트 맵을 순회하며 소켓 닫기
    for (std::map<int, Client>::iterator c_it = clients.begin(); c_it != clients.end(); c_it++)
        close(c_it->first);
}
