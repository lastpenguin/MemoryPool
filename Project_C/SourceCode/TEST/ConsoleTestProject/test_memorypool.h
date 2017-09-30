// main 함수에서 test()를 실행
//----------------------------------------------------------------
// 필요한 코드 미 포함시 _DEF_PUBLIC_VER를 사용
//#define _DEF_PUBLIC_VER  // 공개버전 컴파일 옵션
//----------------------------------------------------------------
// 할당 크기 방식(고정 0 , 가변 1)
// 활성화시 _DEF_USING_MEMORYPOOL_VER 는 3번 또는 30 적용
#define _DEF_USING_RANDOMSIZE_ALLOC 1
// 객체당 크기(고정크기일때 유효)
#define _TYPE_SIZE  (256)
//----------------------------------------------------------------
// 메모리풀 사용방법
// 0    메모리풀 사용안함 : 사용자 추가 코드 동작
// 1    상속버전
// 2    _MACRO_ALLOC__FROM_MEMORYPOOL   _MACRO_FREE__FROM_MEMORYPOOL
// 3    가변사이즈에 해당되는 메모리풀 접근
// 4    메모리풀 객체에 직접 접근(가장 빠른)
// 13   가변사이즈에 해당되는 메모리풀 접근 - Quick Free(Unsafe)
// 30   가변사이즈에 해당되는 메모리풀 접근(DLL 외부 인터페이스 접근)
// 33   가변사이즈에 해당되는 메모리풀 접근(DLL 외부 인터페이스 접근) - Quick Free(Unsafe) : 약 14% 빠른 해제 버전
#define _DEF_USING_MEMORYPOOL_VER   13
//----------------------------------------------------------------
// alloc / free 전 캐시 무효화 시도
// 이 옵션을 사용하면 alloc 과 alloc 사이, free 와 free 사이에 L1 DATA CAHCE내용을 무효화 시도할 것이다
// 데이터 캐시에 올라와 있는 메모리풀 관련된 것을 제거 하기위해
//      실제 측정결과 옵션을 주고 안주고 차이는 +a 시간이 비슷하게 추가 되었으며
//      메모리풀별로 조금씩 좀더 차이가 난다
//      사용하는 내부 데이터 영향인듯 하다
#define _DEF_USING_CLEAR_CPUCACHE   0
const size_t gc_CpuL1DataCacheSize_per_Core = 16 * 1024;
//----------------------------------------------------------------
#define _DEF_USING_DONT_DELETE  0
//----------------------------------------------------------------
//  ▲ 이상, 컴파일 설정
//----------------------------------------------------------------

#if _DEF_USING_RANDOMSIZE_ALLOC
#if _DEF_USING_MEMORYPOOL_VER == 1 || _DEF_USING_MEMORYPOOL_VER == 2 && _DEF_USING_MEMORYPOOL_VER == 4
#undef _DEF_USING_MEMORYPOOL_VER
#define _DEF_USING_MEMORYPOOL_VER 3
#endif
#endif

#ifndef DECLSPEC_CACHEALIGN
#define DECLSPEC_CACHEALIGN __declspec(align(64))
#endif

#if _DEF_USING_CLEAR_CPUCACHE
namespace L1CACHE{
    const size_t gc_SizeCacheLine = 64;
    const UINT32 g_nData = gc_CpuL1DataCacheSize_per_Core / sizeof(UINT32);
    DECLSPEC_CACHEALIGN UINT32 g_Data[g_nData] ={0};
}
void gFN_Clear_CpuCache()
{
    const size_t c_Step = L1CACHE::gc_SizeCacheLine / sizeof(L1CACHE::g_Data[0]);

    for(size_t i=0; i<L1CACHE::g_nData; i+= c_Step)
    {
        _mm_prefetch((LPCSTR)L1CACHE::g_Data+i, _MM_HINT_T0);
    }
    //// 이것이 정상 동작하다면
    //// 캐시크기를 n등분하여 n회반복하는 것이 캐시크기전체를 1회 반복하는 것 보다 실행속도가 빠를 것이다
    //// -> 테스트 결과, 이것은 의도대로 동작한다
    ////    캐시1/4 크기를 4회 반복하는 것 보다 캐시 전체 크기만큼 1회 반복하는 것이 느렸다
    //const size_t c_nDivision = 1; // L1캐시 읽기쓰기 1/n
    //const size_t c_Step = L1CACHE::gc_SizeCacheLine / sizeof(L1CACHE::g_Data[0]);
    //const size_t c_nData = L1CACHE::g_nData / c_nDivision;
    //UINT32 ValPrevious = 1;
    //for(size_t n=0; n<c_nDivision; n++)
    //{
    //    for(size_t i=0; i<c_nData; i += c_Step)
    //    {
    //        L1CACHE::g_Data[i] += ValPrevious;
    //        ValPrevious = L1CACHE::g_Data[i];
    //    }
    //}
}
#else
#define gFN_Clear_CpuCache  __noop
#endif

#ifdef _DEF_PUBLIC_VER
#include <Windows.h>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <bitset>
#include <conio.h>
class CUnCopyAble{};
    #if defined(_WIN64)
        #define __X86   0
        #define __X64   1
    #elif defined(_WIN32)
        #define __X86   1
        #define __X64   0
    #else
        #error 대상 플렛폼을 추가 하십시오
    #endif
    #define _Assert(...)    __noop

    #define _MACRO_BIT_COUNT(n)     __popcnt(n)
    #if _DEF_USING_MEMORYPOOL_VER != 0 && _DEF_USING_MEMORYPOOL_VER != 30
        #pragma message("비공개 사용자가 사용하기 위해 _DEF_USING_MEMORYPOOL_VER = 30이 강제되었습니다. 사용하지 않으려면 0을 적용하십시오")
        #undef _DEF_USING_MEMORYPOOL_VER
        #define _DEF_USING_MEMORYPOOL_VER 30
    #endif
namespace CORE{
    HMODULE g_hDLL_Core = 0;
}
template<typename T>
void gFN_Call_Destructor(T* p){
    p->~T();
}
    #ifndef NEW
        #define NEW new
    #endif
    #define _MACRO_CALL_CONSTRUCTOR(P, T)   new(P) T
    #define _MACRO_CALL_DESTRUCTOR(P)       gFN_Call_Destructor(P)

    // 매롱
    #define _SAFE_NEW_ARRAY_ALIGNED_CACHELINE(P,T,N)    P = (void**)::_aligned_malloc(sizeof(void*)*N, 64)
    #define _SAFE_DELETE_ARRAY_ALIGNED_CACHELINE(P,N)   ::_aligned_free(P), P=nullptr
    #define _MACRO_OUTPUT_DEBUG_STRING_ALWAYS           OutputDebugStringA
    #define _SAFE_CLOSE_HANDLE(H)                       CloseHandle(H), H=(HANDLE)12345678
namespace ENGINE{

