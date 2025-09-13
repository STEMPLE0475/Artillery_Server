#pragma once
#include <string>
#include <memory>
#include <mysqlx/xdevapi.h>

class DBManager
{
public:
    DBManager();
    ~DBManager();

    // host: "127.0.0.1" 권장, X Plugin 기본 포트는 33060
    bool Init(const std::string& host, const std::string& user,
        const std::string& pw, const std::string& db);

    bool InsertUser(const std::string& id, const std::string& pw, const std::string& nick);
    bool IsUserIDExist(const std::string& id);
    bool IsNicknameExist(const std::string& nick);
    bool VerifyLogin(const std::string& id, const std::string& pw);
    bool GetUserNickname(const std::string& id, std::string& outNickname);

private:
    std::unique_ptr<mysqlx::Session> m_Session;
    std::string m_DbName; // Schema는 보관하지 않고 이름만 저장
};
