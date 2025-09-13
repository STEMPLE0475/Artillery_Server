#include <algorithm>

#include "User.h"
#include "UserManager.h"
#include "DBManager.h"


UserManager::UserManager()
{
}

UserManager::~UserManager()
{
}

void UserManager::Init(const int maxUserCount, DBManager* dbManager)
{
	m_pDBManager = dbManager;
	for (int i = 0; i < maxUserCount; ++i)
	{
		User user;
		user.Init((short)i);

		m_UserObjPool.push_back(user);
		m_UserObjPoolIndex.push_back(i);
	}
}

User* UserManager::AllocUserObjPoolIndex()
{
	if (m_UserObjPoolIndex.empty()) {
		return nullptr;
	}

	int index = m_UserObjPoolIndex.front();
	m_UserObjPoolIndex.pop_front();
	return &m_UserObjPool[index];
}

void UserManager::ReleaseUserObjPoolIndex(const int index)
{
	m_UserObjPoolIndex.push_back(index);
	m_UserObjPool[index].Clear();
}

ERROR_CODE UserManager::AddUser(const int sessionIndex, const uint8_t* pszID)
{
	const char* idStr = reinterpret_cast<const char*>(pszID);
	if (FindUser(idStr) != nullptr) {
		return ERROR_CODE::USER_MGR_ID_DUPLICATION;
	}

	auto pUser = AllocUserObjPoolIndex();
	if (pUser == nullptr) {
		return ERROR_CODE::USER_MGR_MAX_USER_COUNT;
	}

	int newUserHandle = ++m_UserHandleCounter;

	pUser->Set(sessionIndex, idStr, newUserHandle);

	m_UserSessionDic.insert({ sessionIndex, pUser });
	m_UserIDDic.insert({ std::string(idStr), pUser });

	return ERROR_CODE::NONE;
}

ERROR_CODE UserManager::RemoveUser(const int sessionIndex)
{
	auto pUser = FindUser(sessionIndex);

	if (pUser == nullptr) {
		return ERROR_CODE::USER_MGR_REMOVE_INVALID_SESSION;
	}

	auto index = pUser->GetIndex();
	auto pszID = pUser->GetID();

	m_UserSessionDic.erase(sessionIndex);
	m_UserIDDic.erase(std::string(pszID.c_str()));
	ReleaseUserObjPoolIndex(index);

	return ERROR_CODE::NONE;
}

std::tuple<ERROR_CODE, User*> UserManager::GetUser(const int sessionIndex)
{
	auto pUser = FindUser(sessionIndex);

	if (pUser == nullptr) {
		return { ERROR_CODE::USER_MGR_INVALID_SESSION_INDEX, nullptr };
	}

	if (pUser->IsConfirm() == false) {
		return{ ERROR_CODE::USER_MGR_NOT_CONFIRM_USER, nullptr };
	}

	return{ ERROR_CODE::NONE, pUser };
}

ERROR_CODE UserManager::RegisterUser(const std::string& id, const std::string& pw, const std::string& nick)
{
	if (!m_pDBManager) return ERROR_CODE::DB_ERROR;

	if (m_pDBManager->IsUserIDExist(id))
		return ERROR_CODE::REGISTER_DUPLICATE_ID;

	if (m_pDBManager->IsNicknameExist(nick))
		return ERROR_CODE::REGISTER_DUPLICATE_NICKNAME;

	if (!m_pDBManager->InsertUser(id, pw, nick))
		return ERROR_CODE::DB_ERROR;

	return ERROR_CODE::NONE;
}

ERROR_CODE UserManager::TryLogin(const int sessionIndex, const std::string& id, const std::string& pw)
{
	if (!m_pDBManager) return ERROR_CODE::DB_ERROR;

	if (!m_pDBManager->VerifyLogin(id, pw))
		return ERROR_CODE::LOGIN_FAIL;

	// 중복 로그인 방지
	if (FindUser(id.c_str())) return ERROR_CODE::LOGIN_ALREADY;

	
	return AddUser(sessionIndex, reinterpret_cast<const uint8_t*>(id.data()));

}

User* UserManager::FindUser(const int sessionIndex)
{
	auto findIter = m_UserSessionDic.find(sessionIndex);

	if (findIter == m_UserSessionDic.end()) {
		return nullptr;
	}

	return (User*)findIter->second;
}

/**
 * @brief 새로운 봇 User 객체를 생성하고, 튜플로 반환합니다.
 * @return std::tuple<ERROR_CODE, User*>
*/
std::tuple<ERROR_CODE, User*> UserManager::AddNewBot()
{
	// 1. 객체 풀에서 사용 가능한 User 객체를 하나 할당받습니다.
	User* pBot = AllocUserObjPoolIndex();
	if (pBot == nullptr)
	{
		return { ERROR_CODE::USER_MGR_MAX_USER_COUNT, nullptr };
	}

	// 2. 봇을 위한 고유 정보를 생성합니다.
	m_BotUIDCounter++;
	int fakeSessionIndex = -(m_BotUIDCounter);
	int32_t botHandle = fakeSessionIndex;
	std::string botId = "Bot_" + std::to_string(m_BotUIDCounter);

	// 3. User 객체를 봇 정보로 설정합니다.
	pBot->Set(fakeSessionIndex, botId.c_str(), botHandle);
	pBot->SetBotState(User::BOT_STATE::IN_LOBBY);

	// 4. 매니저의 관리 목록에 봇을 추가합니다.
	m_UserSessionDic.insert({ fakeSessionIndex, pBot });
	m_UserIDDic.insert({ botId, pBot });

	// 5. 튜플에 결과물을 담아 반환합니다.
	return { ERROR_CODE::NONE, pBot };
}

User* UserManager::FindUser(const char* pszID)
{
	auto findIter = m_UserIDDic.find(pszID);

	if (findIter == m_UserIDDic.end()) {
		return nullptr;
	}

	return (User*)findIter->second;
}