    class CEngine{
    public:
        CEngine()
        {
            const LPCSTR szName_CoreDLL =
        #if __X64
        #ifdef _DEBUG
            ("Core_x64D.dll");
        #else
            ("Core_x64.dll");
        #endif
        #elif __X86
        #ifdef _DEBUG
            ("Core_x86D.dll");
        #else
            ("Core_x86.dll");
        #endif
        #endif
            CORE::g_hDLL_Core = LoadLibraryA(szName_CoreDLL);
            if(!CORE::g_hDLL_Core)
            {
                MessageBoxA(NULL, szName_CoreDLL, "Failed Load", MB_ICONERROR);
                return;
            }
            struct TLSTCACHE{
                size_t _1;
                size_t _2;
                static TLSTCACHE& Get()
                {
                    static __declspec(thread) TLSTCACHE _Instance;
                    return _Instance;
                }
            };
            __interface IHandle{
                void __1();
                void __2();
                void __3();
                void __4();
                void __5();
                void __6();
                void __7();
                void __8(TLSTCACHE&(*)(void));
            };
            __interface ICore{
                void __1();
                IHandle* __2();
            };

            typedef ICore* (*_TYPE_FN_GetCOre)(void);
            _TYPE_FN_GetCOre pFN = reinterpret_cast<_TYPE_FN_GetCOre>(::GetProcAddress(CORE::g_hDLL_Core, "GetCore"));
            auto pCore = pFN();
            auto pTool = pCore->__2();
            pTool->__8(TLSTCACHE::Get);
        }
        ~CEngine()
        {
            if(CORE::g_hDLL_Core)
                ::FreeLibrary(CORE::g_hDLL_Core);
        }
    };
}
    
#else
#include "../../Output/Engine_include.h"
#include "../../Output/Engine_import.h"
#endif

#include <Psapi.h>
#include <atlpath.h>
#include "DLL_Opener.h"

#ifndef _DEF_CACHELINE_SIZE
#define _DEF_CACHELINE_SIZE 64
#endif



//#define _DEF_TEST_TCMALLOC
#define _DEF_TEST_TBBMALLOC


#ifdef _DEF_TEST_TCMALLOC
  #include "e:/lib/tcmalloc/tcmalloc.h"
  #pragma comment(lib, "e:/lib/tcmalloc/libtcmalloc_minimal.lib")
#elif defined(_DEF_TEST_TBBMALLOC)
  //#include "e:/lib/tbbmalloc/tbbmalloc_proxy.h"
  //#pragma comment(lib, "e:/lib/tbbmalloc/tbbmalloc_proxy.lib")
  //#pragma comment(linker, "/include:__TBB_malloc_proxy")

typedef void* (*FunctionPTR_Malloc)(size_t);
typedef void (*FunctionPTR_Free)(void*);

FunctionPTR_Malloc scalable_malloc = nullptr;
FunctionPTR_Free scalable_free = nullptr;


 struct TBB_Initialize{
     CDLL_Opener o;

     static LPCSTR s_szName[];
     static LPCSTR s_szFileName[];

     TBB_Initialize()
     {
         //Initialize();
     }
     DECLSPEC_NOINLINE void Initialize(int index_Ver)
     {
         if(!o.mFN_Load_Library(s_szFileName[index_Ver]))
             MessageBoxA(NULL, s_szFileName[index_Ver], "Failed Load", MB_ICONERROR);


         scalable_malloc = (FunctionPTR_Malloc)o.mFN_Get_ProcAddress("scalable_malloc");
         scalable_free = (FunctionPTR_Free)o.mFN_Get_ProcAddress("scalable_free");
         //if(scalable_malloc)
         //    OutputDebugStringA("linked scalable_malloc\n");
         //if(scalable_free)
         //    OutputDebugStringA("linked scalable_free\n");
     }
 }tbbload;
 LPCSTR TBB_Initialize::s_szName[] =
 {
     "TBB Ver4.4 20150728",
     "TBB Ver4.4 20160128",
 };
 LPCSTR TBB_Initialize::s_szFileName[] =
 {
 #if __X86
     "tbbmalloc86_44_20150728.dll",
     "tbbmalloc86_44_20160128.dll",
 #elif __X64
     "tbbmalloc64_44_20150728.dll",
     "tbbmalloc64_44_20160128.dll",
 #else
 #error
 #endif
 };
#endif

 namespace CORE{
     extern HMODULE g_hDLL_Core;
 #if _DEF_USING_DEBUG_MEMORY_LEAK
     void*(*CoreAlloc)(size_t, const char*, int) = nullptr;
 #else
     void*(*CoreAlloc)(size_t) = nullptr;
 #endif
     void(*CoreFree)(void*)=nullptr;
 }







// 2GB
#if __X64
const size_t GuideSize = (size_t)2 * 1024 * 1024 * 1024;
#else
const size_t GuideSize = (size_t)1 * 1024 * 1024 * 1024;
#endif

#if _DEF_USING_RANDOMSIZE_ALLOC
size_t iSizeMin_Rnd;
size_t iSizeMax_Rnd;
std::vector<size_t> vRandomSeed;
#endif

size_t nLoopTest;
size_t iSize_UseMemory;
size_t nTotalCNT;
size_t nCNTPerThread;
BOOL    bUsingThread    = TRUE;
DWORD   nNumThread      = 2;
BOOL    bUseMemoryPool  = FALSE;


volatile LONG g_nCntReady = 0;
const DWORD gc_MaxThread = 32;
HANDLE hThreads[gc_MaxThread] = {NULL};
HANDLE hEvent_Start;
HANDLE hEvents_Done[gc_MaxThread] = {NULL};

BOOL bTest_Value  = 1;







class CA{
public:
    char data[_TYPE_SIZE];
};
#if _DEF_USING_MEMORYPOOL_VER == 1
class CAm: public UTIL::MEM::CMemoryPoolResource{
public:
    char data[_TYPE_SIZE];
};
#else
class CAm{
public:
    char data[_TYPE_SIZE];
};
#endif


#if _DEF_USING_RANDOMSIZE_ALLOC
typedef void (*_TYPE_FN_New_Deletel)(void**, size_t);
typedef void (*_TYPE_FN_New_Delete__Pool)(void**, size_t);
#else
typedef void (*_TYPE_FN_New_Deletel)(CA**);
typedef void (*_TYPE_FN_New_Delete__Pool)(CAm**);
#endif

_TYPE_FN_New_Deletel        gpFN_NEW_DELETE_OTHER = nullptr;
_TYPE_FN_New_Delete__Pool   gpFN_NEW_DELETE_POOL  = nullptr;





void _Delay()
{
    return;
    int r = rand();
    if(r == 0)
        return;
    r &= 0x07;
    Sleep(r);
}
// 직접실행창 테스트
size_t FindAddress(void** pTable, size_t nTable, void* pAddress, size_t size)
{
    char* pS = (char*)pAddress;
    char* pE = pS + size;
    size_t cnt=0;
    for(size_t i=0; i<nTable; i++)
    {
        char* pT = (char*)pTable[i];
        if(pS <= pT && pT < pE)
        {
            cnt++;
            _MACRO_OUTPUT_DEBUG_STRING("Thread[%u] index = %Iu [0x%IX]\n", ::GetCurrentThreadId(), i, i);
        }
    }
    return cnt;
}

