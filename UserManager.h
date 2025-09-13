#pragma once
#include <unordered_map>
#include <deque>
#include <string>
#include <vector>
#include <tuple>

#include "ProtocolCommon.h"

class User;
class DBManager;

class UserManager
{
public:
	UserManager();
	virtual ~UserManager();

	void Init(const int maxUserCount, DBManager* dbManager);

	ERROR_CODE AddUser(const int sessionIndex, const uint8_t* pszID);
	ERROR_CODE RemoveUser(const int sessionIndex); // 접속 끊어지면 인덱스 반환
	std::tuple<ERROR_CODE, User*> GetUser(const int sessionIndex);

	ERROR_CODE RegisterUser(const std::string& id, const std::string& pw, const std::string& nick);
	ERROR_CODE TryLogin(const int sessionIndex, const std::string& id, const std::string& pw);
	User* FindUser(const int sessionIndex);
	std::tuple<ERROR_CODE, User*> AddNewBot();


private:
	User* AllocUserObjPoolIndex();
	void ReleaseUserObjPoolIndex(const int index);

	
	User* FindUser(const char* pszID);

private:
	std::vector<User> m_UserObjPool; // 미리 객체 풀을 만들음. 
	std::deque<int> m_UserObjPoolIndex; // 사용하지 않는 객체의 인덱스

	std::unordered_map<int, User*> m_UserSessionDic;
	std::unordered_map<std::string, User*> m_UserIDDic;

	DBManager* m_pDBManager = nullptr;
	int m_BotUIDCounter = 0;
	int m_UserHandleCounter = 0;

};
