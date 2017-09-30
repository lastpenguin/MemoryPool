#include "stdafx.h"
#include "./MiniDump.h"

#include <atlpath.h>

CMiniDump::TSignal::TSignal()
: m_bCMD_ExitThread(FALSE)
, m_Exception_Continue__Code(0)
, m_pExParam(NULL)
{
}

CMiniDump::CMiniDump()
: m_bConnected_dbghelp(FALSE)
, m_bInstalled(FALSE)
, m_hCallbackThread(NULL)
, m_hEventDump(NULL)
, m_pPrev_Filter(NULL)
, m_DumpLevel(MiniDumpNormal)
, m_hDLL_dbghelp(NULL)
, m_pFN_WriteDump(NULL)
, m_pFN_ErrorReportFunction(NULL)
, m_pFN_Reduce_VirtualMemory(NULL)
{
    _tcscpy_s(m_szWriteFolder, _T("DUMP"));

    Connect_dbghelp();
    _LOG_DEBUG__WITH__OUTPUTDEBUGSTR(TRUE, _T("--- Create CMiniDump : %s ---"), ((m_bConnected_dbghelp)? _T("Succeed") : _T("Failed")));
}

CMiniDump::~CMiniDump()
{
    _Uninstall();

    Disconnect_dbghelp();
    _LOG_DEBUG__WITH__OUTPUTDEBUGSTR(TRUE, _T("--- Destroy CMiniDump ---"));
}

BOOL CMiniDump::Install(LPCTSTR _szWriteFolder
    , LPFUNCTION_Request_Reduce_HeapMemory _FN_Reduce_HeapMemory
    , LPFUNCTION_ERROR_REPORT _FN_ErrorReport
    , MINIDUMP_TYPE _DumpLevel)
{
    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();

    if(!_THIS)
        return FALSE;

    _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);

#ifdef _DEBUG
    if(_THIS->m_bInstalled)
        _DebugBreak(_T("중복 호출"));
#endif
    if(!_THIS->m_bConnected_dbghelp || _THIS->m_bInstalled)
        return FALSE;

    _LOG_DEBUG__WITH__OUTPUTDEBUGSTR(TRUE, _T("--- Install CMiniDump ---"));

    _THIS->_Change_WriteFolder(_szWriteFolder);             // 덤프기록 폴더 변경
    _THIS->_Change_DumpLevel(_DumpLevel);                   // 덤프 레벨 변경
    _THIS->_Change_ErrorReport_Function(_FN_ErrorReport);   // 예외 보고함수 변경
    _THIS->_Change_ReduceHeapMemory_Function(_FN_Reduce_HeapMemory); // 힙메모리 정리함수 지정

    // 예외 필터 변경
    _THIS->m_pPrev_Filter = SetUnhandledExceptionFilter(ExceptionFilter);
    
    // 이벤트 생성(초기 잠금)
    if(!_THIS->m_hEventDump)
        _THIS->m_hEventDump = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(!_THIS->m_hEventDump)
        return FALSE;

    // 스레드 생성
    unsigned _IDThread;
    _THIS->m_hCallbackThread = reinterpret_cast<HANDLE>( _beginthreadex(NULL, 0, _THIS->CallbackThread, _THIS, 0, &_IDThread) );
    if(0 == _THIS->m_hCallbackThread)
        return FALSE;

    _THIS->m_bInstalled = TRUE;
    return TRUE;
}

BOOL CMiniDump::Uninstall()
{
    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();

    if(!_THIS)
        return FALSE;

    return _THIS->_Uninstall();
}

void CMiniDump::ForceDump(BOOL _bExitProgram)
{
    _LOG_DEBUG__WITH__TRACE(TRUE, _T("--- Call Force Dump(ProgramExit: %s) ---"), (_bExitProgram)? _T("YES") :_T("NO"));

    if(_bExitProgram)
        ::RaiseException(NOERROR, EXCEPTION_NONCONTINUABLE, 0, NULL);
    else
        ::RaiseException(NOERROR, 0, 0, NULL);
}
void __stdcall CMiniDump::WriteDump_OutofMemory__stdcall()
{
    _Error_OutOfMemory();
}
void __cdecl CMiniDump::WriteDump_OutofMemory__cdecl()
{
    _Error_OutOfMemory();
}

