#pragma once
#include <string>
#include "Common.h"

class User
{
public:
	enum class DOMAIN_STATE {
		NONE = 0,
		LOGIN = 1,
		ROOM = 2,
	};

	enum class BOT_STATE {
		NONE,
		IN_LOBBY,
		IN_MATCHMAKING,
		IN_GAME,
	};

public:
	User() {}
	virtual ~User() {}

	void Init(const short index)
	{
		m_Index = index;
	}

	void Clear()
	{
		m_SessionIndex = 0;
		m_ID = "";
		m_IsConfirm = false;
		m_CurDomainState = DOMAIN_STATE::NONE;
		m_RoomIndex = -1;
	}

	void Set(const int sessionIndex, const char* pszID, const int32_t userHandle)
	{
		m_IsConfirm = true;
		m_CurDomainState = DOMAIN_STATE::LOGIN;
		m_SessionIndex = sessionIndex;
		m_ID = pszID;
		m_UserHandle = userHandle; // [추가] 전달받은 핸들 값을 멤버 변수에 저장
	}

	void SetConfirm(bool value)
	{
		m_IsConfirm = value;
	}

	short GetIndex() { return m_Index; }

	int GetSessionIndex() { return m_SessionIndex; }

	const std::string& GetID() { return m_ID; }

	bool IsConfirm() { return m_IsConfirm; }

	short GetRoomIndex() { return m_RoomIndex; }

	void EnterRoom(const short roomIndex)
	{
		m_RoomIndex = roomIndex;
		m_CurDomainState = DOMAIN_STATE::ROOM;
	}

	void LeaveRoom() {
		m_RoomIndex = -1;
		m_CurDomainState = DOMAIN_STATE::LOGIN;
	}

	bool IsCurDomainInLogIn() {
		return m_CurDomainState == DOMAIN_STATE::LOGIN ? true : false;
	}

	bool IsCurDomainInRoom() {
		return m_CurDomainState == DOMAIN_STATE::ROOM ? true : false;
	}
	int32_t GetUserHandle() const { return m_UserHandle; }
	const Vec2D& GetPosition() const { return m_Position; }
	void SetPosition(const Vec2D& pos) { m_Position = pos; }
	
	bool IsBot() const { return m_BotState != BOT_STATE::NONE; }
	BOT_STATE GetBotState() const { return m_BotState; }
	void SetBotState(BOT_STATE newState) { m_BotState = newState; }

	float& GetTimeUntilNextAction() { return m_TimeUntilNextAction; }

	void SetTeam(const int teamId) { m_team = teamId; }
	int GetTeam() const { return m_team; }

	void SetUserName(const std::string nickname) { m_nickName = nickname; }
	std::string GetUserName() const { return m_nickName; }
	bool IsDead() const { return m_isDead; }
	void Kill() { m_isDead = true; }
	void Respawn() { m_isDead = false; }

private:
	BOT_STATE m_BotState = BOT_STATE::NONE;
	float m_TimeUntilNextAction = 0.0f; // 다음 행동까지 남은 시간
protected:

	// 서버 관련 변수
	short m_Index = -1;
	int m_SessionIndex = -1; 
	std::string m_ID; // 로그인 ID
	bool m_IsConfirm = false;
	DOMAIN_STATE m_CurDomainState = DOMAIN_STATE::NONE;

	// 방 관련 변수
	short m_RoomIndex = -1; // 방의 번호
	int32_t m_UserHandle = -1; // 인게임 ID
	Vec2D m_Position;
	int m_team = -1;
	std::string m_nickName = "";
	bool m_isDead = true;
};
