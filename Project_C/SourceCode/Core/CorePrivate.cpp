#include "stdafx.h"
#include "./CorePrivate.h"

#include "./Core.h"
namespace CORE{
    CCore g_instance_CORE;
    UTIL::TUTIL g_instance_UTIL;

    namespace SETTING{
    #if _DEF_MEMORYPOOL_MAJORVERSION == 1
        const size_t gc_MemoryPool_minUnits_per_Alloc = 4;
        const size_t gc_MemoryPool_maxAllocSize_per_Alloc = 1024*1024*256; // 256MB
    #endif
    }
}
namespace UTIL{
    TUTIL* g_pUtil                   = &CORE::g_instance_UTIL;
    MEM::IMemoryPool_Manager* g_pMem = nullptr;

    ILogWriter* g_pLogSystem    = nullptr;
    ILogWriter* g_pLogDebug     = nullptr;
}