// 덤프기록 경로 변경
BOOL CMiniDump::Change_WriteFolder(LPCTSTR _szWriteFolder)
{
    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();
    if(!_THIS || !_szWriteFolder)
        return FALSE;
    if(!_THIS->m_bInstalled)
        return FALSE;

    _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);
    return _THIS->_Change_WriteFolder(_szWriteFolder);
}
// 덤프 레벨 변경
BOOL CMiniDump::Change_DumpLevel(MINIDUMP_TYPE _DumpLevel)
{
    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();
    if(!_THIS)
        return FALSE;
    if(!_THIS->m_bInstalled)
        return FALSE;

    _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);
    return _THIS->_Change_DumpLevel(_DumpLevel);
}
// 예외 보고함수 변경
BOOL CMiniDump::Change_ErrorReport_Function(LPFUNCTION_ERROR_REPORT _ErrorReportFunction)
{
    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();
    if(!_THIS)
        return FALSE;
    if(!_THIS->m_bInstalled)
        return FALSE;

    _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);
    return _THIS->_Change_ErrorReport_Function(_ErrorReportFunction);
}
// Get: 예외 보고함수
CMiniDump::LPFUNCTION_ERROR_REPORT CMiniDump::Get_ErrorReport_Function()
{
    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();
    if(!_THIS)
        return NULL;
    if(!_THIS->m_bInstalled)
        return nullptr;

    _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);
    return _THIS->_Get_ErrorReport_Function();
}
// 힙메모리 정리함수 변경
BOOL CMiniDump::Change_ReduceHeapMemory_Function(LPFUNCTION_Request_Reduce_HeapMemory _ReduceHeapMemory_Function)
{
    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();
    if(!_THIS)
        return FALSE;
    if(!_THIS->m_bInstalled)
        return FALSE;

    _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);
    return _THIS->_Change_ReduceHeapMemory_Function(_ReduceHeapMemory_Function);
}
// Get: 힙메모리 정리함수
CMiniDump::LPFUNCTION_Request_Reduce_HeapMemory CMiniDump::Get_ReduceHeapMemory_Function()
{
    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();
    if(!_THIS)
        return NULL;
    if(!_THIS->m_bInstalled)
        return nullptr;

    _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);
    return _THIS->_Get_ReduceHeapMemory_Function();
}

