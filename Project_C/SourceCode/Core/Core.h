#pragma once
#include "./Core_Interface.h"



namespace CORE{
    class CCore : public ICore{
    public:
        CCore();
        ~CCore();

    public:
        virtual UTIL::TUTIL* mFN_Get_Util() override;
        virtual UTIL::MEM::IMemoryPool_Manager* mFN_Get_MemoryPoolManager() override;
        virtual const UTIL::ISystem_Information* CCore::mFN_Get_System_Information() override;

        virtual void mFN_Set_LogWriterInstance__System(::UTIL::ILogWriter* p) override;
        virtual void mFN_Set_LogWriterInstance__Debug(::UTIL::ILogWriter* p) override;

        virtual const ::UTIL::TSystemTime* mFN_Query_CompileTime() override;
        virtual void mFN_Write_CoreInformation(::UTIL::ILogWriter* pLog) override;

    public:
        UINT32 mFN_Query_MemoryPool_Version_Major() const;
        UINT32 mFN_Query_MemoryPool_Version_Minor() const;
        void mFN_Set_ModuleName(HMODULE hModule);
        void mFN_Set_OnOff_IsLimitedVersion_Messagebox(BOOL _ON);

    private:
        ::UTIL::TSystemTime     m_time_Compile;

        TCHAR m_szCurrentDirectory[MAX_PATH];
        TCHAR m_szCoreDLL_FilePath[MAX_PATH];
        TCHAR m_szExecute_FilePath[MAX_PATH];
        LPCTSTR m_pszCoreDLL_FileName;
        LPCTSTR m_pszExecute_FileName;
    };
}