bool memtest(const void* pPTR, char code, size_t size)
{
    char* pC = (char*)pPTR;
    if(size < 16) {
        for(size_t i=0; i<size; i++)
            if(*(pC+i) != code)
                return false;
        return true;
    }



    while((size_t)pC % sizeof(size_t) != 0)
    {
        if(*pC != code)
            return false;

        pC++;
        size--;
    }
    size_t wCode = 0;
    for(size_t i=0; i<sizeof(size_t); i++)
        wCode = (wCode << 8) | (size_t)(byte)code;

    size_t nWord = size / sizeof(size_t);
    size_t nChar = size % sizeof(size_t);

    size_t* pWord = (size_t*)pC;
    size_t* pWordEnd = pWord + nWord;
    do{
        if(*pWord != wCode)
            return false;
        pWord++;
    }while(pWord < pWordEnd);

    pC = (char*)pWord;
    char* pCEnd = pC + nChar;
    for(;pC < pCEnd;pC++)
    {
        if(*pC != code)
            return false;
    }
    return true;
}
bool memtest2(const void* pPTR, char code, size_t size)
{
    if(size < _DEF_CACHELINE_SIZE)
        return memtest(pPTR, code, size);

    const char* pC = (char*)pPTR;

    const char* pCS_Aligned = (char*)((size_t)pC & ~(_DEF_CACHELINE_SIZE-1));
    if(pC != pCS_Aligned)
    {
        pCS_Aligned += _DEF_CACHELINE_SIZE;
        memtest(pC, code, pCS_Aligned - pC);
        pC = pCS_Aligned;
    }

    // 캐시라인 단위 처리
    const char* pE = (char*)pPTR + size;
    const char* pE_Aligned = (char*)((size_t)pE & ~(_DEF_CACHELINE_SIZE-1));

    size_t wCode = 0;
    for(size_t i=0; i<sizeof(size_t); i++)
        wCode = (wCode << 8) | (size_t)(byte)code;


    //DECLSPEC_CACHEALIGN __m128 code128;
    //memset(&code128, code, sizeof(code128));

    for(; pC != pE_Aligned; pC += _DEF_CACHELINE_SIZE)
    {
        // 4단계후를  미리 로드
        _mm_prefetch(pC+_DEF_CACHELINE_SIZE*4, _MM_HINT_T0);

        const size_t* pW = (size_t*)pC;
        size_t t = 0;
        #define _MACRO_UNROLLING4(i) t |= (wCode ^ pW[i+0]), t |= (wCode ^ pW[i+1]), t |= (wCode ^ pW[i+2]), t |= (wCode ^ pW[i+3])

        #if __X86
        _MACRO_UNROLLING4(0);
        _MACRO_UNROLLING4(4);
        _MACRO_UNROLLING4(8);
        _MACRO_UNROLLING4(12);
        #elif __X64
        //const __m128 r1 = _mm_xor_ps(*(__m128*)pC, code128);
        //t = r1.m128_u64[0] | r1.m128_u64[1];
        //const __m128 r2 = _mm_xor_ps(*(__m128*)(pC+16), code128);
        //t |= r2.m128_u64[0], t |= r2.m128_u64[1];
        //const __m128 r3 = _mm_xor_ps(*(__m128*)(pC+32), code128);
        //t |= r3.m128_u64[0], t |= r3.m128_u64[1];
        //const __m128 r4 = _mm_xor_ps(*(__m128*)(pC+48), code128);
        //t |= r4.m128_u64[0], t |= r4.m128_u64[1];
        //if(t)
        //    return false;
        _MACRO_UNROLLING4(0);
        _MACRO_UNROLLING4(4);
        #else
        #error
        #endif
        #undef _MACRO_UNROLLING4
        if(t)
            return false;
    }

    if(pC == pE)
        return true;
    // 남은 부분 처리
    return memtest(pC, code, pE-pC);
}
void ValueSet(void* val, size_t code, size_t size)
{
    //memset(val, code, size);

    DWORD tid = ::GetCurrentThreadId();
    DWORD* ptid = (DWORD*)val;
    *ptid = tid;
    UINT32* pindex =(UINT32*)(ptid+1);
    *pindex = (UINT32)code;
    char* p = (char*)(pindex+1);
    memset(p, code&0xff, size-sizeof(DWORD)-sizeof(UINT32));
}
void ValueCheck(void* val, size_t code, size_t size)
{
    //if(!memtest2(val, code, size))
    //    MessageBoxA(NULL, "WTF?", "", MB_ICONERROR);

    BOOL Failed = FALSE;
    DWORD tid = ::GetCurrentThreadId();

    DWORD* ptid = (DWORD*)val;
    *ptid = tid;
    UINT32* pindex =(UINT32*)(ptid+1);
    *pindex = (UINT32)code;
    char* p = (char*)(pindex+1);

    if(*ptid != tid)
        Failed = TRUE;
    if(*pindex != (UINT32)code)
        Failed = TRUE;
    const byte _temp_code = *(byte*)p;
    if(!memtest2(p, code&0xff, size-sizeof(DWORD)-sizeof(UINT32)))
        Failed = TRUE;
    
    if(Failed)
        MessageBoxA(NULL, "WTF?", "", MB_ICONERROR);
}
template<typename T>
void ValueSet(T* p, size_t code)
{
    memset(p->data, code&0xff, sizeof(T::data));
}
template<typename T>
void ValueCheck(T* p, size_t code)
{
    if(!memtest2(p->data, code&0xff, sizeof(T::data)))
        MessageBoxA(NULL, "WTF?", "", MB_ICONERROR);
}
void ValueSet__Default(void* val, size_t size)
{
    byte* p = (byte*)val;
    byte* pE = p + size;
    // TBB의 경우 메모리풀 자체에서 VirtualAlloc후 주소에 값을 쓰지 않는 영역이 많다
    // 이때 실제 물리적인 메모리 연결이 일어나지 않기 때문에 실제사용에서의 속도와 차이가 있다
    // 이에 따라 시스템 할당단위인 4KB 단위마다 값을 기록해준다
    do{
        *p = 0;
        p += (1024*4);
    }while(p < pE);
}

