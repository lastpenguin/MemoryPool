#pragma once
namespace CORE{
    class CCore;
}
namespace UTIL{
    struct TUTIL;
    namespace MEM{
        __interface IMemoryPool_Manager;
    }
    __interface ILogWriter;
}



namespace CORE{
    extern CCore g_instance_CORE;
    extern UTIL::TUTIL g_instance_UTIL;

    namespace SETTING{
    #if _DEF_MEMORYPOOL_MAJORVERSION == 1
        extern const size_t gc_MemoryPool_minUnits_per_Alloc;
        extern const size_t gc_MemoryPool_maxAllocSize_per_Alloc;
    #endif
    }
}

namespace UTIL{
    extern TUTIL* g_pUtil;
    extern MEM::IMemoryPool_Manager* g_pMem;

    extern ILogWriter*    g_pLogSystem;
    extern ILogWriter*    g_pLogDebug;

    namespace SETTING{
    #ifndef _DEF__LEN__TEMP_STRING_BUFFER
    #define _DEF__LEN__TEMP_STRING_BUFFER
        static const size_t gc_LenTempStrBuffer = 64 * 1024; // 64KB
    #endif
    }
}

#ifndef _LOG_SYSTEM_INSTANCE_POINTER
#define _LOG_SYSTEM_INSTANCE_POINTER (::UTIL::g_pLogSystem)
#endif
#ifndef _LOG_DEBUG__INSTANCE_POINTER
#define _LOG_DEBUG__INSTANCE_POINTER (::UTIL::g_pLogDebug)
#endif