// Write Dump
LONG __stdcall CMiniDump::WriteDump(MINIDUMP_EXCEPTION_INFORMATION* pExParam)
{
#define _LOCAL_MACRO_RETURN_FAIL {_LOG_SYSTEM__WITH__TRACE(TRUE, _T("---- WriteMiniDump Failed ----")); return EXCEPTION_CONTINUE_SEARCH;}

    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();
    if(!_THIS)
        _LOCAL_MACRO_RETURN_FAIL;

    _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);

    struct TFinally{
        TFinally()
        : m_hFile(INVALID_HANDLE_VALUE), m_pExParam(NULL)
        {
        }
        ~TFinally()
        {
            if(m_hFile != INVALID_HANDLE_VALUE)
                ::CloseHandle(m_hFile);
            CSingleton<CMiniDump>::Get_Instance()->WriteLog((m_pExParam)?m_pExParam->ExceptionPointers : NULL);
        }
        HANDLE m_hFile;
        MINIDUMP_EXCEPTION_INFORMATION* m_pExParam;
    }finally;
    finally.m_pExParam = pExParam;

    // EXCEPTION_EXECUTE_HANDLER    __except블록의 코드를 실행하도록 한다.
    // EXCEPTION_CONTINUE_EXECUTION 예외를 무시, 예외가 발생한곳의 코드를 다시 실행(예외가 해결된 경우에만 사용한다)
    // EXCEPTION_CONTINUE_SEARCH    __except블록의 코드가 아닌 상위 예외 처리 핸들러에 넘긴다.

    TCHAR _szPath[MAX_PATH];
    finally.m_hFile = _THIS->CreateDumpFile(_szPath, MAX_PATH);
    if(finally.m_hFile == INVALID_HANDLE_VALUE)
        _LOCAL_MACRO_RETURN_FAIL;
    //----------------------------------------------------------------
    BOOL bSucceed_WritedDump;
    for(;;)
    {
        bSucceed_WritedDump = _THIS->m_pFN_WriteDump(::GetCurrentProcess(), ::GetCurrentProcessId()
            , finally.m_hFile, _THIS->m_DumpLevel, pExParam, NULL, NULL);
        
        if(bSucceed_WritedDump)
            break;

        // 실패한다면 힙 메모리 해제 시도
        //      덤프 기록은 더는 힙 할당이 불가능할때 실패하는데
        //       내부적으로 힙 메모리 할당을 사용한다고 추측할 수 있다
        LPFUNCTION_Request_Reduce_HeapMemory pFN_ReduceHeapMemory = _THIS->_Get_ReduceHeapMemory_Function();
        if(!pFN_ReduceHeapMemory)
            break;
        if(!pFN_ReduceHeapMemory())
            break;
    }
    if(!bSucceed_WritedDump)
    {
        _LOG_SYSTEM__WITH__TRACE(FALSE, _T("Failed to Write Dump File: %s (Error: %0X)")
            , _THIS->m_szWriteFolder, ::GetLastError());

        _LOCAL_MACRO_RETURN_FAIL;
    }
    //----------------------------------------------------------------
    _LOG_SYSTEM__WITH__TRACE(TRUE, _T("---- WriteMiniDump Succeed ----"));

    // 후처리
    // 사용자 정의 에러 리포트 : 완성된 덤프파일을 전송하는등의 일을 처리한다.
    LPFUNCTION_ERROR_REPORT pFN_ErrorReport = _THIS->_Get_ErrorReport_Function();
    if(pFN_ErrorReport)
        pFN_ErrorReport(_szPath);

    if(pExParam->ExceptionPointers->ExceptionRecord->ExceptionCode == NOERROR &&
        !(pExParam->ExceptionPointers->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE))
        return static_cast<unsigned>(EXCEPTION_CONTINUE_EXECUTION);
    else
        return EXCEPTION_EXECUTE_HANDLER;
}

unsigned __stdcall CMiniDump::CallbackThread(void* pParam)
{
    CMiniDump* _THIS = static_cast<CMiniDump*>(pParam);
    for(;;)
    {
        ::WaitForSingleObject(_THIS->m_hEventDump, INFINITE);
        if(_THIS->m_signal.m_bCMD_ExitThread)
        {
            ::SetEvent(_THIS->m_hEventDump);
            break;
        }

        _THIS->m_signal.m_Exception_Continue__Code = CMiniDump::WriteDump(_THIS->m_signal.m_pExParam);
        ::SetEvent(_THIS->m_hEventDump);

        if(EXCEPTION_CONTINUE_EXECUTION != _THIS->m_signal.m_Exception_Continue__Code)
            break;
    }
    _THIS->m_hCallbackThread = NULL;
    return 0;
}

