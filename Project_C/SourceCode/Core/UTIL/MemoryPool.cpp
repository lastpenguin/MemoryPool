#include "stdafx.h"
#if _DEF_MEMORYPOOL_MAJORVERSION == 1
#include "./MemoryPool.h"

#include "./ObjectPool.h"

#include "../Core.h"
/*----------------------------------------------------------------
/   메모리 유닛 관리 형태(스택/큐)
/       가장 많이 사용하게될 동적메모리 수요 패턴을 예측하자면 다음과 같다
/           T** ppT; for(;;) ppT[i] = new... for(;;) delete ppT[i]...
/       이것은 메모리 관리자에서 [....] 에서 pop front 한후
/       반납받아 push front 하게 되면 0 1 2 3 4 ... 등이 3 2 1 0 4... 식으로 뒤집힌다
/       이것을 다시 사용자가 같은식으로 할당하면 사용자가 쓰게될 메모리는 역순으로 뒤집힌상태다
/       가능한 사용자가 빠른주소~느린주소 순으로 사용이 가능하도록, 하기 위해
/       pop front 한후 push back을 사용한다
/-----------------------------------------------------------------
/   잠재적 오류 수정
/       CMemoryPool::mFN_Get_Memory_Process()
/           #7 [all] Basket의 지역 저장소 확인
/           if(mFN_Fill_Basket_from_AllOtherBasketLocalStorage(mb))
/           mb1 mb2 가 서로 같은 로직에 진입하여(스스로 잠근 상태 에서)
/           상호간에 조사를 위해 잠금을 시도한다면 데드락에 빠지게 된다
/           SpinLock에 대기 스핀횟수를 파라미터로 하여 사용
/   메모리풀 전역 접근 인덱스에 사용하는 모든크기가 들어가도록 수정
/       테이블 크기또한 줄임
/-----------------------------------------------------------------
/   m_map_Lend_MemoryUnits
/       디버깅시 좀더 빠르게 처리가 가능한가?
/       map -> unordered_map            20.4516 sec -> 18.0069 sec
/-----------------------------------------------------------------
/   불필요해진 기능 제거
/       _DEF_USING_MEMORYPOOLMANAGER_SAFEDELETE
/-----------------------------------------------------------------
/   치명적 오류 수정
/       대용량의 메모리가 Basket에 갇히는 현상
/       이 문제는 1스레드 동작일때 스레드 친화도를 설정안하고 OS가 n개의 프로세서를 사용할때 간혹 발생
/       오류 재현이 매우 어려운 문제가 있었다
/       Report 결과 수요가 0인 Basket에 대량의 메모리가 보관된 현상을 확인
/       mFN_Return_Memory_Process_Default 에서 Basket에서 cache로 이동시킬때 일정수 이상만 이동시키기 위한 조건이 잘못되어 대용량의 메모리가 basket에 고립되는 현상
/       if(mb.m_nDemand < GLOBAL::gc_LimitMinCounting_List) 유닛수가 아닌 수요와 비교한게 잘못
/----------------------------------------------------------------*/
#define _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A

#define _DEF_INLINE_CHANGE_DEMAND __forceinline


namespace UTIL{
namespace MEM{
    namespace GLOBAL{
    #pragma region 설정(빌드타이밍)
        // 리스트 자료구조 카운팅 제한
        // 기본128, 클럭이 낮은 CPU 대상으로는 낮춰 사용
        const size_t gc_LimitMinCounting_List = 4;
        const size_t gc_LimitMaxCounting_List = 128;
        // 유닛 크기 계산에 사용하는 환경변수
        // 이 값을 변경시 gc_Size_Table_MemoryPool , gFN_Get_MemoryPoolTableIndex 등을 업데이트할것
        const size_t gc_KB = 1024;
        const size_t gc_MB = gc_KB * 1024;
        const size_t gc_Array_Limit[]   = {256 , 512 , 1*gc_KB , 2*gc_KB , 4*gc_KB , 8*gc_KB , 16*gc_KB , 64*gc_KB , 512*gc_KB , gc_MB    , 10*gc_MB};
        const size_t gc_Array_MinUnit[] = {8   , 16  , 32      , 64      , 128     , 256     , 512      , 1*gc_KB  , 8*gc_KB   , 64*gc_KB , 512*gc_KB};
        const size_t gc_minSize_of_MemoryPoolUnitSize = 8;
        const size_t gc_maxSize_of_MemoryPoolUnitSize = gc_Array_Limit[_MACRO_ARRAY_COUNT(gc_Array_Limit) - 1];

        const size_t gc_SmallUnitSize_Limit = 2048;

        // 실행속도의 하락을 예상하는 너무 많은 CPU 수(실제 동시 스레드)
        const size_t gc_Max_ThreadAccessKeys = 16;
        // 정렬 단위
        const size_t gc_AlignSize_LargeUnit = _DEF_CACHELINE_SIZE;
        // 최소 페이지유닛 크기는 Windows: 64KB
        const size_t gc_minPageUnitSize = 64 * 1024;
        _MACRO_STATIC_ASSERT(_DEF_CACHELINE_SIZE == 64);
    #pragma endregion

    #pragma region 전역 데이터
        // 메모리풀 바로가기 테이블
        // 354 슬롯 x86(1.38KB) x64(2.76KB)
        const size_t gc_Size_Table_MemoryPool = gc_KB / 8
            + (gc_KB*4 - gc_KB) / 64
            + (gc_KB*16 - gc_KB*4) / 256
            + (gc_KB*64 - gc_KB*16) / gc_KB
            + (gc_KB*512 - gc_KB*64) / (gc_KB*8)
            + (gc_MB - gc_KB*512) / (gc_KB*64)
            + (gc_MB*10 - gc_MB) / (gc_KB*512);
        CMemoryPool* g_pTable_MemoryPool[gc_Size_Table_MemoryPool] = {nullptr};

        // 무결성 확인용 테이블
        _DEF_CACHE_ALIGN CTable_DataBlockHeaders    g_Table_BlockHeaders_Normal;
        _DEF_CACHE_ALIGN CTable_DataBlockHeaders    g_Table_BlockHeaders_Big;
    #pragma endregion



    #pragma region 동적 설정
        _DEF_CACHE_ALIGN byte g_Array_Key__CpuCore_to_CACHE[64] = {0};

    #if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT
        #if __X64
        size_t g_ArrayProcessorCode[64] = {0};
        #elif __X86
        size_t g_ArrayProcessorCode[32] = {0};
        #else
        size_t g_ArrayProcessorCode[?] = {0};
        #endif
    #endif
    #if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
        TMemoryPool_TLS_CACHE&(*gpFN_TlsCache_AccessFunction)(void) = nullptr;
    #endif

        size_t g_nProcessor = 1;
        size_t g_nBaskets   = 1;

        size_t g_nBasket__CACHE = 1;
        size_t g_nCore_per_Moudule = 1;  // (만약 배수가 아닌 비율이라면 1로 유지한다)
        size_t g_iCACHE_AccessRate = 1;

        BOOL g_bDebug__Trace_MemoryLeak = FALSE;
        BOOL g_bDebug__Report_OutputDebug = FALSE;
    #pragma endregion


        size_t gFN_Get_MemoryPoolTableIndex(size_t _Size)
        {
            _Assert(_Size >= 1);

            _Size -= 1;

            const size_t l[] = {gc_KB,  gc_KB*4,    gc_KB*16,   gc_KB*64,   gc_KB*512,  gc_MB,      gc_MB*10};
            const size_t u[] = {8,      64,         256,        gc_KB,      gc_KB*8,    gc_KB*64,   gc_KB*512};
            const size_t s0 = 0;
            const size_t s1 = s0 + l[0] / u[0];
            const size_t s2 = s1 + (l[1]-l[0]) / u[1];
            const size_t s3 = s2 + (l[2]-l[1]) / u[2];
            const size_t s4 = s3 + (l[3]-l[2]) / u[3];
            const size_t s5 = s4 + (l[4]-l[3]) / u[4];
            const size_t s6 = s5 + (l[5]-l[4]) / u[5];


        #define __temp_macro_return_index(N) if(_Size < l[N]) return (_Size-l[N-1])/u[N] + s##N

            if(_Size < l[0])
                return _Size / u[0];
            __temp_macro_return_index(1);
            __temp_macro_return_index(2);
            __temp_macro_return_index(3);
            __temp_macro_return_index(4);
            __temp_macro_return_index(5);
            __temp_macro_return_index(6);

            return MAXSIZE_T;
        #undef __temp_macro_return_index
        }
        void gFN_Set_MemoryPoolTable(size_t _Index, CMemoryPool* p)
        {
            if(_Index >= gc_Size_Table_MemoryPool)
                return;

            g_pTable_MemoryPool[_Index] = p;
        }
        DECLSPEC_NOINLINE size_t gFN_Calculate_UnitSize(size_t _Size)
        {
            // 이 함수의 호출은 비율은 매우 낮다
            // 이 함수는 다음의 함수에서 호출하는 것을 제한한다 : CMemoryPool_Manager::mFN_Get_MemoryPool(size_t _Size)
            if(gc_maxSize_of_MemoryPoolUnitSize < _Size)
                return 0;
            if(!_Size)
                return _Size;

        #pragma message("gFN_Calculate_UnitSize 수정시 체크 사항: 할당정렬 단위의 배수는 캐시라인의 크기의 배수 또는 약수")

            // 풀을 다양하게 분할할 수록 스레드가 단독접근할 확률은 올라가겠지만,
            // 핸들을 많이 소모한다

            // ※ 캐시라인 크기를 기준으로 작성한다
            //    (8 미만은 8단위 유닛을 사용한다)
            //  256 이하 8의 배수
            //      8 16 .. 244 256                     [32]
            //  512 이하 16의 배수
            //      272 288 .. 496 512                  [16]        최대 낭비 6.25% 미만
            //  1KB 이하 32의 배수
            //      544 576 .. 992 1024                 [16]        최대 낭비 6.25% 미만
            //  2KB 이하 64의 배수
            //      1088 1152 .. 1984 2048              [16]        최대 낭비 6.25% 미만
            //  4KB 이하 128의 배수
            //      2176 2304 .. 3968 4096              [16]        최대 낭비 6.25% 미만
            //  8KB 이하 256의 배수
            //      4352 4608 .. 7936 8192              [16]        최대 낭비 6.25% 미만
            //  16KB 이하 512의 배수
            //      8704 9216 .. 12872 16384            [16]        최대 낭비 6.25% 미만
            //----------------------------------------------------------------
            //  64KB 이하 1KB의 배수
            //      17KB 18KB .. 63KB 64KB              [48]        최대 낭비 6.25% 미만
            //----------------------------------------------------------------
            //  512KB 이하 8KB의 배수
            //      72KB 80KB .. 504KB 512KB            [56]        최대 낭비 12.5% 미만
            //  1MB 이하 64KB의 배수
            //      576KB 640KB .. 960KB 1024KB         [8]         최대 낭비 12.5% 미만
            //  10MB 이하 초과 512KB의 배수
            //      1.5MB 2MB .. 9.5MB 10MB             [18]        최대 낭비 50% 미만
            //  10MB 초과 무시
        #if _DEF_CACHELINE_SIZE != 64
        #error _DEF_CACHELINE_SIZE != 64
        #endif


        #pragma warning(push)
        #pragma warning(disable: 4127)
            _Assert((_MACRO_ARRAY_COUNT(gc_Array_Limit) == _MACRO_ARRAY_COUNT(gc_Array_MinUnit)));
        #pragma warning(pop)

            for(int i=0; i<_MACRO_ARRAY_COUNT(gc_Array_Limit); i++)
            {
                if(_Size <= gc_Array_Limit[i])
                {
                    const auto m = gc_Array_MinUnit[i];
                    return (_Size + m - 1) / m * m;
                }
            }

            // 0 크기는 나올수 없다
            _DebugBreak("Return 0 : Calculate_UnitSize");
            return gc_minSize_of_MemoryPoolUnitSize;
        }

#if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT
        __forceinline size_t gFN_Get_SIDT()
        {
        #pragma pack(push, 1)
            struct idt_t
            {
                UINT16  limit;
                size_t  base;
            };
            // base size : 16bit(16) 32bit(32) 64bit(64)
        #pragma pack(pop)
            idt_t idt;
            __sidt(&idt);
            return idt.base;
        }
#endif


    #pragma region Baskets , Basket_CACHE 의 실행타이밍에 의한 동적배수에 해당하는 오브젝트풀의 고속 접근을 위한 함수
        TMemoryBasket* gFN_Baskets_Alloc__Default()
        {
            return (TMemoryBasket*)::_aligned_malloc(sizeof(TMemoryBasket)*g_nProcessor, _DEF_CACHELINE_SIZE);
        }
        void gFN_Baskets_Free__Default(void* p)
        {
            ::_aligned_free(p);
        }
        template<size_t Size>
        TMemoryBasket* gFN_Baskets_Alloc()
        {
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket)>::Reference_Attach();
            return (TMemoryBasket*)CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket)>::Alloc();
        }
        template<size_t Size>
        void gFN_Baskets_Free(void* p)
        {
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket)>::Free(p);
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket)>::Reference_Detach();
        }

        template<size_t Size>
        TMemoryBasket_CACHE* gFN_BasketCACHE_Alloc()
        {
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket_CACHE)>::Reference_Attach();
            return (TMemoryBasket_CACHE*)CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket_CACHE)>::Alloc();
        }
        template<size_t Size>
        void gFN_BasketCACHE_Free(void* p)
        {
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket_CACHE)>::Free(p);
            CObjectPool_Handle_TSize<Size*sizeof(TMemoryBasket_CACHE)>::Reference_Detach();
        }
        TMemoryBasket_CACHE* gFN_BasketCACHE_Alloc__Default()
        {
            return (TMemoryBasket_CACHE*)::_aligned_malloc(sizeof(TMemoryBasket_CACHE)*g_nBasket__CACHE, _DEF_CACHELINE_SIZE);
        }
        void gFN_BasketCACHE_Free__Default(void* p)
        {
            ::_aligned_free(p);
        }

        TMemoryBasket* (*gpFN_Baskets_Alloc)(void) = nullptr;
        void (*gpFN_Baskets_Free)(void*) = nullptr;
        TMemoryBasket_CACHE* (*gpFN_BasketCACHE_Alloc)(void) = nullptr;
        void (*gpFN_BasketCACHE_Free)(void*) = nullptr;
    #pragma endregion

    }
    using namespace GLOBAL;
}
}


