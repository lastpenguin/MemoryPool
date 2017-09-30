#pragma once
/*----------------------------------------------------------------
/
/----------------------------------------------------------------
/
/   다음의 자료를 참조하였다.
/       - http://serious-code.net/moin.cgi/MiniDump
/----------------------------------------------------------------
/   사용자는 프로그램 실행후 가능한 빠른 시기 CMiniDump::Install 을 호출하여 사용
/----------------------------------------------------------------
/   메모리 부족 예외
/       new 예외 핸들러는 사용자가 지정해야 합니다.
/       std::set_new_handler
/       필요한 경우 WriteDump_OutofMemory를 지정 하십시오
/       E_OUTOFMEMORY 예외로써 처리합니다.
/----------------------------------------------------------------
/   메모리 부족 문제
/       Stack Memory
/           문제 없음
/       Heap Memory
/           dbghelp 함수내 동적메모리 필요로 인한 문제
/           Install 파라미터 _pFN_Reduce_HeapMemory 를 지정
/
/           사용자는 LPFUNCTION_Request_Reduce_HeapMemory 타입 함수를 정의
/           리턴 BOOL
/               정리한 Heap 메모리가 있을 때만 TRUE를 리턴해야 함
/               (결과와 무관하게 TRUE를 리턴하면 무한루프 가능성이 있음)
/           구현
/               가능한 VirtualFree를 직접 호출하는 것이 좋음
/               일반적인 메모리할당자의 free/delete 는 메모리가 OS에 즉시 반납된다는 보장이 없음
/----------------------------------------------------------------
/   개선사항
/       동적메모리 부족으로 덤프기록 실패처리를 위해
/       기존 미니덤프객체가 미리 가상메모리를 확보해두는 형태는 적절하지 않아,
/       사용자가 가상메모리를 정리하는 형태로 수정
----------------------------------------------------------------*/
#pragma warning(push)
#pragma warning(disable: 4091)
#include <DbgHelp.h>
#pragma warning(pop)
#include "../../Core/UTIL/DebugMemory.h"
#include "../../BasisClass/BasisClass.h"

class CMiniDump{
public:
    _DEF_FRIEND_SINGLETON;
    // 예외보고 함수 : 만약 덤프파일 생성에 실패하였다면, _szDumpFile : nullptr
    typedef void (__stdcall *LPFUNCTION_ERROR_REPORT)(LPCTSTR _szDumpFile);
    typedef BOOL(__stdcall *LPFUNCTION_Request_Reduce_HeapMemory)(void);
    typedef BOOL(__stdcall *LPFUNCTION_MINIDUMPWRITEDUMP)(
        IN HANDLE hProcess,
        IN DWORD ProcessId,
        IN HANDLE hFile,
        IN MINIDUMP_TYPE DumpType,
        IN CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
        IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
        IN CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
        );
protected:
    CMiniDump();
    ~CMiniDump();

private:
    BOOL        m_bConnected_dbghelp;
    BOOL        m_bInstalled;

    HANDLE      m_hCallbackThread;
    HANDLE      m_hEventDump;
    UTIL::LOCK::CCompositeLock m_Lock_Request_Dump;
    UTIL::LOCK::CSpinLock m_Lock_ThisOBJ;

    struct TSignal{
        TSignal();
        BOOL    m_bCMD_ExitThread;
        LONG    m_Exception_Continue__Code;

        MINIDUMP_EXCEPTION_INFORMATION* m_pExParam;
    }m_signal;

    LPTOP_LEVEL_EXCEPTION_FILTER    m_pPrev_Filter;
    MINIDUMP_TYPE                   m_DumpLevel;
    TCHAR                           m_szWriteFolder[MAX_PATH];

    HMODULE m_hDLL_dbghelp;
    LPFUNCTION_MINIDUMPWRITEDUMP m_pFN_WriteDump;
    LPFUNCTION_ERROR_REPORT      m_pFN_ErrorReportFunction;
    LPFUNCTION_Request_Reduce_HeapMemory m_pFN_Reduce_VirtualMemory;

public:
    //----------------------------------------------------------------
    //  이하 사용자 함수
    //----------------------------------------------------------------

