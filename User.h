#pragma once
#include <string>


class User
{
public:
	enum class DOMAIN_STATE {
		NONE = 0,
		LOGIN = 1,
		ROOM = 2,
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

	void Set(const int sessionIndex, const char* pszID)
	{
		m_IsConfirm = true;
		m_CurDomainState = DOMAIN_STATE::LOGIN;

		m_SessionIndex = sessionIndex;
		m_ID = pszID;

	}

	short GetIndex() { return m_Index; }

	int GetSessioIndex() { return m_SessionIndex; }

	std::string& GetID() { return m_ID; }

	bool IsConfirm() { return m_IsConfirm; }

	short GetRoomIndex() { return m_RoomIndex; }

	void EnterRoom(const short roomIndex)
	{
		m_RoomIndex = roomIndex;
		m_CurDomainState = DOMAIN_STATE::ROOM;
	}

	bool IsCurDomainInLogIn() {
		return m_CurDomainState == DOMAIN_STATE::LOGIN ? true : false;
	}

	bool IsCurDomainInRoom() {
		return m_CurDomainState == DOMAIN_STATE::ROOM ? true : false;
	}


protected:
	short m_Index = -1;

	int m_SessionIndex = -1; // 네트워크쪽 인덱스 번호

	std::string m_ID; // 로그인할 때 아이디

	bool m_IsConfirm = false;

	DOMAIN_STATE m_CurDomainState = DOMAIN_STATE::NONE; // 로그인 했는지 방에 들어갔는지.

	short m_RoomIndex = -1; // 방의 번호
};
