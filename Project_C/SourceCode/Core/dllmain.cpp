// dllmain.cpp : DLL 응용 프로그램의 진입점을 정의합니다.
#include "stdafx.h"

#include "Core.h"

#if _DEF_MEMORYPOOL_MAJORVERSION > 1
#include "./UTIL/MemoryPool_Interface.h"
#include "./UTIL//MemoryPool_v2/MemoryPool_v2.h"
#endif


BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(lpReserved);

    switch(ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ::CORE::g_instance_CORE.mFN_Set_ModuleName(hModule);
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH:
        break;
    }

#if _DEF_MEMORYPOOL_MAJORVERSION > 1
    if(FALSE == UTIL::MEM::CMemoryPool_Manager::sFN_Callback_from_DllMain(hModule, ul_reason_for_call, lpReserved))
        return FALSE;
#endif

    return TRUE;
}