#pragma region 할당 해제 함수
DECLSPEC_NOINLINE void gFN_NEW_DELETE__POOL(CAm** p)
{
    typedef CAm T;

#if _DEF_USING_MEMORYPOOL_VER == 4
    UTIL::MEM::IMemoryPool* pPool = UTIL::g_pMem->mFN_Get_MemoryPool(sizeof(T));
#endif

    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

    #if _DEF_USING_MEMORYPOOL_VER == 0
        #error //do to
    #elif _DEF_USING_MEMORYPOOL_VER == 1
        p[i] =  NEW T;
    #elif _DEF_USING_MEMORYPOOL_VER == 2
        _MACRO_ALLOC__FROM_MEMORYPOOL(p[i]);
        _MACRO_CALL_CONSTRUCTOR(p[i], T);
    #elif _DEF_USING_MEMORYPOOL_VER == 3
        p[i] = (T*)_MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC(sizeof(CAm));
        _MACRO_CALL_CONSTRUCTOR(p[i], T);
    #elif _DEF_USING_MEMORYPOOL_VER == 4
        p[i] = (T*)pPool->mFN_Get_Memory(sizeof(T));
        _MACRO_CALL_CONSTRUCTOR(p[i], T);
    #elif _DEF_USING_MEMORYPOOL_VER == 13
        p[i] = (T*)_MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC(sizeof(CAm));
        _MACRO_CALL_CONSTRUCTOR(p[i], T);
    #elif _DEF_USING_MEMORYPOOL_VER == 30 || _DEF_USING_MEMORYPOOL_VER == 33
        #if _DEF_USING_DEBUG_MEMORY_LEAK
        p[i] = (T*)CORE::CoreAlloc(sizeof(CAm), __FILE__, __LINE__);
        #else
        p[i] = (T*)CORE::CoreAlloc(sizeof(CAm));
        #endif
        _MACRO_CALL_CONSTRUCTOR(p[i], T);
    #else
        #error 
        _SAFE_NEW__MEMORYPOOL_MANAGER__FORCEALIGNED(p[i], CAm);
    #endif

        if(bTest_Value)
            ValueSet(p[i], i);
        else
            ValueSet__Default(p[i], sizeof(CAm::data));
    }
    _Delay();
#if _DEF_USING_DONT_DELETE
    return;
#endif
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        if(bTest_Value)
            ValueCheck(p[i], i);

    #if _DEF_USING_MEMORYPOOL_VER == 0 
        #error //do to
    #elif _DEF_USING_MEMORYPOOL_VER == 1 
        delete p[i];
    #elif _DEF_USING_MEMORYPOOL_VER == 2
        _MACRO_CALL_DESTRUCTOR(p[i]);
        _MACRO_FREE__FROM_MEMORYPOOL(p[i]);
    #elif _DEF_USING_MEMORYPOOL_VER == 3
        _MACRO_CALL_DESTRUCTOR(p[i]);
        _MACRO_FREE__FROM_MEMORYPOOL_GENERIC(p[i]);
    #elif _DEF_USING_MEMORYPOOL_VER == 4
        _MACRO_CALL_DESTRUCTOR(p[i]);
        pPool->mFN_Return_Memory(p[i]);
    #elif _DEF_USING_MEMORYPOOL_VER == 13
        _MACRO_CALL_DESTRUCTOR(p[i]);
        _MACRO_FREEQ__FROM_MEMORYPOOL_GENERIC(p[i]);
    #elif _DEF_USING_MEMORYPOOL_VER == 30 || _DEF_USING_MEMORYPOOL_VER == 33
        _MACRO_CALL_DESTRUCTOR(p[i]);
        CORE::CoreFree(p[i]);
    #else
        #error
        _SAFE_DELETE__MEMORYPOOL_MANAGER__FORCEALIGNED(p[i]);
    #endif
    }
}
DECLSPEC_NOINLINE void gFN_NEW_DELETE_LFH(CA** p)
{
    typedef CA T;
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        p[i] = (T*)::malloc(sizeof(T));
        _MACRO_CALL_CONSTRUCTOR(p[i], T);

        if(bTest_Value)
            ValueSet(p[i], i);
        else
            ValueSet__Default(p[i], sizeof(CA::data));
    }
    _Delay();
#if _DEF_USING_DONT_DELETE
    return;
#endif
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        if(bTest_Value)
            ValueCheck(p[i], i);

        _MACRO_CALL_DESTRUCTOR(p[i]);
        ::free(p[i]);
    }
}
DECLSPEC_NOINLINE void gFN_NEW_DELETE_TBB(CA** p)
{
    typedef CA T;
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        p[i] = (T*)::scalable_malloc(sizeof(T));
        _MACRO_CALL_CONSTRUCTOR(p[i], T);

        if(bTest_Value)
            ValueSet(p[i], i);
        else
            ValueSet__Default(p[i], sizeof(CA::data));
    }
    _Delay();
#if _DEF_USING_DONT_DELETE
    return;
#endif
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        if(bTest_Value)
            ValueCheck(p[i], i);

        _MACRO_CALL_DESTRUCTOR(p[i]);
        ::scalable_free(p[i]);
    }
}

#if _DEF_USING_RANDOMSIZE_ALLOC
DECLSPEC_NOINLINE void gFN_NEW_DELETE__POOL(void** p, size_t iS)
{
    typedef void T;
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        const size_t size = vRandomSeed[iS+i];

    #if _DEF_USING_MEMORYPOOL_VER == 0 
        #error //do to
    #elif _DEF_USING_MEMORYPOOL_VER == 30 || _DEF_USING_MEMORYPOOL_VER == 33
        #if _DEF_USING_DEBUG_MEMORY_LEAK
        p[i] = (T*)CORE::CoreAlloc(size, __FILE__, __LINE__);
        #else
        p[i] = (T*)CORE::CoreAlloc(size);
        #endif
    #elif _DEF_USING_MEMORYPOOL_VER == 13
        p[i] = (T*)_MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC(size);
    #else
        p[i] = (T*)_MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC(size);
    #endif

        if(bTest_Value)
            ValueSet(p[i], i, size);
        else
            ValueSet__Default(p[i], size);
    }
    _Delay();
#if _DEF_USING_DONT_DELETE
    return;
#endif
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        const size_t size = vRandomSeed[iS+i];

        if(bTest_Value)
            ValueCheck(p[i], i, size);
    #if _DEF_USING_MEMORYPOOL_VER == 0 
        #error //do to
    #elif _DEF_USING_MEMORYPOOL_VER == 30 || _DEF_USING_MEMORYPOOL_VER == 33
        CORE::CoreFree(p[i]);
    #elif _DEF_USING_MEMORYPOOL_VER == 13
        _MACRO_FREEQ__FROM_MEMORYPOOL_GENERIC(p[i]);
    #else
        _MACRO_FREE__FROM_MEMORYPOOL_GENERIC(p[i]);
    #endif
    }
}
DECLSPEC_NOINLINE void gFN_NEW_DELETE_LFH(void** p, size_t iS)
{
    typedef void T;
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        const size_t size = vRandomSeed[iS+i];

        p[i] = ::malloc(size);

        if(bTest_Value)
            ValueSet(p[i], i, size);
        else
            ValueSet__Default(p[i], size);
    }
    _Delay();
#if _DEF_USING_DONT_DELETE
    return;
#endif
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        const size_t size = vRandomSeed[iS+i];

        if(bTest_Value)
            ValueCheck(p[i], i, size);

        ::free(p[i]);
    }
}
DECLSPEC_NOINLINE void gFN_NEW_DELETE_TBB(void** p, size_t iS)
{
    typedef void T;
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        const size_t size = vRandomSeed[iS+i];

        p[i] = (T*)scalable_malloc(size);

        if(bTest_Value)
            ValueSet(p[i], i, size);
        else
            ValueSet__Default(p[i], size);
    }
    _Delay();
#if _DEF_USING_DONT_DELETE
    return;
#endif
    for(size_t i=0; i<nCNTPerThread; i++)
    {
        gFN_Clear_CpuCache();

        const size_t size = vRandomSeed[iS+i];

        if(bTest_Value)
            ValueCheck(p[i], i, size);

        scalable_free(p[i]);
    }
}
#endif