// Exception Filter
LONG __stdcall CMiniDump::ExceptionFilter(PEXCEPTION_POINTERS exPtrs)
{
    // 리턴값 정책:
    // 덤프기록 성공의 경우
    //      EXCEPTION_EXECUTE_HANDLER
    // 실패의 경우
    //      EXCEPTION_CONTINUE_SEARCH

    CMiniDump* _THIS = CSingleton<CMiniDump>::Get_Instance();
    if(!_THIS)
        return EXCEPTION_CONTINUE_SEARCH;

    // 주소 유효성 검사
    BOOL bBadPtr_ExceptionPtr = ::IsBadReadPtr(exPtrs, sizeof(EXCEPTION_POINTERS));

    // Out Of Memory의 경우 미리 확보한 메모리를 해제한다.
    // 덤프기록에 필요한 메모리
    if(!bBadPtr_ExceptionPtr && exPtrs->ExceptionRecord->ExceptionCode == E_OUTOFMEMORY)
    {
        _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);

        auto pFN_REduceHeapMemory = _THIS->_Get_ReduceHeapMemory_Function();
        if(pFN_REduceHeapMemory)
            pFN_REduceHeapMemory();
    }

    // 예외포인터가 무효하다면 기타 정보만을 기록
    if(bBadPtr_ExceptionPtr)
    {
        _SAFE_LOCK_SPINLOCK(&_THIS->m_Lock_ThisOBJ, FALSE);

        _LOG_SYSTEM(_T("---- WriteMiniDump Failed: Bad Exception Pointers ----"));
        _THIS->WriteLog(exPtrs);

        // 후처리
        // 사용자 정의 에러 리포트 : 완성된 덤프파일을 전송하는등의 일을 처리한다.
        LPFUNCTION_ERROR_REPORT Function = _THIS->_Get_ErrorReport_Function();
        if(Function)
            Function(NULL);

        return EXCEPTION_CONTINUE_SEARCH;
    }

    // 예외 정보를 현재 스레드 기준으로
    MINIDUMP_EXCEPTION_INFORMATION ExParam;
    ExParam.ThreadId            = ::GetCurrentThreadId();
    ExParam.ExceptionPointers   = reinterpret_cast<PEXCEPTION_POINTERS>(exPtrs);
    ExParam.ClientPointers      = FALSE;


    LONG _ReturnValue = EXCEPTION_CONTINUE_SEARCH;
    _THIS->m_Lock_Request_Dump.Lock();
    if(_THIS->m_bInstalled)
    {
        _THIS->m_signal.m_pExParam = &ExParam;
        ::SetEvent(_THIS->m_hEventDump);
        ::WaitForSingleObject(_THIS->m_hEventDump, INFINITE);
        _ReturnValue = _THIS->m_signal.m_Exception_Continue__Code;
    }
    _THIS->m_Lock_Request_Dump.UnLock();

    return _ReturnValue;
}

BOOL CMiniDump::_Uninstall()
{
    if(!m_bInstalled)
        return FALSE;

    _SAFE_LOCK_SPINLOCK(&m_Lock_ThisOBJ, FALSE);

    if(m_hCallbackThread)
    {
        m_signal.m_bCMD_ExitThread = TRUE;
        ::SetEvent(m_hEventDump);
        ::WaitForSingleObject(m_hCallbackThread, INFINITE);
    }
    _SAFE_CLOSE_HANDLE(m_hEventDump);

    if(m_pPrev_Filter)
        SetUnhandledExceptionFilter(m_pPrev_Filter);
    m_pPrev_Filter            = NULL;
    m_pFN_ErrorReportFunction = NULL;
    m_DumpLevel               = MiniDumpNormal;

    m_bInstalled = FALSE;
    return TRUE;
}
BOOL CMiniDump::Connect_dbghelp()
{
    m_hDLL_dbghelp = LoadDebugHelpLibrary();
    if(!m_hDLL_dbghelp)
    {
        MessageBox(NULL, _T("file not found : dbghelp.dll"), _T("Error"), MB_ICONERROR);
        return FALSE;
    }

    m_pFN_WriteDump = (LPFUNCTION_MINIDUMPWRITEDUMP)::GetProcAddress(m_hDLL_dbghelp, "MiniDumpWriteDump");
    if(!m_pFN_WriteDump)
    {
        MessageBox(NULL, _T("Function not found : MiniDumpWriteDump"), _T("Error"), MB_ICONERROR);
        ::FreeLibrary(m_hDLL_dbghelp);
        return FALSE;
    }

    m_bConnected_dbghelp = TRUE;
    return TRUE;
}
void CMiniDump::Disconnect_dbghelp()
{
    if(m_hDLL_dbghelp)
    {
        ::FreeLibrary(m_hDLL_dbghelp);
        m_hDLL_dbghelp = NULL;
    }
    m_pFN_WriteDump = NULL;

    m_bConnected_dbghelp = FALSE;
}

