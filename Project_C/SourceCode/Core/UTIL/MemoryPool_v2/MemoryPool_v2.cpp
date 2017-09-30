#include "stdafx.h"
#if _DEF_MEMORYPOOL_MAJORVERSION == 2
#include "./MemoryPool_v2.h"
#include "./VMEM_LIST.h"

#include "../ObjectPool.h"

#include "../../Core.h"
#pragma warning(disable: 6255)
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
/-----------------------------------------------------------------
/   V1 에서 사용하던 TLS 를 리턴하는 함수포인터 전달 형식은
/       DLL->DLL 등의 확장성이 없어, V2에서는 사용하지 않음
/----------------------------------------------------------------*/
#define _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A
#if !_DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
#error _DEF_MEMORYPOOL_MAJORVERSION Ver 2 는 _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE 를 사용해야 함
#endif

// 외부 변수/함수
namespace UTIL{
    namespace MEM{
        namespace GLOBAL{
            extern UINT32 gFN_Precalculate_MemoryPoolExpansionSize__GetSmallSize(UINT32 _IndexThisPool);
            extern UINT32 gFN_Precalculate_MemoryPoolExpansionSize__GetBigSize(UINT32 _IndexThisPool);
        }
    }
}
/*----------------------------------------------------------------
/   설정
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    _MACRO_STATIC_ASSERT(_DEF_CACHELINE_SIZE == 64);
    _MACRO_STATIC_ASSERT(sizeof(TDATA_BLOCK_HEADER) % _DEF_CACHELINE_SIZE == 0);

    const size_t gc_KB = 1024;
    const size_t gc_MB = gc_KB * 1024;

    namespace GLOBAL{
        namespace SETTING{
            _MACRO_STATIC_ASSERT(gc_Num_MemoryPool == 256 / 8
                + (512 - 256) / 16
                + (gc_KB - 512) / 32
                + (gc_KB*2 - gc_KB) / 64
                + (gc_KB*4 - gc_KB*2) / 128
                + (gc_KB*8 - gc_KB*4) / 256
                + (gc_KB*16 - gc_KB*8) / 512
                + (gc_KB*64 - gc_KB*16) / gc_KB
                + (gc_KB*512 - gc_KB*64) / (gc_KB*8)
                + (gc_MB - gc_KB*512) / (gc_KB*64)
                + (gc_MB*10 - gc_MB) / (gc_KB*512));
            _MACRO_STATIC_ASSERT(gc_SizeMemoryPoolAllocation_DefaultCost < 1024);

            const size_t gc_Max_ThreadAccessKeys = 16;

            // 유닛 크기 계산에 사용하는 환경변수
            // 이 값을 변경시 gc_Num_QuickLink_MemoryPool , gFN_Get_Index__QuickLink_MemoryPool 등을 업데이트할것
            const size_t gc_Array_Limit[]   ={256, 512, 1*gc_KB, 2*gc_KB, 4*gc_KB, 8*gc_KB, 16*gc_KB, 64*gc_KB, 512*gc_KB, gc_MB, 10*gc_MB};
            const size_t gc_Array_MinUnit[] ={8, 16, 32, 64, 128, 256, 512, 1*gc_KB, 8*gc_KB, 64*gc_KB, 512*gc_KB};
            const size_t gc_Array_Limit__Count = _MACRO_ARRAY_COUNT(gc_Array_Limit);
            const size_t gc_Array_MinUnit__Count = _MACRO_ARRAY_COUNT(gc_Array_MinUnit);
            _MACRO_STATIC_ASSERT(0 < gc_Array_Limit__Count && gc_Array_Limit__Count == gc_Array_MinUnit__Count);

            const size_t gc_minSize_of_MemoryPoolUnitSize = 8;
            const size_t gc_maxSize_of_MemoryPoolUnitSize = gc_Array_Limit[_MACRO_ARRAY_COUNT(gc_Array_Limit) - 1];
        }
    }
}
}
/*----------------------------------------------------------------
/   데이터
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    namespace GLOBAL{
        // 메모리풀 바로가기 테이블
        // 354 슬롯 x86(1.38KB) x64(2.76KB)
        const size_t gc_Num_QuickLink_MemoryPool = gc_KB / 8
            + (gc_KB*4 - gc_KB) / 64
            + (gc_KB*16 - gc_KB*4) / 256
            + (gc_KB*64 - gc_KB*16) / gc_KB
            + (gc_KB*512 - gc_KB*64) / (gc_KB*8)
            + (gc_MB - gc_KB*512) / (gc_KB*64)
            + (gc_MB*10 - gc_MB) / (gc_KB*512);
        TQuickLinkMemoryPool g_Array_QuickLink_MemoryPool[gc_Num_QuickLink_MemoryPool] = {0};

        // 무결성 확인용 테이블
        _DEF_CACHE_ALIGN CTable_DataBlockHeaders g_Table_BlockHeaders_Normal;
        _DEF_CACHE_ALIGN CTable_DataBlockHeaders g_Table_BlockHeaders_Other;

        size_t g_nProcessor = 0;
        BOOL g_bDebug__Trace_MemoryLeak = FALSE;
        BOOL g_bDebug__Report_OutputDebug = TRUE;
    }
}
}
/*----------------------------------------------------------------
/   기타
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    namespace GLOBAL{
        size_t gFN_Get_Index__QuickLink_MemoryPool(size_t _Size)
        {
            _Assert(_Size >= 1);

            _Size -= 1;

            // 최적화에 의해 모든 비교값은 상수로서 코드에 삽입된다
            // VS2015 기준
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
        BOOL gFN_BuildData__QuickLink_MemoryPool()
        {
            size_t iFocus_L = 0;
            size_t size_Pool = SETTING::gc_Array_MinUnit[0];
            UINT32 iCountingPool = 0;

            const size_t l[] = {gc_KB,  gc_KB*4,    gc_KB*16,   gc_KB*64,   gc_KB*512,  gc_MB,      gc_MB*10};
            const size_t u[] = {8,      64,         256,        gc_KB,      gc_KB*8,    gc_KB*64,   gc_KB*512};
            size_t step_l = 0;
            size_t size_q = 0;
            for(auto index = gc_Num_QuickLink_MemoryPool - gc_Num_QuickLink_MemoryPool; index<gc_Num_QuickLink_MemoryPool; index++)
            {
                size_q += u[step_l];

                _Assert(size_q % u[step_l] == 0);
                if(size_q == l[step_l])
                    step_l++;

                if(size_q > size_Pool)
                {
                    size_Pool += SETTING::gc_Array_MinUnit[iFocus_L];

                    _Assert(size_Pool % SETTING::gc_Array_MinUnit[iFocus_L] == 0);
                    if(size_Pool == SETTING::gc_Array_Limit[iFocus_L])
                        iFocus_L++;

                    _Assert(size_Pool >= size_q);
                    iCountingPool++;
                }

                g_Array_QuickLink_MemoryPool[index].m_index_Pool = iCountingPool;
                g_Array_QuickLink_MemoryPool[index].m_pPool = nullptr;

                //_MACRO_OUTPUT_DEBUG_STRING("[%Iu] %Iu -> %Iu\n", index, size_q, size_Pool);
            }

            _Assert(iCountingPool == SETTING::gc_Num_MemoryPool-1);
            _Assert(size_q == SETTING::gc_maxSize_of_MemoryPoolUnitSize && size_Pool == SETTING::gc_maxSize_of_MemoryPoolUnitSize);
            return TRUE;
        }
        void gFN_Set_QuickLink_MemoryPoolPTR(size_t _Index, CMemoryPool* p)
        {
            if(_Index >= gc_Num_QuickLink_MemoryPool)
                return;

            g_Array_QuickLink_MemoryPool[_Index].m_pPool = p;
            //std::_Atomic_thread_fence(std::memory_order::memory_order_release);
        }
        DECLSPEC_NOINLINE size_t gFN_Calculate_UnitSize(size_t _Size)
        {
            // 이 함수는 다음의 함수에서 사용하기 위해 작성되었으며 가능한 호출되지 않아야 한다
            //      CMemoryPool_Manager::mFN_Get_MemoryPool_Internal(size_t _Size)
            if(SETTING::gc_maxSize_of_MemoryPoolUnitSize < _Size)
                return 0;
            if(!_Size)
                return _Size;


        #pragma message("gFN_Calculate_UnitSize 수정시 체크 사항: 할당정렬 단위의 배수는 캐시라인의 크기의 배수 또는 약수")

            // 풀을 다양하게 분할할 수록 스레드가 단독접근할 확률은 올라가겠지만,
            // 핸들을 많이 소모한다
            // 종류 258

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

            for(int i=0; i<SETTING::gc_Array_Limit__Count; i++)
            {
                if(_Size <= SETTING::gc_Array_Limit[i])
                {
                    const auto m = SETTING::gc_Array_MinUnit[i];
                    return (_Size + m - 1) / m * m;
                }
            }

            _DebugBreak(_T("Failed gFN_Calculate_UnitSize(%Iu)"), _Size);
            return SETTING::gc_maxSize_of_MemoryPoolUnitSize;
        }
        DECLSPEC_NOINLINE size_t gFN_Calculate_UnitSizeMin(size_t _UnitSizeMax)
        {
            size_t iChunk = MAXSIZE_T;
            for(size_t i=0; i<SETTING::gc_Array_Limit__Count; i++)
            {
                if(_UnitSizeMax <= SETTING::gc_Array_Limit[i])
                {
                    iChunk = i;
                    break;
                }
            }
            if(iChunk == MAXSIZE_T)
            {
                _DebugBreak("Failed gFN_Calculate_UnitSizeMin(%Iu)", _UnitSizeMax);
                return _UnitSizeMax;
            }

            return _UnitSizeMax - SETTING::gc_Array_MinUnit[iChunk] + 1;
        }

    #pragma region Baskets , Basket_CACHE 의 실행타이밍에 의한 동적배수에 해당하는 오브젝트풀의 고속 접근을 위한 함수
        CMemoryBasket* gFN_Baskets_Alloc__Default()
        {
            // 정의 되지 않은 수는 기본 할당자 사용
            auto p = ::_aligned_malloc(sizeof(CMemoryBasket)*g_nProcessor, _DEF_CACHELINE_SIZE);
            if(!p)
                _Error_OutOfMemory();
            return (CMemoryBasket*)p;
        }
        void gFN_Baskets_Free__Default(void* p)
        {
            ::_aligned_free(p);
        }
        template<size_t Size>
        CMemoryBasket* gFN_Baskets_Alloc()
        {
            CObjectPool_Handle_TSize<Size*sizeof(CMemoryBasket)>::Reference_Attach();
            return (CMemoryBasket*)CObjectPool_Handle_TSize<Size*sizeof(CMemoryBasket)>::Alloc();
        }
        template<size_t Size>
        void gFN_Baskets_Free(void* p)
        {
            CObjectPool_Handle_TSize<Size*sizeof(CMemoryBasket)>::Free(p);
            CObjectPool_Handle_TSize<Size*sizeof(CMemoryBasket)>::Reference_Detach();
        }

        CMemoryBasket* (*gpFN_Baskets_Alloc)(void) = nullptr;
        void (*gpFN_Baskets_Free)(void*) = nullptr;
    #pragma endregion

    }
}
}


/*----------------------------------------------------------------
/   메모리풀 - BadSize
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
        _DEF_CACHE_ALIGNED_TEST__THIS();

        _MACRO_STATIC_ASSERT(CMemoryPool__BadSize::sc_bUse_Header_per_Unit == TRUE);
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
        TDATA_BLOCK_HEADER* p = (TDATA_BLOCK_HEADER*)::_aligned_malloc_dbg(_RealSize,  sc_AlignSize, _FileName, _Line);
        if(!p)
            return nullptr;

        p->ParamBad.m_RealUnitSize = _RealSize;


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
                if(nMaxMemNow == InterlockedCompareExchange(&m_stats_Maximum_AllocatedSize, nUsingSizeNow, nMaxMemNow))
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
        TDATA_BLOCK_HEADER* p = (TDATA_BLOCK_HEADER*)::_aligned_malloc(_RealSize, sc_AlignSize);
        if(!p)
            return nullptr;

        p->ParamBad.m_RealUnitSize = _RealSize;


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
                if(nMaxMemNow == InterlockedCompareExchange(&m_stats_Maximum_AllocatedSize, nUsingSizeNow, nMaxMemNow))
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
            m_stats_Counting_Free_BadPTR++;
            MemoryPool_UTIL::sFN_Report_Invalid_Deallocation(pAddress);
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
    BOOL CMemoryPool__BadSize::mFN_Query_This_UnitsSizeLimit(size_t * pOUT_Min, size_t * pOUT_Max) const
    {
        UNREFERENCED_PARAMETER(pOUT_Min);
        UNREFERENCED_PARAMETER(pOUT_Max);
        return FALSE;
    }
    BOOL CMemoryPool__BadSize::mFN_Query_isSlowMemoryPool() const
    {
        return TRUE;
    }
    BOOL CMemoryPool__BadSize::mFN_Query_MemoryUnit_isAligned(size_t AlignSize) const
    {
        if(!AlignSize)
            return FALSE;

        return (0 == sc_AlignSize % AlignSize);
    }
    BOOL CMemoryPool__BadSize::mFN_Query_Stats(TMemoryPool_Stats* p)
    {
        if(!p)
            return FALSE;

        ::memset(p, 0, sizeof(TMemoryPool_Stats));
        
        // 바쁜 비교
        // 문제가 된다면 잠금 추가
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

    size_t CMemoryPool__BadSize::mFN_Reserve_Memory__nSize(size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Reserve_Memory__nUnits(size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Reserve_Memory__Max_nSize(size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Reserve_Memory__Max_nUnits(size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Hint_MaxMemorySize_Add(size_t, BOOL)
    {
        return 0;
    }
    size_t CMemoryPool__BadSize::mFN_Hint_MaxMemorySize_Set(size_t, BOOL)
    {
        return 0;
    }
    BOOL CMemoryPool__BadSize::mFN_Set_OnOff_ProcessorCACHE(BOOL)
    {
        return FALSE;
    }
    BOOL CMemoryPool__BadSize::mFN_Query_OnOff_ProcessorCACHE() const
    {
        return FALSE;
    }
#pragma endregion
    //----------------------------------------------------------------
    void CMemoryPool__BadSize::mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On)
    {
        m_bWriteStats_to_LogFile = _On;
        std::_Atomic_thread_fence(std::memory_order::memory_order_release);
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
        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Other_HN){}
        else if(pH->m_pPool != this){}
        else if(!pH->m_pFirstHeader || pH->m_pFirstHeader->m_pPool != this){}
        else if(!pH->mFN_Query_ContainPTR(pAddress)){}
        else if(pH->m_User.m_pValidPTR_S != pH->m_User.m_pValidPTR_L){}
        else { bBroken = FALSE; }
        _AssertMsg(FALSE == bBroken, "Broken Header Data");
    #endif

        TDATA_BLOCK_HEADER* p = (TDATA_BLOCK_HEADER*)pAddress;
        p--;

        const auto size = p->ParamBad.m_RealUnitSize;
        auto prevSize = InterlockedExchangeSubtract(&m_UsingSize, size);
        auto prevCNT  = InterlockedExchangeSubtract(&m_UsingCounting, 1);
        if(prevSize < size || prevCNT == 0)
        {
        #ifdef _DEBUG
            size_t a = prevSize;
            size_t b = prevCNT;
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("메모리 반납 무효: 메모리풀 이전값(UsingCounting %Iu, UsingSize %Iu Byte) 반납크기(%Iu Byte)\n"
                , (const size_t)b, (const size_t)a, (const size_t)size);
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

        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Other_HN)
            return FALSE;
        if(pH != pH->m_pFirstHeader)
            return FALSE;
        if(!pH->mFN_Query_ContainPTR(pAddress))
            return FALSE;

        const TDATA_BLOCK_HEADER* pHTrust = GLOBAL::g_Table_BlockHeaders_Other.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
        if(pH->m_pFirstHeader != pHTrust)
            return FALSE;

        // 테스트 하지 않아도 신뢰한다
        //if(pHTrust->m_pPool != this)
        //    return nullptr;
        return TRUE;
    }
    size_t CMemoryPool__BadSize::mFN_Calculate_UnitSize(size_t size) const
    {
        _MACRO_STATIC_ASSERT(1 == _MACRO_STATIC_BIT_COUNT(_DEF_CACHELINE_SIZE));
        const size_t under_mask_on = _DEF_CACHELINE_SIZE - 1;
        const size_t under_mask_off = ~under_mask_on;
        if(size & under_mask_on)
            size = (size & under_mask_off) + _DEF_CACHELINE_SIZE;
        else if(0 == size)
            size = _DEF_CACHELINE_SIZE;
        
        return size;
    }
    void CMemoryPool__BadSize::mFN_Register(TDATA_BLOCK_HEADER * p, size_t /*size*/)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(p != nullptr);

        p->m_Type = TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Other_HN;

        p->m_User.m_pValidPTR_S = p+1;
        p->m_User.m_pValidPTR_L = p+1;

        p->m_pPool = this;
        p->m_pFirstHeader = p;

        GLOBAL::g_Table_BlockHeaders_Other.mFN_Register(p);
    #pragma warning(pop)
    }

    void CMemoryPool__BadSize::mFN_UnRegister(TDATA_BLOCK_HEADER * p)
    {
        _Assert(p != nullptr);
        GLOBAL::g_Table_BlockHeaders_Other.mFN_UnRegister(p);
    }
    void CMemoryPool__BadSize::mFN_Debug_Report()
    {
        if(0 < m_stats_Maximum_AllocatedSize)
        {
            LPCTSTR szWarning = _T("MemoryPool Performance Warning : Used Not Supported Size(%Iu ~ ...) Total %IuByte");
            _DebugBreak(szWarning, GLOBAL::SETTING::gc_maxSize_of_MemoryPoolUnitSize+1, m_stats_Maximum_AllocatedSize);
            _LOG_SYSTEM(szWarning, GLOBAL::SETTING::gc_maxSize_of_MemoryPoolUnitSize+1, m_stats_Maximum_AllocatedSize);
        }

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
            _LOG_SYSTEM__WITH__OUTPUTDEBUGSTR(FALSE, _T("MemoryPool[OtherSize] Invalid Free Reques Count : %Iu"), m_stats_Counting_Free_BadPTR);
        }
    }
}
}


