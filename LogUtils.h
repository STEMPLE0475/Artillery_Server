
#pragma once
#include <mutex>
#include <fstream>

using LOG_TYPE = NServerNetLib::LOG_TYPE;

// 로그 유틸리티 네임스페이스
namespace LogUtils {

    // 콘솔 미리보기 바이트 수 (기본 64)
    inline int& PreviewBytes() { static int n = 64; return n; }

    // 덤프 파일 경로
    inline const char*& DumpPath() { static const char* p = "echo_dump.bin"; return p; }

    // 내부 뮤텍스 (멀티스레드 보호)
    inline std::mutex& Mtx() { static std::mutex m; return m; }

    // 전체 메시지를 파일에 기록
    inline void DumpToFile(const char* data, int len) {
#ifdef NETTRACE_ENABLE // 디버그 전용 플래그
        std::lock_guard<std::mutex> lock(Mtx());
        std::ofstream ofs(DumpPath(), std::ios::binary | std::ios::app);
        if (!ofs) return;
        ofs.write(data, len);
        ofs.put('\n'); // 구분용
        ofs.flush();
#endif
    }

    // 콘솔/로거에 앞부분만 안전하게 출력
    template <class Logger>
    inline void LogPreview(Logger* logger, int session, int pktId, int len, const char* data) {
#ifdef NETTRACE_ENABLE
        const int show = (len > PreviewBytes()) ? PreviewBytes() : len;
        logger->Write(LOG_TYPE::L_INFO,
            "[SRV-RX] session=%d id=%d len=%d preview(%02d)=\"%.*s\"%s",
            session, pktId, len,
            show, show, data,
            (len > show ? " ..." : ""));
#endif
    }

    // 파일 + 콘솔 동시에
    template <class Logger>
    inline void PreviewAndDump(Logger* logger, int session, int pktId, int len, const char* data) {
        DumpToFile(data, len);
        LogPreview(logger, session, pktId, len, data);
    }

} // namespace LogUtils
