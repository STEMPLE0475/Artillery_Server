#pragma once

#include <vector>

namespace NServerNetLib
{
	class ITcpNetwork;
	class ILog;
}

using TcpNet = NServerNetLib::ITcpNetwork;
using ILog = NServerNetLib::ILog;

class GameManager;
class User;

class BotManager
{
public:
	void Init(GameManager* pGameMgr, ILog* pLogger);
	void Tick(const float deltaTime);
	void AddBot(User* pBot);
private:
	std::vector<User*> m_BotList;
	GameManager* m_pRefGameMgr;
	ILog* m_pRefLogger;
};

