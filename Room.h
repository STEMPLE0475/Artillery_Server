#pragma once

#include <vector>
#include <string>
#include <memory>

#include "User.h"
#include "ProtocolCommon.h"


namespace NServerNetLib { class ITcpNetwork; }
namespace NServerNetLib { class ILog; }



using TcpNet = NServerNetLib::ITcpNetwork;
using ILog = NServerNetLib::ILog;

class Game;

class Room
{
public:
	Room();
	virtual ~Room();

	void Init(const short index, const short maxUserCount);

	void SetNetwork(TcpNet* pNetwork, ILog* pLogger);

	void Clear();

	short GetIndex() { return m_Index; }

	bool IsUsed() { return m_IsUsed; }

	bool IsStart() { return m_IsStart; }

	short MaxUserCount() { return m_MaxUserCount; }

	short GetUserCount() { return (short)m_UserList.size(); }

	bool IsUserFull() { return MaxUserCount() == GetUserCount(); }

	void EnableRoom();

	void EnterUser(User* pUser);
	void LeaveUser(User* pUser);
	const std::vector<User*>& GetUserList(); // &은 뭐임.

	void StartGame();

	void BroadcastPacket(const short packetId, char* pPacket, const int packetSize);

	void EndGame();

private:
	ILog* m_pRefLogger;
	TcpNet* m_pRefNetwork;

	short m_Index = -1;
	short m_MaxUserCount;

	bool m_IsUsed = false;
	bool m_IsStart = false;

	std::vector<User*> m_UserList;
};
