// stdafx.h : ���� ��������� ���� ��������� �ʴ�
// ǥ�� �ý��� ���� ���� �� ������Ʈ ���� ���� ������
// ��� �ִ� ���� �����Դϴ�.
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // ���� ������ �ʴ� ������ Windows ������� �����մϴ�.


#ifndef _LOG_SYSTEM_INSTANCE_POINTER
#define _LOG_SYSTEM_INSTANCE_POINTER    (::ENGINE::DATA::g_pLog_System)
#endif
#ifndef _LOG_DEBUG__INSTANCE_POINTER
#define _LOG_DEBUG__INSTANCE_POINTER    (::ENGINE::DATA::g_pLog_Debug)
#endif


// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.
#include "../Output/Core_include.h"

#include "./UTIL/Macro_Engine.h"
#include "./UTIL/LogWriter.h"
#include "./UTIL/MiniDump.h"
//#include "./UTIL/StringPool.h"

#include "./UTIL/Timer.h"
#include "./UTIL/SimpleTimer.h"

//// Direct X
//#pragma warning(push)
//#pragma warning(disable: 4005)
//#include "../DirectX/Include/D3D11.h"
//#include "../DirectX/Include/D3DX11.h"
//
//#define DIRECTINPUT_VERSION 0x0800
//#include "../DirectX/Include/dinput.h"
//#pragma warning(pop)



namespace ENGINE{
    class CEngine;
    class CWindowManager;
    class CDX_Manager;

    namespace DATA{
        extern HINSTANCE            g_hInstance;
        extern ::UTIL::CLogWriter*  g_pLog_System;
        extern ::UTIL::CLogWriter*  g_pLog_Debug;

        extern CEngine*             g_pEngine;
        extern CWindowManager*      g_pWindowManager;
        extern CDX_Manager*         g_pDXManager;
    }
}