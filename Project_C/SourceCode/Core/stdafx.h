// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 및 프로젝트 관련 포함 파일이
// 들어 있는 포함 파일입니다.
//

#pragma once
#pragma warning(disable: 6262)
#include "targetver.h"

typedef unsigned char byte;

#if _M_AMD64 || _M_X64
    #define __X86   0
    #define __X64   1
#elif _M_IX86
    #define __X86   1
    #define __X64   0
#else
    #error
#endif



#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용은 Windows 헤더에서 제외합니다.
// 기본 헤더
#include <windows.h>
#include <process.h>
#include <intrin.h>
#include <tchar.h>
#include <cstdio>
#include <malloc.h>
#include <assert.h>
#include <atomic>

#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <bitset>

//----------------------------------------------------------------
#if defined(CORE_EXPORTS) && defined(__cplusplus)
#define CORE_API  extern "C" __declspec(dllexport)
#elif defined(CORE_EXPORTS)
#define CORE_API  __declspec(dllexport)
#else
#define CORE_API  __declspec(dllimport)
#endif
//----------------------------------------------------------------
// 1~2
#define _DEF_MEMORYPOOL_MAJORVERSION 2
//----------------------------------------------------------------
// 사용자 헤더
#ifdef CORE_EXPORTS
#include "./CorePrivate.h"
#endif

#include "./UTIL/Macro.h"