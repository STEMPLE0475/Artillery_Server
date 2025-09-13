#include <thread>
#include <chrono>


#include "NetLib/ServerNetErrorCode.h"
#include "NetLib/Define.h"
#include "NetLib/TcpNetwork.h"
#include "ConsoleLogger.h"
#include "RoomManager.h"
#include "PacketProcess.h"
#include "UserManager.h"
#include "DBManager.h"
#include "GameManager.h"
#include "BotManager.h"
#include "Server.h"



using NET_ERROR_CODE = NServerNetLib::NET_ERROR_CODE;
using LOG_TYPE = NServerNetLib::LOG_TYPE;

Server::Server()
{
	setlocale(LC_ALL, "");
	SetConsoleOutputCP(CP_UTF8); // 콘솔 인코딩
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

		m_pLogger->Write(LOG_TYPE::L_ERROR, "%s | Init Fail. NetErrorCode(%hd)", __FUNCTION__, (short)result);
		return ERROR_CODE::MAIN_INIT_NETWORK_INIT_FAIL;
	}
	

	//DBManager init
	m_pDBMgr = std::make_unique<DBManager>();
	m_pDBMgr->Init("127.0.0.1", "stemule0", "597408", "gamedb");

	//UserManager Init
	m_pUserMgr = std::make_unique<UserManager>();
	m_pUserMgr->Init(m_pServerConfig->MaxClientCount, m_pDBMgr.get());

	//RoomManager Init
	m_pRoomMgr = std::make_unique<RoomManager>();
	m_pRoomMgr->Init(m_pServerConfig->MaxRoomCount, m_pServerConfig->MaxRoomUserCount);
	m_pRoomMgr->SetNetwork(m_pNetwork.get(), m_pLogger.get());

	//GameManager Init
	m_pGameMgr = std::make_unique<GameManager>();
	m_pGameMgr->Init(m_pUserMgr.get(), m_pRoomMgr.get(), m_pNetwork.get(), m_pLogger.get(), m_pDBMgr.get());

	//BotManager Init
	m_pBotMgr = std::make_unique<BotManager>();
	m_pBotMgr->Init(m_pGameMgr.get(), m_pLogger.get());

	//PacketProcess Init
	m_pPacketProc = std::make_unique<PacketProcess>();
	m_pPacketProc->Init(m_pNetwork.get(), m_pUserMgr.get(), m_pRoomMgr.get(), m_pServerConfig.get(), m_pLogger.get(), m_pDBMgr.get(), m_pGameMgr.get());


	//다 되면 실행
	m_IsRun = true;

	m_pLogger->Write(LOG_TYPE::L_INFO, "%s | Init Success. Server Run", __FUNCTION__);

	LogUtils::DumpPath() = "C:\\temp\\echo_dump.bin";  
	LogUtils::PreviewBytes() = 128;


	return ERROR_CODE::NONE;
}

void Server::Release()
{
	if (m_pNetwork) {
		m_pNetwork->Release();
	}
}

void Server::CreateBots(const int botCount)
{
	for (int i = 0; i < botCount; ++i)
	{
		auto [err, pBot] = m_pUserMgr->AddNewBot();
		if (err == ERROR_CODE::NONE && pBot != nullptr)
		{
			m_pBotMgr->AddBot(pBot);
		}
		else
		{
			m_pLogger->Write(LOG_TYPE::L_WARN, "Failed to create bot. Maybe server is full.");
			break;
		}
	}
	m_pLogger->Write(LOG_TYPE::L_INFO, "[DEBUG] Requested to create %d bots.", botCount);
}

void Server::Stop()
{
	m_IsRun = false;
}

void Server::Run()
{
	auto lastTickTime = std::chrono::steady_clock::now(); // deltaTime 계산을 위한 마지막 Tick 시간 저장

	while (m_IsRun)
	{
		// deltaTime 계산
		auto currentTicktime = std::chrono::steady_clock::now();
		auto duration = currentTicktime - lastTickTime;
		lastTickTime = currentTicktime;

		// duration을 float 타입의 초 단위로 변환
		float deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000000.0f;

		// select() 호출
		m_pNetwork->Run();

		// 수집된 패킷 처리
		while (true)
		{
			auto packetInfo = m_pNetwork->DispatchPacket(); // 패킷을 받아온다.

			if (packetInfo.PacketId == 0) // packetInfo(네트워크 이벤트)가 없으면 break;
			{
				break;
			}
			else
			{
				m_pPacketProc->Process(packetInfo); // 패킷이 있으면 처리

			}
		}

		// Tick 호출 -> 게임 상태 업데이트
		m_pGameMgr->Tick(deltaTime);
		m_pBotMgr->Tick(deltaTime);

		
		// 1초에 60번 루프 돌도록 제한
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
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

