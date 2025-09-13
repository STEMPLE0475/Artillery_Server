#include <cstring>

#include "NetLib/ILog.h"
#include "NetLib/TcpNetwork.h"
#include "User.h"
#include "UserManager.h"
//#include "Room.h"
#include "RoomManager.h"
#include "PacketProcess.h"
#include "GameManager.h"
#include "DBManager.h";

using LOG_TYPE = NServerNetLib::LOG_TYPE;
using ServerConfig = NServerNetLib::ServerConfig;

// 수신한 패킷을 처리하고, 맞는 함수를 ㅗㅎ출함.

PacketProcess::PacketProcess() {}
PacketProcess::~PacketProcess() {}

void PacketProcess::Init(TcpNet* pNetwork, UserManager* pUserMgr, RoomManager* pLobbyMgr, ServerConfig* pConfig, ILog* pLogger, DBManager* pDBMgr, GameManager* pGameMgr)
{
	m_pRefLogger = pLogger;
	m_pRefNetwork = pNetwork;
	m_pRefUserMgr = pUserMgr;
	m_pRefRoomMgr = pLobbyMgr;
	m_pRefDBMgr = pDBMgr;
	m_pRefGameMgr = pGameMgr;
}

//모든 작업을 여기서 처리함. 새로운 패킷이 오면 패킷을 어떻게 처리하는지를 정함.
void PacketProcess::Process(PacketInfo packetInfo)
{
	using netLibPacketId = NServerNetLib::PACKET_ID;
	using commonPacketId = PACKET_ID;

	auto packetId = packetInfo.PacketId;

	//상용 서비스에서 switch 문을 사용하면 너무 길어짐. 함수 포인터로 가독성 향상 시킬 수 있음.
	switch (packetId)
	{
	case (int)netLibPacketId::NTF_SYS_CONNECT_SESSION:
		NtfSysConnctSession(packetInfo);
		break;
	case (int)netLibPacketId::NTF_SYS_CLOSE_SESSION:
		NtfSysCloseSession(packetInfo);
		break;
		//여기서부터가 진짜 클라가 보낸 패킷을 알아서 처리하는 방법.
	case (int)commonPacketId::LOGIN_REQ:
		Login(packetInfo);
		break;
	case (int)commonPacketId::REGISTER_REQ:
		Register(packetInfo);
		break;
		//이걸 따라서 만들기. 방 만들기 방 나가기 등.
	case (int)commonPacketId::ECHO_CHAT_REQ:
		EchoChat(packetInfo);
		break;
	case (int)commonPacketId::MATCH_REQUEST_REQ:
		m_pRefGameMgr->OnMatchRequest(packetInfo.SessionIndex);
		break;

	case (int)commonPacketId::FIRE_START_REQ:
		FireStart(packetInfo);
	}


}


ERROR_CODE PacketProcess::NtfSysConnctSession(PacketInfo packetInfo)
{
	m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysConnctSession. sessionIndex(%d)", __FUNCTION__, packetInfo.SessionIndex);
	return ERROR_CODE::NONE;
}

ERROR_CODE PacketProcess::NtfSysCloseSession(PacketInfo packetInfo)
{
	auto pUser = std::get<1>(m_pRefUserMgr->GetUser(packetInfo.SessionIndex));

	if (pUser) {
		m_pRefUserMgr->RemoveUser(packetInfo.SessionIndex);
	}

	m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d)", __FUNCTION__, packetInfo.SessionIndex);
	return ERROR_CODE::NONE;
}


ERROR_CODE PacketProcess::Login(PacketInfo packetInfo)
{
	// 1. 요청 패킷 파싱
	auto reqPkt = (PktLogInReq*)packetInfo.pRefData;
	m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Login. sessionIndex(%d), ID(%s) | 로그인 요청 수신",
		__FUNCTION__, packetInfo.SessionIndex, reqPkt->szID);

	// 2. 응답 패킷 미리 생성
	PktLogInRes resPkt;

	// 3. ID/PW 인증 (DB 연동)
	std::string userNickname; // DB에서 조회된 유저 닉네임
	auto authRet = AuthenticateUser(reqPkt->szID, reqPkt->szPW, userNickname);

	if (authRet != ERROR_CODE::NONE)
	{
		// 인증 실패 (ID 없음, PW 틀림 등)
		m_pRefLogger->Write(LOG_TYPE::L_ERROR, "%s | Login. sessionIndex(%d), ID(%s) | 인증 실패: %d",
			__FUNCTION__, packetInfo.SessionIndex, reqPkt->szID, (int)authRet);

		resPkt.SetError(authRet);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_RES, sizeof(resPkt), (char*)&resPkt);
		return authRet;
	}

	// 4. 인증 성공 후, 현재 접속 중인 유저인지 확인 및 추가
	auto addRet = m_pRefUserMgr->AddUser(packetInfo.SessionIndex, (const uint8_t*)reqPkt->szID);
	if (addRet != ERROR_CODE::NONE)
	{
		// 유저 추가 실패 (이미 접속 중인 ID 등)
		m_pRefLogger->Write(LOG_TYPE::L_ERROR, "%s | Login. sessionIndex(%d), ID(%s) | 유저 추가 실패: %d",
			__FUNCTION__, packetInfo.SessionIndex, reqPkt->szID, (int)addRet);

		resPkt.SetError(addRet);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_RES, sizeof(resPkt), (char*)&resPkt);
		return addRet;
	}

	// 5. 최종 성공 응답 패킷 구성 및 전송
	resPkt.SetError(ERROR_CODE::NONE);

	// 닉네임 정보 채우기
	const int32_t nickLen = userNickname.length();
	const int32_t copyLen = std::min(nickLen, MAX_USER_NICKNAME_SIZE);
	resPkt.NickLen = static_cast<uint16_t>(copyLen);
	memcpy(resPkt.Nick, userNickname.c_str(), copyLen);

	m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_RES, sizeof(resPkt), (char*)&resPkt);

	m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Login. sessionIndex(%d), ID(%s) | 로그인 성공",
		__FUNCTION__, packetInfo.SessionIndex, reqPkt->szID);

	return ERROR_CODE::NONE;
}