/*----------------------------------------------------------------
/   메모리풀 - 유닛그룹 보관 컨테이너
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    CMemoryUnitsGroup_List::CMemoryUnitsGroup_List(TMemoryUnitsGroup::EState _this_list_state)
        : m_pF(nullptr), m_pL(nullptr)
        , m_cntUnitsGroup(0)
        , m_cntRecycleChunk(0)
        , m_pRecycleF(nullptr), m_pRecycleL(nullptr)
        , mc_GroupState(_this_list_state)
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();
    }
    CMemoryUnitsGroup_List::~CMemoryUnitsGroup_List()
    {
        // 보유한 유닛그룹들의 사용상태는 확인하지 않는다
        // 프로세서단위, 스레드단위로 지역캐시공간에 흩어져 있기 때문이다
        for(auto p = m_pRecycleF; p;)
        {
            auto pNext = p->m_pNext;

            mFN_Chunk_FreeMemory(p);
            
            m_cntRecycleChunk--;
            p = pNext;
        }
    }
    BOOL CMemoryUnitsGroup_List::mFN_Query_IsEmpty() const
    {
        return 0 == m_cntUnitsGroup;
    }
    //----------------------------------------------------------------
    //  CMemoryUnitsGroup_List 잠금 정책 순위
    //      잠금이 확보된 상태의 지정된 유닛그룹을 Pop   - Busy
    //      1개 또는 n개 의 유닛그룹 Push                - Busy
    //      1개의 미지정 유닛그룹 Pop                    - Lazy
    //  ※ 유닛그룹 지정POP은 미지정POP 보다 우선순위가 높아야 한다
    //      만약 그렇지 않다면 지정POP을 요청한 스레드는 대기시간이 길어질 가능성이 높다
    //      싱글스레드에서 아주 약간의 성능 감소가 있지만, 멀티스레드에서 큰 성능 향상이 있다
    //----------------------------------------------------------------
    UINT32 CMemoryUnitsGroup_List::mFN_Pop(TMemoryUnitsGroup* p)
    {
        _Assert(p);

        UINT32 ret;
        m_Lock.Lock__Busy();
        {
            ret = ((mFN_Private__Pop(p))? 1 : 0);
        }
        m_Lock.UnLock();
        
        return ret;
    }
    UINT32 CMemoryUnitsGroup_List::mFN_Pop(TMemoryUnitsGroup* p, UINT32 n)
    {
    #pragma warning(push)
    #pragma warning(disable: 28182)
        _Assert(p && 0 < n);

        UINT32 ret = 0;
        m_Lock.Lock__Busy();
        for(UINT32 i=0; i<n; i++)
        {
            TMemoryUnitsGroup* pUG = p+i;
            _Assert(pUG->m_Lock.Query_isLocked_from_CurrentThread());
            if(pUG->m_State != mc_GroupState)
                continue;

            _Assert(pUG->m_pContainerChunkFocus);
            if(mFN_Private__Pop(pUG))
                ret++;
        }
        m_Lock.UnLock();

        return ret;
    #pragma warning(pop)
    }
    TMemoryUnitsGroup* CMemoryUnitsGroup_List::mFN_PopFront()
    {
    #pragma warning(push)
    #pragma warning(disable: 6385)
        m_Lock.Lock__Lazy();

        if(0 == m_cntUnitsGroup)
        {
            m_Lock.UnLock();
            return nullptr;
        }

        _Assert(m_pF && m_pL);
        for(auto pChunk = m_pF; pChunk; pChunk = pChunk->m_pNext)
        {
            // 높은 우선순위로 다른 곳에서 잠금중인 유닛그룹들이 있다면
            // 첫번째 청크에서 끝나지 않을 가능성이 있다
            _Assert(0 != pChunk->m_iSlotPush);
            _Assert(pChunk->m_iSlotPush <= TChunk::sc_MaxSlot);
            _Assert(pChunk->m_iSlotPop < pChunk->m_iSlotPush);
            for(auto i=pChunk->m_iSlotPop; i<pChunk->m_iSlotPush; i++)
            {
                auto pUG = pChunk->m_Slot[i];
                if(pUG)
                {
                    if(pUG->m_Lock.Lock__NoInfinite(4))
                    {
                        // Chunk 상태 갱신
                        mFN_Chunk_UpdatePop(pChunk, i);
                        m_Lock.UnLock();

                        // 유닛그룹 업데이트
                        _Assert(pUG->m_State == mc_GroupState);
                        pUG->m_State = TMemoryUnitsGroup::EState::E_Unknown;
                        // 잘못된 사용시 에러가 발생하도록 유도한다
                        pUG->m_pContainerChunkFocus = nullptr;
                        pUG->m_iContainerChunk_Detail_Index = TMemoryUnitsGroup::sc_InitialValue_iContainerChunk_Detail_Index;

                        return pUG;
                    }
                }
            }
        }


        m_Lock.UnLock();
        // 보유중인 모든 유닛그룹들이 잠겨 있는 경우가 있다
        // 이 확률은 매우 낮다
        return nullptr;
    #pragma warning(pop)
    }
    void CMemoryUnitsGroup_List::mFN_Push(TMemoryUnitsGroup* p)
    {
        _Assert(p);

        m_Lock.Lock__Busy();
        {
            mFN_Private__Push(p);
        }
        m_Lock.UnLock();
    }
    void CMemoryUnitsGroup_List::mFN_Push(TMemoryUnitsGroup* p, UINT32 n)
    {
        _Assert(p && 0 < n);

        m_Lock.Lock__Busy();
        for(UINT32 i=0;i<n;i++)
            mFN_Private__Push(p+i);
        m_Lock.UnLock();
    }
    DECLSPEC_NOINLINE BOOL CMemoryUnitsGroup_List::mFN_Private__PopFullSeach(TMemoryUnitsGroup* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        // 이 메소드는 비공개 메소드이며, mFN_Private__Pop(p, ...) 계열에서 사용한다
        // 이 함수가 사용되는 경우는 유닛그룹의 m_pContainerChunkFocus 데이터가 손상된 경우이다
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(p && p->m_Lock.Query_isLocked_from_CurrentThread());
        BOOL bFound = FALSE;
        for(auto pChunk = m_pF; pChunk; pChunk = pChunk->m_pNext)
        {
            for(auto i=pChunk->m_iSlotPop; i<pChunk->m_iSlotPush; i++)
            {
                if(pChunk->m_Slot[i] == p)
                {
                    mFN_Chunk_UpdatePop(pChunk, i);
                    bFound = TRUE;
                    break;
                }
            }
            if(bFound)
                break;
        }

        // 손상된 데이터 리포트
        _LOG_DEBUG__WITH__OUTPUTDEBUGSTR(FALSE, _T("Damaged MemoryPool UG\n")
            _T("\t0x%p Value :0x%p\n")
            _T("\t0x%p Value :0x%X")
            , &p->m_pContainerChunkFocus, p->m_pContainerChunkFocus
            , &p->m_iContainerChunk_Detail_Index, p->m_iContainerChunk_Detail_Index);

        if(bFound)
        {
            // 유닛그룹 업데이트
            p->m_State = TMemoryUnitsGroup::EState::E_Unknown;
            // 잘못된 사용시 에러가 발생하도록 유도한다
            p->m_pContainerChunkFocus = nullptr;
            p->m_iContainerChunk_Detail_Index = TMemoryUnitsGroup::sc_InitialValue_iContainerChunk_Detail_Index;
        }
        return bFound;
    #pragma warning(pop)
    }
    BOOL CMemoryUnitsGroup_List::mFN_Private__Pop(TMemoryUnitsGroup * p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
    #pragma warning(disable: 28182)
        // 이 메소드는 비공개 메소드이며, mFN_Pop(p, ...) 계열에서 사용한다
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());

        _Assert(p && p->m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(p->m_pContainerChunkFocus && p->m_iContainerChunk_Detail_Index < TChunk::sc_MaxSlot && p->m_State == mc_GroupState);

        // 만약 m_pContainerChunkFocus 이 엉뚱한 값으로 손상되어 있다면 에러가 발생할 가능성이 있다
        auto pChunk = (TChunk*)p->m_pContainerChunkFocus;
        const auto iQuickIndex = p->m_iContainerChunk_Detail_Index;
        if(p->m_State != mc_GroupState)
            return FALSE;
        if(!pChunk)
            mFN_Private__PopFullSeach(p);

        if(iQuickIndex < pChunk->sc_MaxSlot
            && pChunk->m_Slot[iQuickIndex] == p)
        {
            // 저장된 인덱스가 유효하다면 즉시 처리
            mFN_Chunk_UpdatePop(pChunk, iQuickIndex);
        }
        else
        {
            // 그렇지 않다면 청크의 슬롯들을 탐색한다
            BOOL bFound = FALSE;
            for(auto i=pChunk->m_iSlotPop; i<pChunk->m_iSlotPush; i++)
            {
                if(pChunk->m_Slot[i] == p)
                {
                    mFN_Chunk_UpdatePop(pChunk, i);
                    bFound = TRUE;
                    break;
                }
            }
            if(!bFound)
                return mFN_Private__PopFullSeach(p);
        }

        // 유닛그룹 업데이트
        p->m_State = TMemoryUnitsGroup::EState::E_Unknown;
        // 잘못된 사용시 에러가 발생하도록 유도한다
        p->m_pContainerChunkFocus = nullptr;
        p->m_iContainerChunk_Detail_Index = TMemoryUnitsGroup::sc_InitialValue_iContainerChunk_Detail_Index;

        return TRUE;
    #pragma warning(pop)
    }
    void CMemoryUnitsGroup_List::mFN_Private__Push(TMemoryUnitsGroup* p)
    {
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());

        _Assert(p->m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(p->m_State == TMemoryUnitsGroup::EState::E_Unknown);

        if(p->m_State != TMemoryUnitsGroup::EState::E_Unknown)
        {
            p->m_Lock.UnLock();
            return;
        }

        TChunk* pChunk;
        UINT32 iSlot;

        if(m_pL && m_pL->m_iSlotPush < m_pL->sc_MaxSlot)
            pChunk = m_pL;
        else
            pChunk = mFN_Chunk_Add();

        iSlot = pChunk->m_iSlotPush++;
        pChunk->m_Slot[iSlot] = p;
        m_cntUnitsGroup++;

        // 유닛 그룹 업데이트
        p->m_State = mc_GroupState;
        p->m_pContainerChunkFocus = pChunk;
        p->m_iContainerChunk_Detail_Index = iSlot;
        
        p->m_Lock.UnLock();
    }
    void CMemoryUnitsGroup_List::mFN_Chunk_UpdatePop(CMemoryUnitsGroup_List::TChunk* pChunk, UINT32 index)
    {
    #pragma warning(push)
    #pragma warning(disable: 6385)
    #pragma warning(disable: 6386)
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());

        _Assert(0 < m_cntUnitsGroup);
        _Assert(pChunk);
        _Assert(index < TChunk::sc_MaxSlot && index >= pChunk->m_iSlotPop && index < pChunk->m_iSlotPush);
        _Assert(pChunk->m_Slot[index]);

        m_cntUnitsGroup--;
        pChunk->m_Slot[index] = nullptr;
        if(index+1 == pChunk->m_iSlotPush)
        {
            // 가장 마지막 유닛이 비어, Push 위치를 앞당김
            auto iLastFreeSlot = index;
            if(index > pChunk->m_iSlotPop)
            {
                
                for(auto i=iLastFreeSlot-1;i>pChunk->m_iSlotPop;i--)
                {
                    if(pChunk->m_Slot[i])
                        break;
                    iLastFreeSlot = i;
                }
            }
            pChunk->m_iSlotPush = iLastFreeSlot;
        }

        if(index == pChunk->m_iSlotPop)
        {
            for(auto i=index+1; i<pChunk->m_iSlotPush; i++)
            {
                if(pChunk->m_Slot[i])
                {
                    pChunk->m_iSlotPop = i;
                    return;
                }
            }
            // 청크의 모든 슬롯이 비었음
            pChunk->m_iSlotPop = pChunk->m_iSlotPush = 0;
        }

        if(pChunk->m_iSlotPush == 0)
        {
            mFN_Chunk_Remove(pChunk);
            return;
        }
    #pragma warning(pop)
    }
    DECLSPEC_NOINLINE CMemoryUnitsGroup_List::TChunk* CMemoryUnitsGroup_List::mFN_Chunk_Add()
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
    #pragma warning(disable: 28182)
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());

        // 메모리 확보
        CMemoryUnitsGroup_List::TChunk* pNew;
        if(0 == m_cntRecycleChunk)
        {
            _Assert(!m_pRecycleF && !m_pRecycleL);
            pNew = mFN_Chunk_AllocMemory();
        }
        else
        {
            _Assert(m_pRecycleF && m_pRecycleL);
            pNew = m_pRecycleL;

            if(1 == m_cntRecycleChunk)
            {
                _Assert(m_pRecycleF == m_pRecycleL && !m_pRecycleL->m_pPrev && !m_pRecycleL->m_pNext);
                m_pRecycleF = m_pRecycleL = nullptr;
            }
            else
            {
                _Assert(2 <= m_cntRecycleChunk);
                _Assert(m_pRecycleF != m_pRecycleL && m_pRecycleL->m_pPrev && !m_pRecycleL->m_pNext);
                m_pRecycleL = m_pRecycleL->m_pPrev;
                m_pRecycleL->m_pNext = nullptr;
            }
            m_cntRecycleChunk--;
        }

        // 관리
        _MACRO_CALL_CONSTRUCTOR(pNew, CMemoryUnitsGroup_List::TChunk);
        if(m_pL)
        {
            _Assert(m_pF);
            
            m_pL->m_pNext = pNew;
            pNew->m_pPrev = m_pL;
            m_pL = pNew;
        }
        else
        {
            _Assert(!m_pF);
            m_pF = m_pL = pNew;
        }
        return pNew;
    #pragma warning(pop)
    }
    DECLSPEC_NOINLINE void CMemoryUnitsGroup_List::mFN_Chunk_Remove(CMemoryUnitsGroup_List::TChunk* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(p && m_Lock.Query_isLocked_from_CurrentThread());
    
        _MACRO_CALL_DESTRUCTOR(p);

        if(m_pF == p)
        {
            _Assert(!p->m_pPrev);
            m_pF = p->m_pNext;
        }
        else
        {
            _Assert(p->m_pPrev);
            p->m_pPrev->m_pNext = p->m_pNext;
        }

        if(m_pL == p)
        {
            _Assert(!p->m_pNext);
            m_pL = p->m_pPrev;
        }
        else
        {
            _Assert(p->m_pNext);
            p->m_pNext->m_pPrev = p->m_pPrev;
        }


        // 객체 재활용 처리 / 메모리 반납
        if(m_cntRecycleChunk < sc_MaxRecycleStorage)
        {
            p->m_pPrev = m_pRecycleL;
            p->m_pNext = nullptr;

            if(m_pRecycleL)
            {
                m_pRecycleL->m_pNext = p;
            }
            else
            {
                _Assert(!m_pRecycleF);
                m_pRecycleF = p;
            }
            m_pRecycleL = p;
            m_cntRecycleChunk++;
        }
        else
        {
            mFN_Chunk_FreeMemory(p);
        }
    #pragma warning(pop)
    }
    CMemoryUnitsGroup_List::TChunk* CMemoryUnitsGroup_List::mFN_Chunk_AllocMemory()
    {
        return (CMemoryUnitsGroup_List::TChunk*)::UTIL::MEM::CObjectPool_Handle<CMemoryUnitsGroup_List::TChunk>::Alloc();
    }
    void CMemoryUnitsGroup_List::mFN_Chunk_FreeMemory(CMemoryUnitsGroup_List::TChunk* p)
    {
        ::UTIL::MEM::CObjectPool_Handle<CMemoryUnitsGroup_List::TChunk>::Free(p);
    }
}
}
/*----------------------------------------------------------------
/   메모리풀 - TLS 
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
#pragma warning(push)
#pragma warning(disable: 4324)
    struct _DEF_CACHE_ALIGN TMemoryPool_TLS_CACHE{
        // gFN_Calculate_UnitSize(size_t _Size) 참조
        static const size_t c_Invalid_ID = MAXSIZE_T;
        static const size_t c_Invalid_Code = 0;
        static const UINT32 c_Num_ExclusiveUnitsSlot = 32 + 16*4; // MAX Pool Size : 4KB
        //----------------------------------------------------------------
        BOOL m_bUsedtAffinity; // 설정시 m_ID 즉시 접근
    #if __X64
        BOOL : 32;
    #endif

        size_t m_ID;
        size_t m_Code;
    #pragma pack(push, 2)
        struct TUNITS{
            static const UINT32 sc_Fill_Size_Maximal = 64 * 1024;   // Fill 크기 : 최대
            static const UINT32 sc_Fill_nUnits_Minimal = 1;         // Fill 수량 : 최소
            static const UINT32 sc_Unfill_nUnits_DemandNonZero = 8; // Unfill 수량 : 수요가 있을때
            static const UINT32 sc_Unfill_nUnits_DemandZero = 4;    // Unfill 수량 : 수요가 없을때
            //--------------------------------
        #if __X64
            typedef typename UINT32 _TYPE_UnitCount;
        #elif __X86
            typedef typename UINT16 _TYPE_UnitCount;
        #endif
            static const _TYPE_UnitCount sc_Demand_Max = 64;
            //----------------------------------------------------------------

            TMemoryObject*  m_pUnit = nullptr;
            _TYPE_UnitCount m_cntUnits = 0;
            ::UTIL::NUMBER::TCounting_T<_TYPE_UnitCount, 0, sc_Demand_Max> m_Demand = 0;

            TMemoryObject* Get_Unit();
            UINT32 Ret_Unit(TMemoryObject* p);
            void Fill(TMemoryUnitsGroup* pChunk);
            void UnFill(CMemoryPool* pPool, UINT32 nUnits);
            void Storage(TMemoryObject* pFirst, TMemoryObject* pLast, UINT32 nUnits);

        }m_Array_ExclusiveUnits[c_Num_ExclusiveUnitsSlot]; // Units CACHE
    #pragma pack(pop)
        //----------------------------------------------------------------
        static const size_t sc_TypeSize_TUnits = sizeof(TUNITS);
        static const size_t sc_SizeExclusiveUnits = sc_TypeSize_TUnits * c_Num_ExclusiveUnitsSlot;
        _MACRO_STATIC_ASSERT(sizeof(TUNITS::m_Demand) == sizeof(TUNITS::_TYPE_UnitCount));
        _MACRO_STATIC_ASSERT(sc_TypeSize_TUnits == sizeof(void*) + sizeof(TUNITS::_TYPE_UnitCount)*2);
        //----------------------------------------------------------------
        TMemoryPool_TLS_CACHE()
            : m_bUsedtAffinity(FALSE)
            , m_Code(c_Invalid_Code)
            , m_ID(c_Invalid_ID) // 초기값 모든 비트 ON
        {
        }
        ~TMemoryPool_TLS_CACHE()
        {
            BOOL isDamaged = FALSE;
            for(UINT32 i=0; i<c_Num_ExclusiveUnitsSlot; i++)
            {
                auto& s = m_Array_ExclusiveUnits[i];
                if(!s.m_cntUnits && !s.m_pUnit)
                    continue;

                _Assert(0 < s.m_cntUnits && s.m_pUnit);
                if(!s.m_cntUnits || !s.m_pUnit)
                {
                    isDamaged = TRUE;
                    continue;
                }

                auto pPool = ((UTIL::MEM::CMemoryPool_Manager*)g_pMem)->mFN_Get_MemoryPool_QuickAccess(i);
                if(pPool)
                    s.UnFill(pPool, s.m_cntUnits);
                else
                    isDamaged = TRUE;
            }
            if(isDamaged)
                mFN_Report_Damaged_TLS();
        }
        DECLSPEC_NOINLINE void mFN_Set_On_AffinityProcessor(size_t iProcessor)
        {
            _Assert(iProcessor < GLOBAL::g_nProcessor);
            m_bUsedtAffinity = TRUE;
            m_ID = iProcessor;
            m_Code = c_Invalid_Code;
        }
        DECLSPEC_NOINLINE void mFN_Set_Off_AffinityProcessor()
        {
            m_bUsedtAffinity = FALSE;
            m_ID = c_Invalid_ID;
            m_Code = c_Invalid_Code;
        }

        DECLSPEC_NOINLINE void mFN_Report_Damaged_TLS()
        {
            _LOG_SYSTEM__WITH__OUTPUTDEBUGSTR(TRUE, _T("Damaged MemoryPool Thread Local Storage Bin(please check stack memory overflow"));
        }
    };
    namespace{
        const size_t c_Size_TMemoryPool_TLS_CACHE = sizeof(TMemoryPool_TLS_CACHE);
        _MACRO_STATIC_ASSERT(c_Size_TMemoryPool_TLS_CACHE % _DEF_CACHELINE_SIZE == 0);
    }
    //----------------------------------------------------------------
    __forceinline TMemoryObject* TMemoryPool_TLS_CACHE::TUNITS::Get_Unit()
    {
        if(!m_pUnit)
            return nullptr;

        TMemoryObject* ret = m_pUnit;
        m_pUnit = m_pUnit->pNext;
        m_cntUnits--;

        ++m_Demand;

        return ret;
    }
    UINT32 TMemoryPool_TLS_CACHE::TUNITS::Ret_Unit(TMemoryObject* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(p);
        p->pNext = m_pUnit;
        m_pUnit = p;
        m_cntUnits++;

        --m_Demand;
        if(m_cntUnits <= m_Demand)
            return 0;

        const UINT32 nOverDemand = m_cntUnits - m_Demand;
        if(m_Demand && nOverDemand < sc_Unfill_nUnits_DemandNonZero)
            return 0u;
        else if(!m_Demand && nOverDemand < sc_Unfill_nUnits_DemandZero)
            return 0u;

        return nOverDemand;
    #pragma warning(pop)
    }
    void TMemoryPool_TLS_CACHE::TUNITS::Fill(TMemoryUnitsGroup* pChunk)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _CompileHint(pChunk && !pChunk->mFN_Query_isEmpty());
        _Assert(pChunk->m_Lock.Query_isLocked_from_CurrentThread());
        
        UINT32 n = m_Demand - m_cntUnits;
        size_t size = n * pChunk->m_UnitSize;
        if(size > sc_Fill_Size_Maximal)
            n = sc_Fill_Size_Maximal / (UINT32)pChunk->m_UnitSize;
        if(n < sc_Fill_nUnits_Minimal)
            n = sc_Fill_nUnits_Minimal;

        TMemoryObject* pF;
        TMemoryObject* pL;
        const auto cntResult = pChunk->mFN_Pop(&pF, &pL, n);
        Storage(pF, pL, cntResult);
    #pragma warning(pop)
    }
    namespace{
        DECLSPEC_NOINLINE void gFN_Error_Report__Damaged_MemoryPoolTLS(void* pUnit)
        {
            _MACRO_OUTPUT_DEBUG_STRING_TRACE_ALWAYS(_T("Damaged MemoryPool TLS(Address : 0x%p)\n"), pUnit);
        }
    }
    void TMemoryPool_TLS_CACHE::TUNITS::UnFill(CMemoryPool* pPool, UINT32 nUnits)
    {
        _Assert(pPool && nUnits <= m_cntUnits);
    #pragma warning(push)
    #pragma warning(disable: 6011)
    #pragma warning(disable: 6385)
    #pragma warning(disable: 6386)
    #pragma warning(disable: 28182)
        struct __declspec(align(sizeof(void*))) TGiveBackChunk{
            const TMemoryUnitsGroup* pUG;
            TMemoryObject* pF;
            TMemoryObject* pL;
            UINT32 cntUnits;
        };
        // 스택 크기 사용 제한
        {
            _MACRO_STATIC_ASSERT(0 == sizeof(TGiveBackChunk) % sizeof(void*));

            const size_t c_Max_Units = max((TUNITS::sc_Demand_Max + TUNITS::sc_Unfill_nUnits_DemandNonZero), TUNITS::sc_Unfill_nUnits_DemandZero);
            const size_t c_Max_Allocasize = (c_Max_Units * sizeof(TMemoryObject*) + sizeof(TGiveBackChunk));
            const size_t c_Other_StackSize = 512;
        #if __X86
            const size_t c_Limit_StackSize = 1 * 1024;
        #elif __X64
            const size_t c_Limit_StackSize = 2 * 1024;
        #else
            const size_t c_Limit_StackSize = ?;
        #endif
            _MACRO_STATIC_ASSERT(c_Max_Allocasize + c_Other_StackSize <= c_Limit_StackSize);
        }

        const size_t c_NeedSize__Units = nUnits * sizeof(TMemoryObject*);
        const size_t c_NeedSize__GiveBackChunk = nUnits * sizeof(TGiveBackChunk);
        const size_t c_NeedSize = c_NeedSize__Units + c_NeedSize__GiveBackChunk;
        byte* const pLocalTempBuffer = (byte*)_alloca(c_NeedSize);

        TMemoryObject** pUnFill_Units = (TMemoryObject**)pLocalTempBuffer;
        TGiveBackChunk* pGiveBackChunk = (TGiveBackChunk*)(pLocalTempBuffer + c_NeedSize__Units);
        UINT32 countingGiveBackChunk = 0;

        // 수집
        TMemoryObject* pFocus = m_pUnit;
        for(UINT32 i=0; i<nUnits; i++)
        {
            pUnFill_Units[i] = pFocus;
            pFocus = pFocus->pNext;
        }

        m_cntUnits -= decltype(m_cntUnits)(nUnits);
        m_pUnit = pFocus;

        // 주소순 정렬
        std::sort(pUnFill_Units, pUnFill_Units+nUnits);

        // 반납유닛의 소속유닛그룹별 구분
        // _DEF_USING_MEMORYPOOL_DEBUG 모드가 아니라면
        // 사용자가 반납했던 자원들은 손상되지 않았다고 가정한다
        for(UINT32 i=0; i<nUnits;)
        {
            auto pCur = pUnFill_Units[i];
            // 그룹중 첫번째
            const TDATA_BLOCK_HEADER* pH;
            if(!pPool->mFN_Query_UsingHeader_per_Unit())
                pH = CMemoryPool_Manager::sFN_Get_DataBlockHeader_SmallObjectSize(pCur);
            else
                pH = CMemoryPool_Manager::sFN_Get_DataBlockHeader_NormalObjectSize(pCur);

        #if _DEF_USING_MEMORYPOOL_DEBUG
            if(!pH || !pH->mFN_Query_AlignedPTR(pCur))
            {
                gFN_Error_Report__Damaged_MemoryPoolTLS(pCur);
                i++;
                continue;
            }
        #endif

            TGiveBackChunk& refChunk = pGiveBackChunk[countingGiveBackChunk];
            countingGiveBackChunk++;

            refChunk.pUG = pH->mFN_Get_UnitsGroup(pH->mFN_Calculate_UnitsGroupIndex(pCur));
            refChunk.pF = refChunk.pL = pCur;
            refChunk.cntUnits = 1;
            const TMemoryObject* pEnd = (const TMemoryObject*)refChunk.pUG->mFN_Query_Valid_Memory_End();

            // 뒤이어 오는 유닛들은 같은 소속일 확률이 높다
            for(++i; i<nUnits; ++i)
            {
                auto pNext = pUnFill_Units[i];
                if(pEnd <= pNext)
                    break;

            #if _DEF_USING_MEMORYPOOL_DEBUG
                if(!pH->mFN_Query_AlignedPTR(pNext))
                {
                    gFN_Error_Report__Damaged_MemoryPoolTLS(pNext);
                    break;
                }
            #endif

                refChunk.pL->pNext = pNext;
                refChunk.pL = pNext;
                refChunk.cntUnits++;
            }
            refChunk.pL->pNext = nullptr;
        }
        // 반납
        for(UINT32 i=0; i<countingGiveBackChunk; i++)
        {
            TGiveBackChunk& refChunk = pGiveBackChunk[i];
            pPool->mFN_GiveBack_Units(const_cast<TMemoryUnitsGroup*>(refChunk.pUG), refChunk.pF, refChunk.pL, refChunk.cntUnits);
        }
    #pragma warning(pop)
    }

    void TMemoryPool_TLS_CACHE::TUNITS::Storage(TMemoryObject* pFirst, TMemoryObject* pLast, UINT32 nUnits)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        if(!nUnits)
            return;
        _Assert(pFirst && pLast && 0 < nUnits);
        pLast->pNext = m_pUnit;
        m_pUnit = pFirst;
        m_cntUnits += decltype(m_cntUnits)(nUnits);
    #pragma warning(pop)
    }
}
}


/*----------------------------------------------------------------
/   메모리풀 - 기타
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    namespace{
        // ...
    }
}
}

/*----------------------------------------------------------------
/   메모리풀 - 일반
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    CMemoryPool::CMemoryPool(size_t _TypeSize, UINT32 _IndexThisPool)
        : m_UnitSize(_TypeSize) // (m_UnitSizeMax)
        , m_UnitSizeMin(GLOBAL::gFN_Calculate_UnitSizeMin(_TypeSize))
        , m_Index_ThisPool(_IndexThisPool)
        , m_bUse_Header_per_Unit(0 < GLOBAL::gFN_Precalculate_MemoryPoolExpansionSize__GetSmallSize(_IndexThisPool))
        , m_bUse_TLS_BIN(_IndexThisPool < TMemoryPool_TLS_CACHE::c_Num_ExclusiveUnitsSlot)
        , m_bShared_UnitsGroup_from_MultiProcessor(_TypeSize > 8192)
        , m_bUse_OnlyBasket_0(((1 < GLOBAL::g_nProcessor)? FALSE : TRUE))
        //, m_nMinUnitsShare_per_Processor_from_UnitsGroup(?)
        //, m_nMinUnits_UnitGroupStateChange_Empty_to_Other(?)
        , m_pBaskets(nullptr)
        // 경계
        , m_Lock_Pool(1)
        , m_pExpansion_from_Basket(nullptr)
        // 경계
        , m_Allocator(_TypeSize, _IndexThisPool, this, 0 < GLOBAL::gFN_Precalculate_MemoryPoolExpansionSize__GetSmallSize(_IndexThisPool))
        // 경계
        , m_ListUnitGroup_Rest(TMemoryUnitsGroup::EState::E_Rest)
        , m_ListUnitGroup_Full(TMemoryUnitsGroup::EState::E_Full)
        // 경계
        , m_stats_Units_Allocated(0)
        , m_stats_OrderCount_Reserve_nSize(0)
        , m_stats_OrderCount_Reserve_nUnits(0)
        , m_stats_OrderCount_Reserve_Max_nSize(0)
        , m_stats_OrderCount_Reserve_Max_nUnits(0)
        , m_stats_Order_Result_Total_nSize(0)
        , m_stats_Order_Result_Total_nUnits(0)
        // 경계
        , m_stats_Order_MaxRequest_nSize(0)
        , m_stats_Order_MaxRequest_nUnits(0)
        , m_stats_Counting_Free_BadPTR(0)
        , m_bWriteStats_to_LogFile(TRUE)
        // 경계
    #if _DEF_USING_DEBUG_MEMORY_LEAK
        , m_Debug_stats_UsingCounting(0)
    #endif
    {
        _Assert(1 <= GLOBAL::g_nProcessor);
        _Assert(sizeof(void*) <= m_UnitSize);

        const size_t c_MinSizeShare_per_Processor_from_UnitsGroup = 16*1024;
        const size_t c_MinSizeUnitGroupStateChange_Empty_to_Other = 128;
        typedef decltype(m_nMinUnitsShare_per_Processor_from_UnitsGroup) _T1;
        typedef decltype(m_nMinUnits_UnitGroupStateChange_Empty_to_Other) _T2;
        _MACRO_STATIC_ASSERT(c_MinSizeShare_per_Processor_from_UnitsGroup == (_T1)c_MinSizeShare_per_Processor_from_UnitsGroup);
        _MACRO_STATIC_ASSERT(c_MinSizeUnitGroupStateChange_Empty_to_Other == (_T2)c_MinSizeUnitGroupStateChange_Empty_to_Other);

        // 유닛그룹 공유시 프로세서별 최소 요구 몫
        //      크게 할수록 메모리는 더 차지하고 경쟁은 적어진다
        m_nMinUnitsShare_per_Processor_from_UnitsGroup = (_T1)(c_MinSizeShare_per_Processor_from_UnitsGroup / m_UnitSize);
        if(m_nMinUnitsShare_per_Processor_from_UnitsGroup == 0)
            m_nMinUnitsShare_per_Processor_from_UnitsGroup = 1;
        _Assert(0 < m_nMinUnitsShare_per_Processor_from_UnitsGroup);

        // 유닛그룹의 상태 Empty 에서 다른 상태로 변환하기 위한 보유 유닛수량 요구
        m_nMinUnits_UnitGroupStateChange_Empty_to_Other = (_T2)(c_MinSizeUnitGroupStateChange_Empty_to_Other / m_UnitSize);
        if(m_nMinUnits_UnitGroupStateChange_Empty_to_Other == 0)
            m_nMinUnits_UnitGroupStateChange_Empty_to_Other = 1;
        _Assert(0 < m_nMinUnits_UnitGroupStateChange_Empty_to_Other);




        m_pBaskets = GLOBAL::gpFN_Baskets_Alloc();
        for(size_t i=0; i<GLOBAL::g_nProcessor; i++)
        {
            _MACRO_CALL_CONSTRUCTOR(m_pBaskets+i, CMemoryBasket(i));
        }

#if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(_T(" - MemoryPool Constructor [%u]\n"), m_UnitSize);
#endif
    }
    CMemoryPool::~CMemoryPool()
    {
        if(m_Lock_Pool.Query_isLocked())
            MemoryPool_UTIL::sFN_Report_DamagedMemoryPool();

        {
            // 남은 유닛그룹은 메모리릭 상태
            // 프로세서 캐시에 연결된 유닛그룹 정리
            for(size_t i=0; i<GLOBAL::g_nProcessor; i++)
            {
                auto p = m_pBaskets[i].mFN_Get_UnitsGroup();
                if(!p)
                    continue;
                if(!p->m_Lock.Lock__NoInfinite(0xFFFF))
                    continue;
                m_pBaskets[i].mFN_Detach(p);
                p->m_Lock.UnLock();
            }
            // 컨테이너에 남은 유닛그룹 정리
            while(auto p = ListUnitGroup_Full.mFN_PopFront())
                p->m_Lock.UnLock();
            while(auto p = ListUnitGroup_Rest.mFN_PopFront())
                p->m_Lock.UnLock();
        }

        mFN_Debug_Report();

        for(size_t i=0; i<GLOBAL::g_nProcessor; i++)
            _MACRO_CALL_DESTRUCTOR(m_pBaskets + i);
        GLOBAL::gpFN_Baskets_Free(m_pBaskets);

        // m_ListUnitGroup_Full 모두 반납
        while(auto p = ListUnitGroup_Full.mFN_PopFront())
            p->m_Lock.UnLock();

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
                auto _result_insert = m_map_Lend_MemoryUnits.insert(std::make_pair(p, TTrace_SourceCode(_FileName, _Line)));
                if(!_result_insert.second)
                {
                    _DebugBreak("이미 할당된 주소");
                }
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
        const TDATA_BLOCK_HEADER* pH = mFN_GetHeader_Safe(pUnit);
        if(!pH)
        {
            mFN_ExceptionProcess_InvalidAddress_Deallocation(pAddress);
            return;
        }

        CMemoryPool::mFN_Return_Memory_Process(pUnit, pH);
    }
    void CMemoryPool::mFN_Return_MemoryQ(void* pAddress)
    {
        if(!pAddress)
            return;

        TMemoryObject* pUnit = static_cast<TMemoryObject*>(pAddress);
        #ifdef _DEBUG
        const TDATA_BLOCK_HEADER* pH = mFN_GetHeader_Safe(pUnit);
        if(!pH)
        {
            mFN_ExceptionProcess_InvalidAddress_Deallocation(pAddress);
            return;
        }
        #else
        const TDATA_BLOCK_HEADER* pH = mFN_GetHeader(pUnit);
        #endif

        CMemoryPool::mFN_Return_Memory_Process(pUnit, pH);
    }
    //----------------------------------------------------------------
    size_t CMemoryPool::mFN_Query_This_UnitSize() const
    {
        return m_UnitSize;
    }
    BOOL CMemoryPool::mFN_Query_This_UnitsSizeLimit(size_t* pOUT_Min, size_t* pOUT_Max) const
    {
        if(!pOUT_Min || !pOUT_Max)
            return FALSE;
        *pOUT_Min = m_UnitSizeMin;
        *pOUT_Max = m_UnitSizeMax;

        return TRUE;
    }
    BOOL CMemoryPool::mFN_Query_isSlowMemoryPool() const
    {
        return FALSE;
    }
    BOOL CMemoryPool::mFN_Query_MemoryUnit_isAligned(size_t AlignSize) const
    {
        if(!AlignSize)
            return FALSE;
        const size_t c_minimalVirtualAllocSize = ::CORE::g_instance_CORE.mFN_Get_System_Information()->mFN_Get_SystemInfo()->dwAllocationGranularity;
        const size_t c_Aligned_UserSpace = c_minimalVirtualAllocSize + GLOBAL::SETTING::gc_SizeMemoryPoolAllocation_DefaultCost;
        size_t Aligned_First;
        size_t UnitRange;
        if(!m_bUse_Header_per_Unit)
        {
            Aligned_First = c_Aligned_UserSpace;
            UnitRange = m_UnitSize;
        }
        else
        {
            Aligned_First = c_Aligned_UserSpace + sizeof(TDATA_BLOCK_HEADER);
            UnitRange = m_UnitSize + sizeof(TDATA_BLOCK_HEADER);
        }
        const BOOL b1 = (0 == Aligned_First % AlignSize);
        const BOOL b2 = (0 == UnitRange % AlignSize);

        return (b1 && b2);
    }
    BOOL CMemoryPool::mFN_Query_Stats(TMemoryPool_Stats* pOUT)
    {
        if(!pOUT) return FALSE;
        ::memset(pOUT, 0, sizeof(TMemoryPool_Stats));
        auto& r = *pOUT;

        m_Lock_Pool.Begin_Write__INFINITE();
        {
            r.Base.nUnits_Created   = m_stats_Units_Allocated;
            r.Base.nUnits_Using     = m_stats_Units_Allocated - mFN_Counting_KeepingUnits();
            r.Base.nCount_Expansion     = m_Allocator.m_stats_cnt_Succeed_VirtualAlloc;
            r.Base.nCount_Reduction     = m_Allocator.m_stats_cnt_Succeed_VirtualFree;
            r.Base.nTotalSize_Expansion = m_Allocator.m_stats_size_TotalAllocated;
            r.Base.nTotalSize_Reduction = m_Allocator.m_stats_size_TotalDeallocate;
            r.Base.nCurrentSize_Pool    = m_Allocator.m_stats_Current_AllocatedSize;
            r.Base.nMaxSize_Pool        = m_Allocator.m_stats_Max_AllocatedSize;

            r.UserOrder.OrderCount_Reserve_nSize        = m_stats_OrderCount_Reserve_nSize;
            r.UserOrder.OrderCount_Reserve_nUnits       = m_stats_OrderCount_Reserve_nUnits;
            r.UserOrder.OrderCount_Reserve_Max_nSize    = m_stats_OrderCount_Reserve_Max_nSize;
            r.UserOrder.OrderCount_Reserve_Max_nUnits   = m_stats_OrderCount_Reserve_Max_nUnits;
            r.UserOrder.MaxRequest_nSize    = m_stats_Order_MaxRequest_nSize;
            r.UserOrder.MaxRequest_nUnits   = m_stats_Order_MaxRequest_nUnits;
            r.UserOrder.Result_Total_nSize  = m_stats_Order_Result_Total_nSize;
            r.UserOrder.Result_Total_nUnits = m_stats_Order_Result_Total_nUnits;
        }
        m_Lock_Pool.End_Write();

        return TRUE;
    }
    //----------------------------------------------------------------
    size_t CMemoryPool::mFN_Reserve_Memory__nSize(size_t nByte, BOOL bWriteLog)
    {
        size_t ret;
        m_Lock_Pool.Begin_Write__INFINITE();
        {
            const size_t prev_nSize  = m_Allocator.m_stats_Current_AllocatedSize;
            const size_t prev_nUnits = m_stats_Units_Allocated;

            m_Allocator.mFN_MemoryPreAlloc_Add(nByte, bWriteLog);

            const size_t result_nSize  = m_Allocator.m_stats_Current_AllocatedSize - prev_nSize;
            const size_t result_nUnits = m_stats_Units_Allocated - prev_nUnits;
            m_stats_Order_Result_Total_nSize += result_nSize;
            m_stats_Order_Result_Total_nUnits += result_nUnits;

            m_stats_OrderCount_Reserve_nSize++;
            if(m_stats_Order_MaxRequest_nSize < nByte)
                m_stats_Order_MaxRequest_nSize = nByte;

            ret = result_nSize;
        }
        m_Lock_Pool.End_Write();

        return ret;
    }
    size_t CMemoryPool::mFN_Reserve_Memory__nUnits(size_t nUnits, BOOL bWriteLog)
    {
        size_t ret;
        m_Lock_Pool.Begin_Write__INFINITE();
        {
            const size_t prev_nSize  = m_Allocator.m_stats_Current_AllocatedSize;
            const size_t prev_nUnits = m_stats_Units_Allocated;

            const size_t nByte = m_Allocator.mFN_Calculate__Units_to_MemorySize(nUnits);
            m_Allocator.mFN_MemoryPreAlloc_Add(nByte, bWriteLog);

            const size_t result_nSize  = m_Allocator.m_stats_Current_AllocatedSize - prev_nSize;
            const size_t result_nUnits = m_stats_Units_Allocated - prev_nUnits;
            m_stats_Order_Result_Total_nSize += result_nSize;
            m_stats_Order_Result_Total_nUnits += result_nUnits;

            m_stats_OrderCount_Reserve_nUnits++;
            if(m_stats_Order_MaxRequest_nUnits < nUnits)
                m_stats_Order_MaxRequest_nUnits = nUnits;

            ret = result_nUnits;
        }
        m_Lock_Pool.End_Write();

        return ret;
    }
    size_t CMemoryPool::mFN_Reserve_Memory__Max_nSize(size_t nByte, BOOL bWriteLog)
    {
        size_t ret;
        m_Lock_Pool.Begin_Write__INFINITE();
        {
            if(0 < nByte)
            {
                const size_t prev_nSize  = m_Allocator.m_stats_Current_AllocatedSize;
                const size_t prev_nUnits = m_stats_Units_Allocated;

                m_Allocator.mFN_MemoryPreAlloc_Set(nByte, bWriteLog);

                const size_t result_nSize  = m_Allocator.m_stats_Current_AllocatedSize - prev_nSize;
                const size_t result_nUnits = m_stats_Units_Allocated - prev_nUnits;
                m_stats_Order_Result_Total_nSize += result_nSize;
                m_stats_Order_Result_Total_nUnits += result_nUnits;
            }
            m_stats_OrderCount_Reserve_Max_nSize++;
            if(m_stats_Order_MaxRequest_nSize < nByte)
                m_stats_Order_MaxRequest_nSize = nByte;

            ret = m_Allocator.m_stats_Current_AllocatedSize;
        }
        m_Lock_Pool.End_Write();

        return ret;
    }
    size_t CMemoryPool::mFN_Reserve_Memory__Max_nUnits(size_t nUnits, BOOL bWriteLog)
    {
        size_t ret;
        m_Lock_Pool.Begin_Write__INFINITE();
        {
            if(m_stats_Units_Allocated < nUnits)
            {
                const size_t prev_nSize  = m_Allocator.m_stats_Current_AllocatedSize;
                const size_t prev_nUnits = m_stats_Units_Allocated;

                const size_t nByte = m_Allocator.mFN_Calculate__Units_to_MemorySize(nUnits - m_stats_Units_Allocated);
                m_Allocator.mFN_MemoryPreAlloc_Add(nByte, bWriteLog);

                const size_t result_nSize  = m_Allocator.m_stats_Current_AllocatedSize - prev_nSize;
                const size_t result_nUnits = m_stats_Units_Allocated - prev_nUnits;
                m_stats_Order_Result_Total_nSize += result_nSize;
                m_stats_Order_Result_Total_nUnits += result_nUnits;
            }
            m_stats_OrderCount_Reserve_Max_nUnits++;
            if(m_stats_Order_MaxRequest_nUnits < nUnits)
                m_stats_Order_MaxRequest_nUnits = nUnits;

            ret = m_stats_Units_Allocated;
        }
        m_Lock_Pool.End_Write();

        return ret;
    }
    size_t CMemoryPool::mFN_Hint_MaxMemorySize_Add(size_t nByte, BOOL bWriteLog)
    {
        size_t ret;
        m_Lock_Pool.Begin_Write__INFINITE();
        {
            ret = m_Allocator.mFN_MemoryPredict_Add(nByte, bWriteLog);
        }
        m_Lock_Pool.End_Write();
        return ret;
    }
    size_t CMemoryPool::mFN_Hint_MaxMemorySize_Set(size_t nByte, BOOL bWriteLog)
    {
        size_t ret;
        m_Lock_Pool.Begin_Write__INFINITE();
        {
            ret = m_Allocator.mFN_MemoryPredict_Set(nByte, bWriteLog);
        }
        m_Lock_Pool.End_Write();
        return ret;
    }
    //----------------------------------------------------------------
    BOOL CMemoryPool::mFN_Set_OnOff_ProcessorCACHE(BOOL _On)
    {
        _On = !_On;

        m_bUse_OnlyBasket_0 = _On;
        std::_Atomic_thread_fence(std::memory_order::memory_order_release);
        if(_On)
        {
            // 0번 을 제외한 모든 프로세서 저장소의 연결된 유닛그룹 제거
            for(size_t i=1; i<GLOBAL::g_nProcessor; i++)
            {
                auto& mb = m_pBaskets[i];
                for(;;)
                {
                    auto pUG = mb.mFN_Get_UnitsGroup();
                    if(!pUG)
                        break;
                    const BOOL bLocked = pUG->m_Lock.Lock__NoInfinite(4);
                    const BOOL bSame = (pUG == mb.mFN_Get_UnitsGroup());
                    if(bLocked && bSame)
                    {
                        if(mb.mFN_Detach(pUG))
                            mFN_UnitsGroup_Relocation(pUG);
                        else
                            pUG->m_Lock.UnLock();
                    }
                }
            }
        }
        return TRUE;
    }
    BOOL CMemoryPool::mFN_Query_OnOff_ProcessorCACHE() const
    {
        return !m_bUse_OnlyBasket_0;
    }
    //----------------------------------------------------------------
    void CMemoryPool::mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On)
    {
        m_bWriteStats_to_LogFile = _On;
        std::_Atomic_thread_fence(std::memory_order::memory_order_release);
    }
    //----------------------------------------------------------------
    void CMemoryPool::mFN_GiveBack_Unit(TMemoryUnitsGroup* pChunk, TMemoryObject* pUnit)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(pChunk && pUnit);

        pChunk->m_Lock.Lock__Busy();

        pChunk->mFN_DebugTest__IsNotFull();
        pChunk->mFN_Push(pUnit);
        mFN_GiveBack_PostProcess(pChunk);
    #pragma warning(pop)
    }
    void CMemoryPool::mFN_GiveBack_Units(TMemoryUnitsGroup* pChunk, TMemoryObject* pUnitsF, TMemoryObject* pUnitsL, UINT32 cnt)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(pChunk && pUnitsF && pUnitsL && 0 < cnt);

        pChunk->m_Lock.Lock__Busy();

        pChunk->mFN_DebugTest__IsNotFull();

        pChunk->mFN_Push(pUnitsF, pUnitsL, cnt);
        mFN_GiveBack_PostProcess(pChunk);
    #pragma warning(pop)
    }
    void CMemoryPool::mFN_GiveBack_PostProcess(TMemoryUnitsGroup* pChunk)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(pChunk && pChunk->m_Lock.Query_isLocked_from_CurrentThread());
        // 이 프로세스에서 가능한 관리상태 시나리오
        // 프로세서에 연결된 상태
        //      Busy -> Full
        // UnitsGropList에서 관리중인 상태
        //      Empty -> Rest / Full
        //      Rest -> Full

        BOOL bRequestVirtualFree = FALSE;
        if(!pChunk->mFN_Query_isAttached_to_Processor())
        {
            // 대부분은, 유닛그룹 리스트에서 관리중인 상태다(대부분의 발생빈도)
            if(pChunk->m_State == TMemoryUnitsGroup::EState::E_Rest)
            {
                _Assert(1 <= pChunk->m_nUnits);

                if(pChunk->mFN_Query_isFull())
                {
                    ListUnitGroup_Rest.mFN_Pop(pChunk);
                    bRequestVirtualFree = TRUE;
                }
            }
            else if(pChunk->m_State == TMemoryUnitsGroup::EState::E_Empty)
            {
                if(pChunk->mFN_Query_isFull())
                {
                    pChunk->m_State = TMemoryUnitsGroup::EState::E_Unknown;
                    bRequestVirtualFree = TRUE;
                }
                else if(pChunk->m_nUnits >= m_nMinUnits_UnitGroupStateChange_Empty_to_Other)
                {
                    pChunk->m_State = TMemoryUnitsGroup::EState::E_Unknown;
                    // 최소한의 수가 확보되어야 Empty -> Rest 로 변환한다
                    // 컨테이너에 삽입/삭제가 빈번하지 않도록(잠금 비용이 발생하기 때문에)
                    _Assert(!pChunk->mFN_Query_isEmpty());
                    ListUnitGroup_Rest.mFN_Push(pChunk);
                    _Assert(!pChunk->m_Lock.Query_isLocked_from_CurrentThread());
                    return;
                }
            }
            else
            {
                _Assert(pChunk->m_State != TMemoryUnitsGroup::EState::E_Busy);
                _Assert(pChunk->m_State != TMemoryUnitsGroup::EState::E_Full);
                _Error("메모리풀의 관리객체가 손상되거나\n이미 반납한 메모리를 다시 반납하였습니다\n메모리 침범을 확인하십시오");
            }
        }
        else
        {
            _Assert(pChunk->m_State == TMemoryUnitsGroup::EState::E_Busy);

            // n개의 프로세서에 연결된상태(유닛그룹 리스트에서 관리하지 않는)
            if(pChunk->mFN_Query_isFull())
            {
                mFN_UnitsGroup_Detach_from_All_Processor(pChunk);
                bRequestVirtualFree = TRUE;
            }
        }

        if(bRequestVirtualFree)
        {
            // 이 유닛그룹은 연결된 곳이 없는 상태
            // 메모리풀에서 연결해제를 시도
            if(!mFN_Request_Disconnect_UnitsGroup(pChunk))
            {
                _Assert(!pChunk->m_Lock.Query_isLocked_from_CurrentThread());
            }
            return;
        }
        //----------------------------------------------------------------
        _Assert(pChunk->m_Lock.Query_isLocked_from_CurrentThread());
        pChunk->m_Lock.UnLock();
    #pragma warning(pop)
    }

    namespace{
        DWORD g_TLS_index = TLS_OUT_OF_INDEXES;
    }
    BOOL CMemoryPool::sFN_TLS_InitializeIndex()
    {
        if(g_TLS_index == TLS_OUT_OF_INDEXES)
        {
            g_TLS_index = ::TlsAlloc();
            if(g_TLS_index != TLS_OUT_OF_INDEXES)
                return TRUE;
        }
        sFN_ExceptionProcess_from_sFN_TLS_InitializeIndex();
        return FALSE;
    }
    BOOL CMemoryPool::sFN_TLS_ShutdownIndex()
    {
        UTIL::MEM::CMemoryPool::sFN_TLS_Destroy();
        const BOOL ret = ::TlsFree(g_TLS_index);
        g_TLS_index = TLS_OUT_OF_INDEXES;
        if(ret)
            return TRUE;

        sFN_ExceptionProcess_from_sFN_TLS_ShutdownIndex();
        return FALSE;
    }
    __forceinline void* CMemoryPool::sFN_TLS_Get()
    {
        TMemoryPool_TLS_CACHE* p = (TMemoryPool_TLS_CACHE*)::TlsGetValue(g_TLS_index);
        if(p)
            return p;

        return sFN_TLS_Create();
    }
    DECLSPEC_NOINLINE void* CMemoryPool::sFN_TLS_Create()
    {
        TMemoryPool_TLS_CACHE* p = CObjectPool_Handle<TMemoryPool_TLS_CACHE>::Alloc_and_CallConstructor();
        ::TlsSetValue(g_TLS_index, p);
        _Assert(nullptr != p);
        return p;
    }
    void CMemoryPool::sFN_TLS_Destroy()
    {
        TMemoryPool_TLS_CACHE* p = (TMemoryPool_TLS_CACHE*)::TlsGetValue(g_TLS_index);
        if(!p)
            return;

        CObjectPool_Handle<TMemoryPool_TLS_CACHE>::Free_and_CallDestructor(p);
        ::TlsSetValue(g_TLS_index, nullptr);
    }
    namespace{
    #if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT
    #if __X64
        _DEF_CACHE_ALIGN size_t g_ArrayProcessorCode[64] ={0};
    #elif __X86
        _DEF_CACHE_ALIGN size_t g_ArrayProcessorCode[32] ={0};
    #else
        _DEF_CACHE_ALIGN size_t g_ArrayProcessorCode[?] ={0};
    #endif
    #endif
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
        DECLSPEC_NOINLINE void gFN_Build_ProcessorCodeTalbe()
        {
            // SIDT 를 이용해, 프로세서별 코드조사, 캐시로써 작성

            _MACRO_STATIC_ASSERT(GLOBAL::SETTING::gc_Max_ThreadAccessKeys <= 64); // SetThreadAffinityMask 함수 관련

            auto hThisThread  = GetCurrentThread();
            auto Mask_Before  = ::SetThreadAffinityMask(hThisThread, 1);
            _AssertRelease(0 != Mask_Before);

            for(DWORD_PTR i=0; i<GLOBAL::g_nProcessor; i++)
            {
                ::SetThreadAffinityMask(hThisThread, (DWORD_PTR)1 << i);
                g_ArrayProcessorCode[i] = gFN_Get_SIDT();
            }
            ::SetThreadAffinityMask(hThisThread, Mask_Before);
        }
        DECLSPEC_NOINLINE size_t gFN_Get_Basket__CacheMiss(TMemoryPool_TLS_CACHE& tls, size_t code)
        {
            TMemoryPool_TLS_CACHE& ref_This_Thread__CACHE = tls;

            // 익명 네임스페이스는 디버깅중 값을 확인하는 과정이 힘들기 때문에...
            const auto& refArrayProcessorCode = g_ArrayProcessorCode;

            size_t Key = 0;
            do{
                if(refArrayProcessorCode[Key] == code)
                    break;
            } while(++Key < GLOBAL::g_nProcessor);
            ref_This_Thread__CACHE.m_Code = code;
            ref_This_Thread__CACHE.m_ID = Key;

            return Key;
        }
    }
    CMemoryBasket& CMemoryPool::mFN_Get_Basket(TMemoryPool_TLS_CACHE* pTLS)
    {
        _CompileHint(pTLS);
        TMemoryPool_TLS_CACHE& ref_This_Thread__CACHE = *pTLS;

        const auto code = gFN_Get_SIDT(); // 미리 얻는다
        if(m_bUse_OnlyBasket_0 & ref_This_Thread__CACHE.m_bUsedtAffinity) // ams test 로 한번에 비교된다
        {
            if(m_bUse_OnlyBasket_0)
                return m_pBaskets[0];
            if(ref_This_Thread__CACHE.m_bUsedtAffinity)
                return m_pBaskets[ref_This_Thread__CACHE.m_ID];
        }

        size_t Key;
        if(ref_This_Thread__CACHE.m_Code == code)
        {
            // 연속적으로 동작하는 스레드의 현재 프로세서가 바뀌는 빈도는 높지 않다
            Key = ref_This_Thread__CACHE.m_ID;
        }
        else
        {
            // 캐시 미스
            // 이 스레드가 작동하는 프로세서가 바뀜
            Key = gFN_Get_Basket__CacheMiss(ref_This_Thread__CACHE, code);

    #if _DEF_USING_MEMORYPOOL_DEBUG
            // 디버깅용 통계 수집
            {
                m_pBaskets[Key].m_stats_cntCacheMiss_from_Get_Basket++;
            }
    #endif
        }

        #if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
        // 현재 스레드가 이 프로세서에서 다시 실행될 확률은 매우 높다
        // 데이터를 바로 읽을수 있도록...
        _mm_prefetch((const char*)&ref_This_Thread__CACHE, _MM_HINT_NTA);
        #endif

        _Assert(Key < GLOBAL::g_nProcessor);
        return m_pBaskets[Key];
    }
    // MB가 지정된 경우, MB에 유닛그룹을 연결한다.
    DECLSPEC_NOINLINE BOOL CMemoryPool::mFN_ExpansionMemory(CMemoryBasket& MB)
    {
        //----------------------------------------------------------------
        // 설정
        const BOOL c_bCheck_All_StorageBin = FALSE;
        //----------------------------------------------------------------
        const auto c_nUnits_Allocated_Previous = m_stats_Units_Allocated;

        m_Lock_Pool.Begin_Write__INFINITE();
        ::UTIL::LOCK::CAutoLockW<decltype(&m_Lock_Pool)> _AutoLock(&m_Lock_Pool, TRUE);

        const BOOL c_Detected_Expansion = (c_nUnits_Allocated_Previous < m_stats_Units_Allocated);
        //----------------------------------------------------------------
        // 잠금 획득후, 존재하는 자원 획득 시도
        // ※ 다중 스레드에서만 기능 유효
        // ※ 적용시 잠금시간이 길어져 다중스레드 속도 감소
        // 성공시 풀을 확장하지 않고, 리턴
        while(c_bCheck_All_StorageBin || c_Detected_Expansion)
        {
            auto pChunk = MB.mFN_Get_UnitsGroup();
            if(pChunk /*&& !pChunk->mFN_Query_isEmpty()*/) // 다른 스레드가 프로세서 저장소에 유닛그룹을 연결해 놓았다
                return TRUE;

            if(!ListUnitGroup_Rest.mFN_Query_IsEmpty())
                pChunk = ListUnitGroup_Rest.mFN_PopFront();

            std::_Atomic_thread_fence(std::memory_order::memory_order_consume);
            // 다른 스레드에서 바로 전에 확장을 요청했다면 변동사항이 있을 수 있다
            if(!pChunk && !ListUnitGroup_Full.mFN_Query_IsEmpty())
                pChunk = ListUnitGroup_Full.mFN_PopFront();

            if(!pChunk) // 존재하는 자원이 없다면 더는 시도하지 않는다
                break;

            if(pChunk->mFN_Query_isEmpty())
            {
                mFN_UnitsGroup_Relocation(pChunk);
                continue;
            }
            if(MB.mFN_Attach(pChunk))
            {
                pChunk->m_Lock.UnLock();
                return TRUE;
            }
            else
            {
                // 다른 스레드가 끼어들어 다른 유닛그룹을 연결해 놓았다
                mFN_UnitsGroup_Relocation(pChunk);
                continue;
            }
        }
        //----------------------------------------------------------------
        // 확장 요청
        m_pExpansion_from_Basket = &MB;
        auto pVMEM = m_Allocator.mFN_Expansion();
        if(!pVMEM)
            return FALSE;

        mFN_Request_Connect_UnitsGroup_Step1(pVMEM->m_pUnitsGroups, pVMEM->m_nAllocatedUnits);
        _AutoLock.End_Write();
        mFN_Request_Connect_UnitsGroup_Step2__LockFree(pVMEM->m_pUnitsGroups+1, pVMEM->m_nUnitsGroup-1);
        return TRUE;
    }
    TMemoryObject* CMemoryPool::mFN_Get_Memory_Process()
    {
        TMemoryObject* p = mFN_Get_Memory_Process__Internal();

    #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
        if(p)
        {
            MemoryPool_UTIL::sFN_Debug_Overflow_Check(p+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
            MemoryPool_UTIL::sFN_Debug_Overflow_Set(p, m_UnitSize, 0xCD);
        }
    #endif

        return p;
    }

    _DEF_INLINE_TYPE__PROFILING__FORCEINLINE TMemoryObject* CMemoryPool::mFN_Get_Memory_Process__Internal()
    {
        if(m_bUse_TLS_BIN)
            return mFN_Get_Memory_Process__Internal__TLS();
        else
            return mFN_Get_Memory_Process__Internal__Default();
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Get_Memory_Process__Internal__Default()
    {
        TMemoryObject* pRet;

        TMemoryPool_TLS_CACHE* pTLS = (TMemoryPool_TLS_CACHE*)sFN_TLS_Get();
        CMemoryBasket& mb = mFN_Get_Basket(pTLS);
        for(;;)
        {
            // #1 현재 프로세서 저장소에 연결된 유닛그룹에서 유닛을 얻음
            auto pChunk = mFN_Get_Avaliable_UnitsGroup_from_ProcessorStorageBin(mb);
            if(pChunk)
            {
                _CompileHint(!pChunk->mFN_Query_isEmpty());
                pRet = pChunk->mFN_Pop_Front();
                pChunk->m_Lock.UnLock();
                break;
            }

            // #2 다른 가능한 방법에서 유닛그룹을 얻음
            pChunk = mFN_Get_Avaliable_UnitsGroup(mb);
            if(pChunk)
            {
                _CompileHint(!pChunk->mFN_Query_isEmpty());
                // #3 유닛그룹에서 유닛을 얻고, 필요에 따라 프로세서 저장소에 연결
                pRet = mFN_AttachUnitGroup_and_GetUnit(mb, pChunk);
                if(pRet)
                    break;
            }
        }

        if(pRet)
            mFN_Debug_Counting__Get(&mb);
        return pRet;
    }
    DECLSPEC_NOINLINE TMemoryObject* CMemoryPool::mFN_Get_Memory_Process__Internal__TLS()
    {
        TMemoryObject* pRet;
        CMemoryBasket* pMB = nullptr;

        TMemoryPool_TLS_CACHE* pTLS = (TMemoryPool_TLS_CACHE*)sFN_TLS_Get();
        auto& refCACHE = pTLS->m_Array_ExclusiveUnits[m_Index_ThisPool];
        for(;;)
        {
            // #1 현재 TLS에서 유닛을 얻음
            pRet = refCACHE.Get_Unit();
            if(pRet)
                break;

            // #2 현재 TLS의 유닛(n)을 채움
            CMemoryBasket& mb = mFN_Get_Basket(pTLS);
            pMB = &mb;
            for(;;)
            {
                // #2-1 현재 프로세서 저장소로부터
                auto pChunk = mFN_Get_Avaliable_UnitsGroup_from_ProcessorStorageBin(mb);
                if(pChunk)
                {
                    _CompileHint(!pChunk->mFN_Query_isEmpty());
                    refCACHE.Fill(pChunk);
                    pChunk->m_Lock.UnLock();
                    break;
                }

                // #2-2 다른 가능한 방법에서
                pChunk = mFN_Get_Avaliable_UnitsGroup(mb);
                if(pChunk)
                {
                    _CompileHint(!pChunk->mFN_Query_isEmpty());
                    refCACHE.Fill(pChunk);
                    if(!pChunk->mFN_Query_isEmpty())
                    {
                        if(mb.mFN_Attach(pChunk))
                            pChunk->m_Lock.UnLock();
                        else
                            mFN_UnitsGroup_Relocation(pChunk);
                    }
                    else
                    {
                        mb.mFN_Detach(pChunk);
                        mFN_UnitsGroup_Relocation(pChunk);
                    }
                    break;
                }
            }
        }

        if(pRet)
            mFN_Debug_Counting__Get(pMB);
        return pRet;
    }
    // 프로세서 저장소로부터 이용이 가능한 유닛그룹을 잠금확보 상태로 리턴
    TMemoryUnitsGroup* CMemoryPool::mFN_Get_Avaliable_UnitsGroup_from_ProcessorStorageBin(CMemoryBasket& mb)
    {
        for(;;)
        {
            TMemoryUnitsGroup* pChunk;
            //----------------------------------------------------------------
            // 잠금 시도
            //----------------------------------------------------------------
            for(;;)
            {
                auto pChunk_Temp = mb.mFN_Get_UnitsGroup();
                if(!pChunk_Temp)
                    return nullptr;

                BOOL bSucceedLock = pChunk_Temp->m_Lock.Lock__NoInfinite(4);
                BOOL bSame = (pChunk_Temp == mb.mFN_Get_UnitsGroup());
                if(bSucceedLock)
                {
                    if(bSame)
                    {
                        _Assert(!pChunk_Temp->mFN_Query_isInvalidState());
                        // SUCCEED
                        pChunk = pChunk_Temp;
                        break;
                    }
                    else
                    {
                        pChunk_Temp->m_Lock.UnLock();
                        continue;
                    }
                }
                else
                {
                    continue;
                }
            }
            //----------------------------------------------------------------
            // 유효성 확인
            //----------------------------------------------------------------
            _Assert(pChunk && pChunk->m_Lock.Query_isLocked_from_CurrentThread());
            _Assert(pChunk->m_State == TMemoryUnitsGroup::EState::E_Busy || pChunk->m_State == TMemoryUnitsGroup::EState::E_Empty);
            if(!pChunk->mFN_Query_isEmpty())
                return pChunk; // SUCCEED


            if(mb.mFN_Detach(pChunk))
            {
                mFN_UnitsGroup_Relocation(pChunk);
                return nullptr;
            }
            else
            {
                // 다른 스레드가 MB의 유닛그룹을 변경시켰음
                // 로직을 한번더 테스트
                pChunk->m_Lock.UnLock();
                continue;
            }
        }
    }
    // 이용이 가능한 유닛그룹을 찾아 잠금확보 상태로 리턴
    TMemoryUnitsGroup* CMemoryPool::mFN_Get_Avaliable_UnitsGroup(CMemoryBasket& mb)
    {
        TMemoryUnitsGroup* pChunk;
        for(UINT32 cntChance = 256;;)
        {
            //----------------------------------------------------------------
            // 우선순위별 탐색
            //----------------------------------------------------------------
            pChunk = ListUnitGroup_Rest.mFN_PopFront();
            if(pChunk)
            {
                _Assert(!pChunk->mFN_Query_isAttached_to_Processor());
                _Assert(!pChunk->mFN_Query_isEmpty());

                if(pChunk->mFN_Query_isEmpty())
                {
                    mFN_UnitsGroup_Relocation(pChunk);
                    continue; // 우선순위순으로 탐색하기 위함
                }
                return pChunk;
            }
            pChunk = ListUnitGroup_Full.mFN_PopFront();
            if(pChunk)
            {
                _Assert(!pChunk->mFN_Query_isAttached_to_Processor());
                _Assert(!pChunk->mFN_Query_isEmpty());

                if(pChunk->mFN_Query_isEmpty())
                {
                    mFN_UnitsGroup_Relocation(pChunk);
                    continue; // 우선순위순으로 탐색하기 위함
                }
                return pChunk;
            }

            // ■ 다른 프로세서가 사용중인 것을 공유해서 사용
            if(m_bShared_UnitsGroup_from_MultiProcessor && m_stats_Units_Allocated && !m_bUse_OnlyBasket_0)
            {
                pChunk = mFN_Take_UnitsGroup_from_OtherProcessor();
                if(pChunk)
                {
                    _Assert(!pChunk->mFN_Query_isEmpty());

                    if(pChunk->mFN_Query_isEmpty())
                    {
                        mFN_UnitsGroup_Relocation(pChunk);
                        continue; // 우선순위순으로 탐색하기 위함
                    }
                    return pChunk;
                }
            }

            if(mFN_ExpansionMemory(mb))
            {
                for(;;)
                {
                    pChunk = mb.mFN_Get_UnitsGroup();
                    if(!pChunk)
                        break;

                    BOOL bSucceedLock = pChunk->m_Lock.Lock__NoInfinite(4);
                    BOOL bSame = (pChunk == mb.mFN_Get_UnitsGroup());
                    if(bSucceedLock)
                    {
                        if(bSame)
                        {
                            _Assert(!pChunk->mFN_Query_isInvalidState());
                            if(pChunk->mFN_Query_isEmpty())
                            {
                                mb.mFN_Detach(pChunk);
                                mFN_UnitsGroup_Relocation(pChunk);
                                break;
                            }

                            // SUCCEED
                            return pChunk;
                        }
                        else
                        {
                            pChunk->m_Lock.UnLock();
                            continue;
                        }
                    }
                    else
                    {
                        continue;
                    }
                }// for(;;)

                continue;
            }

            cntChance--;
            if(0 == cntChance)
            {
                _Error_OutOfMemory();
                return nullptr;
            }
        }// ~ for(...)
    }
    namespace{
        BOOL gFN_Query_isDamagedPTR(const CMemoryPool* pThisPool, const TMemoryObject* pAddress, const TDATA_BLOCK_HEADER* pHTrust, size_t UnitSize, BOOL bUse_Header_per_Unit)
        {
            if(!pThisPool || !pAddress || !pHTrust)
                return TRUE;

            // 지역헤더 , 공유헤더 손상을 확인한다

            // 체크섬 확인도 추가할 것인가?
            if(!bUse_Header_per_Unit)
            {
                const TDATA_BLOCK_HEADER* pH = _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)pAddress, GLOBAL::gc_minAllocationSize);
                if(*pH != *pHTrust)
                    return TRUE;
                if(pH->m_pFirstHeader != pHTrust)
                    return TRUE;

                if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_H1)
                    return TRUE;
                if(pH->m_pPool != pThisPool)
                    return TRUE;
                if(!pH->mFN_Query_ContainPTR(pAddress))
                    return TRUE;
                if(!pH->mFN_Query_AlignedPTR(pAddress))
                    return TRUE;
                if(pH->ParamNormal.m_RealUnitSize != UnitSize)
                    return TRUE;
                if(!pH->ParamNormal.m_nUnitsGroups || !pH->ParamNormal.m_pUnitsGroups || !pH->ParamNormal.m_Size_per_Group)
                    return TRUE;

                return FALSE;
            }
            else
            {
                const TDATA_BLOCK_HEADER* pH = reinterpret_cast<const TDATA_BLOCK_HEADER*>(pAddress) - 1;
                if(*pH != *pHTrust)
                    return TRUE;
                if(pH->m_pFirstHeader != pHTrust)
                    return TRUE;

                if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_HN)
                    return TRUE;
                if(pH->m_pPool != pThisPool)
                    return TRUE;
                if(!pH->mFN_Query_ContainPTR(pAddress))
                    return TRUE;
                if(!pH->mFN_Query_AlignedPTR(pAddress))
                    return TRUE;
                if(pH->ParamNormal.m_RealUnitSize != UnitSize + sizeof(TDATA_BLOCK_HEADER))
                    return TRUE;
                if(!pH->ParamNormal.m_nUnitsGroups || !pH->ParamNormal.m_pUnitsGroups || !pH->ParamNormal.m_Size_per_Group)
                    return TRUE;

                return FALSE;
            }
        }
    }
    void CMemoryPool::mFN_Return_Memory_Process(TMemoryObject* pAddress, const TDATA_BLOCK_HEADER* pH)
    {
        _Assert(nullptr != pAddress);

    #if _DEF_USING_MEMORYPOOL_DEBUG
        // 헤더 손상 확인
        _AssertMsg(FALSE == gFN_Query_isDamagedPTR(this, pAddress, pH, m_UnitSize, m_bUse_Header_per_Unit), "Broken Header Data");
    #endif

    #if _DEF_USING_DEBUG_MEMORY_LEAK
        if(0 == InterlockedExchangeSubtract(&m_Debug_stats_UsingCounting, 1))
        {
            MemoryPool_UTIL::sFN_Report_DamagedMemoryPool();
        }
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
        MemoryPool_UTIL::sFN_Debug_Overflow_Set(pAddress+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
    #endif

        mFN_Return_Memory_Process__Internal(pAddress, pH);
    }
    __forceinline void CMemoryPool::mFN_Return_Memory_Process__Internal(TMemoryObject* pAddress, const TDATA_BLOCK_HEADER* pH)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(pAddress);
        _Assert(pH && pH->ParamNormal.m_pUnitsGroups);

        const BOOL bAlignedPTR = pH->mFN_Query_AlignedPTR(pAddress);
        if(!bAlignedPTR)
        {
            // 정렬단위에서 어긋난 주소는 받납받지 않는다(차라리 메모리 릭을 유도한다)
            mFN_ExceptionProcess_InvalidAddress_Deallocation(pAddress);
            return;
        }

        mFN_Debug_Counting__Ret(nullptr);

        if(m_Index_ThisPool < TMemoryPool_TLS_CACHE::c_Num_ExclusiveUnitsSlot)
            mFN_Return_Memory_Process__Internal__TLS(pAddress);
        else
            mFN_Return_Memory_Process__Internal__Default(pAddress, pH);
    #pragma warning(pop)
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_Return_Memory_Process__Internal__Default(TMemoryObject* pAddress, const TDATA_BLOCK_HEADER* pH)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(pAddress && pH);
        const auto c_pChunk = pH->mFN_Get_UnitsGroup(pH->mFN_Calculate_UnitsGroupIndex(pAddress));
        auto pChunk = const_cast<TMemoryUnitsGroup*>(c_pChunk);
        mFN_GiveBack_Unit(pChunk, pAddress);
    #pragma warning(pop)
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_Return_Memory_Process__Internal__TLS(TMemoryObject* pAddress)
    {
        _Assert(pAddress);

        TMemoryPool_TLS_CACHE* pTLS = (TMemoryPool_TLS_CACHE*)sFN_TLS_Get();
        auto& refCACHE = pTLS->m_Array_ExclusiveUnits[m_Index_ThisPool];

        auto cntGiveBack_Require = refCACHE.Ret_Unit(pAddress);
        if(0 == cntGiveBack_Require)
            return;
        refCACHE.UnFill(this, cntGiveBack_Require);
    }

    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CMemoryPool::mFN_Request_Connect_UnitsGroup_Step1(TMemoryUnitsGroup* pFirst__Attach_to_Basket, size_t nTotalAllocatedUnits)
    {
        _Assert(m_Lock_Pool.Query_isLockedWrite());
        _Assert(0 < nTotalAllocatedUnits);

        m_stats_Units_Allocated += nTotalAllocatedUnits;
        if(!pFirst__Attach_to_Basket)
            return;
        if(m_pExpansion_from_Basket)
        {
            // 현재의 확장을 요청한 CPU CORE CACHE에 우선권
            BOOL bAttached = m_pExpansion_from_Basket->mFN_Attach(pFirst__Attach_to_Basket);
            m_pExpansion_from_Basket = nullptr;
            if(bAttached)
            {
                pFirst__Attach_to_Basket->m_Lock.UnLock();
                return;
            }
        }

        ListUnitGroup_Full.mFN_Push(pFirst__Attach_to_Basket);
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CMemoryPool::mFN_Request_Connect_UnitsGroup_Step2__LockFree(TMemoryUnitsGroup* pOther, UINT32 nGroups)
    {
        if(!nGroups)
            return;

        ListUnitGroup_Full.mFN_Push(pOther, nGroups);
    }

    DECLSPEC_NOINLINE BOOL CMemoryPool::mFN_Request_Disconnect_UnitsGroup(TMemoryUnitsGroup * pChunk)
    {
        if(!mFN_Request_Disconnect_UnitsGroup_Step1(pChunk))
            return FALSE;

        return mFN_Request_Disconnect_UnitsGroup_Step2(pChunk);
    }
    _DEF_INLINE_TYPE__PROFILING__FORCEINLINE BOOL CMemoryPool::mFN_Request_Disconnect_UnitsGroup_Step1(TMemoryUnitsGroup* pChunk)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _CompileHint(nullptr != pChunk);
        _CompileHint(pChunk->m_Lock.Query_isLocked());
        _CompileHint(pChunk->m_State == TMemoryUnitsGroup::EState::E_Unknown);
        _CompileHint(pChunk->mFN_Query_isFull() && !pChunk->mFN_Query_isManaged_from_Container() && !pChunk->mFN_Query_isAttached_to_Processor());
        
        const TDATA_VMEM* pVMEM = pChunk->m_pOwner;
        _Assert(pVMEM->m_cntFullGroup <= pVMEM->m_nUnitsGroup);
        if(pVMEM->m_cntFullGroup == pVMEM->m_nUnitsGroup)
            return TRUE;

        ListUnitGroup_Full.mFN_Push(pChunk);
        return FALSE;
    #pragma warning(pop)
    }
    DECLSPEC_NOINLINE BOOL CMemoryPool::mFN_Request_Disconnect_UnitsGroup_Step2(TMemoryUnitsGroup* pChunk)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        // Step1 에서 VMEM 소속 유닛그룹이 모두 가득찬 상태
        _CompileHint(nullptr != pChunk);
        _CompileHint(pChunk->m_Lock.Query_isLocked());
        _CompileHint(pChunk->m_State == TMemoryUnitsGroup::EState::E_Unknown);
        _CompileHint(pChunk->mFN_Query_isFull() && !pChunk->mFN_Query_isManaged_from_Container() && !pChunk->mFN_Query_isAttached_to_Processor());

        // 다음의 경우 실패할 수 있다
        // VMEM 소속 2개의 유닛그룹이 있다고 할때
        // 2개의 스레드에 의해 각각 0, 1 번 유닛그룹을 확보하고 나머지 유닛그룹(잠긴)을 확보하려 할때
        // 데드락과 원리와 같다

        TDATA_VMEM* pVMEM = pChunk->m_pOwner;
        _CompileHint(nullptr != pVMEM->m_pUnitsGroups);

        const auto nGroups = pVMEM->m_nUnitsGroup;
        TMemoryUnitsGroup* const pGroupFirst = pVMEM->m_pUnitsGroups;
        const TMemoryUnitsGroup* const pGroupEnd = pGroupFirst + nGroups;

        TMemoryUnitsGroup* pFailedGroup = nullptr;

        UINT32 cntFound_FullContainer = 0;
        UINT32 cntFound_RestContainer = 0;

        // 잠금 확보
        for(auto p = pGroupFirst; p != pGroupEnd; p++)
        {
            if(p == pChunk)
                continue;

            // 적용하지 않는 편이 빠르다
        //#if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
        //    // 다음 유닛그룹을 CPU 캐시에 미리 로드
        //    const TMemoryUnitsGroup* const pNext = p + 1;
        //    if(pNext != pGroupEnd)
        //        _mm_prefetch((const CHAR*)pNext, _MM_HINT_T0);
        //#endif

            if(!p->m_Lock.Lock__NoInfinite__Lazy(128))
            {
                pFailedGroup = p;
                break;
            }

            if(!p->mFN_Query_isFull())
            {
                // 가득차지 않은 유닛그룹 발견
                p->m_Lock.UnLock();
                pFailedGroup = p;
                break;
            }
        }
        // 잠금 확보 실패시, 잠금복구
        if(pFailedGroup)
        {
            for(auto p = pGroupFirst; p != pFailedGroup; p++)
            {
                _Assert(p->m_Lock.Query_isLocked_from_CurrentThread());
                if(p == pChunk)
                    continue;
                p->m_Lock.UnLock();
            }
            ListUnitGroup_Full.mFN_Push(pChunk);
            return FALSE;
        }
        // 상태 확인
        for(auto p = pGroupFirst; p != pGroupEnd; p++)
        {
            _Assert(p->mFN_Query_isFull());
            if(p->mFN_Query_isAttached_to_Processor())
            {
                _Assert(!p->mFN_Query_isAttached_to_Processor());
                _Assert(p->m_State == TMemoryUnitsGroup::EState::E_Busy);
                mFN_UnitsGroup_Detach_from_All_Processor(p);
                _Assert(p->m_State == TMemoryUnitsGroup::EState::E_Unknown);
            }
            else if(p->mFN_Query_isManaged_from_Container())
            {
                switch(p->m_State)
                {
                case TMemoryUnitsGroup::EState::E_Full:
                    cntFound_FullContainer++;
                    break;
                case TMemoryUnitsGroup::EState::E_Rest:
                    cntFound_RestContainer++;
                    break;
                default:
                    _Error(_T("추가된 컨테이너가 있는지 확인해야함"));
                }
            }
            else if(p->m_State == TMemoryUnitsGroup::EState::E_Unknown)
            {
                // E_Unknown 상태
                _AssertMsg(p == pChunk,  _T("Invalid TMemoryUnitsGroup State 0x%p"), p);
                // 무효한 상태이지만 진행에는 문제가 없다
            }
            else
            {
                // 실패
                // 1. 연결된 프로세서 없음
                // 2. 연결된 컨테이너 없음
                // 위 1, 2상황이 조합되는 경우 E_Unknown 만이 가능하다
                _Error("VirtualFree 과정중 무효한 상태 : UnitsGroup 0x%p State: %u", p, p->m_State);
            }
        }

        BOOL bDamaged = FALSE;
        if(cntFound_FullContainer)
        {
            if(cntFound_FullContainer != ListUnitGroup_Full.mFN_Pop(pGroupFirst, nGroups))
                bDamaged = TRUE;
        }
        if(cntFound_RestContainer)
        {
            _DebugBreak(_T("Invalid TMemoryUnitsGroup State"));
            // 무효한 상태이지만 진행에는 문제가 없다

            if(cntFound_RestContainer != ListUnitGroup_Rest.mFN_Pop(pGroupFirst, nGroups))
                bDamaged = TRUE;
        }
        if(bDamaged)
        {
            // 소속되는 메모리는 안전한 반납을 위해 해제하지 않는다
            // 이것은 메모리릭으로 이어질 수 있다
            mFN_ExceptionProcess_from_mFN_Request_Disconnect_VMEM(pVMEM, 0);
            return FALSE;
        }
        //----------------------------------------------------------------
        // VMEM 소속의 모든 유닛그룹의 소유권을 확보한 상태
        // 이하 VMEM 해제 과정
        return mFN_Request_Disconnect_VMEM(pVMEM);
    #pragma warning(pop)
    }
    // 호출시 주의
    // 소속 유닛그룹은 모두 잠금을 확보한 상태이어야 하며, 연결된 프로세서나 유닛그룹리스트가 없어야 한다
    // 실패시 pRequestFrom 를 제외한 나머지 유닛그룹은 모두 다시 배치하며, 잠금해제 상태로 만든다
    DECLSPEC_NOINLINE BOOL CMemoryPool::mFN_Request_Disconnect_VMEM(TDATA_VMEM* pVMEM)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(pVMEM);
        const auto pUG = pVMEM->m_pUnitsGroups;
        const auto nUG = pVMEM->m_nUnitsGroup;
        const auto c_nUnits = pVMEM->m_nAllocatedUnits;

        const BOOL ret = m_Allocator.mFN_VMEM_Destroy_UnitsGroups_Step1__LockFree(*pVMEM);
        if(ret)
        {
            m_Lock_Pool.Begin_Write__INFINITE();
            {
                m_Allocator.mFN_VMEM_Destroy_UnitsGroups_Step2(pUG, nUG);
                m_Allocator.mFN_VMEM_Close(pVMEM);
                
                _Assert(m_stats_Units_Allocated >= c_nUnits);
                m_stats_Units_Allocated -= c_nUnits;
            }
            m_Lock_Pool.End_Write();
        }
        else
        {
            // 이것은 심각한 오류이지만 계속 진행 가능하다
            // 원상 복구한다
            mFN_ExceptionProcess_from_mFN_Request_Disconnect_VMEM(pVMEM, 0);
        }
        return ret;
    #pragma warning(pop)
    }

    __forceinline const TDATA_BLOCK_HEADER* CMemoryPool::mFN_GetHeader_Safe(TMemoryObject* pAddress) const
    {
        _CompileHint(this && nullptr != pAddress);
        if(!m_bUse_Header_per_Unit)
            return CMemoryPool_Manager::sFN_Get_DataBlockHeader_SmallObjectSize(pAddress);
        else
            return CMemoryPool_Manager::sFN_Get_DataBlockHeader_NormalObjectSize(pAddress);
    }
    __forceinline const TDATA_BLOCK_HEADER* CMemoryPool::mFN_GetHeader(TMemoryObject* pAddress) const
    {
        _CompileHint(this && nullptr != pAddress);

        if(!m_bUse_Header_per_Unit)
            return _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)pAddress, GLOBAL::gc_minAllocationSize);
        else
            return ((TDATA_BLOCK_HEADER*)(pAddress)) - 1;
    }

    TMemoryObject* CMemoryPool::mFN_AttachUnitGroup_and_GetUnit(CMemoryBasket& mb, TMemoryUnitsGroup* pChunk)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(pChunk);
        _Assert(pChunk->m_Lock.Query_isLocked_from_CurrentThread()); // 잠긴 상태
        TMemoryObject* pRet = pChunk->mFN_Pop_Front();

        if(!pChunk->mFN_Query_isEmpty()) // 추가 유닛이 있다면 연결
        {
            if(mb.mFN_Attach(pChunk))
                pChunk->m_Lock.UnLock();
            else
                mFN_UnitsGroup_Relocation(pChunk);
        }
        else
        {
            mb.mFN_Detach(pChunk);
            mFN_UnitsGroup_Relocation(pChunk);
        }

        _Assert(pRet);
        return pRet;
    #pragma warning(pop)
    }
    void CMemoryPool::mFN_UnitsGroup_Relocation(TMemoryUnitsGroup* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(nullptr != p && TRUE == p->m_Lock.Query_isLocked_from_CurrentThread());

        if(p->mFN_Query_isAttached_to_Processor())
        {
            _Assert(p->m_State == TMemoryUnitsGroup::EState::E_Busy);
            p->m_Lock.UnLock();
            return;
        }

        _Assert(p->m_State == TMemoryUnitsGroup::EState::E_Unknown);
        if(p->mFN_Query_isFull())
        {
            ListUnitGroup_Full.mFN_Push(p);
        }
        else if(!p->mFN_Query_isEmpty())
        {
            ListUnitGroup_Rest.mFN_Push(p);
        }
        else
        {
            _AssertMsg(p->mFN_Query_isEmpty(), "UnitsGroup Relocation");

            p->m_State = TMemoryUnitsGroup::EState::E_Empty;
            p->m_Lock.UnLock();
        }
    #pragma warning(pop)
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_UnitsGroup_Detach_from_All_Processor(TMemoryUnitsGroup* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(p && p->m_Lock.Query_isLocked());

        if(!p->mFN_Query_isAttached_to_Processor())
            return;

        _Assert(p->m_State == TMemoryUnitsGroup::EState::E_Busy);
        for(size_t i=0; i<GLOBAL::g_nProcessor; i++)
        {
            if(!(0x01 & (p->m_ReferenceMask >> i)))
                continue;

            _Assert(i < GLOBAL::g_nProcessor);
            auto& mb = m_pBaskets[i];
            mb.mFN_Detach(p);

        #if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
            // 다른 프로세서 에서 사용되는 MB를 캐시에서 제거
            // 작업중
            // 성능측정을 해볼것
            _mm_clflush(&mb);
        #endif

            if(!p->mFN_Query_isAttached_to_Processor())
                break;
        }

        // 도중에 다른 스레드가 이 유닛그룹에 Attach 하는것은 불가능하다
        _Assert(!p->mFN_Query_isAttached_to_Processor());
        p->m_State = TMemoryUnitsGroup::EState::E_Unknown;
    #pragma warning(pop)
    }

    void CMemoryPool::mFN_Debug_Report()
    {
        size_t cntUnits = mFN_Counting_KeepingUnits();
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
                _MACRO_OUTPUT_DEBUG_STRING_TRACE_ALWAYS("MemoryPool : Detected memory leaks\n");
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
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("MemoryPool[%Iu] n[%10Iu] / n[%10Iu]\n")
                _T("\tCurrent Pool Size[%IuKB(%.2fMB)]\n")
                _T("\tMax Pool Size[%IuKB(%.2fMB)]\n")
                _T("\tTotal Expansion Count[%10Iu] Size[%lluKB(%.2fMB)]\n")
                _T("\tTotal Reduction Count[%10Iu] Size[%lluKB(%.2fMB)]\n")
                
                , m_UnitSize
                , cntUnits // 메모리풀이 보유한(유닛 그룹들의 합을 계산할것)
                , m_stats_Units_Allocated

                , m_Allocator.m_stats_Current_AllocatedSize / 1024
                , m_Allocator.m_stats_Current_AllocatedSize / 1024. / 1024.
                , m_Allocator.m_stats_Max_AllocatedSize / 1024
                , m_Allocator.m_stats_Max_AllocatedSize / 1024. / 1024.

                , m_Allocator.m_stats_cnt_Succeed_VirtualAlloc
                , m_Allocator.m_stats_size_TotalAllocated / 1024
                , m_Allocator.m_stats_size_TotalAllocated / 1024. / 1024.
                , m_Allocator.m_stats_cnt_Succeed_VirtualFree
                , m_Allocator.m_stats_size_TotalDeallocate / 1024
                , m_Allocator.m_stats_size_TotalDeallocate / 1024. / 1024.
                );

            UINT64 total_Get = 0;
            UINT64 total_Ret = 0;
            UINT32 total_Share = 0;
            UINT32 total_CacheMiss = 0;

            for(size_t i=0; i<GLOBAL::g_nProcessor; i++)
            {
                _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tProcessor[%3Iu] = GetCMD[%20llu] RetCMD[%20llu] ShareCMD[%10u]CacheMiss[%u]\n"), i
                    , m_pBaskets[i].m_stats_cntGet
                    , m_pBaskets[i].m_stats_cntRet
                    , m_pBaskets[i].m_stats_cntRequestShareUnitsGroup
                    , m_pBaskets[i].m_stats_cntCacheMiss_from_Get_Basket
                    );

                total_Get += m_pBaskets[i].m_stats_cntGet;
                total_Ret += m_pBaskets[i].m_stats_cntRet;
                total_Share += m_pBaskets[i].m_stats_cntRequestShareUnitsGroup;
                total_CacheMiss += m_pBaskets[i].m_stats_cntCacheMiss_from_Get_Basket;
            }
            _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tTotal          = GetCMD[%20llu] RetCMD[%20llu] ShareCMD[%10u]CacheMiss[%u]\n")
                , total_Get
                , total_Ret
                , total_Share
                , total_CacheMiss);
            if(total_Get != total_Ret)
                _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(FALSE, _T("\tWarning : Total GetCMD != Total RetCMD\n"));
        }
    #endif

        if(m_bWriteStats_to_LogFile)
        {
            if(cntUnits == m_stats_Units_Allocated)
            {
                // 이상 없음
            }
            else if(cntUnits < m_stats_Units_Allocated)
            {
                const size_t leak_cnt   = m_stats_Units_Allocated - cntUnits;
                const size_t leak_size  = leak_cnt * m_UnitSize;
                _LOG_DEBUG(_T("[Warning] Pool Size[%llu] : Memory Leak(Count:%llu , TotalSize:%lluKB)")
                    , (UINT64)m_UnitSize
                    , (UINT64)leak_cnt
                    , (UINT64)leak_size/1024);
            }
            else if(cntUnits > m_stats_Units_Allocated)
            {
                _LOG_DEBUG(_T("[Critical Error] Pool Size[%llu] : Total Allocated < Total Free"), (UINT64)m_UnitSize);
            }
        }

        if(0 < m_stats_Counting_Free_BadPTR)
        {
            _LOG_SYSTEM__WITH__OUTPUTDEBUGSTR(FALSE, _T("MemoryPool[%Iu] Invalid Free Reques Count : %Iu"), m_UnitSize, m_stats_Counting_Free_BadPTR);
        }
    }
    size_t CMemoryPool::mFN_Counting_KeepingUnits()
    {
        size_t cntSum = 0;
        for(auto p=m_Allocator.m_pVMEM_First; p; p=p->m_pNext)
        {
            cntSum += p->mFN_Query_CountUnits__Keeping();
        }
        return cntSum;
    }
    // 다른 프로세서들의 Basket에 연결된 유닛그룹들중, 공유가능 한 것을 가져온다
    // 이때 유닛그룹은 잠금을 확보한다
    DECLSPEC_NOINLINE TMemoryUnitsGroup* CMemoryPool::mFN_Take_UnitsGroup_from_OtherProcessor()
    {
        TMemoryUnitsGroup* pSelectTarget = nullptr;
        CMemoryBasket* pFrom_MB = nullptr;

        CMemoryBasket* pEndMB = m_pBaskets + GLOBAL::g_nProcessor;
        for(auto pMB = m_pBaskets; pMB < pEndMB; pMB++)
        {
            // 느슨하게 확인한다
            auto pG = pMB->mFN_Get_UnitsGroup();
            if(!pG)
                continue;

            _Assert(0 < m_nMinUnitsShare_per_Processor_from_UnitsGroup);
            UINT32 nMaxShare_Processor = pG->m_nUnits / m_nMinUnitsShare_per_Processor_from_UnitsGroup;
            if(!nMaxShare_Processor)
                continue;
            if(nMaxShare_Processor <= pG->mFN_Query_CountAttached_to_Processor())
                continue;
            if(pG->m_nUnits < m_nMinUnitsShare_per_Processor_from_UnitsGroup)
                continue; // 기본공유 수량을 보유하지 못했다면 SKIP

            //----------------------------------------------------------------
            if(!pG->m_Lock.Lock__NoInfinite__Lazy(4))
                continue;
            if(pG->mFN_Query_isInvalidState())
            {
                pG->m_Lock.UnLock();
                continue;
            }

            CMemoryBasket* pDoubleCheckedMB = (pG == pMB->mFN_Get_UnitsGroup()? pMB : nullptr);
            if(pDoubleCheckedMB && pG->mFN_Query_isFull())
            {
                // 사용하지 않는 가득찬 유닛그룹이라면 상대의 연결을 끊는다
                if(pMB->mFN_Detach(pG))
                {
                    // 최적의 결과 발견
                    pSelectTarget = pG;
                    pFrom_MB = pDoubleCheckedMB;
                    break;
                }
            }

            // 다른 조건 테스트
            BOOL bFailedTest = FALSE;
            // 일반화 하여 테스트
            if(pG->m_nUnits < m_nMinUnitsShare_per_Processor_from_UnitsGroup)
                bFailedTest = TRUE; // 기본공유 수량을 보유하지 못했다면 SKIP
            else if(nMaxShare_Processor <= pG->mFN_Query_CountAttached_to_Processor())
                bFailedTest = TRUE; // 공유제한 초과
            else if(pG->mFN_Query_isEmpty())
                bFailedTest = TRUE; // 평균으로부터 유닛 수량 체크는 정확하지 않다

            if(!bFailedTest)
            {
                pSelectTarget = pG;
                pFrom_MB = pDoubleCheckedMB;
                break;
            }
            pG->m_Lock.UnLock();
            //----------------------------------------------------------------
        }


        if(!pSelectTarget)
            return nullptr;

        _Assert(pSelectTarget && !pSelectTarget->mFN_Query_isEmpty());
    #if _DEF_USING_MEMORYPOOL_DEBUG
        // 디버깅용 통계 수집
        if(pFrom_MB)
            pFrom_MB->m_stats_cntRequestShareUnitsGroup++;
    #endif

        return pSelectTarget;
    }
    void CMemoryPool::mFN_Debug_Counting__Get(CMemoryBasket* pMB)
    {
    #if _DEF_USING_MEMORYPOOL_DEBUG
        if(!pMB)
        {
            TMemoryPool_TLS_CACHE* pTLS = (TMemoryPool_TLS_CACHE*)sFN_TLS_Get();
            pMB = &mFN_Get_Basket(pTLS);
        }
        pMB->m_stats_cntGet++;
    #else
        UNREFERENCED_PARAMETER(pMB);
    #endif
    }
    void CMemoryPool::mFN_Debug_Counting__Ret(CMemoryBasket* pMB)
    {
    #if _DEF_USING_MEMORYPOOL_DEBUG
        if(!pMB)
        {
            TMemoryPool_TLS_CACHE* pTLS = (TMemoryPool_TLS_CACHE*)sFN_TLS_Get();
            pMB = &mFN_Get_Basket(pTLS);
        }
        pMB->m_stats_cntRet++;
    #else
        UNREFERENCED_PARAMETER(pMB);
    #endif
    }

    DECLSPEC_NOINLINE void CMemoryPool::mFN_ExceptionProcess_InvalidAddress_Deallocation(const void* p)
    {
        m_stats_Counting_Free_BadPTR++;
        MemoryPool_UTIL::sFN_Report_Invalid_Deallocation(p);
    }
    DECLSPEC_NOINLINE void CMemoryPool::mFN_ExceptionProcess_from_mFN_Request_Disconnect_VMEM(TDATA_VMEM* pVMEM, int iCase)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(m_Lock_Pool.Query_isLockedWrite());
        _Assert(pVMEM);
        if(iCase == 0)
        {
            // 메모리릭 체크
            BOOL bMemoryLeak = TRUE;
            for(UINT32 i=0; i<pVMEM->m_nUnitsGroup; i++)
            {
                if(pVMEM->m_pUnitsGroups[i].mFN_Query_isDamaged__CurrentStateNotManaged())
                {
                    bMemoryLeak = FALSE;
                    break;
                }
            }
            // 메모리릭의 경우 보고하지 않음
            if(!bMemoryLeak)
                MemoryPool_UTIL::sFN_Report_DamagedMemory(pVMEM, sizeof(*pVMEM));

            // 소속 유닛그룹은 모두 잠겨 있는 상태
            const TMemoryUnitsGroup* const pEndUG = pVMEM->m_pUnitsGroups + pVMEM->m_nUnitsGroup;
            for(auto pUG = pVMEM->m_pUnitsGroups; pUG != pEndUG; pUG++)
            {
                if(pUG->mFN_Query_isManaged_from_Container())
                {
                    pUG->m_Lock.UnLock();
                    continue;
                }

                mFN_UnitsGroup_Relocation(pUG);
            }
        }
        else
        {
            MemoryPool_UTIL::sFN_Report_DamagedMemory(pVMEM, sizeof(*pVMEM));
        }
    #pragma warning(pop)
    }
    DECLSPEC_NOINLINE void CMemoryPool::sFN_ExceptionProcess_from_sFN_TLS_InitializeIndex()
    {
        LPCTSTR szText = _T("Failed : TLS Initialize");
        _DebugBreak(szText);
        _LOG_SYSTEM__WITH__TRACE(TRUE, szText);
    }
    DECLSPEC_NOINLINE void CMemoryPool::sFN_ExceptionProcess_from_sFN_TLS_ShutdownIndex()
    {
        LPCTSTR szText = _T("Failed : TLS Shutdown");
        _DebugBreak(szText);
        _LOG_SYSTEM__WITH__TRACE(TRUE, szText);
    }
}
}


