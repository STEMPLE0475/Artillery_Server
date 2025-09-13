#pragma once

#include <memory>
#include <algorithm>

#include "ProtocolCommon.h"
#include "NetLib/Define.h"
#include "NetLib/ILog.h"
#define NETTRACE_ENABLE
#include "LogUtils.h"



namespace NServerNetLib
{
	class ITcpNetwork;
	class ILog;
}


class UserManager;
class RoomManager;
class DBManager;
class GameManager;

class PacketProcess
{
	using PacketInfo = NServerNetLib::RecvPacketInfo;

	using TcpNet = NServerNetLib::ITcpNetwork;
	using ILog = NServerNetLib::ILog;

public:
	PacketProcess();
	~PacketProcess();

	void Init(TcpNet* pNetwork, UserManager* pUserMgr, RoomManager* pRoomMgr, NServerNetLib::ServerConfig* pConfig, ILog* pLogger, DBManager* pDBMgr, GameManager* pGameMgr);

	void Process(PacketInfo packetInfo);


private:
	ILog* m_pRefLogger;
	TcpNet* m_pRefNetwork;
	UserManager* m_pRefUserMgr;
	RoomManager* m_pRefRoomMgr;
	DBManager* m_pRefDBMgr;
	GameManager* m_pRefGameMgr;

private:
	ERROR_CODE NtfSysConnctSession(PacketInfo packetInfo);
	ERROR_CODE NtfSysCloseSession(PacketInfo packetInfo);

	ERROR_CODE Login(PacketInfo packetInfo);
	ERROR_CODE Register(PacketInfo packetInfo);

	ERROR_CODE AuthenticateUser(const uint8_t* id, const uint8_t* pw, std::string& outNickname);

	ERROR_CODE EchoChat(PacketInfo p);
	ERROR_CODE MatchRequest(PacketInfo packetInfo);
	ERROR_CODE MatchCancel(PacketInfo packetInfo);
	ERROR_CODE FireStart(PacketInfo packetInfo);

};