#if !_DEF_USING_RANDOMSIZE_ALLOC
DECLSPEC_NOINLINE double MainThreadFunction()
{
    void** p = nullptr;
    _SAFE_NEW_ARRAY_ALIGNED_CACHELINE(p, void*(nullptr), nCNTPerThread);

    LONGLONG total = 0;
    LARGE_INTEGER TimeBegin, TimeEnd, TimeFrequency;

    if(bUseMemoryPool)
    {
        CAm** pCAm = (CAm**)p;

        for(size_t cntLoop=0; cntLoop<nLoopTest; cntLoop++)
        {
            if(1 < nNumThread)
            {
                ::InterlockedIncrement(&g_nCntReady);
                while(g_nCntReady != nNumThread);
                g_nCntReady = 0;
                for(size_t i=1; i<nNumThread; i++)
                    ::ResetEvent(hEvents_Done[i]);
            }

            auto prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
            ::QueryPerformanceCounter(&TimeBegin);
            SetThreadAffinityMask(::GetCurrentThread(), prev);
            if(1 < nNumThread)
                ::SetEvent(hEvent_Start);

            gpFN_NEW_DELETE_POOL(pCAm);

            if(1 < nNumThread)
            {
                ::WaitForMultipleObjects(nNumThread-1, hEvents_Done+1, TRUE, INFINITE);
            }

            prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
            ::QueryPerformanceCounter(&TimeEnd);
            ::SetThreadAffinityMask(::GetCurrentThread(), prev);
            total += (TimeEnd.QuadPart - TimeBegin.QuadPart);

        }
    }
    else
    {
        CA** pCA = (CA**)p;

        for(size_t cntLoop=0; cntLoop<nLoopTest; cntLoop++)
        {
            if(1 < nNumThread)
            {
                ::InterlockedIncrement(&g_nCntReady);
                while(g_nCntReady != nNumThread);
                g_nCntReady = 0;
                for(size_t i=1; i<nNumThread; i++)
                    ::ResetEvent(hEvents_Done[i]);
            }

            auto prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
            ::QueryPerformanceCounter(&TimeBegin);
            ::SetThreadAffinityMask(::GetCurrentThread(), prev);
            if(1 < nNumThread)
                ::SetEvent(hEvent_Start);

            gpFN_NEW_DELETE_OTHER(pCA);

            if(1 < nNumThread)
            {
                ::WaitForMultipleObjects(nNumThread-1, hEvents_Done+1, TRUE, INFINITE);
            }

            prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
            ::QueryPerformanceCounter(&TimeEnd);
            ::SetThreadAffinityMask(::GetCurrentThread(), prev);
            total += (TimeEnd.QuadPart - TimeBegin.QuadPart);

        }
    }

    _SAFE_DELETE_ARRAY_ALIGNED_CACHELINE(p, nCNTPerThread);

    auto prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
    ::QueryPerformanceFrequency(&TimeFrequency);
    ::SetThreadAffinityMask(::GetCurrentThread(), prev);

    return double(total) / TimeFrequency.QuadPart;
}

unsigned __stdcall ThreadFunction(void* _pParam)
{
    size_t id = (size_t)_pParam;
    void** p = nullptr;
    _SAFE_NEW_ARRAY_ALIGNED_CACHELINE(p, void*, nCNTPerThread);

    if(bUseMemoryPool)
    {
        CAm** pCAm = (CAm**)p;

        for(size_t cntLoop=0; cntLoop<nLoopTest; cntLoop++)
        {
            ::InterlockedIncrement(&g_nCntReady);
            ::WaitForSingleObject(hEvent_Start, INFINITE);

            gpFN_NEW_DELETE_POOL(pCAm);

            ::SetEvent(hEvents_Done[id]);
            ::ResetEvent(hEvent_Start);
        }

    }
    else
    {
        CA** pCA = (CA**)p;

        for(size_t cntLoop=0; cntLoop<nLoopTest; cntLoop++)
        {
            ::InterlockedIncrement(&g_nCntReady);
            ::WaitForSingleObject(hEvent_Start, INFINITE);


            gpFN_NEW_DELETE_OTHER(pCA);

            ::SetEvent(hEvents_Done[id]);
            ::ResetEvent(hEvent_Start);
        }

    }

    _SAFE_DELETE_ARRAY_ALIGNED_CACHELINE(p, nCNTPerThread);
    return 0;
}
#else




DECLSPEC_NOINLINE double MainThreadFunction()
{
    void** p = nullptr;
    _SAFE_NEW_ARRAY_ALIGNED_CACHELINE(p, void*(nullptr), nCNTPerThread);

    LONGLONG total = 0;
    LARGE_INTEGER TimeBegin, TimeEnd, TimeFrequency;

    if(bUseMemoryPool)
    {
        for(size_t cntLoop=0; cntLoop<nLoopTest; cntLoop++)
        {
            if(1 < nNumThread)
            {
                ::InterlockedIncrement(&g_nCntReady);
                while(g_nCntReady != nNumThread);
                g_nCntReady = 0;
                for(size_t i=1; i<nNumThread; i++)
                    ::ResetEvent(hEvents_Done[i]);
            }

            auto prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
            ::QueryPerformanceCounter(&TimeBegin);
            SetThreadAffinityMask(::GetCurrentThread(), prev);
            if(1 < nNumThread)
                ::SetEvent(hEvent_Start);

            gpFN_NEW_DELETE_POOL(p, 0);

            if(1 < nNumThread)
            {
                ::WaitForMultipleObjects(nNumThread-1, hEvents_Done+1, TRUE, INFINITE);
            }

            prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
            ::QueryPerformanceCounter(&TimeEnd);
            ::SetThreadAffinityMask(::GetCurrentThread(), prev);
            total += (TimeEnd.QuadPart - TimeBegin.QuadPart);

        }
    }
    else
    {
        for(size_t cntLoop=0; cntLoop<nLoopTest; cntLoop++)
        {
            if(1 < nNumThread)
            {
                ::InterlockedIncrement(&g_nCntReady);
                while(g_nCntReady != nNumThread);
                g_nCntReady = 0;
                for(size_t i=1; i<nNumThread; i++)
                    ::ResetEvent(hEvents_Done[i]);
            }

            auto prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
            ::QueryPerformanceCounter(&TimeBegin);
            ::SetThreadAffinityMask(::GetCurrentThread(), prev);
            if(1 < nNumThread)
                ::SetEvent(hEvent_Start);

            gpFN_NEW_DELETE_OTHER(p, 0);

            if(1 < nNumThread)
            {
                ::WaitForMultipleObjects(nNumThread-1, hEvents_Done+1, TRUE, INFINITE);
            }

            prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
            ::QueryPerformanceCounter(&TimeEnd);
            ::SetThreadAffinityMask(::GetCurrentThread(), prev);
            total += (TimeEnd.QuadPart - TimeBegin.QuadPart);

        }
    }

    _SAFE_DELETE_ARRAY_ALIGNED_CACHELINE(p, nCNTPerThread);
    
    auto prev = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
    ::QueryPerformanceFrequency(&TimeFrequency);
    ::SetThreadAffinityMask(::GetCurrentThread(), prev);

    return double(total) / TimeFrequency.QuadPart;
}