// 덤프기록 경로 변경
BOOL CMiniDump::_Change_WriteFolder(LPCTSTR _szWriteFolder)
{
    _tcscpy_s(m_szWriteFolder, _szWriteFolder);

    // 마지막문자가 \ / 라면 제거
    LPTSTR p = m_szWriteFolder;
    while(*p)p++;
    if(p != m_szWriteFolder)
    {
        p--;
        if(*p == _T('\\') || *p == _T('/'))
            *p = _T('\0');
    }
    return TRUE;
}
// 덤프 레벨 변경
BOOL CMiniDump::_Change_DumpLevel(MINIDUMP_TYPE _DumpLevel)
{
    // 배포버전에서 DumpLevel이 MiniDumpNormal가 아닌경우를 막기위함
#if !defined(_DEBUG)
    if(_DumpLevel != MiniDumpNormal)
    {
        MessageBox(NULL, _T("DumpLevel is not MiniDumpNormal"), _T("Warning"), MB_ICONWARNING);
    }
#endif
    m_DumpLevel = _DumpLevel;
    return TRUE;
}
// 예외 보고함수 변경
BOOL CMiniDump::_Change_ErrorReport_Function(LPFUNCTION_ERROR_REPORT _ErrorReportFunction)
{
    m_pFN_ErrorReportFunction = _ErrorReportFunction;
    return TRUE;
}
// Get: 예외 보고함수
CMiniDump::LPFUNCTION_ERROR_REPORT CMiniDump::_Get_ErrorReport_Function()
{
    return m_pFN_ErrorReportFunction;
}
// 힙메모리 정리함수 변경
BOOL CMiniDump::_Change_ReduceHeapMemory_Function(LPFUNCTION_Request_Reduce_HeapMemory _ReduceHeapMemory_Function)
{
    m_pFN_Reduce_VirtualMemory = _ReduceHeapMemory_Function;
    return TRUE;
}
// Get: 힙메모리 정리함수
CMiniDump::LPFUNCTION_Request_Reduce_HeapMemory CMiniDump::_Get_ReduceHeapMemory_Function()
{
    return m_pFN_Reduce_VirtualMemory;
}

HMODULE CMiniDump::LoadDebugHelpLibrary()
{
    HMODULE hDLL;
    // #1 실행 파일의 디렉토리 우선
    TCHAR _szDbgHelpPath[MAX_PATH];
    if(::GetModuleFileName(NULL, _szDbgHelpPath, MAX_PATH))
    {
        ATLPath::RemoveFileSpec(_szDbgHelpPath);
        ATLPath::Append(_szDbgHelpPath, _T("dbghelp.dll"));

        hDLL = ::LoadLibrary(_szDbgHelpPath);
        if(hDLL)
            return hDLL;
    }

    // #2 시스템 디렉토리에서 찾는다.
    return ::LoadLibrary(_T("dbghelp.dll"));
}

