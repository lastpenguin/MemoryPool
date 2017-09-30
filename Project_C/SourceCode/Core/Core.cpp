#include "stdafx.h"
#include "./Core.h"

#include <atlpath.h>

#include "./UTIL/MemoryPool.h"
#include "./UTIL/System_Information.h"

#include "./Compile_Log.txt"



namespace CORE{
    namespace{
    #ifdef _DEBUG
        const BOOL gc_isLimitedVersion__Default = FALSE;
    #else
        const BOOL gc_isLimitedVersion__Default = TRUE;
    #endif
        BOOL g_isLimitedVersion = gc_isLimitedVersion__Default;
        
        HANDLE g_hMutex__Identify_ThisDLL = nullptr;
        BOOL   g_bThisDLL_is_First = TRUE;
    }
    
    CORE_API ICore* GetCore()
    {
        return &g_instance_CORE;
    }


    // 가상함수를 거치지 않는 의도로 사용하려 했지만
    // 인라인화가 되지 않는 이 함수가 중간에 끼어들어 오히려 명령어(어셈블리기준)가 더 많아진다
    #if _DEF_USING_DEBUG_MEMORY_LEAK
    CORE_API void* CoreMemAlloc(size_t size, const char* _FileName, int _Line)
    {
        return ((UTIL::MEM::CMemoryPool_Manager*)::UTIL::g_pMem)->mFN_Get_Memory(size, _FileName, _Line);
    }
    #else
    CORE_API void* CoreMemAlloc(size_t size)
    {
        return ((UTIL::MEM::CMemoryPool_Manager*)::UTIL::g_pMem)->mFN_Get_Memory(size);
    }
    #endif
    CORE_API void CoreMemFree(void* p)
    {
        ((UTIL::MEM::CMemoryPool_Manager*)::UTIL::g_pMem)->mFN_Return_Memory(p);
    }
    CORE_API void CoreMemFreeQ(void* p)
    {
        ((UTIL::MEM::CMemoryPool_Manager*)::UTIL::g_pMem)->mFN_Return_MemoryQ(p);
    }



    CCore::CCore()
        : m_pszCoreDLL_FileName(nullptr)
        , m_pszExecute_FileName(nullptr)
    {
        m_szCurrentDirectory[0]= m_szCoreDLL_FilePath[0] = m_szExecute_FilePath[0] = _T('\0');
        m_time_Compile = g_instance_UTIL.mFN_GetCurrentProjectCompileTime(__DATE__, __TIME__);

        g_hMutex__Identify_ThisDLL = ::CreateMutex(nullptr, TRUE, _T("017ECBFD-8D62-40F1-B912-0B34358C5976"));
        if (ERROR_ALREADY_EXISTS == GetLastError())
            g_bThisDLL_is_First = FALSE;
    }

    CCore::~CCore()
    {
        _SAFE_CLOSE_HANDLE(g_hMutex__Identify_ThisDLL);

        if(g_isLimitedVersion && g_bThisDLL_is_First)
        {
            _MACRO_MESSAGEBOX__OK(_T("Beta Version..."),
                _T("Memory Pool Version(%u.%u)\n")
                _T("lastpenguin83@gmail.com")
                , mFN_Query_MemoryPool_Version_Major(), mFN_Query_MemoryPool_Version_Minor()
            );
        }
        else if(!g_isLimitedVersion && gc_isLimitedVersion__Default)
        {
        #ifdef _DEBUG
            // 디버그 버전은 비공개이기 때문에 처리할 것은 없다
        #else
            // TODO: 사용자 파일 변조발견, 처리
                //----------------------------------------------------------------
                // 차후 수정
                //      이 로직은 위, 안내 메세지와 근접하지 않아야 하며
                //      디스어셈블리리 실행가능성을 분석하기 여렵게 해야 한다
        #endif
        }
    }