unsigned __stdcall ThreadFunction(void* _pParam)
{
    size_t id = (size_t)_pParam;
    size_t iS = id * nCNTPerThread;

    void** p = nullptr;
    _SAFE_NEW_ARRAY_ALIGNED_CACHELINE(p, void*, nCNTPerThread);

    if(bUseMemoryPool)
    {
        for(size_t cntLoop=0; cntLoop<nLoopTest; cntLoop++)
        {
            ::InterlockedIncrement(&g_nCntReady);
            ::WaitForSingleObject(hEvent_Start, INFINITE);

            gpFN_NEW_DELETE_POOL(p, iS);

            ::SetEvent(hEvents_Done[id]);
            ::ResetEvent(hEvent_Start);
        }

    }
    else
    {
        for(size_t cntLoop=0; cntLoop<nLoopTest; cntLoop++)
        {
            ::InterlockedIncrement(&g_nCntReady);
            ::WaitForSingleObject(hEvent_Start, INFINITE);


            gpFN_NEW_DELETE_OTHER(p, iS);

            ::SetEvent(hEvents_Done[id]);
            ::ResetEvent(hEvent_Start);
        }

    }

    _SAFE_DELETE_ARRAY_ALIGNED_CACHELINE(p, nCNTPerThread);
    return 0;
}
#endif



MEMORYSTATUS            MemInfoSys  ={0};
PROCESS_MEMORY_COUNTERS MemInfoPro  ={0};
void GetInfo_ProcessMemoryInfo()
{
    MemInfoSys.dwLength = sizeof(MemInfoSys);
    ::GlobalMemoryStatus(&MemInfoSys);
    ::GetProcessMemoryInfo(GetCurrentProcess(), &MemInfoPro, sizeof(MemInfoPro));
}
void Report_ProcessMemoryInfo()
{
    size_t OtherCost = nCNTPerThread * nNumThread * sizeof(void*);
    #if _DEF_USING_RANDOMSIZE_ALLOC
    OtherCost += (sizeof(size_t) * vRandomSeed.capacity());
    #endif

    printf("---- Process Memory Information ----\n");
    printf("Test Cost                   : %10llu B (%10.3fMB)\n", (UINT64)OtherCost, OtherCost / 1024. / 1024.);
    printf("PagefileUsage(PEAK)         : %10Iu B (%10.3fMB) -> (%10.3fMB)\n", MemInfoPro.PeakPagefileUsage, MemInfoPro.PeakPagefileUsage / 1024. / 1024.
        , (MemInfoPro.PeakPagefileUsage - OtherCost) / 1024. / 1024.);
    printf("WorkingSetSize(PEAK)        : %10Iu B (%10.3fMB) -> (%10.3fMB)\n", MemInfoPro.PeakWorkingSetSize, MemInfoPro.PeakWorkingSetSize / 1024. / 1024.
        , (MemInfoPro.PeakWorkingSetSize - OtherCost) / 1024. / 1024.);
    printf("QuotaPagedPoolUsage(PEAK)   : %10Iu B\n", MemInfoPro.QuotaPeakPagedPoolUsage);
    printf("QuotaNonPagedPoolUsage(PEAK): %10Iu B\n", MemInfoPro.QuotaPeakNonPagedPoolUsage);
    printf("PageFaultCount              : %10u\n", MemInfoPro.PageFaultCount);

    printf("---- System Memory Information ----\n");
    printf("MemoryLoad                  : %10u %%\n", MemInfoSys.dwMemoryLoad);
    //printf("Physical Memory(Total)      : %10Iu KB\n", MemInfoSys.dwTotalPhys / 1024);
    //printf("Physical Memory(Free)       : %10Iu KB\n", MemInfoSys.dwAvailPhys / 1024);
    //printf("Paging File(Total)          : %10Iu KB\n", MemInfoSys.dwTotalPageFile / 1024);
    //printf("Paging File(Free)           : %10Iu KB\n", MemInfoSys.dwAvailPageFile / 1024);
    //printf("Virtual Memory(Total)       : %10Iu KB\n", MemInfoSys.dwTotalVirtual / 1024);
    //printf("Virtual Memory(Free)        : %10Iu KB\n", MemInfoSys.dwAvailVirtual / 1024);

    printf("Physical Memory(Free)       : %10.3f / %.3f GB\n"
        , (MemInfoSys.dwAvailPhys / 1024. / 1024. /1024.)
        , (MemInfoSys.dwTotalPhys / 1024. / 1024. / 1024.)
        );
}
#if _DEF_USING_RANDOMSIZE_ALLOC
void Initialize_RandomSeed()
{
    iSize_UseMemory = 0;

    size_t _bBitRAND = 15;// _MACRO_BIT_COUNT(RAND_MAX);

    size_t uRnd = iSizeMax_Rnd - iSizeMin_Rnd + 1;
    vRandomSeed.resize(nTotalCNT);

    //#pragma omp parallel num_threads(omp_get_max_threads())
    ::omp_set_num_threads(::omp_get_max_threads());
    if(iSizeMin_Rnd < iSizeMax_Rnd)
    {
    #pragma omp parallel for reduction(+ : iSize_UseMemory)
        for(INT64 i=0; i<(INT64)nTotalCNT; i++)
        {
            size_t seed = (size_t)rand();
            seed = (seed << _bBitRAND) | (size_t)rand();
            //seed = (seed << _bBitRAND) | (size_t)rand();
            //seed = (seed << _bBitRAND) | (size_t)rand();

            seed %= uRnd;
            seed += iSizeMin_Rnd;
            vRandomSeed[(size_t)i] = seed;

            iSize_UseMemory += seed;
        }
    }
    else
    {
    #pragma omp parallel for reduction(+ : iSize_UseMemory)
        for(INT64 i=0; i<(INT64)nTotalCNT; i++)
        {
            vRandomSeed[(size_t)i] = iSizeMin_Rnd;
        }
        iSize_UseMemory = iSizeMin_Rnd * nTotalCNT;
    }
    ::omp_set_num_threads(0);
    ::omp_set_dynamic(0);
}
#endif

#ifndef _DEF_PUBLIC_VER
//#include "../../Engine/UTIL/XmlParser.h"
#endif

BOOL gFN_Get_Input_YN()
{
    for(;;)
    {
        char code = (char)_getch();
        switch(code)
        {
        case 'y':   case 'Y':
            std::cout << std::endl;
            return TRUE;
        case '0':
            std::cout << "Y\n";
            return TRUE;
        case 'n':   case 'N':
            std::cout << std::endl;
            return FALSE;
        case 13: //enter
            std::cout << "N\n";
            return FALSE;
        }
    }
}

struct TInputData_from_FILE{
    UINT64 nTestLoop;
    UINT64 UnitSize[2];
    UINT64 RandomSeed;
    UINT64 AllocSize;
    UINT64 nThread;
    BOOL bTestReadWrite;
};