ERROR_CODE PacketProcess::AuthenticateUser(const uint8_t* id, const uint8_t* pw, std::string& outNickname)
{
	std::string id_str((const char*)id);
	std::string pw_str((const char*)pw);

	if (!m_pRefDBMgr->VerifyLogin(id_str, pw_str))
	{
		// ID가 없거나 PW가 틀린 경우
		return ERROR_CODE::REGISTER_INVALID_ID;
	}

	if (!m_pRefDBMgr->GetUserNickname(id_str, outNickname))
	{
		// 유저는 있으나 닉네임 조회 실패 (DB 에러)
		return ERROR_CODE::DB_ERROR;
	}

	// 인증 성공
	return ERROR_CODE::NONE;
}

ERROR_CODE PacketProcess::FireStart(PacketInfo packetInfo) {

	auto reqPkt = (PktFireStartReq*)packetInfo.pRefData;
	m_pRefGameMgr->OnFireRequest(packetInfo.SessionIndex, reqPkt);

	return ERROR_CODE::NONE;
}
ERROR_CODE PacketProcess::MatchCancel(PacketInfo packetInfo)
{
	return ERROR_CODE::NONE;
}

ERROR_CODE PacketProcess::Register(PacketInfo packetInfo)
{
	// 1. 요청 패킷 파싱
	auto reqPkt = (PktRegisterReq*)packetInfo.pRefData;
	m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Register. sessionIndex(%d), ID(%s) | 회원가입 요청 수신",
		__FUNCTION__, packetInfo.SessionIndex, reqPkt->szID);

	// 2. 응답 패킷 미리 생성 (PktRegisterRes가 있다고 가정)
	PktRegisterRes resPkt;

	// 3. DB에 등록 시도 (ID/닉네임 중복 확인 등)
	std::string id_str((const char*)reqPkt->szID);
	std::string pw_str((const char*)reqPkt->szPW);
	std::string nick_str((const char*)reqPkt->Nick, reqPkt->NickLen);

	if (m_pRefDBMgr->IsUserIDExist(id_str))
	{
		resPkt.SetError(ERROR_CODE::REGISTER_INVALID_ID);
	}
	else if (m_pRefDBMgr->IsNicknameExist(nick_str))
	{
		resPkt.SetError(ERROR_CODE::REGISTER_INVALID_NICKNAME);
	}
	else if (!m_pRefDBMgr->InsertUser(id_str, pw_str, nick_str)) // TODO: PW는 해시하여 저장해야 함
		//InsertUser 내부에서 PW에 맞게 해시하여 저장?
	{
		resPkt.SetError(ERROR_CODE::DB_ERROR);
	}
	else
	{
		resPkt.SetError(ERROR_CODE::NONE);
	}

	// 4. 결과에 따른 응답 전송
	auto regRet = (ERROR_CODE)resPkt.ErrorCode;
	if (regRet != ERROR_CODE::NONE)
	{
		m_pRefLogger->Write(LOG_TYPE::L_ERROR, "%s | Register. sessionIndex(%d), ID(%s) | 등록 실패: %d",
			__FUNCTION__, packetInfo.SessionIndex, reqPkt->szID, (int)regRet);
	}
	else
	{
		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Register. sessionIndex(%d), ID(%s) | 회원가입 성공",
			__FUNCTION__, packetInfo.SessionIndex, reqPkt->szID);
	}

	m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::REGISTER_RES, sizeof(resPkt), (char*)&resPkt);
	return regRet;
}

ERROR_CODE PacketProcess::EchoChat(PacketInfo p)
{
	const auto* req = reinterpret_cast<const PktEchoChatReq*>(p.pRefData);

	uint16_t len = req->Len;
	if (len > MAX_CHAT_LEN - 1) len = MAX_CHAT_LEN - 1;

	// 로그: 콘솔에는 앞부분만, 파일에는 전체
	LogUtils::PreviewAndDump(m_pRefLogger, p.SessionIndex, p.PacketId,
		len, reinterpret_cast<const char*>(req->Msg));

	// 회신 (널 보장)
	PktEchoChatNty nty{};
	nty.Len = len;
	std::memcpy(nty.Msg, req->Msg, len);
	nty.Msg[len] = '\0';

	m_pRefNetwork->SendData(p.SessionIndex,
		static_cast<short>(PACKET_ID::ECHO_CHAT_NTY),
		sizeof(PktEchoChatNty),
		reinterpret_cast<const char*>(&nty));

	return ERROR_CODE::NONE;
}


