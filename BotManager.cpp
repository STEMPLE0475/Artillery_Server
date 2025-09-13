#include "BotManager.h"
#include "User.h"
#include "GameManager.h"
#include "ProtocolCommon.h"
#include "NetLib/TcpNetwork.h"

using LOG_TYPE = NServerNetLib::LOG_TYPE;
using ServerConfig = NServerNetLib::ServerConfig;

void BotManager::Init(GameManager* pGameMgr, ILog* pLogger)
{
    m_pRefGameMgr = pGameMgr;
    m_pRefLogger = pLogger;
}

void BotManager::Tick(const float deltaTime)
{
    for (auto pBot : m_BotList)
    {
        if (pBot == nullptr) continue;
        
        float& timeUntilNextAction = pBot->GetTimeUntilNextAction();
        timeUntilNextAction -= deltaTime;
        if (timeUntilNextAction > 0.0f) continue;

        switch (pBot->GetBotState())
        {
        case User::BOT_STATE::IN_LOBBY:
            m_pRefGameMgr->OnMatchRequest(pBot->GetSessionIndex());
            pBot->SetBotState(User::BOT_STATE::IN_MATCHMAKING);
            m_pRefLogger->Write(LOG_TYPE::L_INFO, "Bot %s starts matchmaking.", pBot->GetID().c_str());
            break;

        case User::BOT_STATE::IN_GAME:
            
            int botTeam = pBot->GetTeam();
            float randomAngle = 0.0f;

            // 0번 팀은 왼쪽 진영, 1번 팀은 오른쪽 진영
            if (botTeam != 1) 
            {
                // 50도 ~ 80도
                randomAngle = (float)(rand() % 31 + 50);
            }
            else 
            {
                // 100도 ~ 130도
                randomAngle = (float)(rand() % 31 + 100);
            }

            float minCharge = 3.0f * 0.7f; // 2.1초
            float maxCharge = 3.0f * 0.9f; // 2.7초
            float randomRatio = rand() / (float)RAND_MAX; // 0.0f ~ 1.0f
            float randomChargeSec = minCharge + (randomRatio * (maxCharge - minCharge));

            PktFireStartReq fireReq{};
            fireReq.AimAngle = randomAngle;
            fireReq.ChargeSec = randomChargeSec;
            m_pRefGameMgr->OnFireRequest(pBot->GetSessionIndex(), &fireReq);

            // 2~5초 사이의 랜덤한 시간갖고 행동 재시작
            timeUntilNextAction = (float)((rand() % 3001) + 2000) / 1000.0f;
            break;
        }
    }
}

void BotManager::AddBot(User* pBot)
{
    if (pBot == nullptr || !pBot->IsBot())
    {
        return;
    }
    m_BotList.push_back(pBot);
}
