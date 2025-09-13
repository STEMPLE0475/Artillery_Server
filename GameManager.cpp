#include "GameManager.h"
#include "UserManager.h"
#include "RoomManager.h"
#include "DBManager.h"
#include "NetLib/TcpNetwork.h"
#include "ProtocolCommon.h"  
#include "User.h"         
#include "Room.h"       

#include <cmath> 

using LOG_TYPE = NServerNetLib::LOG_TYPE;
using ServerConfig = NServerNetLib::ServerConfig;

const Vec2D SpawnPoints[2][2] = {
	{ {-1000.0f, 0.0f}, {-1200.0f, 0.0f} }, // 팀 A
	{ { 1000.0f, 0.0f}, { 1200.0f, 0.0f} }  // 팀 B
};

GameManager::GameManager() {}
GameManager::~GameManager() {}

void GameManager::Init(UserManager* pUserMgr, RoomManager* pRoomMgr, TcpNet* pNetwork, ILog* pLogger, DBManager* pDBMgr)
{
	m_pRefUserMgr = pUserMgr;
	m_pRefRoomMgr = pRoomMgr;
	m_pRefNetwork = pNetwork;
	m_pRefLogger = pLogger;
	m_pRefDBMgr = pDBMgr;
}

void GameManager::CheckAndStartGame(Room* pRoom)
{
    if (pRoom == nullptr)
    {
        return;
    }

    if (pRoom->IsUserFull())
    {
        ProcessMatchSuccess(pRoom);
    }
}

void GameManager::ProcessMatchSuccess(Room* pRoom)
{
	auto& userList = pRoom->GetUserList();

	// 안정성을 위해 유저 수를 다시 한번 확인합니다.
	if (userList.size() != MAX_PLAYERS_PER_ROOM)
	{
		m_pRefLogger->Write(LOG_TYPE::L_ERROR, "CRITICAL ERROR: Room:%d is full but user count is not 4 (%d). Aborting match.",
			pRoom->GetIndex(), (int)userList.size());
		return;
	}

	m_pRefLogger->Write(LOG_TYPE::L_INFO, "Room:%d is full. Match Success!", pRoom->GetIndex());

	// 모든 유저의 스폰 정보를 먼저 채웁니다.
	PlayerSpawnInfo spawnInfos[MAX_PLAYERS_PER_ROOM];
	for (int i = 0; i < userList.size(); ++i)
	{
		User* pUser = userList[i];
		if (pUser == nullptr) continue;

		pUser->Respawn();

		spawnInfos[i].UserHandle = pUser->GetUserHandle();
		spawnInfos[i].Team = i % 2;
		spawnInfos[i].Slot = i / 2;
		spawnInfos[i].SpawnX = SpawnPoints[spawnInfos[i].Team][spawnInfos[i].Slot].X;
		spawnInfos[i].SpawnY = SpawnPoints[spawnInfos[i].Team][spawnInfos[i].Slot].Y;

		// 서버 내부의 유저 위치 정보도 업데이트합니다.
		pUser->SetPosition({ spawnInfos[i].SpawnX, spawnInfos[i].SpawnY });
		pUser->SetTeam(spawnInfos[i].Team);
	}

	// 각 유저에게 맞춤형 패킷을 전송하고, 봇의 상태를 변경합니다.
	for (int i = 0; i < userList.size(); ++i)
	{
		User* pReceiver = userList[i];
		if (pReceiver == nullptr) continue;

		PktMatchCompleteNtf ntfPkt;
		ntfPkt.PlayerCount = (uint8_t)userList.size();
		ntfPkt.MyPlayerIndexInList = (uint8_t)i;
		memcpy(ntfPkt.PlayerList, spawnInfos, sizeof(PlayerSpawnInfo) * userList.size());

		m_pRefNetwork->SendData(pReceiver->GetSessionIndex(),
			(short)PACKET_ID::MATCH_COMPLETE_NTF,
			sizeof(ntfPkt),
			(char*)&ntfPkt);

		if (pReceiver->IsBot())
		{
			pReceiver->SetBotState(User::BOT_STATE::IN_GAME);
			m_pRefLogger->Write(LOG_TYPE::L_INFO, "Bot %s state changed to IN_GAME.", pReceiver->GetID().c_str());
		}
	}

	pRoom->StartGame();
}

