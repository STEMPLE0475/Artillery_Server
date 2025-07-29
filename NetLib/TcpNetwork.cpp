#include <cstring>
#include <vector>
#include <deque>

#include "ILog.h"
#include "TcpNetwork.h"


namespace NServerNetLib
{
	TcpNetwork::TcpNetwork()
	{

	}

	TcpNetwork::~TcpNetwork()
	{
		for (auto& client : m_ClientSessionPool) // 클라이언트 배열
		{
			if (client.pRecvBuffer) { // client안에 수신 버퍼랑 송신 버퍼를 나누어서 관리한다?
				delete[] client.pRecvBuffer;
			}

			if (client.pSendBuffer) {
				delete[] client.pSendBuffer;
			}
		}
	}

	NET_ERROR_CODE TcpNetwork::Init(const ServerConfig* pConfig, ILog* pLogger) // 서버 실행
	{
		std::memcpy(&m_Config, pConfig, sizeof(ServerConfig));
		//m은 멤버를 표시. p는 포인터

		m_pRefLogger = pLogger;

		auto initRet = InitServerSocket(); // 서버 소켓 생성
		//Ret 변수로으로 오류 감지하는 듯
		if (initRet != NET_ERROR_CODE::NONE)
		{
			return initRet;
		}

		auto bindListenRet = BindListen(pConfig->Port, pConfig->BackLogCount); // 서버 바인드와 리슨
		if (bindListenRet != NET_ERROR_CODE::NONE)
		{
			return bindListenRet;
		}

		FD_ZERO(&m_Readfds); // 감시할 소켓 목록(m_Readfds)을 초기화 FD_ZERO
		FD_SET(m_ServerSockfd, &m_Readfds); // 감시할 소켓 목록(m_Readfds)에 m_ServerSockfd 서버 소켓을 추가 FD_SET
		auto sessionPoolSize = CreateSessionPool(pConfig->MaxClientCount + pConfig->ExtraClientCount);
		//클라이언트 세션 풀을 미리 생성함.

		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Session Pool Size: %d", __FUNCTION__, sessionPoolSize);

		return NET_ERROR_CODE::NONE;
	}

	void TcpNetwork::Release()
	{
		WSACleanup();
	}

	RecvPacketInfo TcpNetwork::GetPacketInfo() // 가장 앞에 있는 패킷을 반환한다.
	{
		RecvPacketInfo packetInfo;

		if (m_PacketQueue.empty() == false) // m_PacketQueue에 받은 패킷이 있는지 확인
		{
			packetInfo = m_PacketQueue.front(); // m_PacketQueue에 맨 앞에 것. ( 가장 먼저 들어온 패킷 확인 )
			m_PacketQueue.pop_front(); // 꺼내고
		}

		return packetInfo; // 반환
	}

	void TcpNetwork::ForcingClose(const int sessionIndex) // 클라 강제 종료
	{
		if (m_ClientSessionPool[sessionIndex].IsConnected() == false) {
			return;
		}

		CloseSession(SOCKET_CLOSE_CASE::FORCING_CLOSE, m_ClientSessionPool[sessionIndex].SocketFD, sessionIndex);
	}

