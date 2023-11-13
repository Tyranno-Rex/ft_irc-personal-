
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <exception>

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

class server
{
// 가지고 잇어야하는 변수
private:
    strcut 
public:
    server();
    ~server();

	/*
	명령어 기반 함수 작성해야함 -> 최소 구현 함수들
	JOIN/PASS/NICK/USER/PINGPONG/QUIT/WHO/PRIVMSG/LIST/TOPIC/PART/KICK/INVITE/MODE
	*/

    void FuncPass();
    void FuncUser();
};

server::server()
{
    server
}

server::~server()
{
}