    // Minidump 사용 준비
    // 임시 메모리 예약: Out Of Memory의 경우 덤프를 기록할때,
    //                   메모리 부족으로 실패하게 되는데,
    //                   미리 할당한 메모리를 해제해주면 해결된다.
    // Parameter
    //      저장될 경로(상대경로 지정시 Current Directory 내의 폴더)
    //      힙메모리 정리함수 지정
    //      예외 보고함수 지정
    //      덤프 레벨
    static BOOL Install(LPCTSTR _szWriteFolder
        , LPFUNCTION_Request_Reduce_HeapMemory _FN_Reduce_HeapMemory = NULL
        , LPFUNCTION_ERROR_REPORT _FN_ErrorReport = NULL
        , MINIDUMP_TYPE _DumpLevel = MiniDumpNormal
    );

    // 사용자 함수: MiniDump 해제(소멸자에서 자동으로 호출된다)
    static BOOL Uninstall();
    // 사용자 함수: 덤프를 강제로 기록한다.
    static void ForceDump(BOOL _bExitProgram=FALSE);

    // Write Dump : Out of Memory
    static void __stdcall WriteDump_OutofMemory__stdcall();
    static void __cdecl WriteDump_OutofMemory__cdecl();

    //----------------------------------------------------------------
    //  이하 함수는 내부에서 잠금을 사용한다
    //  CMiniDump 의 Internal 함수에서 이 함수들을 사용시 데드락상태에 빠진다
    //----------------------------------------------------------------
    // 덤프기록 경로 변경
    static BOOL Change_WriteFolder(LPCTSTR _szWriteFolder);
    // 덤프 레벨 변경
    static BOOL Change_DumpLevel(MINIDUMP_TYPE _DumpLevel);
    // 예외 보고함수 변경
    static BOOL Change_ErrorReport_Function(LPFUNCTION_ERROR_REPORT _ErrorReportFunction);
    // Get: 예외 보고함수
    static LPFUNCTION_ERROR_REPORT Get_ErrorReport_Function();
    // 힙메모리 정리함수 변경
    static BOOL Change_ReduceHeapMemory_Function(LPFUNCTION_Request_Reduce_HeapMemory _ReduceHeapMemory_Function);
    // Get: 힙메모리 정리함수
    static LPFUNCTION_Request_Reduce_HeapMemory Get_ReduceHeapMemory_Function();

private:
    // Write Dump
    static LONG __stdcall WriteDump(MINIDUMP_EXCEPTION_INFORMATION* pExParam);
    // Dump Thread
    static unsigned __stdcall CallbackThread(void* pParam);
    // Exception Filter
    static LONG __stdcall ExceptionFilter(PEXCEPTION_POINTERS exPtrs);

    BOOL _Uninstall();
    BOOL Connect_dbghelp();
    void Disconnect_dbghelp();

    // 덤프기록 경로 변경
    BOOL _Change_WriteFolder(LPCTSTR _szWriteFolder);
    // 덤프 레벨 변경
    BOOL _Change_DumpLevel(MINIDUMP_TYPE _DumpLevel);
    // 예외 보고함수 변경
    BOOL _Change_ErrorReport_Function(LPFUNCTION_ERROR_REPORT _ErrorReportFunction);
    // Get: 예외 보고함수
    LPFUNCTION_ERROR_REPORT _Get_ErrorReport_Function();
    // 힙메모리 정리함수 변경
    BOOL _Change_ReduceHeapMemory_Function(LPFUNCTION_Request_Reduce_HeapMemory _ReduceHeapMemory_Function);
    // Get: 힙메모리 정리함수
    LPFUNCTION_Request_Reduce_HeapMemory _Get_ReduceHeapMemory_Function();

    HMODULE LoadDebugHelpLibrary();

    HANDLE CreateDumpFile(LPTSTR _out_FileName, size_t _len);

public:
    void WriteLog(PEXCEPTION_POINTERS exPtrs);
    static LPCTSTR Get_ExceptionName(DWORD dwExceptionCode);            // Get: 예외코드의 이름
    static void WriteLog__FaultReason(PEXCEPTION_POINTERS exPtrs);      // LOG: 예외 원인을 기록한다.
    static void WriteLog__Os();
    static void WriteLog__CpuInfo();
    static void WriteLog__ProcessInfo();
    static void WriteLog__MemoryInfo();
};