BOOL bParsingInputDataFile(TInputData_from_FILE* pData)
{
#pragma warning(push)
#pragma warning(disable: 4996)
    FILE* pFile = fopen("TestData.txt", "r");
    if(!pFile)
        return FALSE;

    char buffer[4096];
    auto SeekSymbol = [&]() -> char*{
        auto pTemp = ::fgets(buffer, 4096, pFile);
        if(!pTemp)
            return nullptr;

        return ::strchr(pTemp, ':') +1 ;
    };

    __try{
        char* p;
        p = SeekSymbol();
        sscanf(p, "%llu", &pData->nTestLoop);
        p = SeekSymbol();
        sscanf(p, "%llu", &pData->UnitSize[0]);
        p = SeekSymbol();
        sscanf(p, "%llu", &pData->UnitSize[1]);
        p = SeekSymbol();
        sscanf(p, "%llu", &pData->RandomSeed);
        p = SeekSymbol();
        sscanf(p, "%llu", &pData->AllocSize);
        p = SeekSymbol();
        sscanf(p, "%llu", &pData->nThread);


        p = SeekSymbol();
        char cYN[512];
        sscanf(p, "%s", cYN);
        switch(cYN[0])
        {
        case 'y' : case 'Y':
            pData->bTestReadWrite = TRUE;
            break;
        case 'n' : case 'N':
            pData->bTestReadWrite = FALSE;
            break;
        }

        fclose(pFile);
        return TRUE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        fclose(pFile);
        return FALSE;
    }
#pragma warning(pop)
}