/*----------------------------------------------------------------
/   메모리풀 관리자
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    CMemoryPool_Manager::CMemoryPool_Manager()
        : m_MaxSize_of_MemoryPoolUnitSize(GLOBAL::SETTING::gc_maxSize_of_MemoryPoolUnitSize)
        , m_Lock__PoolManager(1)
        , m_bWriteStats_to_LogFile(TRUE)
        , m_stats_Counting_Free_BadPTR(0)
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();

        CObjectPool_Handle<CMemoryPool>::Reference_Attach();
        TMemoryUnitsGroup::sFN_ObjectPool_Initialize();

        _LOG_DEBUG__WITH__OUTPUTDEBUGSTR(TRUE, _T("--- Create CMemoryPool_Manager ---"));

        mFN_Test_Environment_Variable();
        mFN_Initialize_MemoryPool_Shared_Environment_Variable1();
        mFN_Initialize_MemoryPool_Shared_Environment_Variable2();
        mFN_Initialize_MemoryPool_Shared_Variable();

        ::UTIL::g_pMem = this;
        mFN_Report_Environment_Variable();
    }
    CMemoryPool_Manager::~CMemoryPool_Manager()
    {
        ::UTIL::g_pMem = nullptr;

        m_Lock__PoolManager.Begin_Write__INFINITE();
        for(auto& pPool : m_Array_Pool)
        {
            if(!pPool)
                continue;

            //_SAFE_DELETE_ALIGNED_CACHELINE(pPool);
            CObjectPool_Handle<CMemoryPool>::Free_and_CallDestructor(pPool);
            pPool = nullptr;
        }

        mFN_Destroy_MemoryPool_Shared_Variable();
        mFN_Destroy_MemoryPool_Shared_Environment_Variable();
        mFN_Debug_Report();

        CObjectPool_Handle<CMemoryPool>::Reference_Detach();
        TMemoryUnitsGroup::sFN_ObjectPool_Shotdown();
        _LOG_DEBUG__WITH__OUTPUTDEBUGSTR(TRUE, _T("--- Destroy CMemoryPool_Manager ---"));
    }


    DECLSPEC_NOINLINE void CMemoryPool_Manager::mFN_Test_Environment_Variable()
    {
        _MACRO_STATIC_ASSERT(_MACRO_ARRAY_COUNT(m_Array_Pool) == GLOBAL::SETTING::gc_Num_MemoryPool);

        // 비트가 1개만 on 이어야 하는 상수 체크
        //_AssertRelease(1 == _MACRO_BIT_COUNT(GLOBAL::SETTING::gc_Max_ThreadAccessKeys));
        //_AssertRelease(1 == _MACRO_BIT_COUNT(GLOBAL::gc_minAllocationSize));
        _MACRO_STATIC_ASSERT(1 == _MACRO_STATIC_BIT_COUNT(GLOBAL::SETTING::gc_Max_ThreadAccessKeys));
        _MACRO_STATIC_ASSERT(1 == _MACRO_STATIC_BIT_COUNT(GLOBAL::gc_minAllocationSize));

        // 시스템 메모리 예약/할당단위 체크   64KB , 4KB
        const UTIL::ISystem_Information* pSys = CORE::g_instance_CORE.mFN_Get_System_Information();
        const SYSTEM_INFO* pInfo = pSys->mFN_Get_SystemInfo();
        if(pInfo->dwAllocationGranularity != 64*1024 || pInfo->dwPageSize != 4*1024)
        {
            _MACRO_MESSAGEBOX__ERROR("from MemoryPool", "Supported System :\nPageSize %uKB\nAllocationGranularity %uKB\n\n\nThis System :\nPageSize %uKB\nAllocationGranularity %uKB"
                , 4, 64
                , pInfo->dwPageSize / 1024
                , pInfo->dwAllocationGranularity / 1024);
            exit(0);
        }

        // gFN_Calculate_UnitSize 관련
        {
            using namespace GLOBAL::SETTING;
            _AssertRelease(gc_minSize_of_MemoryPoolUnitSize % gc_Array_MinUnit[0] == 0);

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
        GLOBAL::gFN_BuildData__QuickLink_MemoryPool();

        GLOBAL::g_nProcessor = CORE::g_instance_CORE.mFN_Get_System_Information()->mFN_Get_NumProcessor_Logical();

        if(GLOBAL::SETTING::gc_Max_ThreadAccessKeys < GLOBAL::g_nProcessor)
            _LOG_SYSTEM__WITH__TRACE(TRUE, "too many CPU Core : %d", GLOBAL::g_nProcessor);

        gFN_Build_ProcessorCodeTalbe();
    }

    void CMemoryPool_Manager::mFN_Initialize_MemoryPool_Shared_Environment_Variable2()
    {
        using namespace GLOBAL;

    #define __temp_macro_make4case(s)   \
            case s+0: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+0>; gpFN_Baskets_Free = gFN_Baskets_Free<s+0>; break;    \
            case s+1: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+1>; gpFN_Baskets_Free = gFN_Baskets_Free<s+1>; break;    \
            case s+2: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+2>; gpFN_Baskets_Free = gFN_Baskets_Free<s+2>; break;    \
            case s+3: gpFN_Baskets_Alloc = gFN_Baskets_Alloc<s+3>; gpFN_Baskets_Free = gFN_Baskets_Free<s+3>; break;
        switch(g_nProcessor)
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
    }

    void CMemoryPool_Manager::mFN_Destroy_MemoryPool_Shared_Environment_Variable()
    {
    }

    BOOL CMemoryPool_Manager::mFN_Initialize_MemoryPool_Shared_Variable()
    {
        BOOL ret = TRUE;
        if(!GLOBAL::g_VEM_ReserveMemory.mFN_Initialize())
        {
            ret = FALSE;
            _LOG_SYSTEM__WITH__TRACE(TRUE, _T("Failed VMEM ReserveMemory Manager : Initialize"));
        }
        return ret;
    }
    BOOL CMemoryPool_Manager::mFN_Destroy_MemoryPool_Shared_Variable()
    {
        BOOL ret = TRUE;
        if(!GLOBAL::g_VMEM_Recycle.mFN_Shutdown())
        {
            ret = FALSE;
            _LOG_SYSTEM__WITH__TRACE(TRUE, _T("Failed VMEM Recycle : Shutdown"));
        }
        if(!GLOBAL::g_VEM_ReserveMemory.mFN_Shutdown())
        {
            ret = FALSE;
            _LOG_SYSTEM__WITH__TRACE(TRUE, _T("Failed VMEM ReserveMemory Manager : Shutdown"));
        }
        return ret;
    }

#if _DEF_USING_DEBUG_MEMORY_LEAK
    void* CMemoryPool_Manager::mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line)
    {
        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool_Internal(_Size);
        if(pPool)
            return ((CMemoryPool&)(*pPool)).mFN_Get_Memory(_Size, _FileName, _Line);
        else
            return m_SlowPool.mFN_Get_Memory(_Size, _FileName, _Line);
    }
#else
    void* CMemoryPool_Manager::mFN_Get_Memory(size_t _Size)
    {
        auto pPool = CMemoryPool_Manager::mFN_Get_MemoryPool_Internal(_Size);
        if(pPool)
            return pPool->mFN_Get_Memory_Process();
        else
            return m_SlowPool.mFN_Get_Memory(_Size);
    }
#endif

    void CMemoryPool_Manager::mFN_Return_Memory(void* pAddress)
    {
        if(!pAddress)
            return;

        auto pH = sFN_Get_DataBlockHeader_fromPointer(pAddress);
        if(!pH)
        {
            mFN_ExceptionProcess_InvalidAddress_Deallocation(pAddress);
            return;
        }

        switch(pH->m_Type)
        {
        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_H1:
        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_HN:
            ((CMemoryPool*)pH->m_pPool)->mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress), pH);
            return;

        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Other_HN:
            ((CMemoryPool__BadSize*)pH->m_pPool)->mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;

        default: // TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Invalid:
            mFN_ExceptionProcess_InvalidAddress_Deallocation(pAddress);
        }

    }
    void CMemoryPool_Manager::mFN_Return_MemoryQ(void* pAddress)
    {
        if(!pAddress)
            return;

        #ifdef _DEBUG
        auto pH = sFN_Get_DataBlockHeader_fromPointer(pAddress);
        #else
        auto pH = sFN_Get_DataBlockHeader_fromPointerQ(pAddress);
        #endif
        if(!pH)
        {
            mFN_ExceptionProcess_InvalidAddress_Deallocation(pAddress);
            return;
        }

        switch(pH->m_Type)
        {
        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_H1:
        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_HN:
            ((CMemoryPool*)pH->m_pPool)->mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress), pH);
            return;

        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Other_HN:
            ((CMemoryPool__BadSize*)pH->m_pPool)->mFN_Return_Memory_Process(static_cast<TMemoryObject*>(pAddress));
            return;

        default: // TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Invalid:
            mFN_ExceptionProcess_InvalidAddress_Deallocation(pAddress);
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

        return CMemoryPool_Manager::mFN_Get_Memory(_Size, _FileName, _Line);
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

        return CMemoryPool_Manager::mFN_Get_Memory(_Size);
    }
#endif
    void CMemoryPool_Manager::mFN_Return_Memory__AlignedCacheSize(void* pAddress)
    {
        CMemoryPool_Manager::mFN_Return_Memory(pAddress);
    }

    IMemoryPool* CMemoryPool_Manager::mFN_Get_MemoryPool(size_t _Size)
    {
        auto pPool = mFN_Get_MemoryPool_Internal(_Size);
        if(pPool)
            return pPool;
        else
            return &m_SlowPool;
    }
#if _DEF_MEMORYPOOL_MAJORVERSION > 1
     IMemoryPool* CMemoryPool_Manager::mFN_Get_MemoryPool(void* pUserValidMemoryUnit)
     {
         if(!pUserValidMemoryUnit)
             return nullptr;

         auto pH = sFN_Get_DataBlockHeader_fromPointer(pUserValidMemoryUnit);
         if(!pH)
             return nullptr;

         return pH->m_pPool;
     }
#endif

#if _DEF_MEMORYPOOL_MAJORVERSION > 1
    size_t CMemoryPool_Manager::mFN_Query_Possible_MemoryPools_Num() const
    {
        return GLOBAL::SETTING::gc_Num_MemoryPool;
    }
    BOOL CMemoryPool_Manager::mFN_Query_Possible_MemoryPools_Size(size_t * _OUT_Array) const
    {
        if(!_OUT_Array)
            return FALSE;

        size_t UnitSize = GLOBAL::SETTING::gc_minSize_of_MemoryPoolUnitSize;
        *_OUT_Array++ = UnitSize;

        const size_t nLoop = GLOBAL::SETTING::gc_Array_Limit__Count;
        for(size_t k=0; k<nLoop; k++)
        {
            const size_t limit = GLOBAL::SETTING::gc_Array_Limit[k];
            const size_t step = GLOBAL::SETTING::gc_Array_MinUnit[k];
            do
            {
                UnitSize += step;
                *_OUT_Array++ = UnitSize;

            }while(UnitSize < limit);
        }
        return TRUE;
    }
    size_t CMemoryPool_Manager::mFN_Query_Limit_MaxUnitSize_Default() const
    {
        return GLOBAL::SETTING::gc_maxSize_of_MemoryPoolUnitSize;
    }
    size_t CMemoryPool_Manager::mFN_Query_Limit_MaxUnitSize_UserSetting() const
    {
        return m_MaxSize_of_MemoryPoolUnitSize;
    }
    BOOL CMemoryPool_Manager::mFN_Set_Limit_MaxUnitSize(size_t _MaxUnitSize_Byte, size_t* _OUT_Result)
    {
        size_t Result = GLOBAL::gFN_Calculate_UnitSize(_MaxUnitSize_Byte);
        if(0 == Result)
            return FALSE;

        m_Lock__PoolManager.Begin_Write__INFINITE();
        {
            const auto Previous = m_MaxSize_of_MemoryPoolUnitSize;
            m_MaxSize_of_MemoryPoolUnitSize = Result;

            _LOG_SYSTEM__WITH__TIME(_T("CMemoryPool_Manager::mFN_Set_Limit_MaxUnitSize(%Iu) : Limit %Iu->%Iu")
                , _MaxUnitSize_Byte
                , Previous
                , Result);

            // 사용을 원하지 않는 단위크기의 메모리풀중 이미 사용된 것에 대한 경고
            const size_t iTable = GLOBAL::gFN_Get_Index__QuickLink_MemoryPool(Result);
            if(iTable < GLOBAL::gc_Num_QuickLink_MemoryPool)
            {
                for(auto i=GLOBAL::g_Array_QuickLink_MemoryPool[iTable].m_index_Pool+1; i<GLOBAL::SETTING::gc_Num_MemoryPool; i++)
                {
                    if(!m_Array_Pool[i])
                        continue;

                    _LOG_SYSTEM("\tDetected MemoryPool[%u] : %u~%u", (UINT32)m_Array_Pool[i]->m_UnitSize
                        , (UINT32)m_Array_Pool[i]->m_UnitSizeMin
                        , (UINT32)m_Array_Pool[i]->m_UnitSizeMax);
                }
            }
        }
        m_Lock__PoolManager.End_Write();

        if(_OUT_Result)
            *_OUT_Result = Result;
        return TRUE;
    }
    // 힙 메모리 확보(관리하는 힙 메모리중 아무것이나 제거)
    BOOL CMemoryPool_Manager::mFN_Reduce_AnythingHeapMemory()
    {
        return GLOBAL::g_VMEM_Recycle.mFN_Free_AnythingHeapMemory();
    }
#endif

    void CMemoryPool_Manager::mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On)
    {
        m_Lock__PoolManager.Begin_Write__INFINITE();
        {
            m_bWriteStats_to_LogFile = _On;

            for(auto pPool : m_Array_Pool)
            {
                if(pPool)
                    pPool->mFN_Set_OnOff_WriteStats_to_LogFile(_On);
            }
        }
        m_Lock__PoolManager.End_Write();
    }
    void CMemoryPool_Manager::mFN_Set_OnOff_Trace_MemoryLeak(BOOL _On)
    {
        GLOBAL::g_bDebug__Trace_MemoryLeak = _On;
    }
    void CMemoryPool_Manager::mFN_Set_OnOff_ReportOutputDebugString(BOOL _On)
    {
        GLOBAL::g_bDebug__Report_OutputDebug = _On;
    }

#if _DEF_MEMORYPOOL_MAJORVERSION > 1
    BOOL CMemoryPool_Manager::mFN_Set_CurrentThread_Affinity(size_t _Index_CpuCore)
    {
        if(_Index_CpuCore < GLOBAL::g_nProcessor)
        {
            TMemoryPool_TLS_CACHE* pTLS = (TMemoryPool_TLS_CACHE*)CMemoryPool::sFN_TLS_Get();
            if(pTLS)
            {
                pTLS->mFN_Set_On_AffinityProcessor(_Index_CpuCore);
                return TRUE;
            }
            else
            {
                _DebugBreak(_T("Not Found TLS"));
                return FALSE;
            }
        }
        else
        {
            _LOG_SYSTEM__WITH__OUTPUTDEBUGSTR_ALWAYS(TRUE, _T("Failed CMemoryPool_Manager::mFN_Set_CurrentThread_Affinity(%Iu)"), _Index_CpuCore);
            _DebugBreak(_T("Current CPU Valid Range : 0 ~ %llu"), (UINT64)(GLOBAL::g_nProcessor-1));
            
            return FALSE;
        }
    }
    void CMemoryPool_Manager::mFN_Reset_CurrentThread_Affinity()
    {
        TMemoryPool_TLS_CACHE* pTLS = (TMemoryPool_TLS_CACHE*)CMemoryPool::sFN_TLS_Get();
        if(pTLS)
            pTLS->mFN_Set_Off_AffinityProcessor();
    }

    void CMemoryPool_Manager::mFN_Shutdown_CurrentThread_MemoryPoolTLS()
    {
        CMemoryPool::sFN_TLS_Destroy();
    }

    TMemoryPool_Virsion CMemoryPool_Manager::mFN_Query_Version() const
    {
        TMemoryPool_Virsion ret;
        ret.Major = GLOBAL::VERSION::gc_Version_Major;
        ret.Minor = GLOBAL::VERSION::gc_Version_Minor;
        return ret;
    }
#endif


    DECLSPEC_NOINLINE CMemoryPool* CMemoryPool_Manager::mFN_CreatePool_and_SetMemoryPoolTable(size_t iTable, size_t UnitSize)
    {
    #pragma warning(push)
    #pragma warning(disable: 6385)
        // 메모리풀을 찾거나 생성하고, 전역 캐시에 설정
        _Assert(iTable < GLOBAL::gc_Num_QuickLink_MemoryPool);
        const auto IndexPool = GLOBAL::g_Array_QuickLink_MemoryPool[iTable].m_index_Pool;
    #pragma warning(pop)

        CMemoryPool* pReturn = m_Array_Pool[IndexPool];
        // --느슨한 확인--
        if(pReturn)
        {
            GLOBAL::gFN_Set_QuickLink_MemoryPoolPTR(iTable, pReturn);
            return pReturn;
        }


        // --등록 시도--
        _Assert(pReturn == nullptr);
        m_Lock__PoolManager.Begin_Write__INFINITE();
        {
            // 다른 스레드가 끼어들어, 해당되는 풀을 만들었는지 한번더 확인
            pReturn = m_Array_Pool[IndexPool];

            if(!pReturn)
            {
                // 실제 등록
                //_SAFE_NEW_ALIGNED_CACHELINE(pReturn, CMemoryPool(UnitSize, IndexPool));

                pReturn = (CMemoryPool*)CObjectPool_Handle<CMemoryPool>::Alloc();
                _MACRO_CALL_CONSTRUCTOR(pReturn, CMemoryPool(UnitSize, IndexPool));
                pReturn->mFN_Set_OnOff_WriteStats_to_LogFile(m_bWriteStats_to_LogFile);

                m_Array_Pool[IndexPool] = pReturn;
            }
        }
        m_Lock__PoolManager.End_Write();
        
        _Assert(pReturn != nullptr);
        GLOBAL::gFN_Set_QuickLink_MemoryPoolPTR(iTable, pReturn);
        return pReturn;
    }


    CMemoryPool* CMemoryPool_Manager::mFN_Get_MemoryPool_Internal(size_t _Size)
    {
    #pragma warning(push)
    #pragma warning(disable: 6385)
        if(!_Size)
            _Size = GLOBAL::SETTING::gc_minSize_of_MemoryPoolUnitSize;
        else if(_Size > m_MaxSize_of_MemoryPoolUnitSize)
            return nullptr;

        const size_t iTable = GLOBAL::gFN_Get_Index__QuickLink_MemoryPool(_Size);
        _Assert(iTable < GLOBAL::gc_Num_QuickLink_MemoryPool);

        CMemoryPool* const pPool = GLOBAL::g_Array_QuickLink_MemoryPool[iTable].m_pPool;
        if(pPool)
            return pPool;

        // 해당 사이즈의 MemoryPool을 찾거나 생성
        const size_t UnitSize = GLOBAL::gFN_Calculate_UnitSize(_Size);
        return CMemoryPool_Manager::mFN_CreatePool_and_SetMemoryPoolTable(iTable, UnitSize);
    #pragma warning(pop)
    }
    CMemoryPool* CMemoryPool_Manager::mFN_Get_MemoryPool_QuickAccess(UINT32 _Index)
    {
        if(_MACRO_ARRAY_COUNT(m_Array_Pool) <= _Index)
            return nullptr;
        return m_Array_Pool[_Index];
    }


    const TDATA_BLOCK_HEADER* CMemoryPool_Manager::sFN_Get_DataBlockHeader_SmallObjectSize(const void* p)
    {
        _CompileHint(nullptr != p);
        const TDATA_BLOCK_HEADER* pH = _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)p, GLOBAL::gc_minAllocationSize);

    #if _DEF_USING_MEMORYPOOL_TEST_SMALLUNIT__FROM__DBH_TABLE
        // 올바른 것처럼 속인 헤더 + 잘못된 주소로 인한 위험을 예방하는 방법
        const TDATA_BLOCK_HEADER* pHTrust = GLOBAL::g_Table_BlockHeaders_Normal.mFN_Get_Link(pH->m_Index.high, pH->m_Index.low);
        if(pHTrust->m_Type != TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_H1)
            return nullptr;
        if(!pHTrust->mFN_Query_ContainPTR(p))
            return nullptr;

        return pHTrust;
    #else
        // 다른 모든 유닛크기를 확인후에 사용해야 한다
        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_H1)
            return nullptr;
        if(!pH->mFN_Query_ContainPTR(p))
            return nullptr;

        return pH;
    #endif
    }
    const TDATA_BLOCK_HEADER* CMemoryPool_Manager::sFN_Get_DataBlockHeader_NormalObjectSize(const void* p)
    {
        _CompileHint(nullptr != p);
        if(!_MACRO_POINTER_IS_ALIGNED(p, _DEF_CACHELINE_SIZE))
            return nullptr;

        const TDATA_BLOCK_HEADER* pH = static_cast<const TDATA_BLOCK_HEADER*>(p) - 1;

        if(pH->m_Type != TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_HN)
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

        return pHTrust;
    }

    const TDATA_BLOCK_HEADER* CMemoryPool_Manager::sFN_Get_DataBlockHeader_fromPointer(const void* p)
    {
        _CompileHint(nullptr != p);

        // p의 앞 64바이트는 무조건 존재한다
        const TDATA_BLOCK_HEADER& HN = *(static_cast<const TDATA_BLOCK_HEADER*>(p) - 1);
        // 헤더가 아닌 값이 우연히 패턴과 일치할 수 있기 때문에 테이블을 확인해야 한다
        switch(HN.m_Type)
        {
        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_HN:
        {
            const CTable_DataBlockHeaders& Table = GLOBAL::g_Table_BlockHeaders_Normal;
            const TDATA_BLOCK_HEADER* const pHTrust = Table.mFN_Get_Link(HN.m_Index.high, HN.m_Index.low);
            if(pHTrust && pHTrust->mFN_Query_ContainPTR(p))
                return pHTrust;
            else
                return nullptr;
        }
        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Other_HN:
        {
            const CTable_DataBlockHeaders& Table = GLOBAL::g_Table_BlockHeaders_Other;
            const TDATA_BLOCK_HEADER* const pHTrust = Table.mFN_Get_Link(HN.m_Index.high, HN.m_Index.low);
            if(pHTrust && pHTrust->mFN_Query_ContainPTR(p))
                return pHTrust;
            else
                return nullptr;
        }
        }

        return sFN_Get_DataBlockHeader_SmallObjectSize(p);
    }
    __forceinline const TDATA_BLOCK_HEADER* CMemoryPool_Manager::sFN_Get_DataBlockHeader_fromPointerQ(const void* p)
    {
        _CompileHint(nullptr != p);

        // p의 앞 64바이트는 무조건 존재한다
        const TDATA_BLOCK_HEADER& HN = *(static_cast<const TDATA_BLOCK_HEADER*>(p) - 1);
        // 무효한 주소일 수 있다
        const TDATA_BLOCK_HEADER* const pHS = _MACRO_POINTER_GET_ALIGNED((TDATA_BLOCK_HEADER*)p, GLOBAL::gc_minAllocationSize);

        // 헤더가 아닌 값이 우연히 패턴과 일치할 수 있기 때문에 테이블을 확인해야 한다
        switch(HN.m_Type)
        {
        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_HN:
        {
            const CTable_DataBlockHeaders& Table = GLOBAL::g_Table_BlockHeaders_Normal;
            const TDATA_BLOCK_HEADER* const pHTrust = Table.mFN_Get_Link(HN.m_Index.high, HN.m_Index.low);
            if(pHTrust && pHTrust->mFN_Query_ContainPTR(p))
                return pHTrust;
            else
                return nullptr;
        }
        case TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Other_HN:
        {
            const CTable_DataBlockHeaders& Table = GLOBAL::g_Table_BlockHeaders_Other;
            const TDATA_BLOCK_HEADER* const pHTrust = Table.mFN_Get_Link(HN.m_Index.high, HN.m_Index.low);
            if(pHTrust && pHTrust->mFN_Query_ContainPTR(p))
                return pHTrust;
            else
                return nullptr;
        }
        }

    #ifdef _DEBUG
        UNREFERENCED_PARAMETER(pHS);
        return sFN_Get_DataBlockHeader_SmallObjectSize(p);
    #else
        if(pHS->m_Type == TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_H1)
            return pHS;
        else
            return nullptr;
    #endif
    }
    

    void CMemoryPool_Manager::mFN_Report_Environment_Variable()
    {
        auto sysinfo = CORE::g_instance_CORE.mFN_Get_System_Information();
        _LOG_DEBUG__WITH__OUTPUTDEBUGSTR(FALSE, "CPU Core Count(Physical: %u Logical: %u)"
            , sysinfo->mFN_Get_NumProcessor_PhysicalCore()
            , sysinfo->mFN_Get_NumProcessor_Logical()
            );
    }
    void CMemoryPool_Manager::mFN_Debug_Report()
    {
        if(0 < m_stats_Counting_Free_BadPTR)
        {
            _LOG_SYSTEM__WITH__OUTPUTDEBUGSTR(FALSE, _T("MemoryPoolManager Invalid Free Reques Count : %Iu"), m_stats_Counting_Free_BadPTR);
        }

        // 전역 VMEM
        _MACRO_STATIC_ASSERT(sizeof(TVMEM_CACHE_LIST) == sizeof(CVMEM_CACHE) * TVMEM_CACHE_LIST::sc_NumCache);
    #if _DEF_USING_MEMORYPOOL_DEBUG
        _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("Report VMEM Recycle\n");
        for(size_t iVMEM_Recycle=0; iVMEM_Recycle<TVMEM_CACHE_LIST::sc_NumCache; iVMEM_Recycle++)
        {
            const CVMEM_CACHE* pRecycleCACHE = ((CVMEM_CACHE*)&GLOBAL::g_VMEM_Recycle) + iVMEM_Recycle;
            const size_t ChunkSizeKB = pRecycleCACHE->mFN_Query__SizeKB_per_VMEM();
            const CVMEM_CACHE::TStats& stats = pRecycleCACHE->mFN_Get_Stats();

            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(_T("\tCACHE[%IuKB]\n")
                _T("\t\tMax Cnt %Iu(%IuKB)\n")
                _T("\t\tPush Cnt %I64u\n")
                , ChunkSizeKB
                , stats.m_Max_cnt_VMEM
                , stats.m_Max_cnt_VMEM * ChunkSizeKB
                , stats.m_Counting_VMEM_Push);
        }
    #endif
    }
    BOOL CMemoryPool_Manager::sFN_Callback_from_DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
    {
        UNREFERENCED_PARAMETER(hModule);
        UNREFERENCED_PARAMETER(lpReserved);
        switch(ul_reason_for_call)
        {
        case DLL_PROCESS_ATTACH:
            if(!UTIL::MEM::CMemoryPool::sFN_TLS_InitializeIndex())
                return FALSE;
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            UTIL::MEM::CMemoryPool::sFN_TLS_Destroy();
            break;

        case DLL_PROCESS_DETACH:
            if(!UTIL::MEM::CMemoryPool::sFN_TLS_ShutdownIndex())
                return FALSE;
            break;
        }

        return TRUE;
    }

    DECLSPEC_NOINLINE void CMemoryPool_Manager::mFN_ExceptionProcess_InvalidAddress_Deallocation(const void* p)
    {
        m_stats_Counting_Free_BadPTR++;
        MemoryPool_UTIL::sFN_Report_Invalid_Deallocation(p);
    }

}
}
#endif