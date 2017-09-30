#include "stdafx.h"
#include "./Engine.h"

#include "../Output/Core_import.h"

#include "./Compile_Log.txt"



namespace ENGINE{
    CEngine_Basis::CEngine_Basis(BOOL _RootPath_is_CurrentModuleDirectory, LPFUNCTION__Delete_AnythingHeapMemory pFN_Delete_AnythingHeapMemory)
        : m_LogWriter_System(300, _T("log.txt"), _T("LOG"), ((_RootPath_is_CurrentModuleDirectory)? nullptr : _T("./")))
        , m_LogWriter_Debug(500, _T("log_debug.txt"), _T("LOG"), ((_RootPath_is_CurrentModuleDirectory)? nullptr : _T("./")))
        //, m_szName_CoreDLL(?)
        , m_szName_DumpFolder(_T("./DUMP"))
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();
        _AssertRelease(!DATA::g_pLog_System && !DATA::g_pLog_Debug);

        DATA::g_pLog_System = &m_LogWriter_System;
        DATA::g_pLog_Debug = &m_LogWriter_Debug;

        // Core DLL Name Set
        {
        #if __X64
            #ifdef _DEBUG
            m_szName_CoreDLL = _T("Core_x64D.dll");
            #else
            m_szName_CoreDLL = _T("Core_x64.dll");
            #endif
        #elif __X86
            #ifdef _DEBUG
            m_szName_CoreDLL = _T("Core_x86D.dll");
            #else
            m_szName_CoreDLL = _T("Core_x86.dll");
            #endif
        #else
            #error
        #endif
        }

        CMiniDump::Install(m_szName_DumpFolder, pFN_Delete_AnythingHeapMemory);
    }
    CEngine_Basis::~CEngine_Basis()
    {
        DATA::g_pLog_System = nullptr;
        DATA::g_pLog_Debug  = nullptr;

        // 종료전까지 최대한 유지한다
        //CMiniDump::Uninstall(); 
    }
}


namespace ENGINE{
    namespace{
        BOOL __stdcall gFN_ReduceHeapMemory()
        {
            if(!::UTIL::g_pMem)
                return FALSE;

        #if _DEF_MEMORYPOOL_MAJORVERSION > 1
            if(::UTIL::g_pMem->mFN_Reduce_AnythingHeapMemory())
                return TRUE;
        #endif

            return FALSE;
        }
    }
    CEngine::CEngine(BOOL _RootPath_is_CurrentModuleDirectory)
        : CEngine_Basis(_RootPath_is_CurrentModuleDirectory, gFN_ReduceHeapMemory)
    {
        mFN_Initialize_Engine();
        mFN_Write_EngineInformation(DATA::g_pLog_System);
    }
    CEngine::~CEngine()
    {
        CORE::Disconnect_CORE();

        //CSingleton<::UTIL::CStringPoolA>::Get_Instance()->DestroyPool();
        //CSingleton<::UTIL::CStringPoolW>::Get_Instance()->DestroyPool();
    }
    BOOL CEngine::mFN_Initialize_Engine()
    {
        BOOL bSUCCEED = FALSE;
        __try
        {
            #define _LOCAL_MACRO_FAIL(name) {m_stats_Engine_Initialized.m_FailedCode.##name = true; __leave;}

            if(!CORE::Connect_CORE(m_szName_CoreDLL, _LOG_SYSTEM_INSTANCE_POINTER, _LOG_DEBUG__INSTANCE_POINTER))
                _LOCAL_MACRO_FAIL(_ConnectCore);

            _AssertRelease(CORE::g_pCore && UTIL::g_pMem && UTIL::g_pUtil && UTIL::g_pSystem_Information);

            m_time_Compile = ::UTIL::g_pUtil->mFN_GetCurrentProjectCompileTime(__DATE__, __TIME__);

            if(!mFN_Test_System_Intrinsics())
                _LOCAL_MACRO_FAIL(_Intrinsics);

            if(!mFN_Test_System_OS())
                _LOCAL_MACRO_FAIL(_OS);

            if(!mFN_Test_System_Memory())
                _LOCAL_MACRO_FAIL(_SystemMemory);

            if(!mFN_Initialize_MemoryPool())
                _LOCAL_MACRO_FAIL(_InitializeMemoryPool);

            #undef _LOCAL_MACRO_FAIL
            bSUCCEED = TRUE;
        }
        __finally
        {
            m_stats_Engine_Initialized.m_FailedCode._NotDoneYetInitialize = false;
        }
        return bSUCCEED;
    }


}