/*----------------------------------------------------------------
/   메모리풀
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{



    CMemoryPool__BadSize::CMemoryPool__BadSize()
        : m_UsingSize(0)
        , m_UsingCounting(0)
        , m_bWriteStats_to_LogFile(TRUE)
        , m_stats_Maximum_AllocatedSize(0)
        , m_stats_Counting_Free_BadPTR(0)
    {
    }

    CMemoryPool__BadSize::~CMemoryPool__BadSize()
    {
        mFN_Debug_Report();
    }
    //----------------------------------------------------------------
    #if _DEF_USING_DEBUG_MEMORY_LEAK
    void * CMemoryPool__BadSize::mFN_Get_Memory(size_t _Size, const char * _FileName, int _Line)
    {
        const size_t _RealSize = mFN_Calculate_UnitSize(_Size) + sizeof(TDATA_BLOCK_HEADER);
        TDATA_BLOCK_HEADER* p = (TDATA_BLOCK_HEADER*)::_aligned_malloc_dbg(_RealSize,  _DEF_CACHELINE_SIZE, _FileName, _Line);
        if(!p)
            return nullptr;

        p->ParamBad.SizeThisUnit = _RealSize;


        InterlockedExchangeAdd(&m_UsingCounting, 1);
        const auto nUsingSizeNow = InterlockedExchangeAdd(&m_UsingSize, _RealSize) + _RealSize;
        for(;;)
        {
            const auto nMaxMemNow = m_stats_Maximum_AllocatedSize;
            if(nMaxMemNow >= nUsingSizeNow)
            {
                break;
            }
            else //if(nMaxMemNow < nUsingSizeNow)
            {
                if(nMaxMemNow == InterlockedCompareExchange(&m_stats_Maximum_AllocatedSize, nMaxMemNow, nUsingSizeNow))
                    break;
            }
        }

        mFN_Register(p, _RealSize);
        return p+1;
    }
    #else
    void * CMemoryPool__BadSize::mFN_Get_Memory(size_t _Size)
    {
        const size_t _RealSize = mFN_Calculate_UnitSize(_Size) + sizeof(TDATA_BLOCK_HEADER);
        TDATA_BLOCK_HEADER* p = (TDATA_BLOCK_HEADER*)::_aligned_malloc(_RealSize, _DEF_CACHELINE_SIZE);
        if(!p)
            return nullptr;

        p->ParamBad.SizeThisUnit = _RealSize;


        InterlockedExchangeAdd(&m_UsingCounting, 1);
        const auto nUsingSizeNow = InterlockedExchangeAdd(&m_UsingSize, _RealSize) + _RealSize;
        for(;;)
        {
            const auto nMaxMemNow = m_stats_Maximum_AllocatedSize;
            if(nMaxMemNow >= nUsingSizeNow)
            {
                break;
            }
            else //if(nMaxMemNow < nUsingSizeNow)
            {
                if(nMaxMemNow == InterlockedCompareExchange(&m_stats_Maximum_AllocatedSize, nMaxMemNow, nUsingSizeNow))
                    break;
            }
        }

        mFN_Register(p, _RealSize);
        return p+1;
    }
    #endif

    void CMemoryPool__BadSize::mFN_Return_Memory(void * pAddress)
    {
        if(!pAddress)
            return;

        TMemoryObject* pUnit = static_cast<TMemoryObject*>(pAddress);

        // 사용자용 함수이기 때문에 검증을 한다
        if(!mFN_TestHeader(pUnit))
        {
            InterlockedExchangeAdd(&m_stats_Counting_Free_BadPTR, 1);
            _DebugBreak("잘못된 주소 반환 또는 손상된 식별자");
            return;
        }

        mFN_Return_Memory_Process(pUnit);
    }
    void CMemoryPool__BadSize::mFN_Return_MemoryQ(void* pAddress)
    {
        #ifdef _DEBUG
        CMemoryPool__BadSize::mFN_Return_Memory(pAddress);
        #else
        if(!pAddress)
            return;

        mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
        #endif
    }
    //----------------------------------------------------------------
#pragma region 제 기능을 하지 않는 메소드
    size_t CMemoryPool__BadSize::mFN_Query_This_UnitSize() const
    {
        return 0;
    }
    BOOL CMemoryPool__BadSize::mFN_Query_Stats(TMemoryPool_Stats* p)
    {
        if(!p)
            return FALSE;

        ::memset(p, 0, sizeof(TMemoryPool_Stats));
        // 바쁜 비교
        for(size_t i=0; i<0xffff; i++)
        {
            p->Base.nUnits_Using      = m_UsingCounting;
            p->Base.nCurrentSize_Pool = m_UsingSize;
            p->Base.nMaxSize_Pool     = m_stats_Maximum_AllocatedSize;

            if(p->Base.nUnits_Using != m_UsingCounting)
                continue;
            if(p->Base.nCurrentSize_Pool != m_UsingSize)
                continue;
            if(p->Base.nMaxSize_Pool != m_stats_Maximum_AllocatedSize)
                continue;

            return TRUE;
        }

        return FALSE;
    }
    size_t CMemoryPool__BadSize::mFN_Query_LimitBlocks_per_Expansion()
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_Units_per_Block()
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_MemorySize_per_Block()
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(size_t)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_PreCalculate_ReserveUnits_to_Units(size_t)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(size_t)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_PreCalculate_ReserveMemorySize_to_Units(size_t)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Reserve_Memory__nSize(size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Reserve_Memory__nUnits(size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Set_ExpansionOption__BlockSize(size_t, size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const
    {
        return 0;
    }
#pragma endregion
    //----------------------------------------------------------------
    void CMemoryPool__BadSize::mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On)
    {
        m_bWriteStats_to_LogFile = _On;
    }
    //----------------------------------------------------------------
    void CMemoryPool__BadSize::mFN_Return_Memory_Process(TMemoryObject * pAddress)
    {
        _Assert(pAddress != nullptr);

        // 헤더 손상 확인
    #ifdef _DEBUG
        BOOL bBroken = TRUE;
        // 체크섬 확인도 추가할 것인가?
        const TDATA_BLOCK_HEADER* pH = reinterpret_cast<TDATA_BLOCK_HEADER*>(pAddress) - 1;
        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize){}
        else if(pH->m_pPool != this){}
        else if(!pH->m_pGroupFront || pH->m_pGroupFront->m_pPool != this){}
        else if(!pH->mFN_Query_ContainPTR(pAddress)){}
        else if(pH->m_pUserValidPTR_S != pH->m_pUserValidPTR_L){}
        else { bBroken = FALSE; }
        _AssertMsg(FALSE == bBroken, "Broken Header Data");
    #endif

        TDATA_BLOCK_HEADER* p = (TDATA_BLOCK_HEADER*)pAddress;
        p--;

        const auto size = p->ParamBad.SizeThisUnit;
        auto prevSize = InterlockedExchangeSubtract(&m_UsingSize, size);
        auto prevCNT  = InterlockedExchangeSubtract(&m_UsingCounting, 1);
        if(prevSize < size || prevCNT == 0)
        {
        #ifdef _DEBUG
            UINT64 a = prevSize;
            UINT64 b = prevCNT;
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("메모리 반납 무효: 메모리풀 이전값(UsingCounting %d, UsingSize %d Byte) 반납크기(%d Byte)\n", b, a, size);
        #endif
            InterlockedExchangeAdd(&m_UsingSize, size);
            InterlockedExchangeAdd(&m_UsingCounting, 1);
            return;
        }

        mFN_UnRegister(p);
    #if _DEF_USING_DEBUG_MEMORY_LEAK
        ::_aligned_free_dbg(p);
    #else
        ::_aligned_free(p);
    #endif
    }
    BOOL CMemoryPool__BadSize::mFN_TestHeader(TMemoryObject * pAddress)
    {
        _CompileHint(nullptr != pAddress);

        const TDATA_BLOCK_HEADER* pH = reinterpret_cast<const TDATA_BLOCK_HEADER*>(pAddress) - 1;

        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize)
            return FALSE;
        if(nullptr == pH->m_pGroupFront)
            return FALSE;

        const TDATA_BLOCK_HEADER* pHTrust = GLOBAL::g_Table_BlockHeaders_Big.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
        if(pH->m_pGroupFront != pHTrust)
            return FALSE;
        if(!pHTrust->mFN_Query_ContainPTR(pAddress))
            return FALSE;
        // 테스트 하지 않아도 신뢰한다
        //if(pHTrust->m_pPool != this)
        //    return nullptr;
        return TRUE;
    }
    size_t CMemoryPool__BadSize::mFN_Calculate_UnitSize(size_t size) const
    {
        if(!size)
            size = 1;

        return (size - 1 + _DEF_CACHELINE_SIZE) / _DEF_CACHELINE_SIZE * _DEF_CACHELINE_SIZE;
    }
    void CMemoryPool__BadSize::mFN_Register(TDATA_BLOCK_HEADER * p, size_t /*size*/)
    {
        _Assert(p != nullptr);

        p->m_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize;

        p->m_pUserValidPTR_S = p+1;
        p->m_pUserValidPTR_L = p+1;

        p->m_pPool = this;
        p->m_pGroupFront = p;

        GLOBAL::g_Table_BlockHeaders_Big.mFN_Register(p);
    }

    void CMemoryPool__BadSize::mFN_UnRegister(TDATA_BLOCK_HEADER * p)
    {
        _Assert(p != nullptr);
        GLOBAL::g_Table_BlockHeaders_Big.mFN_UnRegister(p);
    }
    void CMemoryPool__BadSize::mFN_Debug_Report()
    {
        if(0 < m_UsingCounting || 0 < m_UsingSize)
        {
            UINT64 _64_UsingSize = (UINT64)m_UsingSize;
            UINT64 _64_UsingCounting = (UINT64)m_UsingCounting;
        #if _DEF_USING_DEBUG_MEMORY_LEAK
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("================ MemoryPool : Detected memory leaks ================\n");
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("MemoryPool(Not Managed Size)\n");
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("UsingCounting[%llu] UsingSize[%llu]\n", _64_UsingCounting, _64_UsingSize);
        #endif
            if(m_bWriteStats_to_LogFile)
            {
                _LOG_DEBUG(_T("[Warning] Pool Size[Not Managed Size] : Memory Leak(UsingCounting: %llu , UsingSize: %lluKB)"), _64_UsingCounting, _64_UsingSize);
            }
        }
        
        if(0 < m_stats_Counting_Free_BadPTR)
        {
            // 심각한 상황이니 Release 버전에서도 출력한다
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, "================ Critical Error : Failed Count(%Iu) -> MemoryPool[OtherSize]:Free ================"
                , m_stats_Counting_Free_BadPTR);
        }
    }



    CMemoryPool::CMemoryPool(size_t _TypeSize)
        : m_UnitSize(_TypeSize)
        //, m_UnitSize_per_IncrementDemand(?)
        //, m_nLimit_Basket_KeepMax(?)
        //, m_nLimit_Cache_KeepMax(?)
        //, m_nLimit_CacheShare_KeepMax(?)
        , m_pBaskets(nullptr)
        , m_pBasket__CACHE(nullptr)
        // 경계
        , m_nUnit(0), m_pUnit_F(nullptr), m_pUnit_B(nullptr)
        , m_stats_Units_Allocated(0)
        , m_stats_OrderCount__ReserveSize(0)
        , m_stats_OrderCount__ReserveUnits(0)
        , m_stats_OrderSum__ReserveSize(0)
        , m_stats_OrderSum__ReserveUnits(0)
        // 경계
        , m_stats_OrderResult__Reserve_Size(0)
        , m_stats_OrderResult__Reserve_Units(0)
        , m_stats_OrderCount__ExpansionOption_UnitsPerBlock(0)
        , m_stats_OrderCount__ExpansionOption_LimitBlocksPerExpansion(0)
        , m_stats_Counting_Free_BadPTR(0)
        , m_bWriteStats_to_LogFile(TRUE)
        // 경계
        , m_Allocator(this)
        // 경계
        , m_Lock(1)
    #if _DEF_USING_DEBUG_MEMORY_LEAK
        , m_Debug_stats_UsingCounting(0)
    #endif
    {
        // 실행이 곤란한 플렛폼의 예외는 로그나 안내문대신
        // _AssertRelease, _AssertReleaseMsg 로 덤프를 기록한다
#if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
        _AssertReleaseMsg(nullptr != gpFN_TlsCache_AccessFunction, "지정되지 않은 함수 : mFN_Set_TlsCache_AccessFunction 을 사용하십시오");
#endif

        _Assert(sizeof(void*) <= m_UnitSize);

        const UTIL::ISystem_Information* pSys = CORE::g_instance_CORE.mFN_Get_System_Information();
        const SYSTEM_INFO* pInfo = pSys->mFN_Get_SystemInfo();

        // 페이지 단위 크기가 너무 크다면 에러처리
        // 128KB 기준
        _AssertReleaseMsg(128*1024 >= pInfo->dwPageSize, "이 컴퓨터의 페이지 단위 크기가 너무 큽니다");

        // 프로세서 로컬 저장소의 수요증가 단위는 페이지크기(4KB) / 유닛수
        // Val : 1 ~
        m_UnitSize_per_IncrementDemand = pInfo->dwPageSize / m_UnitSize;
        if(m_UnitSize_per_IncrementDemand == 0)
            m_UnitSize_per_IncrementDemand = 1;
        // 프로세서 로컬 저장소의 최대 수요는 VirtualAlloc 시작 주소단위(64KB)를 분할해 정한다
        // Val : 0 ~
        m_nLimit_Basket_KeepMax = pInfo->dwAllocationGranularity / (m_UnitSize * g_nBaskets);
        if(m_nLimit_Basket_KeepMax < GLOBAL::gc_LimitMinCounting_List)
            m_nLimit_Basket_KeepMax = 0; // 유닛 단위가 크다면 메모리 낭비 예방을 위해 Basket의 최대 수요를 0으로

        // 페이지크기(상수)의 시스템 페이지크기 배율 확인
        _AssertRelease(gc_minPageUnitSize >= pInfo->dwAllocationGranularity && (gc_minPageUnitSize % pInfo->dwAllocationGranularity == 0));

        // 캐시의 유닛 최대 보유수는 가장 큰 유닛크기를 기준으로 한다
        m_nLimit_Cache_KeepMax  = gc_maxSize_of_MemoryPoolUnitSize / m_UnitSize;
        if(m_nLimit_Cache_KeepMax < 1)
            m_nLimit_Cache_KeepMax = 1;
        m_nLimit_CacheShare_KeepMax = gc_maxSize_of_MemoryPoolUnitSize * 16 / m_UnitSize;
        _AssertReleaseMsg(1 <= m_nLimit_Cache_KeepMax && 1 <  m_nLimit_CacheShare_KeepMax, "사용 불가능한 유닛 크기");



        m_pBasket__CACHE = gpFN_BasketCACHE_Alloc();
        for(size_t i=0; i<g_nBasket__CACHE; i++)
        {
            _MACRO_CALL_CONSTRUCTOR(m_pBasket__CACHE+i, TMemoryBasket_CACHE);
        }

        m_pBaskets = gpFN_Baskets_Alloc();
        for(size_t i=0; i<g_nBaskets; i++)
        {
            _MACRO_CALL_CONSTRUCTOR(m_pBaskets+i, TMemoryBasket(g_Array_Key__CpuCore_to_CACHE[i]));
        }

#if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(_T(" - MemoryPool Constructor [%u]\n"), m_UnitSize);
#endif
    }
    CMemoryPool::~CMemoryPool()
    {
        if(m_Lock.Query_isLocked())
        {
            _LOG_DEBUG(_T("[Critical Error] Pool Size[%u] : is Locked ... from Destructor"), m_UnitSize);
        }

        mFN_Debug_Report();
        
        for(size_t i=0; i<g_nBaskets; i++)
            _MACRO_CALL_DESTRUCTOR(m_pBaskets + i);
        gpFN_Baskets_Free(m_pBaskets);

        for(size_t i=0; i<g_nBasket__CACHE; i++)
            _MACRO_CALL_DESTRUCTOR(m_pBasket__CACHE + i);
        gpFN_BasketCACHE_Free(m_pBasket__CACHE);

#if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(_T(" - MemoryPool Destructor [%u]\n"), m_UnitSize);
#endif
    }
    //----------------------------------------------------------------
    #if _DEF_USING_DEBUG_MEMORY_LEAK
    void* CMemoryPool::mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line)
    {
        if(_Size > m_UnitSize)
        {
            _DebugBreak("_Size > m_UnitSize");
            return nullptr;
        }
        auto p = CMemoryPool::mFN_Get_Memory_Process();
        if(p)
        {
            InterlockedExchangeAdd(&m_Debug_stats_UsingCounting, 1);
            if(GLOBAL::g_bDebug__Trace_MemoryLeak)
            {
                m_Lock_Debug__Lend_MemoryUnits.Lock();
                m_map_Lend_MemoryUnits.insert(std::make_pair(p, TTrace_SourceCode(_FileName, _Line)));
                m_Lock_Debug__Lend_MemoryUnits.UnLock();
            }
        }
        return p;
    }
    #else
    void* CMemoryPool::mFN_Get_Memory(size_t _Size) 
    {
        if(_Size <= m_UnitSize)
            return CMemoryPool::mFN_Get_Memory_Process();

        return nullptr;
    }
    #endif
    void CMemoryPool::mFN_Return_Memory(void* pAddress)
    {
        if(!pAddress)
            return;

        TMemoryObject* pUnit = static_cast<TMemoryObject*>(pAddress);

        // 사용자용 함수이기 때문에 검증을 한다
        if(!mFN_TestHeader(pUnit))
        {
            InterlockedExchangeAdd(&m_stats_Counting_Free_BadPTR, 1);
            _DebugBreak("잘못된 주소 반환 또는 손상된 식별자");
            return;
        }

        CMemoryPool::mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pUnit));
    }
    void CMemoryPool::mFN_Return_MemoryQ(void* pAddress)
    {
        #ifdef _DEBUG
        CMemoryPool::mFN_Return_Memory(pAddress);
        return;
        #else
        if(!pAddress)
            return;

        TMemoryObject* pUnit = static_cast<TMemoryObject*>(pAddress);
        CMemoryPool::mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pUnit));
        #endif
    }
    //----------------------------------------------------------------
    size_t CMemoryPool::mFN_Query_This_UnitSize() const
    {
        return m_UnitSize;
    }
    BOOL CMemoryPool::mFN_Query_Stats(TMemoryPool_Stats * pOUT)
    {
        if(!pOUT) return FALSE;
        ::memset(pOUT, 0, sizeof(TMemoryPool_Stats));
        auto& r = *pOUT;

        m_Lock.Begin_Write__INFINITE();
        mFN_all_Basket_Lock();
        mFN_all_BasketCACHE_Lock();
        {
            r.Base.nUnits_Created   = m_stats_Units_Allocated;
            r.Base.nUnits_Using     = m_stats_Units_Allocated - mFN_Counting_KeepingUnits();
            r.Base.nCount_Expansion     = m_Allocator.m_stats_cnt_Succeed_VirtualAlloc;
            r.Base.nTotalSize_Expansion = m_Allocator.m_stats_size_TotalAllocated;

            r.UserOrder.ExpansionOption_OrderCount_UnitsPerBlock            = m_stats_OrderCount__ExpansionOption_UnitsPerBlock;
            r.UserOrder.ExpansionOption_OrderCount_LimitBlocksPerExpansion  = m_stats_OrderCount__ExpansionOption_LimitBlocksPerExpansion;
            r.UserOrder.Reserve_OrderCount_Size         = m_stats_OrderCount__ReserveSize;
            r.UserOrder.Reserve_OrderCount_Units        = m_stats_OrderCount__ReserveUnits;
            r.UserOrder.Reserve_OrderSum_TotalSize      = m_stats_OrderSum__ReserveSize;
            r.UserOrder.Reserve_OrderSum_TotalUnits     = m_stats_OrderSum__ReserveUnits;
            r.UserOrder.Reserve_result_TotalSize        = m_stats_OrderResult__Reserve_Size;
            r.UserOrder.Reserve_result_TotalUnits       = m_stats_OrderResult__Reserve_Units;
        }
        mFN_all_BasketCACHE_UnLock();
        mFN_all_Basket_UnLock();
        m_Lock.End_Write();

        return TRUE;
    }
    size_t CMemoryPool::mFN_Query_LimitBlocks_per_Expansion()
    {
        m_Lock.Begin_Read();
        const size_t r = m_Allocator.m_nLimitBlocks_per_Expansion;
        m_Lock.End_Read();
        return r;
    }
    size_t CMemoryPool::mFN_Query_Units_per_Block()
    {
        m_Lock.Begin_Read();
        const size_t r = m_Allocator.mFN_Calculate__Units_per_Block();
        m_Lock.End_Read();
        return r;
    }
    size_t CMemoryPool::mFN_Query_MemorySize_per_Block()
    {
        m_Lock.Begin_Read();
        const size_t r = m_Allocator.mFN_Calculate__Size_per_Block();
        m_Lock.End_Read();
        return r;
    }
    //----------------------------------------------------------------
    size_t CMemoryPool::mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(size_t nUnits)
    {
        const size_t nPage = m_Allocator.mFN_Convert__Units_to_PageCount(nUnits);
        const size_t r = m_Allocator.mFN_Convert__PageCount_to_Size(nPage);
        return max(r, CMemoryPool::mFN_Query_MemorySize_per_Block());
    }
    size_t CMemoryPool::mFN_Query_PreCalculate_ReserveUnits_to_Units(size_t nUnits)
    {
        const size_t nSize = CMemoryPool::mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(nUnits);
        const size_t r = m_Allocator.mFN_Convert__Size_to_Units(nSize);
        return max(r, CMemoryPool::mFN_Query_Units_per_Block());
    }
    size_t CMemoryPool::mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(size_t nByte)
    {
        const size_t nPage = m_Allocator.mFN_Convert__Size_to_PageCount(nByte);
        const size_t r = m_Allocator.mFN_Convert__PageCount_to_Size(nPage);
        return max(r, CMemoryPool::mFN_Query_MemorySize_per_Block());
    }
    size_t CMemoryPool::mFN_Query_PreCalculate_ReserveMemorySize_to_Units(size_t nByte)
    {
        const size_t nSize = CMemoryPool::mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(nByte);
        const size_t r =  m_Allocator.mFN_Convert__Size_to_Units(nSize);
        return max(r, CMemoryPool::mFN_Query_Units_per_Block());
    }


    size_t CMemoryPool::mFN_Reserve_Memory__nSize(size_t nByte, BOOL bWriteLog)
    {
        size_t r, n;
        m_Lock.Begin_Write__INFINITE();
        {
            const size_t minimal = m_Allocator.mFN_Calculate__Size_per_Block();
            const size_t nRealByte = max(minimal, nByte);
            r = m_Allocator.mFN_Add_ElementSize(nRealByte, bWriteLog);
            n = m_Allocator.mFN_Convert__Size_to_Units(r);

            m_stats_OrderCount__ReserveSize++;
            m_stats_OrderSum__ReserveSize += nByte;
            m_stats_OrderResult__Reserve_Size += r; 
            m_stats_OrderResult__Reserve_Units += n;
        }
        m_Lock.End_Write();

        return r;
    }

    size_t CMemoryPool::mFN_Reserve_Memory__nUnits(size_t nUnits, BOOL bWriteLog)
    {
        size_t r, n;
        m_Lock.Begin_Write__INFINITE();
        {
            const size_t minimal = m_Allocator.mFN_Calculate__Units_per_Block();
            const size_t nRealUnits = max(minimal, nUnits);
            r = m_Allocator.mFN_Add_ElementSize(nRealUnits * m_UnitSize, bWriteLog);
            n = m_Allocator.mFN_Convert__Size_to_Units(r);

            m_stats_OrderCount__ReserveUnits++;
            m_stats_OrderSum__ReserveUnits += nUnits;
            m_stats_OrderResult__Reserve_Size += r; 
            m_stats_OrderResult__Reserve_Units += n;
        }
        m_Lock.End_Write();

        return n;
    }

    size_t CMemoryPool::mFN_Set_ExpansionOption__BlockSize(size_t nUnits_per_Block, size_t nLimitBlocks_per_Expansion, BOOL bWriteLog)
    {
        if(!nUnits_per_Block && !nLimitBlocks_per_Expansion)
            return 0;

        size_t r;
        m_Lock.Begin_Write__INFINITE();
        {
            BOOL bSucceed = TRUE;
            if(0 < nUnits_per_Block)
            {
                bSucceed &= m_Allocator.mFN_Set_ExpansionOption__Units_per_Block(nUnits_per_Block, bWriteLog);
                m_stats_OrderCount__ExpansionOption_UnitsPerBlock++;
            }
            if(0 < nLimitBlocks_per_Expansion)
            {
                bSucceed &= m_Allocator.mFN_Set_ExpansionOption__LimitBlocks_per_Expansion(nLimitBlocks_per_Expansion, bWriteLog);
                m_stats_OrderCount__ExpansionOption_LimitBlocksPerExpansion++;
            }

            r = (bSucceed)? m_Allocator.mFN_Calculate__Units_per_Block() : 0;
        }
        m_Lock.End_Write();

        return r;
    }
    size_t CMemoryPool::mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const
    {
        return m_Allocator.mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion();
    }
    //----------------------------------------------------------------
    void CMemoryPool::mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On)
    {
        m_bWriteStats_to_LogFile = _On;
    }
    //----------------------------------------------------------------
    DECLSPEC_NOINLINE void CMemoryPool::mFN_KeepingUnit_from_AllocatorS(byte* pStart, size_t size)
    {
        _Assert(size % gc_minPageUnitSize == 0 && size >= gc_minPageUnitSize);
        const size_t nPage = size / gc_minPageUnitSize;
        const size_t nUnitsPerPage  = (gc_minPageUnitSize - sizeof(TDATA_BLOCK_HEADER)) / m_UnitSize;
        const size_t SizeValidUnit_Page = nUnitsPerPage * m_UnitSize;
        const size_t SizeValid_Page = sizeof(TDATA_BLOCK_HEADER) + SizeValidUnit_Page;
        const size_t cntNewUnit = nUnitsPerPage * nPage;
        if(!cntNewUnit)
            return;

        byte* pUserS = pStart + sizeof(TDATA_BLOCK_HEADER);
        byte* pUserL = pStart + (((nPage-1)*gc_minPageUnitSize) + SizeValid_Page - m_UnitSize);

        TMemoryObject* pPrevious = nullptr;
        union{
            TMemoryObject* pOBJ = nullptr;
            byte* pobj;
        };

        byte* pCulPage = pStart;
        for(size_t iP=0; iP<nPage; iP++)
        {
            TDATA_BLOCK_HEADER* pHF = (TDATA_BLOCK_HEADER*)pCulPage;

            pHF->m_Type  = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize;
            pHF->m_pPool = this;
            pHF->m_pUserValidPTR_S = pUserS;
            pHF->m_pUserValidPTR_L = pUserL;
            pHF->m_pGroupFront = (TDATA_BLOCK_HEADER*)pStart;
            // 이하 나머지는 쓰레기 값을 넣는다
            // pHF->m_Index

            pobj = pCulPage + sizeof(TDATA_BLOCK_HEADER);
            const byte* pE = pobj + SizeValidUnit_Page;
            if(pPrevious)
                pPrevious->pNext = pOBJ;

            for(;;)
            {
            #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
                mFN_Debug_Overflow_Set(pobj+sizeof(TMemoryObject), m_UnitSize-sizeof(TMemoryObject), 0xDD);
            #endif

                byte* pNext = pobj + m_UnitSize;
                if(pNext >= pE)
                    break;

                pOBJ->pNext = (TMemoryObject*)pNext;
                pobj = pNext;
            }
            pPrevious = pOBJ;

            pCulPage += gc_minPageUnitSize;
        }
        pOBJ->pNext = nullptr;

        TMemoryObject* pUnitF = (TMemoryObject*)(pStart + sizeof(TDATA_BLOCK_HEADER));
        TMemoryObject* pUnitL = pOBJ;

        MemoryPool_UTIL::sFN_PushBackAll(m_pUnit_F, m_pUnit_B, pUnitF, pUnitL);
        m_nUnit                 += cntNewUnit;
        m_stats_Units_Allocated += cntNewUnit;
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_KeepingUnit_from_AllocatorS_With_Register(byte * pStart, size_t size)
    {
        _Assert(size % gc_minPageUnitSize == 0 && size >= gc_minPageUnitSize);
        const size_t nPage = size / gc_minPageUnitSize;
        const size_t nUnitsPerPage  = (gc_minPageUnitSize - sizeof(TDATA_BLOCK_HEADER)) / m_UnitSize;
        const size_t SizeValidUnit_Page = nUnitsPerPage * m_UnitSize;
        const size_t SizeValid_Page = sizeof(TDATA_BLOCK_HEADER) + SizeValidUnit_Page;
        const size_t cntNewUnit = nUnitsPerPage * nPage;
        if(!cntNewUnit)
            return;

        byte* pUserS = pStart + sizeof(TDATA_BLOCK_HEADER);
        byte* pUserL = pStart + (((nPage-1)*gc_minPageUnitSize) + SizeValid_Page - m_UnitSize);

        TMemoryObject* pPrevious = nullptr;
        union{
            TMemoryObject* pOBJ = nullptr;
            byte* pobj;
        };

        byte* pCulPage = pStart;
        for(size_t iP=0; iP<nPage; iP++)
        {
            // 모든 주소는 전체
            TDATA_BLOCK_HEADER* pHF = (TDATA_BLOCK_HEADER*)pCulPage;
            if(iP == 0)
            {
                pHF->m_Type  = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize;
                pHF->m_pUserValidPTR_S = pUserS;
                pHF->m_pUserValidPTR_L = pUserL;
                pHF->m_pPool = this;
                pHF->m_pGroupFront = pHF;

                g_Table_BlockHeaders_Normal.mFN_Register(pHF);
            }
            else
            {
                TDATA_BLOCK_HEADER* pFirstHead = (TDATA_BLOCK_HEADER*)pStart;
                *pHF = *pFirstHead;
            }

            pobj = pCulPage + sizeof(TDATA_BLOCK_HEADER);
            const byte* pE = pobj + SizeValidUnit_Page;
            if(pPrevious)
                pPrevious->pNext = pOBJ;

            for(;;)
            {
            #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
                mFN_Debug_Overflow_Set(pobj+sizeof(TMemoryObject), m_UnitSize-sizeof(TMemoryObject), 0xDD);
            #endif

                byte* pNext = pobj + m_UnitSize;
                if(pNext >= pE)
                    break;

                pOBJ->pNext = (TMemoryObject*)pNext;
                pobj = pNext;
            }
            pPrevious = pOBJ;

            pCulPage += gc_minPageUnitSize;
        }
        pOBJ->pNext = nullptr;

        TMemoryObject* pUnitF = (TMemoryObject*)(pStart + sizeof(TDATA_BLOCK_HEADER));
        TMemoryObject* pUnitL = pOBJ;

        MemoryPool_UTIL::sFN_PushBackAll(m_pUnit_F, m_pUnit_B, pUnitF, pUnitL);
        m_nUnit                 += cntNewUnit;
        m_stats_Units_Allocated += cntNewUnit;
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_KeepingUnit_from_AllocatorN_With_Register(byte * pStart, size_t size)
    {
        _Assert(size % gc_minPageUnitSize == 0 && size >= gc_minPageUnitSize);

        const size_t nRealUnitSize = m_UnitSize + sizeof(TDATA_BLOCK_HEADER);
        const size_t cntNewUnit = size / nRealUnitSize;
        const size_t TotalRealSize = cntNewUnit * nRealUnitSize;

        if(!cntNewUnit)
            return;

        byte* pUserS = pStart + sizeof(TDATA_BLOCK_HEADER);
        byte* pUserL = pStart + (TotalRealSize - m_UnitSize);

        TDATA_BLOCK_HEADER* pFirstHeader = (TDATA_BLOCK_HEADER*)pStart;
        TMemoryObject* pFirstOBJ = (TMemoryObject*)(pFirstHeader+1);
        {
            pFirstHeader->m_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize;
            pFirstHeader->m_pUserValidPTR_S = pUserS;
            pFirstHeader->m_pUserValidPTR_L = pUserL;
            pFirstHeader->m_pPool = this;
            pFirstHeader->m_pGroupFront = pFirstHeader;

            g_Table_BlockHeaders_Normal.mFN_Register(pFirstHeader);
        #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
            mFN_Debug_Overflow_Set(pFirstOBJ+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
        #endif
            pFirstOBJ->pNext = (TMemoryObject*)((byte*)pFirstOBJ + nRealUnitSize);
        }

        union{
            TDATA_BLOCK_HEADER* pHead;
            TMemoryObject* pOBJ;
            byte* pobj;
        };
        const byte* pE = pStart + TotalRealSize;
        for(pobj = (byte*)pFirstOBJ+m_UnitSize; pobj < pE;)
        {
            *pHead = *pFirstHeader;
            pHead++;
        #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
            mFN_Debug_Overflow_Set(pOBJ+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
        #endif
            pOBJ->pNext = (TMemoryObject*)(pobj + nRealUnitSize);
            pobj += m_UnitSize;
        }

        TMemoryObject* pUnitF = pFirstOBJ;
        TMemoryObject* pUnitL = (TMemoryObject*)(pobj - m_UnitSize);
        pUnitL->pNext = nullptr;

        MemoryPool_UTIL::sFN_PushBackAll(m_pUnit_F, m_pUnit_B, pUnitF, pUnitL);
        m_nUnit                 += cntNewUnit;
        m_stats_Units_Allocated += cntNewUnit;
    }


    // GetCurrentProcessorNumberXP 는 XP에서 동작이 가능하다
    //  하지만 DX11 이상을 사용하므로 XP는 제외한다
    //DWORD GetCurrentProcessorNumberXP(void)
    //{
    //    _asm{
    //        mov eax, 1;
    //        cpuid;
    //        shr ebx, 24;
    //        mov eax, ebx;
    //    }
    //}
    DWORD GetCurrentProcessorNumberVerX86()
    {
        int CPUInfo[4];
        __cpuid(CPUInfo, 1);
        // CPUInfo[1] is EBX, bits 24-31 are APIC ID
        if((CPUInfo[3] & (1 << 9)) == 0) return 0;  // no APIC on chip
        return (unsigned)CPUInfo[1] >> 24;
    }
    __forceinline TMemoryBasket& CMemoryPool::mFN_Get_Basket()
    {
#if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
        // TLS 캐시 사용 버전
        auto code = gFN_Get_SIDT();
        size_t Key;
        TMemoryPool_TLS_CACHE& ref_This_Thread__CACHE = gpFN_TlsCache_AccessFunction();
        if(ref_This_Thread__CACHE.m_Code == code)
        {
            // 연속적으로 동작하는 스레드의 현재 프로세서가 바뀌는 빈도는 높지 않다
            Key = ref_This_Thread__CACHE.m_ID;
        }
        else
        {
            // 캐시 미스
            Key = 0;
            do{
                if(g_ArrayProcessorCode[Key] == code)
                    break;
            } while(++Key < g_nProcessor);
            ref_This_Thread__CACHE.m_Code = code;
            ref_This_Thread__CACHE.m_ID = Key;

    #if _DEF_USING_MEMORYPOOL_DEBUG
            // 디버깅용 통계 수집
            m_pBaskets[Key].m_cnt_CacheMiss_from_Get_Basket++;
    #endif
        }
        #if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
        // 현재 스레드가 이 프로세서에서 다시 실행될 확률은 매우 높다
        // 데이터를 바로 읽을수 있도록...
        _mm_prefetch((const char*)&ref_This_Thread__CACHE, _MM_HINT_NTA);
        #endif

#elif _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT
        // TLS 캐시 미사용 버전
        auto code = gFN_Get_SIDT();
        size_t Key = 0;
        do{
            if(g_ArrayProcessorCode[Key] == code)
                break;
        }while(++Key < g_nProcessor);

    #if _DEF_USING_MEMORYPOOL_DEBUG
            // 디버깅용 통계 수집
            // _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE 를 사용하지 않았을때와 비교 하기위해 조사
            m_pBaskets[Key].m_cnt_CacheMiss_from_Get_Basket++;
    #endif
#elif __X64
        size_t Key = GetCurrentProcessorNumber();
#elif __X86
        size_t Key = GetCurrentProcessorNumberVerX86();
#else
#endif

        _Assert(Key < g_nBaskets);
        return m_pBaskets[Key];
    }
    __forceinline TMemoryObject* CMemoryPool::mFN_Get_Memory_Process()
    {
        TMemoryObject* p;
        // Basket의 수요의 유무를 기준으로 분기
        if(m_nLimit_Basket_KeepMax)
            p = mFN_Get_Memory_Process_Default();
        else
            p = mFN_Get_Memory_Process_DirectCACHE();

    #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
        if(p)
        {
            mFN_Debug_Overflow_Check(p+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
            mFN_Debug_Overflow_Set(p, m_UnitSize, 0xCD);
        }
    #endif

        return p;
    }
    namespace{
        __forceinline void gFN_Prefetch_NextMemory(void* pFrontUnit)
        {
            #if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
            if(!pFrontUnit)
                return;
            // 다음 할당을 대비하여 미리 캐시에 로드해둔다
            //      다음 Front 유닛을 정할때 결국 메모리를 읽어야 하기 때문에
            // 이때, pFrontUnit는 현재 스레드에서 사용될 가능성이 높은 것일 것
            // 옵션
            // _MM_HINT_NTA ?
            // _MM_HINT_T0  L1
            // _MM_HINT_T1  L2
            // _MM_HINT_T2  L3  (공유 캐시)
            
            _mm_prefetch((const CHAR*)pFrontUnit, _MM_HINT_T1);
            #else
            // 아무일도 하지 않는다
            #endif
        }
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Get_Memory_Process_Default()
    {
        TMemoryObject* p;
        TMemoryBasket& mb = mFN_Get_Basket();
        mb.Lock();
        for(;;)
        {
            // #1 [p]Basket 확인
            p = mb.mFN_Get_Object();
            if(p)
            {
                gFN_Prefetch_NextMemory(mb.m_Units.m_pUnit_F);
                break;
            }
            else
            {
                mFN_BasketDemand_Expansion(mb);
            }

            // Basket에 보유 유닛이 부족 하다면...
            // #2 [p]Basket::CACHE 확인
            p = mFN_Fill_Basket_from_ThisCache_andRET1(mb);
            if(p)
                break;

            // #3 [pair]Bakset::CACHE 확인
            //      g_nCore_per_Moudule 를 이용해 짝을 찾을 수 있다
            //  아직 이 기능은 필요하지 않다

            // #4 Pool 확인(느슨한 확인)
            if(0 < m_nUnit)
            {
                m_Lock.Begin_Read();// 낮은 우선순위 잠금(읽기 동시진입 1)
                p = mFN_Fill_Basket_from_Pool_andRET1(mb);
                m_Lock.End_Read();

                if(p)
                    break;
            }

            // #5 [all] Basket::CACHE 확인
            p = mFN_Fill_Basket_from_AllOtherBasketCache_andRET1(mb);
            if(p)
                break;

            // #6 Alloc
            m_Lock.Begin_Write__INFINITE();// 높은 우선순위 잠금
            {
                if(0 <m_nUnit)
                    p = mFN_Fill_Basket_from_Pool_andRET1(mb);
                else if(m_Allocator.mFN_Expansion())
                    p = mFN_Fill_Basket_from_Pool_andRET1(mb);
                else
                    p = nullptr;
            }
            m_Lock.End_Write();
            if(p)
                break;

            // #7 [#6] 실패시, [all] Basket의 지역 저장소 확인
            p = mFN_Fill_Basket_from_AllOtherBasketLocalStorage_andRET1(mb);
            if(p)
                break;

            // #END 실패시 덤프 기록
            ::RaiseException((DWORD)E_OUTOFMEMORY, EXCEPTION_NONCONTINUABLE, 0, NULL);
            mb.UnLock();
            return nullptr;
        }
        mb.UnLock();

        return p;
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Get_Memory_Process_DirectCACHE()
    {
        TMemoryObject* p;
        TMemoryBasket& mb = mFN_Get_Basket();
        TMemoryBasket_CACHE& thisCACHE = m_pBasket__CACHE[mb.m_index_CACHE];

        thisCACHE.Lock();
        for(;;)
        {
            // #1 [p]CACHE 확인
            if(0 < thisCACHE.m_Keep.m_nUnit)
            {
                thisCACHE.m_Keep.m_nUnit--;
                p = MemoryPool_UTIL::sFN_PopFront(thisCACHE.m_Keep.m_pUnit_F, thisCACHE.m_Keep.m_pUnit_B);
                gFN_Prefetch_NextMemory(thisCACHE.m_Keep.m_pUnit_F);
                break;
            }
            else if(0 < thisCACHE.m_Share.m_nUnit)
            {
                _Assert(0 == thisCACHE.m_Keep.m_nUnit);

                thisCACHE.m_Share.m_nUnit--;
                p = MemoryPool_UTIL::sFN_PopFront(thisCACHE.m_Share.m_pUnit_F, thisCACHE.m_Share.m_pUnit_B);
                if(0 == thisCACHE.m_Share.m_nUnit)
                    break;

                // 캐시 수요 증가
                mFN_BasektCacheDemand_Expansion(thisCACHE);

                const size_t demand = min(thisCACHE.m_nMax_Keep, GLOBAL::gc_LimitMaxCounting_List);
                if(demand < thisCACHE.m_Share.m_nUnit)
                {
                    thisCACHE.m_Keep.m_nUnit  = demand;
                    thisCACHE.m_Share.m_nUnit -= demand;
                    MemoryPool_UTIL::sFN_PushBackN(thisCACHE.m_Keep.m_pUnit_F, thisCACHE.m_Keep.m_pUnit_B
                        , thisCACHE.m_Share.m_pUnit_F, thisCACHE.m_Share.m_pUnit_B, demand);
                }
                else if(0 < thisCACHE.m_Share.m_nUnit)
                {
                    thisCACHE.m_Keep.m_nUnit  = thisCACHE.m_Share.m_nUnit;
                    thisCACHE.m_Share.m_nUnit = 0;
                    MemoryPool_UTIL::sFN_PushBackAll(thisCACHE.m_Keep.m_pUnit_F, thisCACHE.m_Keep.m_pUnit_B
                        , thisCACHE.m_Share.m_pUnit_F, thisCACHE.m_Share.m_pUnit_B);
                }
                break;
            }

            // 캐시 수요 증가
            mFN_BasektCacheDemand_Expansion(thisCACHE);

            // #2 Pool 확인(느슨한 확인)
            if(0 < m_nUnit)
            {
                m_Lock.Begin_Read();// 낮은 우선순위 잠금(읽기 동시진입 1)
                p = mFN_Fill_CACHE_from_Pool_andRET1(thisCACHE);
                m_Lock.End_Read();
                if(p)
                    break;
            }

            // #3 [all] Basket::CACHE 확인
            p = mFN_Fill_CACHE_from_AllOtherBasketCache_andRET1(thisCACHE);
            if(p)
                break;

            // #6 Alloc
            m_Lock.Begin_Write__INFINITE();// 높은 우선순위 잠금
            {
                if(0 <m_nUnit)
                    p = mFN_Fill_CACHE_from_Pool_andRET1(thisCACHE);
                else if(m_Allocator.mFN_Expansion())
                    p = mFN_Fill_CACHE_from_Pool_andRET1(thisCACHE);
                else
                    p = nullptr;
            }
            m_Lock.End_Write();
            if(p)
                break;

            // #END 실패시 덤프 기록
            ::RaiseException((DWORD)E_OUTOFMEMORY, EXCEPTION_NONCONTINUABLE, 0, NULL);
            thisCACHE.UnLock();
            return nullptr;
        }
        thisCACHE.UnLock();

        return p;
    }

    namespace{
        BOOL gFN_Query_isBadPTR(IMemoryPool* pThisPool, TMemoryObject* pAddress, size_t UnitSize, BOOL isSmallSize)
        {
            // 체크섬 확인도 추가할 것인가?
            if(isSmallSize)
            {
                const TDATA_BLOCK_HEADER* pH = _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)pAddress, GLOBAL::gc_minPageUnitSize);
                const byte* pS = (byte*)(pH+1);
                const size_t offset = (size_t)((byte*)pAddress - pS);

                if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize){}
                else if(pH->m_pPool != pThisPool){}
                else if(!pH->m_pGroupFront || pH->m_pGroupFront->m_pPool != pThisPool){}
                else if(!pH->mFN_Query_ContainPTR(pAddress)){}
                else if(offset % UnitSize != 0){}
                else { return FALSE; }
            }
            else
            {
                const TDATA_BLOCK_HEADER* pH = reinterpret_cast<TDATA_BLOCK_HEADER*>(pAddress) - 1;
                if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize){}
                else if(pH->m_pPool != pThisPool){}
                else if(!pH->m_pGroupFront || pH->m_pGroupFront->m_pPool != pThisPool){}
                else if(!pH->mFN_Query_ContainPTR(pAddress)){}
                else
                {
                    // 이상의 확인은 헤더의 값을 확인하지만 헤더의 위치는 확신할 수 없다
                    // 하지만 사용자의 실수가 아니면 일어나지 않는 일이다
                    // 마지막으로 주소 오프셋을 확인한다
                    auto pHTrust = GLOBAL::g_Table_BlockHeaders_Normal.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
                    if(!pHTrust) return TRUE;
                    const byte* pS = (byte*)(pHTrust->m_pGroupFront+1);
                    const size_t offset = (size_t)((byte*)pAddress - pS);
                    const size_t RealUnitSize = UnitSize + sizeof(TDATA_BLOCK_HEADER);
                    if(offset % RealUnitSize != 0)
                        return TRUE;

                    return FALSE; 
                }
            }
            return TRUE;
        }
    }
    __forceinline void CMemoryPool::mFN_Return_Memory_Process(TMemoryObject* pAddress)
    {
        _Assert(nullptr != pAddress);

    #ifdef _DEBUG
        // 헤더 손상 확인
        _AssertMsg(FALSE == gFN_Query_isBadPTR(this, pAddress, m_UnitSize, m_Allocator.m_isSmallUnitSize), "Broken Header Data");
    #endif

    #if _DEF_USING_DEBUG_MEMORY_LEAK
        InterlockedExchangeSubtract(&m_Debug_stats_UsingCounting, 1); // 언더플로우는 확인하지 않는다
        if(GLOBAL::g_bDebug__Trace_MemoryLeak)
        {
            m_Lock_Debug__Lend_MemoryUnits.Lock();
            auto iter = m_map_Lend_MemoryUnits.find(pAddress);
            _AssertReleaseMsg(iter != m_map_Lend_MemoryUnits.end(), "사용된적 없는 메모리의 반납");
            m_map_Lend_MemoryUnits.erase(iter);
            m_Lock_Debug__Lend_MemoryUnits.UnLock();
        }
    #endif
    #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
        mFN_Debug_Overflow_Set(pAddress+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
    #endif
        
        if(m_nLimit_Basket_KeepMax)
            mFN_Return_Memory_Process_Default(pAddress);
        else
            mFN_Return_Memory_Process_DirectCACHE(pAddress);
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_Return_Memory_Process_Default(TMemoryObject* pAddress)
    {
        TMemoryBasket& mb = mFN_Get_Basket();
        mb.Lock();
        {
            mb.mFN_Return_Object(pAddress);

            if(mb.m_Units.m_nUnit > mb.m_nDemand)
            {
                //if(mb.m_nDemand < GLOBAL::gc_LimitMinCounting_List) 이것은 치명적 오류이다. Basket에 대용량의 메모리가 갇히는 현상이 종종일어난다
                if(mb.m_Units.m_nUnit < GLOBAL::gc_LimitMinCounting_List)
                    goto Label_END;

                TMemoryBasket_CACHE* p = m_pBasket__CACHE + mb.m_index_CACHE;

                // 계산은 느슨하게 미리 계산
                //      cache의 잠금시간을 최소화 하기 위해
                //      cache는 다수의 mb에 의해 공유될 수 있다(1:1 관계가 아닐 수 있다)
                size_t nMoveK = p->m_nMax_Keep - p->m_Keep.m_nUnit;
                TMemoryObject* pF_to_Keep;
                TMemoryObject* pB_to_Keep;
                size_t nMoveS;
                if(nMoveK >= mb.m_Units.m_nUnit)
                {
                    nMoveK = mb.m_Units.m_nUnit;
                    pF_to_Keep = mb.m_Units.m_pUnit_F;
                    pB_to_Keep = mb.m_Units.m_pUnit_B;
                    nMoveS = 0;
                }
                else if(nMoveK == 0)
                {
                    nMoveS = mb.m_Units.m_nUnit;
                }
                else // nMoveK < mb.m_Units.m_nUnit
                {
                    //nMoveK = min(nMoveK, mb.m_Units.m_nUnit);
                    nMoveK = min(nMoveK, GLOBAL::gc_LimitMaxCounting_List);
                    nMoveS = mb.m_Units.m_nUnit - nMoveK;
                    pF_to_Keep = pB_to_Keep = nullptr;
                    MemoryPool_UTIL::sFN_PushBackN(pF_to_Keep, pB_to_Keep, mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B, nMoveK);
                }

                p->Lock();
                if(nMoveK)
                {
                    p->m_Keep.m_nUnit += nMoveK;
                    MemoryPool_UTIL::sFN_PushBackAll(p->m_Keep.m_pUnit_F, p->m_Keep.m_pUnit_B, pF_to_Keep, pB_to_Keep);
                }
                else
                {
                    // 전용 캐시가 가득 찼다면 Basket 수요 감소
                    //mFN_BasketDemand_Reduction(mb);
                }
                if(nMoveS)
                {
                    p->m_Share.m_nUnit += nMoveS;
                    MemoryPool_UTIL::sFN_PushBackAll(p->m_Share.m_pUnit_F, p->m_Share.m_pUnit_B, mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B);
                }
                mb.m_Units.m_nUnit = 0;
                mb.m_Units.m_pUnit_F = mb.m_Units.m_pUnit_B = nullptr;
                p->UnLock();
            }
        }

    Label_END:
        mb.UnLock();
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_Return_Memory_Process_DirectCACHE(TMemoryObject* pAddress)
    {
        TMemoryBasket& mb = mFN_Get_Basket();
        TMemoryBasket_CACHE* p = m_pBasket__CACHE + mb.m_index_CACHE;

        p->Lock();
        if(p->m_Keep.m_nUnit < p->m_nMax_Keep)
        {
            p->m_Keep.m_nUnit++;
            MemoryPool_UTIL::sFN_PushBack(p->m_Keep.m_pUnit_F, p->m_Keep.m_pUnit_B, pAddress);
        }
        else
        {
            p->m_Share.m_nUnit++;
            MemoryPool_UTIL::sFN_PushBack(p->m_Share.m_pUnit_F, p->m_Share.m_pUnit_B, pAddress);

            // 공유용 캐시가 가득차면, 모두 메모리풀로 옮긴다
            if(m_nLimit_CacheShare_KeepMax < p->m_Share.m_nUnit)
                mFN_Fill_Pool_from_Other(&p->m_Share);
        }
        p->UnLock();
    }
    __forceinline BOOL CMemoryPool::mFN_TestHeader(TMemoryObject * pAddress)
    {
        _CompileHint(this && nullptr != pAddress);
        if(m_UnitSize <= GLOBAL::gc_SmallUnitSize_Limit)
        {
            if(this == CMemoryPool_Manager::sFN_Get_MemoryPool_SmallObjectSize(pAddress))
                return TRUE;
        }
        else if(m_UnitSize <= GLOBAL::gc_maxSize_of_MemoryPoolUnitSize)
        {
            if(this == CMemoryPool_Manager::sFN_Get_MemoryPool_NormalObjectSize(pAddress))
                return TRUE;
        }

        return FALSE;
    }

    TMemoryObject* CMemoryPool::mFN_Fill_Basket_andRET1(TMemoryBasket& mb, TMemoryObject*& F, TMemoryObject*& B, size_t& nSource)
    {
        _Assert(F && B && 0<nSource && 0<mb.m_nDemand);

        if(nSource <= mb.m_nDemand)
        {
            mb.m_Units.m_nUnit += nSource;
            nSource            = 0;
            MemoryPool_UTIL::sFN_PushBackAll(mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B, F, B);
        }
        else//if(nSource > mb.m_nDemand)
        {
            // 너무 많은 수를 카운팅 하지 않도록 제한한다
            const size_t nMove = min(mb.m_nDemand, gc_LimitMaxCounting_List);

            mb.m_Units.m_nUnit += nMove;
            nSource            -= nMove;
            MemoryPool_UTIL::sFN_PushBackN(mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B, F, B, nMove);
        }

        mb.m_Units.m_nUnit--;
        return MemoryPool_UTIL::sFN_PopFront(mb.m_Units.m_pUnit_F, mb.m_Units.m_pUnit_B);
    }
    TMemoryObject* CMemoryPool::mFN_Fill_CACHE_andRET1(TMemoryBasket_CACHE& cache, TMemoryObject*& F, TMemoryObject*& B, size_t& nSource)
    {
        _Assert(F && B && 0<nSource && cache.m_nMax_Keep);

        if(nSource <= cache.m_nMax_Keep)
        {
            cache.m_Keep.m_nUnit += nSource;;
            nSource              = 0;
            MemoryPool_UTIL::sFN_PushBackAll(cache.m_Keep.m_pUnit_F, cache.m_Keep.m_pUnit_B, F, B);
        }
        else//if(nSource > cache.m_nMax_Keep)
        {
            // 너무 많은 수를 카운팅 하지 않도록 제한한다
            const size_t nMove = min(cache.m_nMax_Keep, gc_LimitMaxCounting_List);

            cache.m_Keep.m_nUnit += nMove;
            nSource              -= nMove;
            MemoryPool_UTIL::sFN_PushBackN(cache.m_Keep.m_pUnit_F, cache.m_Keep.m_pUnit_B, F, B, nMove);
        }

        cache.m_Keep.m_nUnit--;
        return MemoryPool_UTIL::sFN_PopFront(cache.m_Keep.m_pUnit_F, cache.m_Keep.m_pUnit_B);
    }
    // 이 매소드는 호출 확률이 높다
    __forceinline TMemoryObject* CMemoryPool::mFN_Fill_Basket_from_ThisCache_andRET1(TMemoryBasket& mb)
    {
        TMemoryObject* pRET;

        auto pThisCACHE = m_pBasket__CACHE + mb.m_index_CACHE;
        pThisCACHE->Lock();
        if(0 < pThisCACHE->m_Keep.m_nUnit)
        {
            pRET =  mFN_Fill_Basket_andRET1(mb, pThisCACHE->m_Keep.m_pUnit_F, pThisCACHE->m_Keep.m_pUnit_B, pThisCACHE->m_Keep.m_nUnit);
        }
        else if(0 < pThisCACHE->m_Share.m_nUnit)
        {
            pRET = mFN_Fill_Basket_andRET1(mb, pThisCACHE->m_Share.m_pUnit_F, pThisCACHE->m_Share.m_pUnit_B, pThisCACHE->m_Share.m_nUnit);

            // 캐시 수요 증가
            _Assert(0 == pThisCACHE->m_Keep.m_nUnit);
            mFN_BasektCacheDemand_Expansion(*pThisCACHE);
        }
        else
        {
            pRET = nullptr;
        }
        pThisCACHE->UnLock();
        return pRET;
    }

    __forceinline TMemoryObject* CMemoryPool::mFN_Fill_Basket_from_Pool_andRET1(TMemoryBasket& mb)
    {
        if(0 == m_nUnit)
            return nullptr;

        _Assert(m_pUnit_F && m_pUnit_B && 0<m_nUnit);
        return mFN_Fill_Basket_andRET1(mb, m_pUnit_F, m_pUnit_B, m_nUnit);
    }
    __forceinline TMemoryObject* CMemoryPool::mFN_Fill_CACHE_from_Pool_andRET1(TMemoryBasket_CACHE& cache)
    {
        if(0 == m_nUnit)
            return nullptr;

        _Assert(m_pUnit_F && m_pUnit_B && 0<m_nUnit);
        return mFN_Fill_CACHE_andRET1(cache, m_pUnit_F, m_pUnit_B, m_nUnit);
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Fill_Basket_from_AllOtherBasketCache_andRET1(TMemoryBasket& mb)
    {
        const auto iThisCACHE = mb.m_index_CACHE;
        for(size_t i=0; i<g_nBasket__CACHE; i++)
        {
            if(i == iThisCACHE)
                continue;

            auto p = m_pBasket__CACHE + i;

            //LoadFence();

            // 느슨한 확인
            if(0 == p->m_Share.m_nUnit && 0 == p->m_Keep.m_nUnit)
                continue;
            p->Lock();

            if(0 < p->m_Share.m_nUnit)
            {
                auto pRET = mFN_Fill_Basket_andRET1(mb, p->m_Share.m_pUnit_F, p->m_Share.m_pUnit_B, p->m_Share.m_nUnit);

                // 대상 바구니의 캐시에 남은 것들을 메모리풀로 가져온다
                if(GLOBAL::gc_LimitMinCounting_List < p->m_Share.m_nUnit)
                    mFN_Fill_Pool_from_Other(&p->m_Share);

                p->UnLock();
                return pRET;
            }
            else if(0 < p->m_Keep.m_nUnit)
            {
                auto pRET = mFN_Fill_Basket_andRET1(mb, p->m_Keep.m_pUnit_F, p->m_Keep.m_pUnit_B, p->m_Keep.m_nUnit);

                // 다른 프로세서의 캐시를 모두 가져 왔다면, 해당 프로세서의 캐시에 대하여,
                // 캐시 수요 감소
                if(0 == p->m_Keep.m_nUnit)
                    mFN_BasektCacheDemand_Reduction(*p);

                p->UnLock();
                return pRET;
            }
            else
            {
                p->UnLock();
            }
        }

        return nullptr;
    }
    DECLSPEC_NOINLINE TMemoryObject * CMemoryPool::mFN_Fill_CACHE_from_AllOtherBasketCache_andRET1(TMemoryBasket_CACHE& cache)
    {
        // 데드락 위험 체크
        for(size_t i=0; i<g_nBasket__CACHE; i++)
        {
            auto& OtherCache = m_pBasket__CACHE[i];
            if(&OtherCache == &cache)
                continue;

            // 느슨한 확인
            if(0 == OtherCache.m_Share.m_nUnit && 0 == OtherCache.m_Keep.m_nUnit)
                continue;

            OtherCache.Lock();
            if(0 < OtherCache.m_Share.m_nUnit)
            {
                auto pRET = mFN_Fill_CACHE_andRET1(cache, OtherCache.m_Share.m_pUnit_F, OtherCache.m_Share.m_pUnit_B, OtherCache.m_Share.m_nUnit);

                // 대상 바구니의 캐시에 남은 것들을 메모리풀로 가져온다
                if(GLOBAL::gc_LimitMinCounting_List < OtherCache.m_Share.m_nUnit)
                    mFN_Fill_Pool_from_Other(&OtherCache.m_Share);

                OtherCache.UnLock();
                return pRET;
            }
            else if(0 < OtherCache.m_Keep.m_nUnit)
            {
                auto pRET = mFN_Fill_CACHE_andRET1(cache, OtherCache.m_Keep.m_pUnit_F, OtherCache.m_Keep.m_pUnit_B, OtherCache.m_Keep.m_nUnit);

                // 다른 프로세서의 캐시를 모두 가져 왔다면, 해당 프로세서의 캐시에 대하여,
                // 캐시 수요 감소
                if(0 == OtherCache.m_Keep.m_nUnit)
                    mFN_BasektCacheDemand_Reduction(OtherCache);

                OtherCache.UnLock();
                return pRET;
            }
            else
            {
                OtherCache.UnLock();
            }
        }

        return nullptr;
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Fill_Basket_from_AllOtherBasketLocalStorage_andRET1(TMemoryBasket& mb)
    {
        for(size_t i=0; i<g_nBaskets; i++)
        {
            TMemoryBasket* p = &m_pBaskets[i];
            if(p == &mb)
                continue;

            // p->Lock();
            // 위 코드는 서로 잠긴 상황,
            // mb1 이 mb2를, 잠긴 mb2가 mb1을 기다리는 데드락에 빠질수 있어 제한된 잠금을 사용해야 한다.
            // 잠금을 실패하면 대상은 skip
            if(!p->m_Lock.Lock__NoInfinite(0x0000FFFF))
                continue;

            if(0 < p->m_Units.m_nUnit)
            {
                auto pRET = mFN_Fill_Basket_andRET1(mb, p->m_Units.m_pUnit_F, p->m_Units.m_pUnit_B, p->m_Units.m_nUnit);
                p->UnLock();
                return pRET;
            }
            else
            {
                p->UnLock();
            }
        }

        return nullptr;
    }

    void CMemoryPool::mFN_Fill_Pool_from_Other(TUnits* pUnits)
    {
        m_Lock.Begin_Write__INFINITE();
        m_nUnit += pUnits->m_nUnit;
        pUnits->m_nUnit = 0;
        MemoryPool_UTIL::sFN_PushBackAll(m_pUnit_F, m_pUnit_B, pUnits->m_pUnit_F, pUnits->m_pUnit_B);
        m_Lock.End_Write();
    }
    _DEF_INLINE_CHANGE_DEMAND void CMemoryPool::mFN_BasketDemand_Expansion(TMemoryBasket& mb)
    {
        if(mb.m_nDemand < m_nLimit_Basket_KeepMax)
        {
            mb.m_nDemand += m_UnitSize_per_IncrementDemand;
            if(mb.m_nDemand > m_nLimit_Basket_KeepMax)
                mb.m_nDemand = m_nLimit_Basket_KeepMax;
        }
    }

    _DEF_INLINE_CHANGE_DEMAND void CMemoryPool::mFN_BasketDemand_Reduction(TMemoryBasket & mb)
    {
        if(mb.m_nDemand > m_UnitSize_per_IncrementDemand)
            mb.m_nDemand -= m_UnitSize_per_IncrementDemand;
        else
            mb.m_nDemand = 0;
    }

    _DEF_INLINE_CHANGE_DEMAND void CMemoryPool::mFN_BasektCacheDemand_Expansion(TMemoryBasket_CACHE & cache)
    {
        if(cache.m_nMax_Keep < m_nLimit_Cache_KeepMax)
        {
            cache.m_nMax_Keep += m_UnitSize_per_IncrementDemand; // (1 ~ )
            if(cache.m_nMax_Keep > m_nLimit_Cache_KeepMax)
                cache.m_nMax_Keep = m_nLimit_Cache_KeepMax;
        }
    }

    _DEF_INLINE_CHANGE_DEMAND DECLSPEC_NOINLINE void CMemoryPool::mFN_BasektCacheDemand_Reduction(TMemoryBasket_CACHE & cache)
    {
        if(cache.m_nMax_Keep > m_UnitSize_per_IncrementDemand)
            cache.m_nMax_Keep -= m_UnitSize_per_IncrementDemand;
        else
            cache.m_nMax_Keep = 0;
    }

    void CMemoryPool::mFN_Debug_Overflow_Set(void* p, size_t size, BYTE code)
    {
        if(!size)
            return;
        ::memset(p, code, size);
    }
    namespace{
        BOOL gFN_Test_Memory(const void* pPTR, size_t size, BYTE code)
        {
            const BYTE* pC = (const BYTE*)pPTR;
            if(size < 16) {
                for(size_t i=0; i<size; i++)
                    if(*(pC+i) != code)
                        return FALSE;
                return TRUE;
            }

            while((size_t)pC % sizeof(size_t) != 0)
            {
                if(*pC != code)
                    return FALSE;

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
                    return FALSE;
                pWord++;
            } while(pWord < pWordEnd);

            pC = (const BYTE*)pWord;
            const BYTE* pCEnd = pC + nChar;
            for(; pC < pCEnd; pC++)
            {
                if(*pC != code)
                    return FALSE;
            }
            return TRUE;
        }
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_Debug_Overflow_Check(const void* p, size_t size, BYTE code) const
    {
        if(!gFN_Test_Memory(p, size, code))
            _DebugBreak("Broken Memory");
    }
    void CMemoryPool::mFN_Debug_Report()
    {
        size_t _TotalFree = mFN_Counting_KeepingUnits();
#if _DEF_USING_DEBUG_MEMORY_LEAK
        // 반납하지 않은 메모리 보고
        m_Lock_Debug__Lend_MemoryUnits.Lock();
        //if(!m_map_Lend_MemoryUnits.empty())
        if(0 < m_Debug_stats_UsingCounting)
        {
            // print caption (제목이 한번만 호출 되도록)
            static BOOL bRequire_PrintCaption = TRUE;
            if(bRequire_PrintCaption)
            {
                _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("================ MemoryPool : Detected memory leaks ================\n");
                bRequire_PrintCaption = FALSE;
            }

            //_MACRO_OUTPUT_DEBUG_STRING_ALWAYS("MemoryPool[%u] : Total: [%0.2f]KB\n"
            //    , (UINT32)m_UnitSize
            //    , static_cast<double>(m_UnitSize) * m_map_Lend_MemoryUnits.size() / 1024.);
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("MemoryPool[%u] : Total: [%0.2f]KB\n"
                , (UINT32)m_UnitSize
                , static_cast<double>(m_UnitSize) * m_Debug_stats_UsingCounting / 1024.);

            if(GLOBAL::g_bDebug__Trace_MemoryLeak)
            {
                for(auto iter : m_map_Lend_MemoryUnits)
                {
                    const void* p = iter.first;
                    const TTrace_SourceCode& tag = iter.second;
                    _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("%s(%d): 0x%p\n", tag.m_File, tag.m_Line, p);
                }
            }
            else if(!m_map_Lend_MemoryUnits.empty())
            {
                m_map_Lend_MemoryUnits.clear();
            }
        }
        m_Lock_Debug__Lend_MemoryUnits.UnLock();
#endif


    #if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
        {
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("MemoryPool[%llu] Shared Storage n[%10llu] / n[%10llu], Expansion Count[%10llu]\n")
                , (UINT64)m_UnitSize
                , (UINT64)m_nUnit
                , (UINT64)m_stats_Units_Allocated
                , (UINT64)m_Allocator.m_stats_cnt_Succeed_VirtualAlloc
                );

            for(size_t i=0; i<GLOBAL::g_nBaskets; i++)
            {
                _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tCACHE Level1[%d] = n[%10llu] nDemand[%10llu] Get[%10llu] Ret[%10llu] CacheMiss[%10llu]\n"), i
                    , (UINT64)m_pBaskets[i].m_Units.m_nUnit
                    , (UINT64)m_pBaskets[i].m_nDemand
                    , (UINT64)m_pBaskets[i].m_cnt_Get
                    , (UINT64)m_pBaskets[i].m_cnt_Ret
                    , (UINT64)m_pBaskets[i].m_cnt_CacheMiss_from_Get_Basket
                    );
            }
            for(size_t i=0; i<GLOBAL::g_nBasket__CACHE; i++)
                _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tCACHE Level2[%d] = n[%10llu]\n"), i, (UINT64)m_pBasket__CACHE[i].m_Keep.m_nUnit);
            for(size_t i=0; i<GLOBAL::g_nBasket__CACHE; i++)
                _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tCACHE Level3[%d] = n[%10llu]\n"), i, (UINT64)m_pBasket__CACHE[i].m_Share.m_nUnit);

            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tTotal : [%10llu]/[%10llu]\n"), (UINT64)_TotalFree, (UINT64)m_stats_Units_Allocated);
        }
    #endif

        if(m_bWriteStats_to_LogFile)
        {
            if(_TotalFree == m_stats_Units_Allocated)
            {
                // 이상 없음
            }
            else if(_TotalFree < m_stats_Units_Allocated)
            {
                const size_t leak_cnt   = m_stats_Units_Allocated - _TotalFree;
                const size_t leak_size  = leak_cnt * m_UnitSize;
                _LOG_DEBUG(_T("[Warning] Pool Size[%llu] : Memory Leak(Count:%llu , TotalSize:%lluKB)")
                    , (UINT64)m_UnitSize
                    , (UINT64)leak_cnt
                    , (UINT64)leak_size/1024);
            }
            else if(_TotalFree > m_stats_Units_Allocated)
            {
                _LOG_DEBUG(_T("[Critical Error] Pool Size[%llu] : Total Allocated < Total Free"), (UINT64)m_UnitSize);
            }

            if(0 == m_Allocator.m_nLimitBlocks_per_Expansion)
            {
                _LOG_DEBUG(_T("[Warning] Pool Size[%llu] : Failed Alloc"), (UINT64)m_UnitSize);
            }
        }

        if(0 < m_stats_Counting_Free_BadPTR)
        {
            // 심각한 상황이니 Release 버전에서도 출력한다
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, "================ Critical Error : Failed Count(%Iu) -> MemoryPool[%Iu]:Free ================"
                , m_stats_Counting_Free_BadPTR
                , m_UnitSize);
        }
    }
    size_t CMemoryPool::mFN_Counting_KeepingUnits()
    {
        size_t _TotalFree = m_nUnit;
        for(size_t i=0; i<g_nBaskets; i++)
        {
            _TotalFree += m_pBaskets[i].m_Units.m_nUnit;
        }
        for(size_t i=0; i<g_nBasket__CACHE; i++)
        {
            _TotalFree += m_pBasket__CACHE[i].m_Keep.m_nUnit;
            _TotalFree += m_pBasket__CACHE[i].m_Share.m_nUnit;
        }
        return _TotalFree;
    }

    DECLSPEC_NOINLINE void CMemoryPool::mFN_all_Basket_Lock()
    {
        for(size_t i=0; i<g_nBaskets; i++)
            m_pBaskets[i].Lock();
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_all_Basket_UnLock()
    {
        for(size_t i=0; i<g_nBaskets; i++)
            m_pBaskets[i].UnLock();
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_all_BasketCACHE_Lock()
    {
        for(size_t i=0; i<g_nBasket__CACHE; i++)
            m_pBasket__CACHE[i].Lock();
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_all_BasketCACHE_UnLock()
    {
        for(size_t i=0; i<g_nBasket__CACHE; i++)
            m_pBasket__CACHE[i].UnLock();
    }

}
}


/*----------------------------------------------------------------
/   메모리풀 관리자
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    CMemoryPool_Manager::CMemoryPool_Manager()
        : m_bWriteStats_to_LogFile(TRUE)
        , m_stats_Counting_Free_BadPTR(0)
    {
        CObjectPool_Handle<CMemoryPool>::Reference_Attach();

        _LOG_DEBUG__WITH__TRACE(TRUE, _T("--- Create CMemoryPool_Manager ---"));

        mFN_Test_Environment_Variable();
        mFN_Initialize_MemoryPool_Shared_Environment_Variable1();
        mFN_Initialize_MemoryPool_Shared_Environment_Variable2();

        ::UTIL::g_pMem = this;
        mFN_Report_Environment_Variable();
    }
    CMemoryPool_Manager::~CMemoryPool_Manager()
    {
        ::UTIL::g_pMem = nullptr;

        m_Lock__set_Pool.Begin_Write__INFINITE();

        for(auto iter : m_set_Pool)
        {
            //_SAFE_DELETE_ALIGNED_CACHELINE(iter.m_pPool);
            CObjectPool_Handle<CMemoryPool>::Free_and_CallDestructor(iter.m_pPool);
        }
        m_set_Pool.clear();


        mFN_Destroy_MemoryPool_Shared_Environment_Variable();
        mFN_Debug_Report();
        _LOG_DEBUG__WITH__TRACE(TRUE, _T("--- Destroy CMemoryPool_Manager ---"));
        CObjectPool_Handle<CMemoryPool>::Reference_Detach();
    }


    DECLSPEC_NOINLINE void CMemoryPool_Manager::mFN_Test_Environment_Variable()
    {
        // 비트가 1개만 on 이어야 하는 상수 체크
        _AssertRelease(1 == _MACRO_BIT_COUNT(gc_Max_ThreadAccessKeys));
        _AssertRelease(1== _MACRO_BIT_COUNT(gc_AlignSize_LargeUnit));
        _AssertRelease(1== _MACRO_BIT_COUNT(gc_minPageUnitSize));
        
        

        // gFN_Calculate_UnitSize 관련
        {
        #pragma warning(push)
        #pragma warning(disable: 4127)
            _AssertRelease(0 < _MACRO_ARRAY_COUNT(gc_Array_Limit) && _MACRO_ARRAY_COUNT(gc_Array_Limit) == _MACRO_ARRAY_COUNT(gc_Array_MinUnit));
        #pragma warning(pop)

            _AssertRelease(gc_minSize_of_MemoryPoolUnitSize == gc_Array_MinUnit[0]);

            BOOL bDetected_SmallUnitSize = FALSE;
            for(auto i=0; i<_MACRO_ARRAY_COUNT(gc_Array_Limit); i++)
            {
                if(gc_SmallUnitSize_Limit == gc_Array_Limit[i])
                {
                    bDetected_SmallUnitSize = TRUE;
                    break;
                }
            }
            _AssertReleaseMsg(TRUE == bDetected_SmallUnitSize, "작은 유닛 크기기준이 잘못 되었습니다");

            size_t tempPrevious = 0;
            for(auto i=0; i<_MACRO_ARRAY_COUNT(gc_Array_Limit); i++)
            {
                _AssertReleaseMsg(0 == gc_Array_Limit[i] %  _DEF_CACHELINE_SIZE, "캐시라인크기의 배수여야 합니다");
                _AssertRelease(tempPrevious < gc_Array_Limit[i]); // 이전 인덱스보다 큰 값이여야 함
                tempPrevious = gc_Array_Limit[i];

                const auto& m = gc_Array_MinUnit[i];
                _AssertRelease(0 < m);
                // 캐시라인 크기의 배수 또는 약수여야 함
                if(m > _DEF_CACHELINE_SIZE)
                {
                    _AssertRelease(0 == m % _DEF_CACHELINE_SIZE);
                }
                else
                {
                    _AssertRelease(0 == _DEF_CACHELINE_SIZE % m);
                }
            }
        }
    }

    void CMemoryPool_Manager::mFN_Initialize_MemoryPool_Shared_Environment_Variable1()
    {
        g_nProcessor = CORE::g_instance_CORE.mFN_Get_System_Information()->mFN_Get_NumProcessor_Logical();
        g_nBaskets = g_nProcessor;

        if(gc_Max_ThreadAccessKeys < g_nProcessor)
            _LOG_SYSTEM__WITH__TRACE(TRUE, "too many CPU Core : %d", g_nProcessor);

    #if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT
        _MACRO_STATIC_ASSERT(gc_Max_ThreadAccessKeys <= 64); // SetThreadAffinityMask 함수 관련

        // SIDT 를 이용해, 프로세서별 코드조사, 캐시로써 작성
        auto hThisThread  = GetCurrentThread();
        auto Mask_Before  = ::SetThreadAffinityMask(hThisThread, 1);
        _AssertRelease(0 != Mask_Before);

        for(DWORD_PTR i=0; i<g_nProcessor; i++)
        {
            ::SetThreadAffinityMask(hThisThread, (DWORD_PTR)1 << i);
            g_ArrayProcessorCode[i] = gFN_Get_SIDT();
        }
        ::SetThreadAffinityMask(hThisThread, Mask_Before);
    #endif
    }

    void CMemoryPool_Manager::mFN_Initialize_MemoryPool_Shared_Environment_Variable2()
    {
    #define __temp_macro_make4case(s)   \
            case s+0: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+0>; gpFN_Baskets_Free = gFN_Baskets_Free<s+0>; break;    \
            case s+1: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+1>; gpFN_Baskets_Free = gFN_Baskets_Free<s+1>; break;    \
            case s+2: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+2>; gpFN_Baskets_Free = gFN_Baskets_Free<s+2>; break;    \
            case s+3: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+3>; gpFN_Baskets_Free = gFN_Baskets_Free<s+3>; break;
        switch(g_nBaskets)
        {
            __temp_macro_make4case(0+1);
            __temp_macro_make4case(4+1);
            __temp_macro_make4case(8+1);
            __temp_macro_make4case(12+1);
            __temp_macro_make4case(16+1);
            __temp_macro_make4case(20+1);
            __temp_macro_make4case(24+1);
            __temp_macro_make4case(28+1);
        default:
            gpFN_Baskets_Alloc = gFN_Baskets_Alloc__Default;
            gpFN_Baskets_Free  = gFN_Baskets_Free__Default;
        }
    #undef __temp_macro_make4case


        // CPU 종류에 따라 좀더 세밀한 조건을 적용하는 것이 좋다(1M2C 어 같은 경우)
        //      g_Array_Key__CpuCore_to_CACHE 의 접근 인덱스
        // 캐시는 너무 많아서는 안된다
        _AssertReleaseMsg(g_nProcessor <= _MACRO_ARRAY_COUNT(g_Array_Key__CpuCore_to_CACHE), "너무 많은 CPU");
        size_t nPhysical_CORE = CORE::g_instance_CORE.mFN_Get_System_Information()->mFN_Get_NumProcessor_PhysicalCore();

        // 모듈당 코어수 조사
        for(size_t i=1;; i++)
        {
            const auto temp = nPhysical_CORE * i;
            if(temp == g_nProcessor)
            {
                g_nCore_per_Moudule = i;
                break;
            }
            if(temp > g_nProcessor)
            {
                // 코어/모듈 이 정확히 나눠 떨어지지 않는다
                g_nCore_per_Moudule = 1;
                break;
            }
        }

        g_iCACHE_AccessRate = g_nCore_per_Moudule;
        g_nBasket__CACHE = g_nProcessor / g_iCACHE_AccessRate;

        // 캐시가 너무 많다면 1개의 캐시당 프로세서 비율을 배로 높인다
        while(8 < g_nBasket__CACHE)
        {
            g_iCACHE_AccessRate *= 2;
            g_nBasket__CACHE = g_nProcessor / g_iCACHE_AccessRate;
            if(g_nProcessor % g_iCACHE_AccessRate)
                g_nBasket__CACHE++;
        }

        for(size_t i=0; i<g_nProcessor; i++)
        {
            auto t = i / g_iCACHE_AccessRate;
            _AssertReleaseMsg(i < 256, "CPU 코어가 너무 많습니다");
            g_Array_Key__CpuCore_to_CACHE[i] = static_cast<byte>(t);
        }

    #define __temp_macro_make4case(s)   \
            case s+0: gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc<s+0>; gpFN_BasketCACHE_Free = gFN_BasketCACHE_Free<s+0>; break;    \
            case s+1: gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc<s+1>; gpFN_BasketCACHE_Free = gFN_BasketCACHE_Free<s+1>; break;    \
            case s+2: gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc<s+2>; gpFN_BasketCACHE_Free = gFN_BasketCACHE_Free<s+2>; break;    \
            case s+3: gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc<s+3>; gpFN_BasketCACHE_Free = gFN_BasketCACHE_Free<s+3>; break;
        switch(g_nBasket__CACHE)
        {
            __temp_macro_make4case(0+1);
            __temp_macro_make4case(4+1);
            __temp_macro_make4case(8+1);
            __temp_macro_make4case(12+1);
            __temp_macro_make4case(16+1);
            __temp_macro_make4case(20+1);
            __temp_macro_make4case(24+1);
            __temp_macro_make4case(28+1);
        default:
            gpFN_BasketCACHE_Alloc = gFN_BasketCACHE_Alloc__Default;
            gpFN_BasketCACHE_Free  = gFN_BasketCACHE_Free__Default;
        }
    #undef __temp_macro_make4case
    }

    void CMemoryPool_Manager::mFN_Destroy_MemoryPool_Shared_Environment_Variable()
    {

    }

#if _DEF_USING_DEBUG_MEMORY_LEAK
    void* CMemoryPool_Manager::mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line)
    {
        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool(_Size);
        return pPool->mFN_Get_Memory(_Size, _FileName, _Line);
    }
#else
    void* CMemoryPool_Manager::mFN_Get_Memory(size_t _Size)
    {
        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool(_Size);
        return pPool->mFN_Get_Memory(_Size);
    }
#endif

    void CMemoryPool_Manager::mFN_Return_Memory(void* pAddress)
    {
        if(!pAddress)
            return;

        TDATA_BLOCK_HEADER::_TYPE_Units_SizeType sizeType;
        auto pPool = sFN_Get_MemoryPool_fromPointer(pAddress, sizeType);
        switch(sizeType)
        {
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize:
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize:
        {
            CMemoryPool& refPool = *(CMemoryPool*)pPool;
            refPool.mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;
        }
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize:
        {
            CMemoryPool__BadSize& refPool = *(CMemoryPool__BadSize*)pPool;
            refPool.mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;
        }
        default: // TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_Invalid:
        {
            InterlockedExchangeAdd(&m_stats_Counting_Free_BadPTR, 1);
            _DebugBreak("잘못된 주소 반환 또는 손상된 식별자");
        }
        }
    }
    void CMemoryPool_Manager::mFN_Return_MemoryQ(void* pAddress)
    {
        if(!pAddress)
            return;

        TDATA_BLOCK_HEADER::_TYPE_Units_SizeType sizeType;
        #ifdef _DEBUG
        auto pPool = sFN_Get_MemoryPool_fromPointer(pAddress, sizeType);
        #else
        auto pPool = sFN_Get_MemoryPool_fromPointerQ(pAddress, sizeType);
        #endif
        switch(sizeType)
        {
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize:
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize:
        {
            CMemoryPool& refPool = *(CMemoryPool*)pPool;
            refPool.mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;
        }
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize:
        {
            CMemoryPool__BadSize& refPool = *(CMemoryPool__BadSize*)pPool;
            refPool.mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;
        }
        default: // TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_Invalid:
        {
            InterlockedExchangeAdd(&m_stats_Counting_Free_BadPTR, 1);
            _DebugBreak("잘못된 주소 반환 또는 손상된 식별자");
        }
        }
    }

#if _DEF_USING_DEBUG_MEMORY_LEAK
    void* CMemoryPool_Manager::mFN_Get_Memory__AlignedCacheSize(size_t _Size, const char* _FileName, int _Line)
    {
        if(!_Size )
        {
            _Size = _DEF_CACHELINE_SIZE;
        }
        else
        {
            const size_t mask = _DEF_CACHELINE_SIZE - 1;
            if(_Size & mask)
                _Size = _DEF_CACHELINE_SIZE + _Size & (~mask);
        }
        
        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool(_Size);
        return pPool->mFN_Get_Memory(_Size, _FileName, _Line);
    }
#else
    void* CMemoryPool_Manager::mFN_Get_Memory__AlignedCacheSize(size_t _Size)
    {
        if(!_Size )
        {
            _Size = _DEF_CACHELINE_SIZE;
        }
        else
        {
            const size_t mask = _DEF_CACHELINE_SIZE - 1;
            if(_Size & mask)
                _Size = _DEF_CACHELINE_SIZE + _Size & (~mask);
        }

        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool(_Size);
        return pPool->mFN_Get_Memory(_Size);
    }
#endif
    void CMemoryPool_Manager::mFN_Return_Memory__AlignedCacheSize(void* pAddress)
    {
        CMemoryPool_Manager::mFN_Return_Memory(pAddress);
    }

    IMemoryPool* CMemoryPool_Manager::mFN_Get_MemoryPool(size_t _Size)
    {
        if(!_Size)
            _Size = 1;

        const size_t iTable = gFN_Get_MemoryPoolTableIndex(_Size);
        if(iTable < gc_Size_Table_MemoryPool)
        {
            if(g_pTable_MemoryPool[iTable])
                return g_pTable_MemoryPool[iTable];
        }
        else if(_Size > GLOBAL::gc_maxSize_of_MemoryPoolUnitSize)
        {
            return &m_SlowPool;
        }

        const size_t UnitSize = gFN_Calculate_UnitSize(_Size);
        return CMemoryPool_Manager::_mFN_Prohibit__CreatePool_and_SetMemoryPoolTable(iTable, UnitSize);
    }

#if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
    void CMemoryPool_Manager::mFN_Set_TlsCache_AccessFunction(TMemoryPool_TLS_CACHE&(*pFN)(void))
    {
        if(!pFN)
            return;
        gpFN_TlsCache_AccessFunction = pFN;
    }
#endif

    void CMemoryPool_Manager::mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On)
    {
        m_bWriteStats_to_LogFile = _On;

        m_Lock__set_Pool.Begin_Read();
        {
            for(auto iter : m_set_Pool)
            {
                if(iter.m_pPool)
                    iter.m_pPool->mFN_Set_OnOff_WriteStats_to_LogFile(_On);
            }
        }
        m_Lock__set_Pool.End_Read();
    }
    void CMemoryPool_Manager::mFN_Set_OnOff_Trace_MemoryLeak(BOOL _On)
    {
        GLOBAL::g_bDebug__Trace_MemoryLeak = _On;
    }
    void CMemoryPool_Manager::mFN_Set_OnOff_ReportOutputDebugString(BOOL _On)
    {
        GLOBAL::g_bDebug__Report_OutputDebug = _On;
    }


    DECLSPEC_NOINLINE CMemoryPool* CMemoryPool_Manager::_mFN_Prohibit__CreatePool_and_SetMemoryPoolTable(size_t iTable, size_t UnitSize)
    {
        // 메모리풀을 찾거나 생성하고, 전역 캐시에 설정

        TLinkPool temp ={UnitSize, nullptr};
        CMemoryPool* pReturn = nullptr;
        // --탐색--
        {
            m_Lock__set_Pool.Begin_Read();
            {
                auto iter = m_set_Pool.find(temp);
                if(iter != m_set_Pool.end())
                    pReturn = iter->m_pPool;
            }
            m_Lock__set_Pool.End_Read();
            if(pReturn)
            {
                gFN_Set_MemoryPoolTable(iTable, pReturn);
                return pReturn;
            }
        }

        // --등록 시도--
        m_Lock__set_Pool.Begin_Write__INFINITE();
        {
            {
                // 다른 스레드가 끼어들어, 해당되는 풀을 만들었는지 한번더 확인
                auto iter = m_set_Pool.find(temp);
                if(iter != m_set_Pool.end())
                    pReturn = iter->m_pPool;
            }
            if(pReturn)
            {
                m_Lock__set_Pool.End_Write();
                gFN_Set_MemoryPoolTable(iTable, pReturn);
                return pReturn;
            }

            // 실제 등록
            //_SAFE_NEW_ALIGNED_CACHELINE(pReturn, CMemoryPool(UnitSize));
            pReturn = (CMemoryPool*)CObjectPool_Handle<CMemoryPool>::Alloc();
            _MACRO_CALL_CONSTRUCTOR(pReturn, CMemoryPool(UnitSize));
            pReturn->mFN_Set_OnOff_WriteStats_to_LogFile(m_bWriteStats_to_LogFile);
            if(pReturn)
            {
                temp.m_pPool = pReturn;
                m_set_Pool.insert(temp);
                gFN_Set_MemoryPoolTable(iTable, pReturn);
            }
        }
        m_Lock__set_Pool.End_Write();

        return pReturn;
    }


    __forceinline IMemoryPool* CMemoryPool_Manager::sFN_Get_MemoryPool_SmallObjectSize(void * p)
    {
        _CompileHint(nullptr != p);
        TDATA_BLOCK_HEADER* pH = _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)p, GLOBAL::gc_minPageUnitSize);

    #if _DEF_USING_MEMORYPOOL_TEST_SMALLUNIT__FROM__DBH_TABLE
        // 올바른 것처럼 속인 헤더 + 잘못된 주소로 인한 위험을 예방하는 방법
        const auto pHTrust = GLOBAL::g_Table_BlockHeaders_Normal.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
        if(pHTrust->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize)
            return nullptr;
        if(!pHTrust->mFN_Query_ContainPTR(p))
            return nullptr;

        return pHTrust->m_pPool;
    #else
        // 다른 모든 유닛크기를 확인후에 사용해야 한다
        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize)
            return nullptr;
        if(!pH->mFN_Query_ContainPTR(p))
            return nullptr;

        return pH->m_pPool;
    #endif
    }
    __forceinline IMemoryPool* CMemoryPool_Manager::sFN_Get_MemoryPool_NormalObjectSize(void * p)
    {
        _CompileHint(nullptr != p);
        if(!_MACRO_POINTER_IS_ALIGNED(p, GLOBAL::gc_AlignSize_LargeUnit))
            return nullptr;

        const TDATA_BLOCK_HEADER* pH = static_cast<TDATA_BLOCK_HEADER*>(p) - 1;

        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize)
            return nullptr;
        //if(nullptr == pH->m_pGroupFront)
        //    return nullptr;

        const TDATA_BLOCK_HEADER* pHTrust = GLOBAL::g_Table_BlockHeaders_Normal.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
        //if(pH->m_pGroupFront != pHTrust)
        //    return nullptr;

        // 위 주석처리된 if(nullptr == pH->m_pGroupFront) if(pH->m_pGroupFront != pHTrust) 는 속도를 위해 무시한다
        // pHTrust 를 신뢰한다
        // 단, pHTrust 가 손상되지 않았다는 가정하에. pH 의 손상여부는 체크하지 못한다
        // 이는 디버깅모드에 한하여 메모리풀 메모리 반납 프로세스 에서 확인한다

        if(!pHTrust->mFN_Query_ContainPTR(p))
            return nullptr;

        return pHTrust->m_pPool;
    }

    __forceinline IMemoryPool* CMemoryPool_Manager::sFN_Get_MemoryPool_fromPointer(void * p, TDATA_BLOCK_HEADER::_TYPE_Units_SizeType& _out_Type)
    {
        _CompileHint(nullptr != p);

        // 작은 유닛크기또한 캐시크기에 정렬된 유닛이 많아 이 조건은 무의미 하다
        //if(!_MACRO_POINTER_IS_ALIGNED(p, GLOBAL::gc_AlignSize_LargeUnit))
        //    goto Label_S;

        // p의 앞 64바이튼 무조건 존재한다
        const TDATA_BLOCK_HEADER& header = *(static_cast<TDATA_BLOCK_HEADER*>(p) - 1);
        // 주석처리함: 헤더가 일부가 손상되더라도 pHTrust를 신뢰한다
        //if(nullptr == header.m_pGroupFront)
        //    goto Label_S;

        const CTable_DataBlockHeaders* pTable;
        const TDATA_BLOCK_HEADER* pHTrust;
        switch(header.m_Type)
        {
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize:
            pTable = &GLOBAL::g_Table_BlockHeaders_Normal;
            pHTrust = pTable->mFN_Get_Link(header.m_Index.high, header.m_Index.low);
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize;
            break;

        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize:
            pTable = &GLOBAL::g_Table_BlockHeaders_Big;
            pHTrust = pTable->mFN_Get_Link(header.m_Index.high, header.m_Index.low);
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize;
            break;

        default:
            goto Label_S;
        }
        // 주석처리함: 헤더가 일부가 손상되더라도 pHTrust를 신뢰한다
        //if(header.m_pGroupFront != pHTrust)
        //    goto Label_S;
        if(!pHTrust->mFN_Query_ContainPTR(p))
            goto Label_S;

        return pHTrust->m_pPool;

    Label_S:

        IMemoryPool* pSmallPool = sFN_Get_MemoryPool_SmallObjectSize(p);
        if(pSmallPool)
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize;
        else
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_Invalid;
        return pSmallPool;
    }
    __forceinline IMemoryPool* CMemoryPool_Manager::sFN_Get_MemoryPool_fromPointerQ(void * p, TDATA_BLOCK_HEADER::_TYPE_Units_SizeType& _out_Type)
    {
        _CompileHint(nullptr != p);

        // p의 앞 64바이튼 무조건 존재한다
        const TDATA_BLOCK_HEADER& header = *(static_cast<TDATA_BLOCK_HEADER*>(p) - 1);

        const CTable_DataBlockHeaders* pTable;
        const TDATA_BLOCK_HEADER* pHTrust;
        // 작은 유닛의 앞부분이 우연히 패턴일 일치할 수 있기 때문에 테이블을 확인해야 한다
        switch(header.m_Type)
        {
        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize:
            pTable = &GLOBAL::g_Table_BlockHeaders_Normal;
            pHTrust = pTable->mFN_Get_Link(header.m_Index.high, header.m_Index.low);
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_NormalSize;
            break;

        case TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize:
            pTable = &GLOBAL::g_Table_BlockHeaders_Big;
            pHTrust = pTable->mFN_Get_Link(header.m_Index.high, header.m_Index.low);
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_OtherSize;
            break;

        default:
            goto Label_S;
        }
        // 반드시 체크해야 한다
        if(!pHTrust->mFN_Query_ContainPTR(p))
            goto Label_S;

        return pHTrust->m_pPool;

    Label_S:
        #ifdef _DEBUG
        IMemoryPool* pSmallPool = sFN_Get_MemoryPool_SmallObjectSize(p);
        #else
        TDATA_BLOCK_HEADER* pHS = _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)p, GLOBAL::gc_minPageUnitSize);
        IMemoryPool* pSmallPool = ((pHS->m_Type == TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize)? pHS->m_pPool : nullptr);
        #endif
        if(pSmallPool)
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_SmallSize;
        else
            _out_Type = TDATA_BLOCK_HEADER::_TYPE_Units_SizeType::E_Invalid;
        return pSmallPool;
    }

    void CMemoryPool_Manager::mFN_Report_Environment_Variable()
    {
        auto sysinfo = CORE::g_instance_CORE.mFN_Get_System_Information();
        _LOG_DEBUG__WITH__TRACE(FALSE, "CPU Core(%d) Thrad(%d) CACHE(%d) per POOL"
            , sysinfo->mFN_Get_NumProcessor_PhysicalCore()
            , sysinfo->mFN_Get_NumProcessor_Logical()
            , g_nBasket__CACHE);
    }
    void CMemoryPool_Manager::mFN_Debug_Report()
    {
        if(0 < m_stats_Counting_Free_BadPTR)
        {
            // 심각한 상황이니 Release 버전에서도 출력한다
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, "================ Critical Error : Failed Count(%Iu) -> MemoryPoolManager:Free ================"
                , m_stats_Counting_Free_BadPTR);
        }
    }

}
}
#endif