	void TcpNetwork::Run() // update 작동 중 로직.
	{
		auto read_set = m_Readfds; // 해당 스레드에서 현재 관리하고 있는 모든 소켓의 정보를 가지고 있다.
		//연결된 모든 세션을 write 이벤트를 조사하고 있는데 사실 다 할 필요는 없다. 이전에 send 버퍼가 다 찼던 세션만 조사해도 된다.
		auto write_set = m_Readfds;

		timeval timeout{ 0, 1000 }; //tv_sec, tv_usec 0초 + 1000마이크로초(1밀리초) 만큼 받는다.

		auto selectResult = select(0, &read_set, &write_set, 0, &timeout); // select 호출. 가장 중요한 부분. timeout시간. 1밀리초 동안 이벤트 발생 확인
		//다만 Server.cpp에서 Run은 서버가 열려있으면 while(true)로 실행되는데, 이러면 중첩해서 무한히 RunCheckSelectResult()를 실행하지 않는가?
		// -> select 함수는 블로킹 함수임. 지정한 밀리초만큼 서버 루프를 대기함.

		auto isFDSetChanged = RunCheckSelectResult(selectResult); // select 함수의 반환을 이용하여. 이벤트 발생 확인
		if (isFDSetChanged == false)
		{
			return; // select로 이벤트 감지중 오류가 발생하면 Run() 루프를 종료. -> 1밀리초 이후에 다음 Run()실행해서 확인
		}

		// Accept
		if (FD_ISSET(m_ServerSockfd, &read_set))
			// read_set(읽을 준비가 된 소켓들의 목록) 안에 m_ServerSockfd(서버 소켓)이 포함되어 있다면.
			// 클라이언트가 서버에 접속을 시도하면, 서버 소켓의 read 버퍼에 이벤트로 기록이 된다. (서버 연결 시도가 있었다)
		{
			NewSession();
		}

		RunCheckSelectClients(read_set, write_set);
	}

	bool TcpNetwork::RunCheckSelectResult(const int result)
	{
		if (result == 0) // 아무런 소켓 이벤트가 없었음.
		{
			return false;
		}
		else if (result == -1) // 에러 발생. -> 멈춰라
		{
			//linux에서 signal을 핸드링하지 않았는데 시그날이 발생하면 select에서 받아서 결과가 -1이 나온다
			return false;
		}

		return true; // result가 0보다 크면 이벤트가 발생했다는 것임.
	}

	//감지된 read_set이나 write_set이 어느 세션의 소켓인지 확인함.
	void TcpNetwork::RunCheckSelectClients(fd_set& read_set, fd_set& write_set)
	{
		for (int i = 0; i < m_ClientSessionPool.size(); ++i)
		{
			auto& session = m_ClientSessionPool[i];

			if (session.IsConnected() == false) {
				continue;
			}

			SOCKET fd = session.SocketFD;
			auto sessionIndex = session.Index;

			// check read
			auto retReceive = RunProcessReceive(sessionIndex, fd, read_set);
			if (retReceive == false) {
				continue;
			}

			// check write
			RunProcessWrite(sessionIndex, fd, write_set);
		}
	}

	bool TcpNetwork::RunProcessReceive(const int sessionIndex, const SOCKET fd, fd_set& read_set)
	{
		if (!FD_ISSET(fd, &read_set))
		{
			return true;
		}

		auto ret = RecvSocket(sessionIndex);
		if (ret != NET_ERROR_CODE::NONE)
		{
			CloseSession(SOCKET_CLOSE_CASE::SOCKET_RECV_ERROR, fd, sessionIndex);
			return false;
		}

		ret = RecvBufferProcess(sessionIndex);
		if (ret != NET_ERROR_CODE::NONE)
		{
			CloseSession(SOCKET_CLOSE_CASE::SOCKET_RECV_BUFFER_PROCESS_ERROR, fd, sessionIndex);
			return false;
		}

		return true;
	}

	NET_ERROR_CODE TcpNetwork::SendData(const int sessionIndex, const short packetId, const short bodySize, const char* pMsg)
	{
		auto& session = m_ClientSessionPool[sessionIndex];

		auto pos = session.SendSize;
		auto totalSize = (int16_t)(bodySize + PACKET_HEADER_SIZE);

		if ((pos + totalSize) > m_Config.MaxClientSendBufferSize) {
			return NET_ERROR_CODE::CLIENT_SEND_BUFFER_FULL;
		}

		PacketHeader pktHeader{ totalSize, packetId, (uint8_t)0 };
		memcpy(&session.pSendBuffer[pos], (char*)&pktHeader, PACKET_HEADER_SIZE);

		if (bodySize > 0)
		{
			memcpy(&session.pSendBuffer[pos + PACKET_HEADER_SIZE], pMsg, bodySize);
		}

		session.SendSize += totalSize;

		return NET_ERROR_CODE::NONE;
	}

