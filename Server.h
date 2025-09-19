#pragma once
#include <memory>

#include "ProtocolCommon.h"
#include <Windows.h>
#include <locale.h>


namespace NServerNetLib
{
	struct ServerConfig;
	class ILog;
	class ITcpNetwork;
}


class UserManager;
class RoomManager;
class PacketProcess;
class DBManager;
class GameManager;
class BotManager;

class Server
{
public:
	Server();
	~Server();

	ERROR_CODE Init();

	void Run();

	void Stop();
	void CreateBots(const int botCount);

private:
	ERROR_CODE LoadConfig();

	void Release();
	


private:
	bool m_IsRun = false;

	std::unique_ptr<NServerNetLib::ServerConfig> m_pServerConfig;
	std::unique_ptr<NServerNetLib::ILog> m_pLogger;

	std::unique_ptr<NServerNetLib::ITcpNetwork> m_pNetwork;

	std::unique_ptr<PacketProcess> m_pPacketProc;

	std::unique_ptr<UserManager> m_pUserMgr;
	std::unique_ptr<RoomManager> m_pRoomMgr;
	std::unique_ptr<DBManager> m_pDBMgr;
	std::unique_ptr<GameManager> m_pGameMgr;
	std::unique_ptr<BotManager> m_pBotMgr;

};

