#pragma once
// 메모리누수 디버깅
#if !defined(_DEF_USING_DEBUG_MEMORY_LEAK)
#if defined(_DEBUG)
#define _DEF_USING_DEBUG_MEMORY_LEAK    1
#else
#define _DEF_USING_DEBUG_MEMORY_LEAK    0
#endif
#endif

#if _DEF_USING_DEBUG_MEMORY_LEAK
#define _DEF_FILE_LINE  , __FILE__, __LINE__
#else
#define _DEF_FILE_LINE
#endif

#if _DEF_USING_DEBUG_MEMORY_LEAK
#define debug_new new(__FILE__, __LINE__)
#else
#define debug_new new
#endif
#define NEW debug_new

#if _DEF_USING_DEBUG_MEMORY_LEAK
#include <crtdbg.h>
/*----------------------------------------------------------------
/   new 재정의를 포함하면서, #define new에 대한 가드가 되어있지 않는 파일은
/   이하에 추가합니다.
----------------------------------------------------------------*/
#include <xmemory>
#include <xtree>


// MFC 메모리 디버깅이 활성화 되지 않은 경우에만 사용됩니다.
// 사용시 호출이 모호해집니다.
#if !defined(__AFX_H__) || defined(_AFX_NO_DEBUG_CRT)
static class CDebugMemoryLeak{
public:
    CDebugMemoryLeak()
    {
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }
} __CDebugMemoryLeak;
#endif

// __autoclassinit 호출 방지를 위한 조건은 예외발생금지(nothrow)가 아닌 size외에 다른 파라미터의 추가가 조건이다
// 그러므로 디버그 릴리즈 모두 __FILE__ __LINE__ 공통 사용하도록 한다
// size 이외 사용하지 않는 파라미터는 컴파일러가 없는것처럼 최적화 해준다
//#define NEW new(std::nothrow, __FILE__, __LINE__)

#ifdef delete
#pragma message("delete가 macro로 사용되었음")
#endif

#endif


//namespace UTIL{
#if _DEF_USING_DEBUG_MEMORY_LEAK
    inline void* operator new(size_t _Size, const char* _FileName, int _Line)
    {
        return ::operator new(_Size, _NORMAL_BLOCK, _FileName, _Line);
    }
    inline void* operator new[](size_t _Size, const char* _FileName, int _Line)
    {
        return ::operator new[](_Size, _NORMAL_BLOCK, _FileName, _Line);
    }
    inline void operator delete(void* _pData, const char* /*_FileName*/, int /*_Line*/)
    {
        ::operator delete(_pData);
    }
    inline void operator delete[](void* _pData, const char* /*_FileName*/, int /*_Line*/)
    {
        ::operator delete[](_pData);
    }
#else
    inline void* operator new(size_t _Size, const char* /*_FileName*/, int /*_Line*/)
    {
        return ::operator new(_Size);
    }
    inline void* operator new[](size_t _Size, const char* /*_FileName*/, int /*_Line*/)
    {
        return ::operator new[](_Size);
    }
        inline void operator delete(void* _pData, const char* /*_FileName*/, int /*_Line*/)
    {
        ::operator delete(_pData);
    }
    inline void operator delete[](void* _pData, const char* /*_FileName*/, int /*_Line*/)
    {
        ::operator delete[](_pData);
    }
#endif
//}
//// 외부로 공개한다.
//using UTIL::operator new;
//using UTIL::operator delete;
//using UTIL::operator new[];
//using UTIL::operator delete[];