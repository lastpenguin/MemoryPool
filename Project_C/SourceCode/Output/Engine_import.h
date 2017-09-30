#include "Engine_include.h"
#pragma message("---- Import : ENGINE LIB ----")
// 이 파일은 메인 cpp에서 1회만 포함 되어야 합니다


#if __X64
    #ifdef _DEBUG
        #pragma comment(lib, "Engine_x64D.lib")
    #else
        #pragma comment(lib, "Engine_x64.lib")
    #endif
#elif __X86
    #ifdef _DEBUG
        #pragma comment(lib, "Engine_x86D.lib")
    #else
        #pragma comment(lib, "Engine_x86.lib")
    #endif
#else
    #error
#endif