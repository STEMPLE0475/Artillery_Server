#include <thread>
#include <chrono>

#include "NetLib/ServerNetErrorCode.h"
#include "NetLib/Define.h"
#include "NetLib/TcpNetwork.h"
#include "ConsoleLogger.h"
#include "RoomManager.h"
#include "PacketProcess.h"
#include "UserManager.h"
#include "Server.h"

using NET_ERROR_CODE = NServerNetLib::NET_ERROR_CODE;
using LOG_TYPE = NServerNetLib::LOG_TYPE;

Server::Server()
{
}

Server::~Server()
{
	Release();
}

ERROR_CODE Server::Init()
{
	m_pLogger = std::make_unique<ConsoleLog>();

	LoadConfig();

	m_pNetwork = std::make_unique<NServerNetLib::TcpNetwork>();
	auto result = m_pNetwork->Init(m_pServerConfig.get(), m_pLogger.get());

	if (result != NET_ERROR_CODE::NONE)
	{
		m_pLogger->Write(LOG_TYPE::L_ERROR, "%s | Init Fail. NetErrorCode(%s)", __FUNCTION__, (short)result);
		return ERROR_CODE::MAIN_INIT_NETWORK_INIT_FAIL;
	}

	//UserManager Init
	m_pUserMgr = std::make_unique<UserManager>();
	m_pUserMgr->Init(m_pServerConfig->MaxClientCount);

	//RoomManager Init
	m_pRoomMgr = std::make_unique<RoomManager>();
	m_pRoomMgr->Init(m_pServerConfig->MaxRoomCount, m_pServerConfig->MaxRoomUserCount);
	m_pRoomMgr->SetNetwork(m_pNetwork.get(), m_pLogger.get());

	//PacketProcess Init
	m_pPacketProc = std::make_unique<PacketProcess>();
	m_pPacketProc->Init(m_pNetwork.get(), m_pUserMgr.get(), m_pRoomMgr.get(), m_pServerConfig.get(), m_pLogger.get());
	//패킷 프로세스랑 pNetwork를 왜 분리하였을까?

	//다 되면 실행
	m_IsRun = true;

	m_pLogger->Write(LOG_TYPE::L_INFO, "%s | Init Success. Server Run", __FUNCTION__);
	return ERROR_CODE::NONE;
}

void Server::Release()
{
	if (m_pNetwork) {
		m_pNetwork->Release();
	}
}

void Server::Stop()
{
	m_IsRun = false;
}

void Server::Run()
{
	while (m_IsRun) // 서버 작동 중
	{
		m_pNetwork->Run(); // select() 함수를 호출. 즉, 네트워크 이벤트를 감지한다.

		//??왜 Run과 패킷을 따로 작동하게 하였을까?
		while (true)
		{
			auto packetInfo = m_pNetwork->GetPacketInfo(); // 패킷을 받아온다.
			//SessionIndex, PacketId, PacketBodySize, pRefData

			if (packetInfo.PacketId == 0) // packetInfo(네트워크 이벤트)가 없으면 break;
			{
				break;
			}
			else
			{
				m_pPacketProc->Process(packetInfo); // 패킷이 있으면 처리

			}
		}
	}
}

ERROR_CODE Server::LoadConfig()
{
	//서버를 제대로 만드려면 하드 코딩 하지 말고, 설정 정보 파일 (JSON 등)으로 만드는 것이 좋음.
	m_pServerConfig = std::make_unique<NServerNetLib::ServerConfig>();

	m_pServerConfig->Port = 11021;
	m_pServerConfig->BackLogCount = 128;
	m_pServerConfig->MaxClientCount = 1000;

	m_pServerConfig->MaxClientSockOptRecvBufferSize = 10240;
	m_pServerConfig->MaxClientSockOptSendBufferSize = 10240;
	m_pServerConfig->MaxClientRecvBufferSize = 8192;
	m_pServerConfig->MaxClientSendBufferSize = 8192;

	m_pServerConfig->ExtraClientCount = 64; // 바로 짜르지 말고 왜 짤리는지는 알려주기 위한 여유
	m_pServerConfig->MaxRoomCount = 20;
	m_pServerConfig->MaxRoomUserCount = 4;

	m_pLogger->Write(NServerNetLib::LOG_TYPE::L_INFO, "%s | Port(%d), Backlog(%d)", __FUNCTION__, m_pServerConfig->Port, m_pServerConfig->BackLogCount);
	return ERROR_CODE::NONE;
}

