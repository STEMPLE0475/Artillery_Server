#include <algorithm>
#include <cstring>
#include <wchar.h>

#include "NetLib/ILog.h"
#include "NetLib/TcpNetwork.h"

#include "User.h"
#include "Room.h"



Room::Room() {}

Room::~Room()
{
}

void Room::Init(const short index, const short maxUserCount)
{
	m_Index = index;
	m_MaxUserCount = maxUserCount;
}

void Room::SetNetwork(TcpNet* pNetwork, ILog* pLogger)
{
	m_pRefLogger = pLogger;
	m_pRefNetwork = pNetwork;
}

void Room::Clear()
{
	m_IsUsed = false;
	m_UserList.clear();
}

void Room::EnableRoom() { 
	m_IsUsed = true;
}

void Room::EnterUser(User* pUser)
{
	m_UserList.push_back(pUser);
}

void Room::LeaveUser(User* pUser)
{
	m_UserList.erase(std::remove(m_UserList.begin(), m_UserList.end(), pUser), m_UserList.end());
}

const std::vector<User*>& Room::GetUserList()
{
	return m_UserList;
}

void Room::StartGame()
{
	m_IsStart = true;
}

void Room::BroadcastPacket(const short packetId, char* pPacket, const int packetSize)
{
	for (auto pUser : m_UserList)
	{
		if (pUser != nullptr)
		{
			m_pRefNetwork->SendData(pUser->GetSessionIndex(), packetId, packetSize, pPacket);
		}
	}
}

void Room::EndGame()
{
	if (!m_IsStart)
	{
		return;
	}

	m_pRefLogger->Write(NServerNetLib::LOG_TYPE::L_INFO, "Room %d: Game Ended.", m_Index);

	m_IsStart = false;

	// 2. 방에 있는 봇들의 상태를 로비로 되돌립니다.
	for (auto pUser : m_UserList)
	{
		if (pUser && pUser->IsBot())
		{
			pUser->SetBotState(User::BOT_STATE::IN_LOBBY);
		}
	}
}

