#include <iostream>
#include <thread>

#include "Server.h"

int main()
{
	Server server;
	server.Init(); // 서버를 실행

	//[&객체] 가 아닌 [&] => 람다 함수 본문에서 사용되는 모든 외부 스코프의 지역 변수들을 자동으로 참조로 캡쳐한다. 
	std::thread runThread([&]() {
		server.Run(); } // 서버가 종료될 때까지 계속해서 스레드에서 실행됨.
	);



	std::cout << "press any key to exit...";
	getchar(); // char을 입력을 받으면 서버 종료되도록 한다. 간단하게 구현할 수 있는 로직.

	server.Stop();
	runThread.join();

	return 0;
}