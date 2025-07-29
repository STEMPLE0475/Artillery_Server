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

// ������ ��Ŷ�� ó���ϰ�, �´� �Լ��� �Ǥ�����.

PacketProcess::PacketProcess() {}
PacketProcess::~PacketProcess() {}

void PacketProcess::Init(TcpNet* pNetwork, UserManager* pUserMgr, RoomManager* pLobbyMgr, ServerConfig* pConfig, ILog* pLogger)
{
	m_pRefLogger = pLogger;
	m_pRefNetwork = pNetwork;
	m_pRefUserMgr = pUserMgr;
	m_pRefRoomMgr = pLobbyMgr;
}

//��� �۾��� ���⼭ ó����. ���ο� ��Ŷ�� ���� ��Ŷ�� ��� ó���ϴ����� ����.
void PacketProcess::Process(PacketInfo packetInfo)
{
	using netLibPacketId = NServerNetLib::PACKET_ID;
	using commonPacketId = PACKET_ID;

	auto packetId = packetInfo.PacketId;

	//��� ���񽺿��� switch ���� ����ϸ� �ʹ� �����. �Լ� �����ͷ� ������ ��� ��ų �� ����.
	switch (packetId)
	{
		//Ŭ���̾�Ʈ�� ���� ���� �ƴѵ�, �ùĿ� ���ؼ� ���̺귯���� �˷��ִ� ����??
	case (int)netLibPacketId::NTF_SYS_CONNECT_SESSION:
		NtfSysConnctSession(packetInfo);
		break;
	case (int)netLibPacketId::NTF_SYS_CLOSE_SESSION:
		NtfSysCloseSession(packetInfo);
		break;
		//���⼭���Ͱ� ��¥ Ŭ�� ���� ��Ŷ�� �˾Ƽ� ó���ϴ� ���.
	case (int)commonPacketId::LOGIN_REQ:
		Login(packetInfo);
		break;
		//�̰� ���� �����. �� ����� �� ������ ��.
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
	// �н������ ������ pass ���ش�.
	// ID �ߺ��̶�� ���� ó���Ѵ�.
	PktLogInRes resPkt;
	auto reqPkt = (PktLogInReq*)packetInfo.pRefData;

	//�ϴ� DB�� ���������� �ʾұ� ������, DB ȣ�� �н����� ���� �ߺ� üũ�� ��.
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