	int TcpNetwork::CreateSessionPool(const int maxClientCount)
	{
		//서버 시작시, 미리 50(최대 접속 클라이언트)개의 클라이언트세션 풀을 만들어둠.
		//할당은 되지 않았으나, 미리 만들들어둠. -> 클라이언트 접속/해제 시 세션을 빌려주고 반환
		//Pooling 최적화 기법 사용 -> 자주 사용하는 자원을 미리 만들어두고 재사용
		//1.  매번 new로 생성/삭제 하고 delete로 해제하면, 비용이 크다. -> 메모리 단편화 발생
		//2. 최대 동접 제한 관리. 리소스 초과를 방지한다.
		for (int i = 0; i < maxClientCount; ++i)
		{
			ClientSession session;
			session.Clear(); // ClientSession을 초기값으로 세팅
			session.Index = i; // ClientSession에 인덱스를 부과한다? 
			session.pRecvBuffer = new char[m_Config.MaxClientRecvBufferSize]; // nullptr 상태의 값에다가 배열을 동적으로 생성해준다.
			session.pSendBuffer = new char[m_Config.MaxClientSendBufferSize];

			m_ClientSessionPool.push_back(session); // 클라이언트 세션 풀 배열 vector
			m_ClientSessionPoolIndex.push_back(session.Index);	// 비어있는 풀을 데큐로 저장. 할당 요청이 오면 앞이든 뒤는 빼서 쓰면 됨.
		}

		return maxClientCount;
	}
	int TcpNetwork::AllocClientSessionIndex() // 클라이언트 새션을 할당한다. -> 풀에서 하나 가져감
	{
		if (m_ClientSessionPoolIndex.empty()) //반약 할당할 풀이 없다면, -1 return
		{
			return -1;
		}

		int index = m_ClientSessionPoolIndex.front();
		m_ClientSessionPoolIndex.pop_front(); // 앞에서 부터 뺀다.
		return index; // 해당 세션의 인덱스를 반환한다. (할당할 인덱스)
	}
	void TcpNetwork::ReleaseSessionIndex(const int index) // 클라이언트 세션을 해제한다. -> 풀에 추가함.
	{
		//클라이언트 세션 풀에다가 해당 인덱스를 추가한다. 또한, 기본값으로 초기화 해줌.
		m_ClientSessionPoolIndex.push_back(index);
		m_ClientSessionPool[index].Clear();
	}

	NET_ERROR_CODE TcpNetwork::InitServerSocket() // 서버 여는 기본 -> 소켓 생성
	{
		WORD wVersionRequested = MAKEWORD(2, 2); // 1. WSADATA 초기화
		WSADATA wsaData;
		WSAStartup(wVersionRequested, &wsaData);

		m_ServerSockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 2. 소켓 생성
		if (m_ServerSockfd < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_CREATE_FAIL;
		}

		auto n = 1;
		if (setsockopt(m_ServerSockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n)) < 0)
			// set socket option을 설정. -> 소켓의 동작 방식을 세밀하게 조정. SO_REUSEADDR를 설정해서, 포트를 사용중에 재사용 해도 된다고 선언.
			// 포트를 이미 사용중이라고 발생하는 오류 없앰. (소켓, 옵션 레벨, 옵션 이름, 옵션값이 저장된 버퍼 주소, 옵션값의 크기)
			//1로 설정하면 켜겠다는 의미.

			//함수 원형이 char*을 요구함. 내부적으로는 int이지만, char*은 어느 자료형도 받아올 수 있음.
		{
			return NET_ERROR_CODE::SERVER_SOCKET_SO_REUSEADDR_FAIL;
		}

