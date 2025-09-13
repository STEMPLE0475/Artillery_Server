
1. 프로젝트 소개 (Introduction)

'Artillery' 는 C++ Winsock 기반 서버와 언리얼 클라이언트로 구현한 2:2 온라인 포격 대전 게임입니다. 과거 끝말잇기 서버 개발 경험을 발전시켜, 복잡한 인게임 로직을 직접 제어하는 서버를 구현하는 것을 목표로 했습니다. 직접 설계한 패킷 프로토콜로 통신하고 MySQL DB로 유저 데이터를 관리합니다.

2. 주요 기능 (Features)

비동기 서버: C++ Winsock의 select 모델 기반 비동기 서버 


DB 연동: MySQL을 이용한 유저 데이터(로그인 등) 관리 

게임 로직:

최대 4인 접속 및 2v2 팀 기반 매치메이킹 


서버 권위(Server-Authoritative)의 포탄 궤적 물리 계산 및 동기화 


디버깅 시스템:

FSM(유한 상태 머신) 기반 테스트 봇(Bot) 시스템 구현 

1인 개발 환경에서 4인 멀티플레이 테스트 가능 


3. 기술 스택 (Tech Stack)

Server: C++, Winsock, MySQL 


Client: C++, Unreal Engine 


Protocol: 직접 설계한 커스텀 패킷 프로토콜 

Build: Visual Studio, CMake (예시)

Version Control: Git, GitHub


4. 참고 소스
클라이언트(언리얼) : https://github.com/STEMPLE0475/Artillery_Client
공용 프로토콜 : https://github.com/STEMPLE0475/Artillery_Protocol
