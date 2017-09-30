#pragma once
///*----------------------------------------------------------------
///       D3D Device 임계구역에 대하여 Flip스레드가 동작중일때 접근하지 못하도록 합니다.
//----------------------------------------------------------------*/
//// TODO: Wait For Render Handle(디바이스 동기화 함수)
//#if defined(_DEF_USING_SAFE_FLIP_THREAD)
//    #define _WAIT_RENDER_HANDLE()                                       \
//    {                                                                   \
//        /*if(ENGINE::DATA::g_pDXManager)*/                              \
//            ENGINE::DATA::g_pDXManager->mFN_Wait_Handle__Render();      \
//    }
//#else
//    #define _WAIT_RENDER_HANDLE()   __noop
//#endif

namespace UTIL{
}