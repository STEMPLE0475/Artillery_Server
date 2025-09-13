#pragma once

#include <cstdint>
#include <vector>
#include "Common.h"

class UserManager;
class RoomManager;
class DBManager;
class Room;
class User;
struct PktFireStartReq; // 포인터로만 사용하므로 전방 선언

namespace NServerNetLib
{
	class ITcpNetwork;
	class ILog;
}

using TcpNet = NServerNetLib::ITcpNetwork;
using ILog = NServerNetLib::ILog;

struct ServerProjectile
{
	uint32_t ProjectileId;
	uint32_t OwnerHandle;
	Room* pOwnerRoom;
	Vec2D Position;
	Vec2D Velocity;
};

class GameManager
{
public:
	GameManager();
	~GameManager();

	void Init(UserManager* pUserMgr, RoomManager* pRoomMgr, TcpNet* pNetwork, ILog* pLogger, DBManager* pDBMgr);

	void CheckAndStartGame(Room* pRoom);
	void ProcessMatchSuccess(Room* pRoom);
	void OnFireRequest(const int sessionIndex, const PktFireStartReq* reqPkt);
	void OnMatchRequest(const int sessionIndex);
	void Tick(const float deltaTime); // 서버에서 매 프레임 호출

private:
	ILog* m_pRefLogger;
	TcpNet* m_pRefNetwork;
	UserManager* m_pRefUserMgr;
	RoomManager* m_pRefRoomMgr;
	DBManager* m_pRefDBMgr;

	void SimulateProjectile(ServerProjectile& projectile, const float deltaTime);
	bool CheckCollision(const ServerProjectile& projectile);
	void ApplyExplosionDamage(const ServerProjectile& projectile);
	void CheckGameEndCondition(Room* pRoom);
	
	std::vector<ServerProjectile> m_ActiveProjectiles;
	uint32_t m_NextProjectileId = 0; // 포탄 고유 ID 발급용
};