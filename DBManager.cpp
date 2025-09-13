#include "DBManager.h"
#include <iostream>

using namespace mysqlx;

DBManager::DBManager() {}
DBManager::~DBManager() { m_Session.reset(); }

bool DBManager::Init(const std::string& host, const std::string& user,
    const std::string& pw, const std::string& db)
{
    try {
        const int XPORT = 33060;

        std::cerr << "[DB] XDev: create Session...\n";
        m_Session = std::make_unique<Session>(
            SessionOption::HOST, host.c_str(),     // 예: "127.0.0.1"
            SessionOption::PORT, XPORT,            // X Plugin 포트
            SessionOption::USER, user.c_str(),
            SessionOption::PWD, pw.c_str(),

            SessionOption::SSL_MODE, SSLMode::REQUIRED,
            SessionOption::CONNECT_TIMEOUT, 5000
        );

        m_DbName = db;

        // 스키마 존재 보장 필요하면 주석 해제
        // m_Session->sql("CREATE DATABASE IF NOT EXISTS " + m_DbName).execute();

        // 간단 핑
        m_Session->sql("SELECT 1").execute();
        std::cerr << "[DB] XDev: session OK\n";
        return true;
    }
    catch (const Error& e) {
        std::cerr << "[DB Init Error] " << e.what() << "\n";
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "[DB Init Error] std::exception: " << e.what() << "\n";
        return false;
    }
}

bool DBManager::InsertUser(const std::string& id, const std::string& pw, const std::string& nick)
{
    try {
        m_Session->sql("INSERT INTO " + m_DbName + ".Users (ID, Password, Nickname) VALUES (?, ?, ?)")
            .bind(id, pw, nick)
            .execute();
        return true;
    }
    catch (const Error& e) {
        std::cerr << "[InsertUser Error] " << e.what() << "\n";
        return false;
    }
}

bool DBManager::IsUserIDExist(const std::string& id)
{
    try {
        auto res = m_Session->sql("SELECT 1 FROM " + m_DbName + ".Users WHERE ID = ? LIMIT 1")
            .bind(id)
            .execute();
        return res.count() > 0;
    }
    catch (const Error& e) {
        std::cerr << "[IsUserIDExist Error] " << e.what() << "\n";
        return false;
    }
}

bool DBManager::IsNicknameExist(const std::string& nick)
{
    try {
        auto res = m_Session->sql("SELECT 1 FROM " + m_DbName + ".Users WHERE Nickname = ? LIMIT 1")
            .bind(nick)
            .execute();
        return res.count() > 0;
    }
    catch (const Error& e) {
        std::cerr << "[IsNicknameExist Error] " << e.what() << "\n";
        return false;
    }
}

bool DBManager::VerifyLogin(const std::string& id, const std::string& pw)
{
    try {
        auto res = m_Session->sql("SELECT Password FROM " + m_DbName + ".Users WHERE ID = ?")
            .bind(id)
            .execute();

        auto it = res.begin();
        if (it == res.end()) return false;

        // mysqlx::Value -> std::string
        std::string stored = (*it)[0].get<std::string>();
        return stored == pw; // (실전은 해시 비교 권장)
    }
    catch (const Error& e) {
        std::cerr << "[VerifyLogin Error] " << e.what() << "\n";
        return false;
    }
}

bool DBManager::GetUserNickname(const std::string& id, std::string& outNickname)
{
    try {
        // 1. "Users" 테이블에서 ID가 일치하는 유저의 "Nickname"을 조회하는 SQL 쿼리를 준비합니다.
        auto res = m_Session->sql("SELECT Nickname FROM " + m_DbName + ".Users WHERE ID = ?")
            .bind(id)
            .execute();

        // 2. 결과(RowResult)에서 첫 번째 행을 가져옵니다.
        auto row = res.fetchOne();

        // 3. 결과 행이 없다면 (해당 ID의 유저가 없다면) 실패를 반환합니다.
        if (!row) {
            return false;
        }

        // 4. 결과 행의 첫 번째 열(인덱스 0)에 있는 닉네임 값을 std::string으로 변환하여
        //    출력 파라미터(outNickname)에 저장합니다.
        outNickname = row[0].get<std::string>();
        return true;
    }
    catch (const mysqlx::Error& e) {
        // 5. DB 작업 중 오류가 발생하면 로그를 남기고 실패를 반환합니다.
        std::cerr << "[GetUserNickname Error] " << e.what() << "\n";
        return false;
    }
}