HANDLE CMiniDump::CreateDumpFile(LPTSTR _out_FileName, size_t _len)
{
    if(!m_pFN_WriteDump)
    {
        _out_FileName[0] = NULL;
        return INVALID_HANDLE_VALUE;
    }
#pragma warning(push)
#pragma warning(disable: 6031)
    // _tmkdir 성공시 0
    // 그러나 이미 존재하는 폴더라면 -1을 리턴한다


    // 디렉토리 생성
    _tmkdir(m_szWriteFolder);
    if(!ATLPath::IsDirectory(m_szWriteFolder))
    {
        // 무효한 경로라면 현재 경로에 DUMP 폴더를 만들어 사용한다
        ::GetModuleFileName(NULL, m_szWriteFolder, MAX_PATH);
        ATLPath::RemoveFileSpec(m_szWriteFolder);
        ATLPath::Append(m_szWriteFolder, _T("DUMP"));
        _tmkdir(m_szWriteFolder);
    }
#pragma warning(pop)

    // 폴더에 이전 덤프들이 n개 이상이라면
    // 그중 가장 빠른 것을 삭제한다
    // 작업중

    // 파일명은 현재 시스템 시간을 이용하도록 하자
    SYSTEMTIME st;
    GetLocalTime(&st);
    _stprintf_s(_out_FileName, _len, _T("%s\\%d_%02d_%02d__%02d%02d%02d.dmp"), m_szWriteFolder
        , st.wYear, st.wMonth, st.wDay
        , st.wHour, st.wMinute, st.wSecond);
    return ::CreateFile(_out_FileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}


void CMiniDump::WriteLog(PEXCEPTION_POINTERS exPtrs)
{
    WriteLog__FaultReason(exPtrs);  // LOG: 예외 원인을 기록한다.
    WriteLog__Os();                 // LOG: Os정보를 기록한다.
    WriteLog__CpuInfo();            // LOG: Cpu 정보를 기록한다.
    WriteLog__ProcessInfo();        // LOG 프로세스 정보를 기록한다.
    WriteLog__MemoryInfo();         // LOG: 메모리 정보를 기록한다.
}
// Get: 예외코드의 이름
LPCTSTR CMiniDump::Get_ExceptionName(DWORD dwExceptionCode)
{
    switch(dwExceptionCode)
    {
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_ACCESS_VIOLATION)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_DATATYPE_MISALIGNMENT)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_BREAKPOINT)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_SINGLE_STEP)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_FLT_DENORMAL_OPERAND)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_FLT_DIVIDE_BY_ZERO)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_FLT_INEXACT_RESULT)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_FLT_INVALID_OPERATION)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_FLT_OVERFLOW)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_FLT_STACK_CHECK)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_FLT_UNDERFLOW)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_INT_DIVIDE_BY_ZERO)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_INT_OVERFLOW)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_PRIV_INSTRUCTION)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_IN_PAGE_ERROR)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_ILLEGAL_INSTRUCTION)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_NONCONTINUABLE_EXCEPTION)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_STACK_OVERFLOW)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_INVALID_DISPOSITION)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_GUARD_PAGE)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(EXCEPTION_INVALID_HANDLE)
    _DEF_LOCAL_STRING_RETURN__BY_CASE(CONTROL_C_EXIT)
    default:
        return NULL;
        break;
    }
}

// LOG: 예외 원인을 기록한다.
void CMiniDump::WriteLog__FaultReason(PEXCEPTION_POINTERS exPtrs)
{
    _LOG_SYSTEM(_T("---- FaultReason ----"));
    
    if(!exPtrs)
    {
        _LOG_SYSTEM(_T("PEXCEPTION_POINTERS is NULL"));
        return;
    }
    
    LPCTSTR pszErrorName = Get_ExceptionName(exPtrs->ExceptionRecord->ExceptionCode);
    if(pszErrorName)
    {
        _LOG_SYSTEM(_T("Exception Code(0x%X): %s")
            , exPtrs->ExceptionRecord->ExceptionCode
            , pszErrorName );
    }
    else
    {
        TCHAR szErrorName[2048];
        if( !::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, exPtrs->ExceptionRecord->ExceptionCode, 0, szErrorName, 2048, NULL) )
        {
            _tcscpy_s(szErrorName, _T("Unknown"));
        }

        _LOG_SYSTEM(_T("Exception Code(0x%X): %s")
            , exPtrs->ExceptionRecord->ExceptionCode
            , szErrorName );
    }
}

