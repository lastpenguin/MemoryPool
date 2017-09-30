#pragma once
#define _INCLUDED__CORE_LIBRARY__B25FF1F6_CC98_405C_9F8D_8827822552B7

#pragma region 전방선언
namespace CORE{
    __interface ICore;

}
namespace UTIL{
    __interface ISystem_Information;
    struct TUTIL;

    namespace MEM{
        __interface IMemoryPool_Manager;
    }
}
#pragma endregion

#pragma region 설정
namespace UTIL{
    namespace SETTING{
    #ifndef _DEF__LEN__TEMP_STRING_BUFFER
    #define _DEF__LEN__TEMP_STRING_BUFFER
        static const size_t gc_LenTempStrBuffer = 64 * 1024;
    #endif
    }
}
#pragma endregion



#include "../Core/stdafx.h"
#include "../Core/Core_Interface.h"


/*----------------------------------------------------------------
/   전역데이터
----------------------------------------------------------------*/
namespace CORE{
    extern HMODULE g_hDLL_Core;
    extern ICore* g_pCore;
}
namespace UTIL{
    extern const ISystem_Information* g_pSystem_Information;

    extern TUTIL* g_pUtil;
    extern MEM::IMemoryPool_Manager* g_pMem;
}

/*----------------------------------------------------------------
/   사용자 함수
----------------------------------------------------------------*/
namespace CORE{
    void Disconnect_CORE();
}