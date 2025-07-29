#include <cstring>

#include "NetLib/ILog.h"
#include "NetLib/TcpNetwork.h"
#include "User.h"
#include "UserManager.h"
//#include "Room.h"
#include "RoomManager.h"
#include "PacketProcess.h"

using LOG_TYPE = NServerNetLib::LOG_TYPE;
using ServerConfig = NServerNetLib::ServerConfig;

// 수신한 패킷을 처리하고, 맞는 함수를 ㅗㅎ출함.

PacketProcess::PacketProcess() {}
PacketProcess::~PacketProcess() {}

void PacketProcess::Init(TcpNet* pNetwork, UserManager* pUserMgr, RoomManager* pLobbyMgr, ServerConfig* pConfig, ILog* pLogger)
{
	m_pRefLogger = pLogger;
	m_pRefNetwork = pNetwork;
	m_pRefUserMgr = pUserMgr;
	m_pRefRoomMgr = pLobbyMgr;
}

//모든 작업을 여기서 처리함. 새로운 패킷이 오면 패킷을 어떻게 처리하는지를 정함.
void PacketProcess::Process(PacketInfo packetInfo)
{
	using netLibPacketId = NServerNetLib::PACKET_ID;
	using commonPacketId = PACKET_ID;

	auto packetId = packetInfo.PacketId;

	//상용 서비스에서 switch 문을 사용하면 너무 길어짐. 함수 포인터로 가독성 향상 시킬 수 있음.
	switch (packetId)
	{
		//클라이언트가 보낸 것은 아닌데, 시뮬에 대해서 라이브러리에 알려주는 역할??
	case (int)netLibPacketId::NTF_SYS_CONNECT_SESSION:
		NtfSysConnctSession(packetInfo);
		break;
	case (int)netLibPacketId::NTF_SYS_CLOSE_SESSION:
		NtfSysCloseSession(packetInfo);
		break;
		//여기서부터가 진짜 클라가 보낸 패킷을 알아서 처리하는 방법.
	case (int)commonPacketId::LOGIN_REQ:
		Login(packetInfo);
		break;
		//이걸 따라서 만들기. 방 만들기 방 나가기 등.
	}

}


ERROR_CODE PacketProcess::NtfSysConnctSession(PacketInfo packetInfo)
{
	m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysConnctSession. sessionIndex(%d)", __FUNCTION__, packetInfo.SessionIndex);
	return ERROR_CODE::NONE;
}

ERROR_CODE PacketProcess::NtfSysCloseSession(PacketInfo packetInfo)
{
	auto pUser = std::get<1>(m_pRefUserMgr->GetUser(packetInfo.SessionIndex));

	if (pUser) {
		m_pRefUserMgr->RemoveUser(packetInfo.SessionIndex);
	}

	m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d)", __FUNCTION__, packetInfo.SessionIndex);
	return ERROR_CODE::NONE;
}


ERROR_CODE PacketProcess::Login(PacketInfo packetInfo)
{
	// 패스워드는 무조건 pass 해준다.
	// ID 중복이라면 에러 처리한다.
	PktLogInRes resPkt;
	auto reqPkt = (PktLogInReq*)packetInfo.pRefData;

	//일단 DB를 연동하지는 않았기 때문에, DB 호출 패스워드 없이 중복 체크만 함.
	auto addRet = m_pRefUserMgr->AddUser(packetInfo.SessionIndex, reqPkt->szID);

	if (addRet != ERROR_CODE::NONE) {
		resPkt.SetError(addRet);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_REQ, sizeof(PktLogInRes), (char*)&resPkt);
		return addRet;
	}

	resPkt.ErrorCode = (short)addRet;
	m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_RES, sizeof(PktLogInRes), (char*)&resPkt);

	return ERROR_CODE::NONE;
}


