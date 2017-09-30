#pragma once
#include "./UTIL/MemoryPool_Interface.h"
#include "./UTIL/System_Information_Interface.h"
#include "./UTIL/LogWriter_Interface.h"


namespace CORE{
    __interface ICore;
    __forceinline ICore* gFN_Get_CORE(HMODULE hDLL)
    {
        if(!hDLL) return NULL;
        typedef ICore* (*FunctionPTR_Get_CORE)(void);
        FunctionPTR_Get_CORE pFN = reinterpret_cast<FunctionPTR_Get_CORE>(::GetProcAddress(hDLL, "GetCore"));
        if(!pFN) return NULL;
        return pFN();
    }
}

namespace CORE{
    __interface ICore{
        UTIL::TUTIL* mFN_Get_Util();
        UTIL::MEM::IMemoryPool_Manager* mFN_Get_MemoryPoolManager();

        const UTIL::ISystem_Information* mFN_Get_System_Information();


        void mFN_Set_LogWriterInstance__System(::UTIL::ILogWriter* p);
        void mFN_Set_LogWriterInstance__Debug(::UTIL::ILogWriter* p);

        const ::UTIL::TSystemTime* mFN_Query_CompileTime();
        void mFN_Write_CoreInformation(::UTIL::ILogWriter* p);
    };
}