		return NET_ERROR_CODE::NONE; // 잘 되면 에러가 없음
	}
	NET_ERROR_CODE TcpNetwork::BindListen(short port, int backlogCount) //주소 바인드
	{
		struct sockaddr_in server_addr;
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		server_addr.sin_port = htons(port);

		if (bind(m_ServerSockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_BIND_FAIL;
		}


		auto netError = SetNonBlockSocket(m_ServerSockfd);
		if (netError != NET_ERROR_CODE::NONE)
		{
			return netError;
		}

		if (listen(m_ServerSockfd, backlogCount) == SOCKET_ERROR)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_LISTEN_FAIL;
		}

		m_MaxSockFD = m_ServerSockfd;

		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Listen. ServerSockfd(%I64u)", __FUNCTION__, m_ServerSockfd);
		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TcpNetwork::NewSession() // accept 부분
	{
		auto tryCount = 0; // 너무 많이 accept를 시도하지 않도록 한다.

		//FD_SETSIZE는 select 함수가 한 번에 감시할 수 있는 소켓의 수.
		//접속 시도하는 수 tryCount가 증가함. 모두 처리할 때까지 반복함.
		do
		{
			++tryCount;

			struct sockaddr_in client_adr;

			auto client_len = static_cast<int>(sizeof(client_adr));
			auto client_sockfd = accept(m_ServerSockfd, (struct sockaddr*)&client_adr, &client_len);
			//client_sockfd에 클아이언트 소켓을 저장함

			//m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "%s | client_sockfd(%I64u)", __FUNCTION__, client_sockfd);
			if (client_sockfd == INVALID_SOCKET)
			{
				if (WSAGetLastError() == WSAEWOULDBLOCK)
				{
					return NET_ERROR_CODE::ACCEPT_API_WSAEWOULDBLOCK;
				}
				m_pRefLogger->Write(LOG_TYPE::L_ERROR, "%s | Wrong socket cannot accept", __FUNCTION__);
				return NET_ERROR_CODE::ACCEPT_API_ERROR;
				//Accept할 것이 없으면 끝남.
			}

			auto newSessionIndex = AllocClientSessionIndex(); // 세션 할당 -> 남아있는 세션 풀의 인덱스를 반환함. (사용할 자리)
			if (newSessionIndex < 0) // 자리가 없으면. (새로운 새션을 할당하는데 실패했을 경우 -1을 반환. 세션 풀링에서 값을 할당함.)
			{
				m_pRefLogger->Write(LOG_TYPE::L_WARN, "%s | client_sockfd(%I64u)  >= MAX_SESSION", __FUNCTION__, client_sockfd);

				// 더 이상 수용할 수 없으므로 바로 짜른다.
				CloseSession(SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY, client_sockfd, -1);
				return NET_ERROR_CODE::ACCEPT_MAX_SESSION_COUNT;
			}


			char clientIP[MAX_IP_LEN] = { 0, };
			inet_ntop(AF_INET, &(client_adr.sin_addr), clientIP, MAX_IP_LEN - 1);

			SetSockOption(client_sockfd); //해당 클라 소켓의 옵션 설정

			SetNonBlockSocket(client_sockfd);
			//클라이언트 소켓을 논블로킹 모드로 연결함. 데이터가 없거나 버퍼가 꽉 차도 기다리지 않도록 반환되도록 기다림.
			// 서버가 멈추지 않기 위해서 중요함

			FD_SET(client_sockfd, &m_Readfds);// 감시하는 소켓에 client_sockfd를 넣음
			//m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "%s | client_sockfd(%I64u)", __FUNCTION__, client_sockfd);
			ConnectedSession(newSessionIndex, client_sockfd, clientIP);

		} while (tryCount < FD_SETSIZE);

		return NET_ERROR_CODE::NONE;
	}

	//해당 클라이언트 연결이 성공적으로 끝난 후, 세션의 정보를 초기화 하고 서버에 등록함.
	void TcpNetwork::ConnectedSession(const int sessionIndex, const SOCKET fd, const char* pIP)
	{

		if (m_MaxSockFD < fd)
		{
			m_MaxSockFD = fd; // 소켓 fd의 가장 큰 값을 넣음.???
		}

		++m_ConnectSeq; // 시퀸스 번호 증가해서 할당

		auto& session = m_ClientSessionPool[sessionIndex]; // 세션 풀중에 선택된 것에다가 정보를 넣음.
		session.Seq = m_ConnectSeq; // 시퀸스 번호 할당
		session.SocketFD = fd; // 소켓(fd)도 할당함.
		memcpy(session.IP, pIP, MAX_IP_LEN - 1); // ip도 넣음

		++m_ConnectedSessionCount;

		AddPacketQueue(sessionIndex, (short)PACKET_ID::NTF_SYS_CONNECT_SESSION, 0, nullptr);

		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | New Session. FD(%I64u), m_ConnectSeq(%d), IP(%s)", __FUNCTION__, fd, m_ConnectSeq, pIP);
	}

	void TcpNetwork::SetSockOption(const SOCKET fd)
	{
		linger ling;
		ling.l_onoff = 0;
		ling.l_linger = 0;
		setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));
		//소켓을 닫을 때 블로킹 되지 않아서, 서버의 응답성 유지하기 위해 사용

		int size1 = m_Config.MaxClientSockOptRecvBufferSize;
		int size2 = m_Config.MaxClientSockOptSendBufferSize;
		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&size1, sizeof(size1));
		//수신 버퍼의 크기 설정 MaxClientSockOptRecvBufferSize
		setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&size2, sizeof(size2));
		//송신 버퍼 크기 설정
	}

	void TcpNetwork::CloseSession(const SOCKET_CLOSE_CASE closeCase, const SOCKET sockFD, const int sessionIndex)
	{
		if (closeCase == SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY)
		{
			closesocket(sockFD);
			FD_CLR(sockFD, &m_Readfds);
			return;
		}

		if (m_ClientSessionPool[sessionIndex].IsConnected() == false) {
			return;
		}


		closesocket(sockFD);
		FD_CLR(sockFD, &m_Readfds);

		m_ClientSessionPool[sessionIndex].Clear();
		--m_ConnectedSessionCount;
		ReleaseSessionIndex(sessionIndex);

		AddPacketQueue(sessionIndex, (short)PACKET_ID::NTF_SYS_CLOSE_SESSION, 0, nullptr);
	}

	NET_ERROR_CODE TcpNetwork::RecvSocket(const int sessionIndex)
	{
		auto& session = m_ClientSessionPool[sessionIndex];
		auto fd = static_cast<SOCKET>(session.SocketFD);

		if (session.IsConnected() == false)
		{
			return NET_ERROR_CODE::RECV_PROCESS_NOT_CONNECTED;
		}

		int recvPos = 0;

		if (session.RemainingDataSize > 0)
		{
			memcpy(session.pRecvBuffer, &session.pRecvBuffer[session.PrevReadPosInRecvBuffer], session.RemainingDataSize);
			recvPos += session.RemainingDataSize;
		}

		auto recvSize = recv(fd, &session.pRecvBuffer[recvPos], (MAX_PACKET_BODY_SIZE * 2), 0);

		if (recvSize == 0)
		{
			return NET_ERROR_CODE::RECV_REMOTE_CLOSE;
		}

		if (recvSize < 0)
		{
			auto netError = WSAGetLastError();

			if (netError != WSAEWOULDBLOCK)
			{
				return NET_ERROR_CODE::RECV_API_ERROR;
			}
			else
			{
				return NET_ERROR_CODE::NONE;
			}
		}

		session.RemainingDataSize += recvSize;
		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TcpNetwork::RecvBufferProcess(const int sessionIndex)
	{
		auto& session = m_ClientSessionPool[sessionIndex];

		auto readPos = 0;
		const auto dataSize = session.RemainingDataSize;
		PacketHeader* pPktHeader;

		while ((dataSize - readPos) >= PACKET_HEADER_SIZE)
		{
			pPktHeader = (PacketHeader*)&session.pRecvBuffer[readPos];
			readPos += PACKET_HEADER_SIZE;
			auto bodySize = (int16_t)(pPktHeader->TotalSize - PACKET_HEADER_SIZE);

			if (bodySize > 0)
			{
				if (bodySize > (dataSize - readPos))
				{
					readPos -= PACKET_HEADER_SIZE;
					break;
				}

				if (bodySize > MAX_PACKET_BODY_SIZE)
				{
					// 더 이상 이 세션과는 작업을 하지 않을 예정. 클라이언트 보고 나가라고 하던가 직접 짤라야 한다.
					return NET_ERROR_CODE::RECV_CLIENT_MAX_PACKET;
				}
			}

			AddPacketQueue(sessionIndex, pPktHeader->Id, bodySize, &session.pRecvBuffer[readPos]);
			readPos += bodySize;
		}

		session.RemainingDataSize -= readPos;
		session.PrevReadPosInRecvBuffer = readPos;

		return NET_ERROR_CODE::NONE;
	}

	void TcpNetwork::AddPacketQueue(const int sessionIndex, const short pktId, const short bodySize, char* pDataPos)
	{
		RecvPacketInfo packetInfo;
		packetInfo.SessionIndex = sessionIndex;
		packetInfo.PacketId = pktId;
		packetInfo.PacketBodySize = bodySize;
		packetInfo.pRefData = pDataPos;

		m_PacketQueue.push_back(packetInfo);
	}

	void TcpNetwork::RunProcessWrite(const int sessionIndex, const SOCKET fd, fd_set& write_set)
	{
		if (!FD_ISSET(fd, &write_set))
		{
			return;
		}

		auto retsend = FlushSendBuff(sessionIndex);
		if (retsend.Error != NET_ERROR_CODE::NONE)
		{
			CloseSession(SOCKET_CLOSE_CASE::SOCKET_SEND_ERROR, fd, sessionIndex);
		}
	}

	NetError TcpNetwork::FlushSendBuff(const int sessionIndex)
	{
		auto& session = m_ClientSessionPool[sessionIndex];
		auto fd = static_cast<SOCKET>(session.SocketFD);

		if (session.IsConnected() == false)
		{
			return NetError(NET_ERROR_CODE::CLIENT_FLUSH_SEND_BUFF_REMOTE_CLOSE);
		}

		auto result = SendSocket(fd, session.pSendBuffer, session.SendSize);

		if (result.Error != NET_ERROR_CODE::NONE) {
			return result;
		}

		auto sendSize = result.Vlaue;
		if (sendSize < session.SendSize)
		{
			memmove(&session.pSendBuffer[0],
				&session.pSendBuffer[sendSize],
				session.SendSize - sendSize);

			session.SendSize -= sendSize;
		}
		else
		{
			session.SendSize = 0;
		}
		return result;
	}

	NetError TcpNetwork::SendSocket(const SOCKET fd, const char* pMsg, const int size)
	{
		NetError result(NET_ERROR_CODE::NONE);
		auto rfds = m_Readfds;

		// 접속 되어 있는지 또는 보낼 데이터가 있는지
		if (size <= 0)
		{
			return result;
		}

		result.Vlaue = send(fd, pMsg, size, 0);

		if (result.Vlaue <= 0)
		{
			result.Error = NET_ERROR_CODE::SEND_SIZE_ZERO;
		}

		return result;
	}

	NET_ERROR_CODE TcpNetwork::SetNonBlockSocket(const SOCKET sock)
	{
		unsigned long mode = 1;

		if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_FIONBIO_FAIL;
		}

		return NET_ERROR_CODE::NONE;
	}

}