void GameManager::OnFireRequest(const int sessionIndex, const PktFireStartReq* reqPkt)
{

	// 1. 유효성 검사 (유저, 방, 게임 상태, 쿨타임 등)
	auto pUser = m_pRefUserMgr->FindUser(sessionIndex);
	if (pUser == nullptr || !pUser->IsCurDomainInRoom()) { return; }
	Room* pRoom = m_pRefRoomMgr->GetRoom(pUser->GetRoomIndex());
	if (pRoom == nullptr || !pRoom->IsStart()) { return; }
	// TODO: 쿨타임 검사

	// 2. 서버에 포탄 객체 생성 및 목록에 추가
	ServerProjectile newProjectile;
	newProjectile.ProjectileId = ++m_NextProjectileId;
	newProjectile.OwnerHandle = pUser->GetUserHandle(); // User 클래스에 핸들 변수가 있다고 가정
	newProjectile.pOwnerRoom = pRoom;
	newProjectile.Position = pUser->GetPosition(); // User의 현재 위치에서 시작

	// 각도(Angle)와 힘(ChargeSec)을 기반으로 초기 속도(Velocity) 계산
	const float angleInRadians = reqPkt->AimAngle * (3.1415926535f / 180.0f); 

	newProjectile.Velocity.X = cos(angleInRadians) * (reqPkt->ChargeSec * POWER_SCALAR);
	newProjectile.Velocity.Y = sin(angleInRadians) * (reqPkt->ChargeSec * POWER_SCALAR);

	m_ActiveProjectiles.push_back(newProjectile);

	// 3. 모든 클라이언트에게 "포탄 생성됨" 알림 (PktProjectileCreateNtf)
	PktProjectileCreateNtf ntfPkt;
	ntfPkt.ProjectileId = newProjectile.ProjectileId;
	ntfPkt.OwnerHandle = newProjectile.OwnerHandle;
	ntfPkt.X = newProjectile.Position.X;
	ntfPkt.Y = newProjectile.Position.Y;
	ntfPkt.VX = newProjectile.Velocity.X;
	ntfPkt.VY = newProjectile.Velocity.Y;
	// ... 나머지 정보 채우기 ...

	pRoom->BroadcastPacket((short)PACKET_ID::PROJECTILE_CREATE_NTF, (char*)&ntfPkt, sizeof(ntfPkt));
	m_pRefLogger->Write(LOG_TYPE::L_INFO, "User:%s fired Projectile:%d", pUser->GetID().c_str(), newProjectile.ProjectileId);
}


void GameManager::OnMatchRequest(const int sessionIndex)
{
	// 1. 요청 유저 정보 확인 (안정성 강화)
	auto pUser = m_pRefUserMgr->FindUser(sessionIndex);
	if (pUser == nullptr)
	{
		m_pRefLogger->Write(LOG_TYPE::L_ERROR, "%s | sessionIndex(%d) | 유저 정보를 찾을 수 없음", __FUNCTION__, sessionIndex);
		return;
	}

	// 2. 유저가 이미 다른 방에 있는지 확인
	if (pUser->GetRoomIndex() != -1)
	{
		m_pRefLogger->Write(LOG_TYPE::L_WARN, "%s | sessionIndex(%d) | 유저가 이미 방(%d)에 있음", __FUNCTION__, sessionIndex, pUser->GetRoomIndex());
		return;
	}

	// 3. 입장 가능한 방을 찾거나 새로 생성
	Room* pRoom = m_pRefRoomMgr->FindOrCreateRoom();
	if (pRoom == nullptr)
	{
		// 서버에 빈 방이 하나도 없는 경우
		PktMatchRequestRes resPkt;
		resPkt.SetError(ERROR_CODE::MATCH_FAIL_SERVER_FULL);
		m_pRefNetwork->SendData(sessionIndex, (short)PACKET_ID::MATCH_REQUEST_RES, sizeof(resPkt), (char*)&resPkt);
		return;
	}

	m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "User(Sess:%d) attempting to enter Room:%d. Current users: %d/%d. IsStart?:%d",
		sessionIndex, pRoom->GetIndex(), pRoom->GetUserCount(), pRoom->MaxUserCount(), (int)pRoom->IsStart());
	// 4. 유저를 방에 입장시키고 상태 변경
	pUser->EnterRoom(pRoom->GetIndex());
	pRoom->EnterUser(pUser);
	m_pRefLogger->Write(LOG_TYPE::L_INFO, "User:%s (Session:%d) entered Room:%d",
		pUser->GetID().c_str(), sessionIndex, pRoom->GetIndex());
	// 5. 방이 꽉 찼는지 확인하여 게임 시작 또는 대기 응답 결정
	if (pRoom->IsUserFull())
	{
		// 5-1. 방이 꽉 찼으므로, '매칭 성공' 로직을 실행
		ProcessMatchSuccess(pRoom);
	}
	else
	{
		// 5-2. 아직 대기 중이므로, "정상적으로 대기열에 들어갔음" 응답 전송
		PktMatchRequestRes resPkt;
		resPkt.SetError(ERROR_CODE::NONE);
		m_pRefNetwork->SendData(sessionIndex, (short)PACKET_ID::MATCH_REQUEST_RES, sizeof(resPkt), (char*)&resPkt);
	}
}