void test()
{
    //::SetThreadAffinityMask(::GetCurrentThread(), 1);
    #if _DEF_USING_MEMORYPOOL_VER != 0
    ENGINE::CEngine* pEngine = nullptr;
    #endif




    TInputData_from_FILE fdata;
    BOOL bUseFileData;
    std::cout << "Using File DATA(Y/N) : ";
    if(bUseFileData = gFN_Get_Input_YN())
    {
        bUseFileData = bParsingInputDataFile(&fdata);
        if(!bUseFileData)
            std::cout << "Failed...\n";
    }

    //// test ~    
    //{
    //    char strpath[MAX_PATH];
    //    GetModuleFileNameA(NULL, strpath, MAX_PATH);
    //    PathRemoveFileSpecA(strpath);
    //    SetCurrentDirectoryA(strpath);

    //    UTIL::CXml_Parser xml;
    //    {
    //        xml.mFN_Load_From_File("test.xml");
    //        UTIL::CXml_Managed_Node refNode = xml.mFN_Find_SingleNode(&UTIL::CStringRefT(_T("phpunit")));
    //        _MACRO_OUTPUT_DEBUG_STRING(_T("node(%s) text(%s)\n"), refNode.GetData()->mFN_Get_NodeName().GetStr(), refNode.GetData()->mFN_Get_NodeText());
    //        UTIL::CXml_NodeDataATT* pATT = refNode.GetData()->mFN_Get_Attribute(&UTIL::CStringRefT(_T("colors")));
    //        _MACRO_OUTPUT_DEBUG_STRING(_T("\tName(%s) val(%s)"), pATT->mFN_Get_AttributeName().GetStr(), pATT->mFN_Get_Value__Text());
    //        BOOL b = pATT->mFN_Get_Value__Boolean();
    //    }
    //    xml.mFN_Load_From_File("test.xml2");
    //}
    //// ~ test


    #pragma region 설정
    {
        std::cout << "Num Test Count(1~) ";
        if(bUseFileData)
            nLoopTest = (size_t)fdata.nTestLoop, printf("%llu\n", fdata.nTestLoop);
        else
            std::cin >> nLoopTest;
        if(nLoopTest < 1)
            nLoopTest = 1;

        #if _DEF_USING_RANDOMSIZE_ALLOC
        {
            std::cout << "Random Object Size : Min(8 Byte ~) = ";
            if(bUseFileData)
                iSizeMin_Rnd = (size_t)fdata.UnitSize[0], printf("%llu\n", fdata.UnitSize[0]);
            else
                std::cin >> iSizeMin_Rnd;
            std::cout << "Random Object Size : Max(~ 10485760 Byte : 10MB) = ";
            if(bUseFileData)
                iSizeMax_Rnd = (size_t)fdata.UnitSize[1], printf("%llu\n", fdata.UnitSize[1]);
            else
                std::cin >> iSizeMax_Rnd;

            if(iSizeMin_Rnd < 8)
                iSizeMin_Rnd = 8;
            //if(iSizeMax_Rnd > 10485760)
            //    iSizeMax_Rnd = 10485760;
            if(iSizeMax_Rnd < iSizeMin_Rnd)
                iSizeMax_Rnd = iSizeMin_Rnd;

            UINT64 val1 = iSizeMin_Rnd, val2 = iSizeMax_Rnd;
            printf("Object Size(Byte) = %llu ~ %llu\n", val1, val2);

            std::cout << "Random Seed : ";
            unsigned int uSeed;
            if(bUseFileData)
                uSeed = (int)fdata.RandomSeed, printf("%llu\n", fdata.RandomSeed);
            else
                std::cin >> uSeed;
            ::srand(uSeed);
        }
        #else
        {
            printf("Object Size(Byte) = %d\n", _TYPE_SIZE);
        }
        #endif

        std::cout << "Memory Per Test(MB) ";
        if(bUseFileData)
            iSize_UseMemory = (size_t)fdata.AllocSize, printf("%llu\n", fdata.AllocSize);
        else
            std::cin >> iSize_UseMemory;
        if(iSize_UseMemory == 0)
            iSize_UseMemory = 128;

        iSize_UseMemory = iSize_UseMemory * 1024 * 1024;

        #if _DEF_USING_RANDOMSIZE_ALLOC
        {
            nTotalCNT = iSize_UseMemory / (iSizeMin_Rnd + iSizeMax_Rnd) * 2;
            std::cout << "Initialize Random Size Seed...\n";
            Initialize_RandomSeed();
        }
        #else
        {
            nTotalCNT = iSize_UseMemory / _TYPE_SIZE;
        }
        #endif

        {
            UINT64 val1 = nTotalCNT, val2 = iSize_UseMemory;
            printf("----------------------------------------------------------------\n");
            printf("CNT : %llu , Size : %llu B(%6.4f MB)...\n", val1, val2, iSize_UseMemory / 1024. / 1024.);
            printf("----------------------------------------------------------------\n");
        }

        printf("Num Thread(1~%u) :", gc_MaxThread);
        if(bUseFileData)
            nNumThread = (DWORD)fdata.nThread, printf("%llu\n", fdata.nThread);
        else
            std::cin >> nNumThread;
        bUsingThread = ((nNumThread==1)? TRUE : FALSE);
        if(gc_MaxThread < nNumThread)
            nNumThread = gc_MaxThread;

        nCNTPerThread = nTotalCNT / nNumThread;


        {
            UINT64 nCNTPerThread64 = nCNTPerThread;
            #if !_DEF_USING_RANDOMSIZE_ALLOC
            printf("----------------------------------------------------------------\n");
            printf("Thread count : %lu\n", nNumThread);
            printf("%lu *   CMD CNT : %llu\n", nNumThread, nCNTPerThread64);
            printf("%lu *   %6.4f MB\n", nNumThread, nTotalCNT * _TYPE_SIZE / 1024. / 1024. / nNumThread);
            #else
            printf("----------------------------------------------------------------\n");
            printf("Thread count : %lu\n", nNumThread);
            printf("%lu *   CMD CNT : %llu\n", nNumThread, nCNTPerThread64);
            printf("%lu *   %6.4f MB\n", nNumThread, iSize_UseMemory / 1024. / 1024. / nNumThread);
            #endif
        }

        std::cout << "Write/Read Test(Y/N) : ";
        if(bUseFileData)
            bTest_Value = fdata.bTestReadWrite, printf("%c\n", fdata.bTestReadWrite? 'Y' : 'N');
        else
            bTest_Value = gFN_Get_Input_YN();

        const char *ppAllocatorName[] =
        {
            "",
            #if _DEF_USING_MEMORYPOOL_VER == 0
            "Memory Pool Other Version",
            #else
            "Memory Pool",
            #endif
            "LFH",
            TBB_Initialize::s_szName[0],
            TBB_Initialize::s_szName[1],
        };
        {
            const int cnt = sizeof(ppAllocatorName) / sizeof(char*);
            for(int i=1; i<cnt; i++)
                printf("[%d] %s\n", i, ppAllocatorName[i]);
            printf("Select Allocator(1~%d) : ", cnt-1);
        }

        int iAllocator = 0;
        for(BOOL bLOOP = TRUE; bLOOP;)
        {
            bLOOP = FALSE;

            std::cin >> iAllocator;
            switch(iAllocator)
            {
            case 1:
                gpFN_NEW_DELETE_POOL = gFN_NEW_DELETE__POOL;
                bUseMemoryPool = TRUE;
                #if _DEF_USING_MEMORYPOOL_VER != 0
                pEngine = (ENGINE::CEngine*)_aligned_malloc(sizeof(ENGINE::CEngine), _DEF_CACHELINE_SIZE);
                new(pEngine) ENGINE::CEngine(TRUE);
                #endif
                #if 30 <= _DEF_USING_MEMORYPOOL_VER && _DEF_USING_MEMORYPOOL_VER < 40
                if(CORE::g_hDLL_Core)
                {
                    #if _DEF_USING_DEBUG_MEMORY_LEAK
                    CORE::CoreAlloc = (void*(*)(size_t, const char*, int))::GetProcAddress(CORE::g_hDLL_Core, "CoreMemAlloc");
                    #else
                    CORE::CoreAlloc = (void*(*)(size_t))::GetProcAddress(CORE::g_hDLL_Core, "CoreMemAlloc");
                    #endif
                    if(CORE::CoreAlloc)
                        _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("Connected CoreAlloc...\n");

                    #if _DEF_USING_MEMORYPOOL_VER == 33
                    CORE::CoreFree = (void(*)(void*)) ::GetProcAddress(CORE::g_hDLL_Core, "CoreMemFreeQ");
                    #else
                    CORE::CoreFree = (void(*)(void*)) ::GetProcAddress(CORE::g_hDLL_Core, "CoreMemFree");
                    #endif
                    if(CORE::CoreFree)
                        _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("Connected CoreFree...\n");
                }
                #endif
                break;
            case 2:
                gpFN_NEW_DELETE_OTHER = gFN_NEW_DELETE_LFH;
                break;
            case 3:
            case 4:
                gpFN_NEW_DELETE_OTHER = gFN_NEW_DELETE_TBB;
                break;
            default:
                bLOOP = TRUE;
            }
            if(!bLOOP)
            {
                std::cout << ppAllocatorName[iAllocator];
                if(gpFN_NEW_DELETE_POOL == gFN_NEW_DELETE__POOL)
                {
                    #if _DEF_USING_MEMORYPOOL_VER == 0
                    printf(" Other Version...\n");
                    #else
                    printf("(Interface Case %d)...\n", _DEF_USING_MEMORYPOOL_VER);
                    #endif
                }
                else
                    printf("...\n");
            }
        }
        #ifdef _DEF_TEST_TBBMALLOC
        if(gpFN_NEW_DELETE_OTHER == gFN_NEW_DELETE_TBB)
            tbbload.Initialize(iAllocator-3);
        #endif

        #if !_DEF_USING_RANDOMSIZE_ALLOC
        if(bUseMemoryPool)
        {

            UINT64 iSize_UseMemory64 = iSize_UseMemory;
            printf("Reserve MemoryPool (0~%lluMB) : ", iSize_UseMemory64/1024/1024);

            size_t iReserve_MemPool; // MB , CNT
            std::cin >> iReserve_MemPool;
            if(0 != iReserve_MemPool)
            {
                // printf("MemoryPool::Add Reserve %u\n", iReserve_MemPool);
                //UTIL::MEM::gFN_Get_MemoryPool<CAm>()->mFN_Initialize(전체크기 / 파라미터2, 페이지크기 배수에 낭비가 적게 들어갈 크기);

                printf("MemoryPool::mFN_Add_Reserve %IuMB\n", iReserve_MemPool);
                UTIL::MEM::gFN_Get_MemoryPool<CAm>()->mFN_Reserve_Memory__nSize(iReserve_MemPool*1024*1024);

                //UTIL::MEM::gFN_Get_MemoryPool<CAm>()->mFN_Reserve_Memory__nUnits(iReserve_MemPool*1024*1024/_TYPE_SIZE);
            }
        }
        #endif

        if(1 < nNumThread)
        {
            hEvent_Start = ::CreateEvent(NULL, TRUE, FALSE, NULL);
            unsigned dwThreadID;
            for(size_t nCntThread = 1; nCntThread < nNumThread; nCntThread++)
            {
                hThreads[nCntThread] = (HANDLE)::_beginthreadex(NULL, 0, ThreadFunction, (void*)nCntThread, 0, &dwThreadID);
                hEvents_Done[nCntThread] = ::CreateEvent(NULL, TRUE, FALSE, NULL);
            }
        }
    }
    #pragma endregion
    //    CAm::__Get_MemoryPool<CAm>()->mFN_Add_Reserve(10000000);

    double dElapsed = MainThreadFunction();
    double dAverage = dElapsed / nLoopTest;

    for(size_t nCntThread = 1; nCntThread < nNumThread; nCntThread++)
    {
        ::CloseHandle(hThreads[nCntThread]);
        ::CloseHandle(hEvents_Done[nCntThread]);
    }
    _SAFE_CLOSE_HANDLE(hEvent_Start);

    printf("----------------------------------------------------------------\n");
    printf("Total   %04.4f Sec\n", dElapsed);
    printf("Average %04.4f Sec\n", dAverage);
    printf("----------------------------------------------------------------\n");
    GetInfo_ProcessMemoryInfo();
    Report_ProcessMemoryInfo();
    printf("----------------------------------------------------------------\n");

    system("pause");
    #if _DEF_USING_MEMORYPOOL_VER != 0
    if(pEngine)
    {
        pEngine->~CEngine();
        _aligned_free(pEngine);
    }
    #endif
}