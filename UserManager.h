#pragma once
#include <unordered_map>
#include <deque>
#include <string>
#include <vector>

#include "ErrorCode.h"

class User;

class UserManager
{
public:
	UserManager();
	virtual ~UserManager();

	void Init(const int maxUserCount);

	ERROR_CODE AddUser(const int sessionIndex, const char* pszID);
	ERROR_CODE RemoveUser(const int sessionIndex); // 접속 끊어지면 인덱스 반환

	std::tuple<ERROR_CODE, User*> GetUser(const int sessionIndex);


private:
	User* AllocUserObjPoolIndex();
	void ReleaseUserObjPoolIndex(const int index);

	User* FindUser(const int sessionIndex);
	User* FindUser(const char* pszID);

private:
	std::vector<User> m_UserObjPool; // 미리 객체 풀을 만들음. 
	std::deque<int> m_UserObjPoolIndex; // 사용하지 않는 객체의 인덱스

	std::unordered_map<int, User*> m_UserSessionDic;
	std::unordered_map<const char*, User*> m_UserIDDic;

};