// LOG: Os정보를 기록한다.
void CMiniDump::WriteLog__Os()
{
    if(!UTIL::g_pSystem_Information)
        return;

    _LOG_SYSTEM(_T("---- Os Information ----\nOS: %s"), UTIL::g_pSystem_Information->mFN_Get_OsName());
    auto osv = *UTIL::g_pSystem_Information->mFN_Get_OsInfo();
    _LOG_SYSTEM(_T("OS Version(%u.%u) ServicePack(%hu.%hu) Build(%u)")
        , osv.dwMajorVersion, osv.dwMinorVersion
        , osv.wServicePackMajor, osv.wServicePackMinor
        , osv.dwBuildNumber);
}

// LOG: Cpu 정보를 기록한다.
void CMiniDump::WriteLog__CpuInfo()
{
    if(!UTIL::g_pSystem_Information)
        return;

    const UTIL::ISystem_Information& Info = *UTIL::g_pSystem_Information;
    const UTIL::TCpu_InstructionSet* pCpu = Info.mFN_Get_CPU_InstructionSet();
    if(!pCpu)
        return;

    _LOG_SYSTEM(_T("---- Cpu Information ----"));
    if(*pCpu->NameVendor())
        _LOG_SYSTEM("CPU Vender : %s", pCpu->NameVendor());
    if(*pCpu->NameBrand())
        _LOG_SYSTEM("CPU Brand  : %s", pCpu->NameBrand());
    _LOG_SYSTEM(_T("Processor Type: %u"), Info.mFN_Get_ProcessorType());
    _LOG_SYSTEM(_T("Logical  Core : %u"), Info.mFN_Get_NumProcessor_Logical());
    _LOG_SYSTEM(_T("Physical Core : %u"), Info.mFN_Get_NumProcessor_PhysicalCore());
}

// LOG 프로세스 정보를 기록한다.
void CMiniDump::WriteLog__ProcessInfo()
{
#if __X86
    LPCTSTR c_szMode = _T("x86");
#elif __X64
    LPCTSTR c_szMode = _T("x64");
#else
    // ...
#endif

    _LOG_SYSTEM(_T("---- Process Information ----"));
    _LOG_SYSTEM(_T("Mode : %s"), c_szMode);
    const auto ProcessHandle = ::GetCurrentProcess();
    const auto ProcessID = :: GetCurrentProcessId();
    const auto ThreadHandle = ::GetCurrentThread();
    const auto ThreadID = ::GetCurrentThreadId();
    _DEF_COMPILE_MSG__FOCUS("작업중...");
}

// LOG: 메모리 정보를 기록한다.
void CMiniDump::WriteLog__MemoryInfo()
{
#pragma warning(push)
#pragma warning(disable: 28159)
    _LOG_SYSTEM(_T("---- System Memory Information ----"));
    MEMORYSTATUS MemInfo;
    MemInfo.dwLength = sizeof(MemInfo);
    ::GlobalMemoryStatus(&MemInfo);
  
    _LOG_SYSTEM(_T("MemoryLoad              : %10u %%"), MemInfo.dwMemoryLoad);
    _LOG_SYSTEM(_T("Physical Memory(Total)  : %10Iu KB"), MemInfo.dwTotalPhys / 1024);
    _LOG_SYSTEM(_T("Physical Memory(Free)   : %10Iu KB"), MemInfo.dwAvailPhys / 1024);
    _LOG_SYSTEM(_T("Paging File(Total)      : %10Iu KB"), MemInfo.dwTotalPageFile / 1024);
    _LOG_SYSTEM(_T("Paging File(Free)       : %10Iu KB"), MemInfo.dwAvailPageFile / 1024);
    _LOG_SYSTEM(_T("Virtual Memory(Total)   : %10Iu KB"), MemInfo.dwTotalVirtual / 1024);
    _LOG_SYSTEM(_T("Virtual Memory(Free)    : %10Iu KB"), MemInfo.dwAvailVirtual / 1024);
#pragma warning(pop)
}