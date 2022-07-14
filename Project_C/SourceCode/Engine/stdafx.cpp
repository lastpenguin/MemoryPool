// stdafx.cpp : 표준 포함 파일만 들어 있는 소스 파일입니다.
// Engine.pch는 미리 컴파일된 헤더가 됩니다.
// stdafx.obj에는 미리 컴파일된 형식 정보가 포함됩니다.
#pragma comment(lib, "winmm.lib")

#include "stdafx.h"



//// Direct X
//#if __X64
//    #ifdef _DEBUG
//        #pragma comment(lib, "../DirectX/Lib/x64/d3dx11d.lib")
//    #else
//        #pragma comment(lib, "../DirectX/Lib/x64/d3dx11.lib")
//    #endif
//
//        #pragma comment(lib, "../DirectX/Lib/x64/d3d11.lib")
//        #pragma comment(lib, "../DirectX/Lib/x64/dinput8.lib")
//        #pragma comment(lib, "../DirectX/Lib/x64/dxguid.lib")
//        #pragma comment(lib, "../DirectX/Lib/x64/d3dcompiler.lib")
//        #pragma comment(lib, "../DirectX/Lib/x64/DxErr.lib")
//        #pragma comment(lib, "../DirectX/Lib/x64/dxgi.lib")
//
//#elif __X86
//    #ifdef _DEBUG
//        #pragma comment(lib, "../DirectX/Lib/x86/d3dx11d.lib")
//    #else
//        #pragma comment(lib, "../DirectX/Lib/x86/d3dx11.lib")
//    #endif
//
//        #pragma comment(lib, "../DirectX/Lib/x86/d3d11.lib")
//        #pragma comment(lib, "../DirectX/Lib/x86/dinput8.lib")
//        #pragma comment(lib, "../DirectX/Lib/x86/dxguid.lib")
//        #pragma comment(lib, "../DirectX/Lib/x86/d3dcompiler.lib")
//        #pragma comment(lib, "../DirectX/Lib/x86/DxErr.lib")
//        #pragma comment(lib, "../DirectX/Lib/x86/dxgi.lib")
//#else
//
//    #error
//#endif







namespace ENGINE{
    namespace DATA{
        HINSTANCE           g_hInstance = ::GetModuleHandle(NULL);

        ::UTIL::CLogWriter* g_pLog_System = nullptr;
        ::UTIL::CLogWriter* g_pLog_Debug  = nullptr;

        CEngine*            g_pEngine        = nullptr;
        CWindowManager*     g_pWindowManager = nullptr;
        CDX_Manager*        g_pDXManager     = nullptr;
    }
}