    UTIL::TUTIL* CCore::mFN_Get_Util()
    {
        return &g_instance_UTIL;
    }
    UTIL::MEM::IMemoryPool_Manager* CCore::mFN_Get_MemoryPoolManager()
    {
        return CSingleton<UTIL::MEM::CMemoryPool_Manager>::Get_Instance();
    }
    const UTIL::ISystem_Information* CCore::mFN_Get_System_Information()
    {
        return CSingleton<UTIL::CSystem_Information>::Get_Instance();
    }


    void CCore::mFN_Set_LogWriterInstance__System(::UTIL::ILogWriter* p)
    {
        ::UTIL::g_pLogSystem = p;
    }
    void CCore::mFN_Set_LogWriterInstance__Debug(::UTIL::ILogWriter* p)
    {
        ::UTIL::g_pLogDebug = p;
    }

    const ::UTIL::TSystemTime* CCore::mFN_Query_CompileTime()
    {
        if(-1 == m_time_Compile.Year)
            return nullptr;
        return &m_time_Compile;
    }
    void CCore::mFN_Write_CoreInformation(::UTIL::ILogWriter* pLog)
    {
        if(!pLog)
            return;
        // 컴파일 시기
        auto pT = mFN_Query_CompileTime();
        if(pT)
        {
            auto t = *pT;
            ::UTIL::TLogWriter_Control::sFN_WriteLog(pLog, FALSE,
                _T("Compile Time : %d-%02d-%02d  %02d:%02d:%02d (Core DLL)")
                , t.Year
                , t.Month
                , t.Day
                , t.Hour
                , t.Minute
                , t.Second);
        }
        // 메모리풀 버전
        ::UTIL::TLogWriter_Control::sFN_WriteLog(pLog, FALSE, _T("\tMemory Pool Version(%u.%u)")
            , mFN_Query_MemoryPool_Version_Major(), mFN_Query_MemoryPool_Version_Minor());
        // 실행 디렉토리
        ::UTIL::TLogWriter_Control::sFN_WriteLog(pLog, FALSE,
            _T("\tCurrent Directory\n")
            _T("\t\t%s"), m_szCurrentDirectory);
        // DLL 경로
        if(m_pszCoreDLL_FileName)
            ::UTIL::TLogWriter_Control::sFN_WriteLog(pLog, FALSE,
                _T("\tCore DLL File Path\n")
                _T("\t\t%s"), m_szCoreDLL_FilePath);
        // EXE 파일 경로
        if(m_pszExecute_FileName)
            ::UTIL::TLogWriter_Control::sFN_WriteLog(pLog, FALSE,
                _T("\tExecute File Path\n")
                _T("\t\t%s"), m_szExecute_FilePath);
    }
    UINT32 CCore::mFN_Query_MemoryPool_Version_Major() const
    {
    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        return ::UTIL::MEM::GLOBAL::VERSION::gc_Version_Major;
    #else
        return 1u;
    #endif
    }
    UINT32 CCore::mFN_Query_MemoryPool_Version_Minor() const
    {
    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        return ::UTIL::MEM::GLOBAL::VERSION::gc_Version_Minor;
    #else
        return 0u;
    #endif
    }
    void CCore::mFN_Set_ModuleName(HMODULE hModule)
    {
        ::GetCurrentDirectory(_MACRO_ARRAY_COUNT(m_szCurrentDirectory), m_szCurrentDirectory);

        if(::GetModuleFileName(hModule, m_szCoreDLL_FilePath, _MACRO_ARRAY_COUNT(m_szCoreDLL_FilePath)))
            m_pszCoreDLL_FileName = ATLPath::FindFileName(m_szCoreDLL_FilePath);
        if(::GetModuleFileName(nullptr, m_szExecute_FilePath, _MACRO_ARRAY_COUNT(m_szExecute_FilePath)))
            m_pszExecute_FileName = ATLPath::FindFileName(m_szExecute_FilePath);
    }
    void CCore::mFN_Set_OnOff_IsLimitedVersion_Messagebox(BOOL _ON)
    {
        g_isLimitedVersion = _ON;
    }
}