void GameManager::Tick(const float deltaTime)
{
	// 현재 날아가는 모든 포탄을 순회하며 업데이트
	for (auto it = m_ActiveProjectiles.begin(); it != m_ActiveProjectiles.end(); )
	{
		ServerProjectile& projectile = (*it);

		// 1. 물리 시뮬레이션 (위치 업데이트)
		SimulateProjectile(projectile, deltaTime);

		// 2. 충돌 판정
		if (CheckCollision(projectile))
		{
			m_pRefLogger->Write(LOG_TYPE::L_INFO, "Projectile:%d exploded at (%.2f, %.2f)",
				projectile.ProjectileId, projectile.Position.X, projectile.Position.Y);

			// 충돌시 폭발
			PktProjectileExplodeNtf ntfPkt;
			ntfPkt.ProjectileId = projectile.ProjectileId;
			ntfPkt.X = projectile.Position.X;
			ntfPkt.Y = projectile.Position.Y;

			projectile.pOwnerRoom->BroadcastPacket((short)PACKET_ID::PROJECTILE_EXPLODE_NTF, (char*)&ntfPkt, sizeof(ntfPkt));
			m_pRefLogger->Write(LOG_TYPE::L_INFO, "Projectile:%d exploded at (%.2f, %.2f)",
				projectile.ProjectileId, projectile.Position.X, projectile.Position.Y);

			// 폭발 데미지
			ApplyExplosionDamage(projectile);

			// 종료 조건 체크
			CheckGameEndCondition(projectile.pOwnerRoom);

			it = m_ActiveProjectiles.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void GameManager::SimulateProjectile(ServerProjectile& projectile, const float deltaTime)
{
	projectile.Velocity.Y += PROJECTILE_GRAVITY * deltaTime;

	projectile.Position.X += projectile.Velocity.X * deltaTime;
	projectile.Position.Y += projectile.Velocity.Y * deltaTime;

	m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "Projectile:%d Pos(%.2f, %.2f)", 
	                    projectile.ProjectileId, projectile.Position.X, projectile.Position.Y);

}

bool GameManager::CheckCollision(const ServerProjectile& projectile)
{
	// 포탄의 Y 위치가 지면(0.0)보다 낮아지면 충돌한 것으로 간주합니다.
	if (projectile.Position.Y <= 0.0f)
	{
		return true;
	}

	// TODO: 나중에 여기에 지형 데이터나 다른 플레이어와의 충돌 판정 로직이 추가됩니다.

	return false;
}

void GameManager::ApplyExplosionDamage(const ServerProjectile& projectile)
{
	Room* pRoom = projectile.pOwnerRoom;
	if (pRoom == nullptr) { return; }

	auto& userList = pRoom->GetUserList();

	for (auto pVictim : userList)
	{
		if (pVictim == nullptr || pVictim->IsDead())
		{
			continue;
		}

		float distSq = pow(pVictim->GetPosition().X - projectile.Position.X, 2) +
			pow(pVictim->GetPosition().Y - projectile.Position.Y, 2);

		if (distSq <= EXPLOSION_RADIUS_SQ)
		{
			pVictim->Kill();
			m_pRefLogger->Write(LOG_TYPE::L_INFO, "User:%s has been killed by Projectile:%d",
				pVictim->GetID().c_str(), projectile.ProjectileId);

			PktPlayerDeadNtf deadNtf;
			deadNtf.VictimHandle = pVictim->GetUserHandle();
			deadNtf.AttackerHandle = projectile.OwnerHandle;
			deadNtf.RespawnMs = 0; // 부활 없음

			pRoom->BroadcastPacket((short)PACKET_ID::PLAYER_DEAD_NTF, (char*)&deadNtf, sizeof(deadNtf));
		}
	}
}

// [새로운 함수] 게임 종료 조건을 확인합니다.
void GameManager::CheckGameEndCondition(Room* pRoom)
{
	if (pRoom == nullptr || !pRoom->IsStart()) { return; }

	int aliveTeamA = 0;
	int aliveTeamB = 0;

	for (auto pUser : pRoom->GetUserList())
	{
		if (pUser && !pUser->IsDead())
		{
			if (pUser->GetTeam() == 0)
			{
				aliveTeamA++;
			}
			else
			{
				aliveTeamB++;
			}
		}
	}

	int winningTeam = -1; // -1: 아직 안 끝남, 0: A팀 승리, 1: B팀 승리
	if (aliveTeamA > 0 && aliveTeamB == 0)
	{
		winningTeam = 0; // A팀 승리
	}
	else if (aliveTeamB > 0 && aliveTeamA == 0)
	{
		winningTeam = 1; // B팀 승리
	}

	if (winningTeam != -1)
	{
		m_pRefLogger->Write(LOG_TYPE::L_INFO, "Game Over in Room:%d. Winning Team: %d", pRoom->GetIndex(), winningTeam);

		PktGameEndNtf endNtf;
		endNtf.WinningTeam = (uint8_t)winningTeam;
		endNtf.TeamAScore = (uint16_t)aliveTeamA;
		endNtf.TeamBScore = (uint16_t)aliveTeamB;
		pRoom->BroadcastPacket((short)PACKET_ID::GAME_END_NTF, (char*)&endNtf, sizeof(endNtf));

		pRoom->EndGame();
	}
}
