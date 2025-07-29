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
		for (auto& client : m_ClientSessionPool) // Ŭ���̾�Ʈ �迭
		{
			if (client.pRecvBuffer) { // client�ȿ� ���� ���۶� �۽� ���۸� ����� �����Ѵ�?
				delete[] client.pRecvBuffer;
			}

			if (client.pSendBuffer) {
				delete[] client.pSendBuffer;
			}
		}
	}

	NET_ERROR_CODE TcpNetwork::Init(const ServerConfig* pConfig, ILog* pLogger) // ���� ����
	{
		std::memcpy(&m_Config, pConfig, sizeof(ServerConfig));
		//m�� ����� ǥ��. p�� ������

		m_pRefLogger = pLogger;

		auto initRet = InitServerSocket(); // ���� ���� ����
		//Ret ���������� ���� �����ϴ� ��
		if (initRet != NET_ERROR_CODE::NONE)
		{
			return initRet;
		}

		auto bindListenRet = BindListen(pConfig->Port, pConfig->BackLogCount); // ���� ���ε�� ����
		if (bindListenRet != NET_ERROR_CODE::NONE)
		{
			return bindListenRet;
		}

		FD_ZERO(&m_Readfds); // ������ ���� ���(m_Readfds)�� �ʱ�ȭ FD_ZERO
		FD_SET(m_ServerSockfd, &m_Readfds); // ������ ���� ���(m_Readfds)�� m_ServerSockfd ���� ������ �߰� FD_SET
		auto sessionPoolSize = CreateSessionPool(pConfig->MaxClientCount + pConfig->ExtraClientCount);
		//Ŭ���̾�Ʈ ���� Ǯ�� �̸� ������.

		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Session Pool Size: %d", __FUNCTION__, sessionPoolSize);

		return NET_ERROR_CODE::NONE;
	}

	void TcpNetwork::Release()
	{
		WSACleanup();
	}

	RecvPacketInfo TcpNetwork::GetPacketInfo() // ���� �տ� �ִ� ��Ŷ�� ��ȯ�Ѵ�.
	{
		RecvPacketInfo packetInfo;

		if (m_PacketQueue.empty() == false) // m_PacketQueue�� ���� ��Ŷ�� �ִ��� Ȯ��
		{
			packetInfo = m_PacketQueue.front(); // m_PacketQueue�� �� �տ� ��. ( ���� ���� ���� ��Ŷ Ȯ�� )
			m_PacketQueue.pop_front(); // ������
		}

		return packetInfo; // ��ȯ
	}

	void TcpNetwork::ForcingClose(const int sessionIndex) // Ŭ�� ���� ����
	{
		if (m_ClientSessionPool[sessionIndex].IsConnected() == false) {
			return;
		}

		CloseSession(SOCKET_CLOSE_CASE::FORCING_CLOSE, m_ClientSessionPool[sessionIndex].SocketFD, sessionIndex);
	}

	void TcpNetwork::Run() // update �۵� �� ����.
	{
		auto read_set = m_Readfds; // �ش� �����忡�� ���� �����ϰ� �ִ� ��� ������ ������ ������ �ִ�.
		//����� ��� ������ write �̺�Ʈ�� �����ϰ� �ִµ� ��� �� �� �ʿ�� ����. ������ send ���۰� �� á�� ���Ǹ� �����ص� �ȴ�.
		auto write_set = m_Readfds;

		timeval timeout{ 0, 1000 }; //tv_sec, tv_usec 0�� + 1000����ũ����(1�и���) ��ŭ �޴´�.

		auto selectResult = select(0, &read_set, &write_set, 0, &timeout); // select ȣ��. ���� �߿��� �κ�. timeout�ð�. 1�и��� ���� �̺�Ʈ �߻� Ȯ��
		//�ٸ� Server.cpp���� Run�� ������ ���������� while(true)�� ����Ǵµ�, �̷��� ��ø�ؼ� ������ RunCheckSelectResult()�� �������� �ʴ°�?
		// -> select �Լ��� ���ŷ �Լ���. ������ �и��ʸ�ŭ ���� ������ �����.

		auto isFDSetChanged = RunCheckSelectResult(selectResult); // select �Լ��� ��ȯ�� �̿��Ͽ�. �̺�Ʈ �߻� Ȯ��
		if (isFDSetChanged == false)
		{
			return; // select�� �̺�Ʈ ������ ������ �߻��ϸ� Run() ������ ����. -> 1�и��� ���Ŀ� ���� Run()�����ؼ� Ȯ��
		}

		// Accept
		if (FD_ISSET(m_ServerSockfd, &read_set))
			// read_set(���� �غ� �� ���ϵ��� ���) �ȿ� m_ServerSockfd(���� ����)�� ���ԵǾ� �ִٸ�.
			// Ŭ���̾�Ʈ�� ������ ������ �õ��ϸ�, ���� ������ read ���ۿ� �̺�Ʈ�� ����� �ȴ�. (���� ���� �õ��� �־���)
		{
			NewSession();
		}

		RunCheckSelectClients(read_set, write_set);
	}

	bool TcpNetwork::RunCheckSelectResult(const int result)
	{
		if (result == 0) // �ƹ��� ���� �̺�Ʈ�� ������.
		{
			return false;
		}
		else if (result == -1) // ���� �߻�. -> �����
		{
			//linux���� signal�� �ڵ帵���� �ʾҴµ� �ñ׳��� �߻��ϸ� select���� �޾Ƽ� ����� -1�� ���´�
			return false;
		}

		return true; // result�� 0���� ũ�� �̺�Ʈ�� �߻��ߴٴ� ����.
	}

	//������ read_set�̳� write_set�� ��� ������ �������� Ȯ����.
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
		//���� ���۽�, �̸� 50(�ִ� ���� Ŭ���̾�Ʈ)���� Ŭ���̾�Ʈ���� Ǯ�� ������.
		//�Ҵ��� ���� �ʾ�����, �̸� �������. -> Ŭ���̾�Ʈ ����/���� �� ������ �����ְ� ��ȯ
		//Pooling ����ȭ ��� ��� -> ���� ����ϴ� �ڿ��� �̸� �����ΰ� ����
		//1.  �Ź� new�� ����/���� �ϰ� delete�� �����ϸ�, ����� ũ��. -> �޸� ����ȭ �߻�
		//2. �ִ� ���� ���� ����. ���ҽ� �ʰ��� �����Ѵ�.
		for (int i = 0; i < maxClientCount; ++i)
		{
			ClientSession session;
			session.Clear(); // ClientSession�� �ʱⰪ���� ����
			session.Index = i; // ClientSession�� �ε����� �ΰ��Ѵ�? 
			session.pRecvBuffer = new char[m_Config.MaxClientRecvBufferSize]; // nullptr ������ �����ٰ� �迭�� �������� �������ش�.
			session.pSendBuffer = new char[m_Config.MaxClientSendBufferSize];

			m_ClientSessionPool.push_back(session); // Ŭ���̾�Ʈ ���� Ǯ �迭 vector
			m_ClientSessionPoolIndex.push_back(session.Index);	// ����ִ� Ǯ�� ��ť�� ����. �Ҵ� ��û�� ���� ���̵� �ڴ� ���� ���� ��.
		}

		return maxClientCount;
	}
	int TcpNetwork::AllocClientSessionIndex() // Ŭ���̾�Ʈ ������ �Ҵ��Ѵ�. -> Ǯ���� �ϳ� ������
	{
		if (m_ClientSessionPoolIndex.empty()) //�ݾ� �Ҵ��� Ǯ�� ���ٸ�, -1 return
		{
			return -1;
		}

		int index = m_ClientSessionPoolIndex.front();
		m_ClientSessionPoolIndex.pop_front(); // �տ��� ���� ����.
		return index; // �ش� ������ �ε����� ��ȯ�Ѵ�. (�Ҵ��� �ε���)
	}
	void TcpNetwork::ReleaseSessionIndex(const int index) // Ŭ���̾�Ʈ ������ �����Ѵ�. -> Ǯ�� �߰���.
	{
		//Ŭ���̾�Ʈ ���� Ǯ���ٰ� �ش� �ε����� �߰��Ѵ�. ����, �⺻������ �ʱ�ȭ ����.
		m_ClientSessionPoolIndex.push_back(index);
		m_ClientSessionPool[index].Clear();
	}

	NET_ERROR_CODE TcpNetwork::InitServerSocket() // ���� ���� �⺻ -> ���� ����
	{
		WORD wVersionRequested = MAKEWORD(2, 2); // 1. WSADATA �ʱ�ȭ
		WSADATA wsaData;
		WSAStartup(wVersionRequested, &wsaData);

		m_ServerSockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 2. ���� ����
		if (m_ServerSockfd < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_CREATE_FAIL;
		}

		auto n = 1;
		if (setsockopt(m_ServerSockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n)) < 0)
			// set socket option�� ����. -> ������ ���� ����� �����ϰ� ����. SO_REUSEADDR�� �����ؼ�, ��Ʈ�� ����߿� ���� �ص� �ȴٰ� ����.
			// ��Ʈ�� �̹� ������̶�� �߻��ϴ� ���� ����. (����, �ɼ� ����, �ɼ� �̸�, �ɼǰ��� ����� ���� �ּ�, �ɼǰ��� ũ��)
			//1�� �����ϸ� �Ѱڴٴ� �ǹ�.

			//�Լ� ������ char*�� �䱸��. ���������δ� int������, char*�� ��� �ڷ����� �޾ƿ� �� ����.
		{
			return NET_ERROR_CODE::SERVER_SOCKET_SO_REUSEADDR_FAIL;
		}

		return NET_ERROR_CODE::NONE; // �� �Ǹ� ������ ����
	}
	NET_ERROR_CODE TcpNetwork::BindListen(short port, int backlogCount) //�ּ� ���ε�
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

	NET_ERROR_CODE TcpNetwork::NewSession() // accept �κ�
	{
		auto tryCount = 0; // �ʹ� ���� accept�� �õ����� �ʵ��� �Ѵ�.

		//FD_SETSIZE�� select �Լ��� �� ���� ������ �� �ִ� ������ ��.
		//���� �õ��ϴ� �� tryCount�� ������. ��� ó���� ������ �ݺ���.
		do
		{
			++tryCount;

			struct sockaddr_in client_adr;

			auto client_len = static_cast<int>(sizeof(client_adr));
			auto client_sockfd = accept(m_ServerSockfd, (struct sockaddr*)&client_adr, &client_len);
			//client_sockfd�� Ŭ���̾�Ʈ ������ ������

			//m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "%s | client_sockfd(%I64u)", __FUNCTION__, client_sockfd);
			if (client_sockfd == INVALID_SOCKET)
			{
				if (WSAGetLastError() == WSAEWOULDBLOCK)
				{
					return NET_ERROR_CODE::ACCEPT_API_WSAEWOULDBLOCK;
				}
				m_pRefLogger->Write(LOG_TYPE::L_ERROR, "%s | Wrong socket cannot accept", __FUNCTION__);
				return NET_ERROR_CODE::ACCEPT_API_ERROR;
				//Accept�� ���� ������ ����.
			}

			auto newSessionIndex = AllocClientSessionIndex(); // ���� �Ҵ� -> �����ִ� ���� Ǯ�� �ε����� ��ȯ��. (����� �ڸ�)
			if (newSessionIndex < 0) // �ڸ��� ������. (���ο� ������ �Ҵ��ϴµ� �������� ��� -1�� ��ȯ. ���� Ǯ������ ���� �Ҵ���.)
			{
				m_pRefLogger->Write(LOG_TYPE::L_WARN, "%s | client_sockfd(%I64u)  >= MAX_SESSION", __FUNCTION__, client_sockfd);

				// �� �̻� ������ �� �����Ƿ� �ٷ� ¥����.
				CloseSession(SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY, client_sockfd, -1);
				return NET_ERROR_CODE::ACCEPT_MAX_SESSION_COUNT;
			}


			char clientIP[MAX_IP_LEN] = { 0, };
			inet_ntop(AF_INET, &(client_adr.sin_addr), clientIP, MAX_IP_LEN - 1);

			SetSockOption(client_sockfd); //�ش� Ŭ�� ������ �ɼ� ����

			SetNonBlockSocket(client_sockfd);
			//Ŭ���̾�Ʈ ������ ����ŷ ���� ������. �����Ͱ� ���ų� ���۰� �� ���� ��ٸ��� �ʵ��� ��ȯ�ǵ��� ��ٸ�.
			// ������ ������ �ʱ� ���ؼ� �߿���

			FD_SET(client_sockfd, &m_Readfds);// �����ϴ� ���Ͽ� client_sockfd�� ����
			//m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "%s | client_sockfd(%I64u)", __FUNCTION__, client_sockfd);
			ConnectedSession(newSessionIndex, client_sockfd, clientIP);

		} while (tryCount < FD_SETSIZE);

		return NET_ERROR_CODE::NONE;
	}

	//�ش� Ŭ���̾�Ʈ ������ ���������� ���� ��, ������ ������ �ʱ�ȭ �ϰ� ������ �����.
	void TcpNetwork::ConnectedSession(const int sessionIndex, const SOCKET fd, const char* pIP)
	{

		if (m_MaxSockFD < fd)
		{
			m_MaxSockFD = fd; // ���� fd�� ���� ū ���� ����.???
		}

		++m_ConnectSeq; // ������ ��ȣ �����ؼ� �Ҵ�

		auto& session = m_ClientSessionPool[sessionIndex]; // ���� Ǯ�߿� ���õ� �Ϳ��ٰ� ������ ����.
		session.Seq = m_ConnectSeq; // ������ ��ȣ �Ҵ�
		session.SocketFD = fd; // ����(fd)�� �Ҵ���.
		memcpy(session.IP, pIP, MAX_IP_LEN - 1); // ip�� ����

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
		//������ ���� �� ���ŷ ���� �ʾƼ�, ������ ���伺 �����ϱ� ���� ���

		int size1 = m_Config.MaxClientSockOptRecvBufferSize;
		int size2 = m_Config.MaxClientSockOptSendBufferSize;
		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&size1, sizeof(size1));
		//���� ������ ũ�� ���� MaxClientSockOptRecvBufferSize
		setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&size2, sizeof(size2));
		//�۽� ���� ũ�� ����
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
					// �� �̻� �� ���ǰ��� �۾��� ���� ���� ����. Ŭ���̾�Ʈ ���� ������� �ϴ��� ���� ©��� �Ѵ�.
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

		// ���� �Ǿ� �ִ��� �Ǵ� ���� �����Ͱ� �ִ���
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