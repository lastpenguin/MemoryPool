#pragma once

namespace ENGINE{

    class _DEF_CACHE_ALIGN CEngine_Basis{
    public:
        typedef BOOL(__stdcall *LPFUNCTION__Delete_AnythingHeapMemory)(void); // 힙메몰 

    protected:
        // _RootPath_is_CurrentModuleDirectory
        //      ROOT PATH
        //          TRUE 실행파일위치
        //          FALSE 실행위치
        CEngine_Basis(BOOL _RootPath_is_CurrentModuleDirectory, LPFUNCTION__Delete_AnythingHeapMemory pFN_Delete_AnythingHeapMemory);
        ~CEngine_Basis();

    protected:
        UTIL::CLogWriter    m_LogWriter_System;
        UTIL::CLogWriter    m_LogWriter_Debug;

        LPCTSTR m_szName_DumpFolder;
        LPCTSTR m_szName_CoreDLL;
    };

    class _DEF_CACHE_ALIGN CEngine : public CEngine_Basis{
    public:
        CEngine(BOOL _RootPath_is_CurrentModuleDirectory);
        virtual ~CEngine();

        inline const ::UTIL::TSystemTime* mFN_Query_CompileTime() const { if(-1 == m_time_Compile.Year)return nullptr; return &m_time_Compile; }
        inline UINT64 mFN_Query_FailedCode__Initialize_Engine() const { return m_stats_Engine_Initialized.m_Failed_Something; }
        inline BOOL mFN_Query_Initialized_Engine() const { return 0 == mFN_Query_FailedCode__Initialize_Engine(); }

    protected:
        virtual BOOL mFN_Initialize_Engine();


        virtual void mFN_Write_EngineInformation(::UTIL::ILogWriter* p);
        virtual BOOL mFN_Test_System_Intrinsics();  // 시스템에서 사용한 내장함수의 사용가능을 테스트
        virtual BOOL mFN_Test_System_OS();
        virtual BOOL mFN_Test_System_Memory();
        virtual BOOL mFN_Initialize_MemoryPool();   // 메모리 미리 예약

    protected:
        ::UTIL::TSystemTime     m_time_Compile;

        // Initialized stats
        #pragma pack(push, 1)
        union TStats_Failed{
            TStats_Failed() : m_Failed_Something(0){m_FailedCode._NotDoneYetInitialize = 1;}
            struct{
                // 추가되는 옵션은 추가 순서를 지켜야 한다
                bool _NotDoneYetInitialize : 1;
                bool _ConnectCore : 1;
                bool _Intrinsics : 1;
                bool _OS : 1;
                bool _SystemMemory : 1;
                bool _InitializeMemoryPool : 1;
            }m_FailedCode;
            UINT64 m_Failed_Something;
        }m_stats_Engine_Initialized;
        #pragma pack(pop)
    };

}