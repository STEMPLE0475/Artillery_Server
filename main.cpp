#include <iostream>
#include <thread>
#include <conio.h> // _kbhit, _getch

#include "Server.h"


int main()
{
	Server server;
	server.Init(); // 서버를 실행

	//[&객체] 가 아닌 [&] => 람다 함수 본문에서 사용되는 모든 외부 스코프의 지역 변수들을 자동으로 참조로 캡쳐한다. 
	std::thread runThread([&]() {
		server.Run(); } // 서버가 종료될 때까지 계속해서 스레드에서 실행됨.
	);

	std::cout << "Server Running... (b: Create 4 Bots, q: Quit)" << std::endl;

	while (true)
	{
		char input = _getch(); // 엔터 없이 바로 키 입력 받기

		if (input == 'b')
		{
			// 'b'를 누르면 Server 객체의 CreateBots 함수를 호출합니다.
			server.CreateBots(1024);
		}
		else if (input == 'q')
		{
			// 'q'를 누르면 서버를 종료합니다.
			break; // while 루프 탈출
		}
	}
	server.Stop();
	runThread.join();

	return 0;
}