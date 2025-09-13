#include <algorithm>

#include "NetLib/ILog.h"
#include "NetLib/TcpNetwork.h"
#include "RoomManager.h"


void RoomManager::Init(const int maxRoomCountByLobby, const int maxRoomUserCount)
{
	for (int i = 0; i < maxRoomCountByLobby; ++i)
	{
		m_RoomList.emplace_back(new Room());
		m_RoomList[i]->Init((short)i, (short)maxRoomUserCount);
	}
}

void RoomManager::Release()
{
	for (int i = 0; i < (int)m_RoomList.size(); ++i)
	{
		delete m_RoomList[i];
	}

	m_RoomList.clear();
}

void RoomManager::SetNetwork(TcpNet* pNetwork, ILog* pLogger)
{
	m_pRefLogger = pLogger;
	m_pRefNetwork = pNetwork;

	for (auto pRoom : m_RoomList)
	{
		pRoom->SetNetwork(pNetwork, pLogger);
	}
}

Room* RoomManager::GetRoom(const short roomIndex)
{
	if (roomIndex < 0 || roomIndex >= m_RoomList.size()) {
		return nullptr;
	}

	return m_RoomList[roomIndex];
}



Room* RoomManager::FindEmptyRoom()
{
	for (short i = 0; i < MaxRoomCount(); i++)
	{
		if (m_RoomList[i]->IsUsed() &&
			!m_RoomList[i]->IsStart() &&
			!m_RoomList[i]->IsUserFull())
		{
			return m_RoomList[i];
		}
	}

	return nullptr;
}

Room* RoomManager::CreateRoom()
{
	for (short i = 0; i < MaxRoomCount(); i++)
	{
		if (!m_RoomList[i]->IsUsed())
		{
			m_RoomList[i]->EnableRoom();
			return m_RoomList[i];
		}
	}
	return nullptr;
	// 에러코드: 생성할 수 있는 방이 없습니다 (서버 full)
}

Room* RoomManager::FindOrCreateRoom()
{
	Room* joinableRoom = FindEmptyRoom();

	if (joinableRoom != nullptr)
	{
		return joinableRoom;
	}
	return CreateRoom();
}

bool RoomManager::CheckMatch(const short roomIndex)
{
	if (m_RoomList[roomIndex]->IsUserFull())
	{
		return true;
	}
	else {
		return false;
	}
}







