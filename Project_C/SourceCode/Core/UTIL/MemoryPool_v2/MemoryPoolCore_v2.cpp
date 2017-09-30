#include "stdafx.h"
#if _DEF_MEMORYPOOL_MAJORVERSION == 2
#include "./MemoryPoolCore_v2.h"

#include "./MemoryPool_v2.h"
#include "../ObjectPool.h"
#include "./VMEM_LIST.h"

#include "../../Core.h"
_MACRO_STATIC_ASSERT(64 == _DEF_CACHELINE_SIZE && sizeof(void*) == sizeof(size_t));
_MACRO_STATIC_ASSERT(sizeof(::UTIL::LOCK::CSpinLock) == 8);

#pragma message("MemoryPoolCore.cpp 수정시 유의사항 : 코드내에서 다음의 매크로를 이용한 조건부 컴파일 금지")
#pragma message("_DEF_USING_DEBUG_MEMORY_LEAK")
#pragma message("_DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT")
#pragma message("_DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE")

#if _DEF_USING_MEMORYPOOL_DEBUG && !defined(_DEBUG)
  _DEF_COMPILE_MSG__WARNING("배포주의!! _DEF_USING_MEMORYPOOL_DEBUG : Release 버전에서 활성화")
  #ifdef _AssertRelease
    #undef _Assert
    #define _Assert _AssertRelease
  #endif
#endif

#if _DEF_USING_MEMORYPOOL__PERFORMANCE_PROFILING && !defined(_DEBUG)
_DEF_COMPILE_MSG__WARNING("배포금지!! _DEF_USING_MEMORYPOOL__PERFORMANCE_PROFILING : 인라인 비활성화")
#endif


namespace UTIL{
namespace MEM{
    namespace GLOBAL{
        extern size_t g_nProcessor;

        extern CTable_DataBlockHeaders g_Table_BlockHeaders_Normal;
        extern CTable_DataBlockHeaders g_Table_BlockHeaders_Other;

        UINT32 gFN_Precalculate_MemoryPoolExpansionSize__GetSmallSize(UINT32 _IndexThisPool);
        UINT32 gFN_Precalculate_MemoryPoolExpansionSize__GetBigSize(UINT32 _IndexThisPool);
    }
    namespace GLOBAL{
    #pragma region 설정(빌드타이밍)
        // gc_minAllocationSize : OS 메모리 예약단위 배수 , ON 비트 1개
        _MACRO_STATIC_ASSERT(gc_minAllocationSize % (64*1024) == 0);
        _MACRO_STATIC_ASSERT(1 == _MACRO_STATIC_BIT_COUNT(gc_minAllocationSize));
        const UINT32 gc_MinimumUnits_per_UnitsGroup = 8u;
    #pragma endregion
    }

    DECLSPEC_NOINLINE void MemoryPool_UTIL::sFN_Debug_Overflow_Set(void * p, size_t size, BYTE code)
    {
        if(!size)
            return;
        ::memset(p, code, size);
    }
    DECLSPEC_NOINLINE void MemoryPool_UTIL::sFN_Debug_Overflow_Check(const void * p, size_t size, BYTE code)
    {
        if(0 < size && !g_pUtil->mFN_MemoryCompareSSE2(p, size, code))
            sFN_Report_DamagedMemory(p, size);
    }
    DECLSPEC_NOINLINE void MemoryPool_UTIL::sFN_Report_DamagedMemoryPool()
    {
    #if _DEF_USING_MEMORYPOOL_DEBUG
        static BOOL s_Ignore = FALSE;
        if(s_Ignore)
            return;

        auto btn = _MACRO_MESSAGEBOX__WITH__OUTPUTDEBUGSTR_ALWAYS(MB_ABORTRETRYIGNORE | MB_ICONERROR, _T("Error"), _T("Damaged MemoryPool"));
        if(btn == IDABORT)
            __debugbreak();
        else if(btn == IDIGNORE)
            s_Ignore = TRUE;
    #else
        static BOOL s_bDone_Report = FALSE;
        if(!s_bDone_Report)
        {
            _LOG_SYSTEM__WITH__TRACE(FALSE, _T("Damaged MemoryPool"));
            s_bDone_Report = TRUE;
        }
    #endif
    }
    DECLSPEC_NOINLINE void MemoryPool_UTIL::sFN_Report_DamagedMemory(const void* p, size_t size)
    {
    #if _DEF_USING_MEMORYPOOL_DEBUG
        static BOOL s_Ignore = FALSE;;
        if(s_Ignore)
            return;

        auto btn = _MACRO_MESSAGEBOX__WITH__OUTPUTDEBUGSTR_ALWAYS(MB_ABORTRETRYIGNORE | MB_ICONERROR, _T("Error"), _T("Damaged Memory 0x%p ~\n%Iu Byte"),  p, size);
        if(btn == IDABORT)
            __debugbreak();
        else if(btn == IDIGNORE)
            s_Ignore = TRUE;
    #else
        _LOG_SYSTEM__WITH__TRACE(TRUE, _T("Damaged Memory 0x%p ~ %Iu Byte"), p, size);
    #endif
    }
    DECLSPEC_NOINLINE void MemoryPool_UTIL::sFN_Report_Invalid_Deallocation(const void* p)
    {
    #if _DEF_USING_MEMORYPOOL_DEBUG
        static BOOL s_Ignore = FALSE;;
        if(s_Ignore)
            return;

        auto btn = _MACRO_MESSAGEBOX__WITH__OUTPUTDEBUGSTR_ALWAYS(MB_ABORTRETRYIGNORE | MB_ICONERROR, _T("Error"), _T("Invalid Memory Deallocation 0x%p"), p);
        if(btn == IDABORT)
            __debugbreak();
        else if(btn == IDIGNORE)
            s_Ignore = TRUE;
    #else
        _LOG_SYSTEM__WITH__TRACE(FALSE, _T("Invalid Memory Deallocation 0x%p"), p);
    #endif
    }
}
}


namespace UTIL{
namespace MEM{


    size_t TMemoryObject::mFN_Counting() const
    {
        _Assert(this);
        size_t cnt = 1;
        for(const TMemoryObject* p = this->pNext; p; p = p->pNext)
            cnt++;
        return cnt;
    }
    TMemoryUnitsGroup::TMemoryUnitsGroup(TDATA_VMEM* pOwner, size_t UnitSize, BOOL _Use_Header_per_Unit)
        : m_nUnits(0), m_State(EState::E_Invalid)
        , m_ReferenceMask(sc_InitialValue_ReferenceMask)
        , m_pContainerChunkFocus(nullptr), m_iContainerChunk_Detail_Index(sc_InitialValue_iContainerChunk_Detail_Index)
        , m_Counting_Used_ReservedMemory(0)
        , m_pStartAddress(nullptr)
        , m_pFirstDataBlockHeader(pOwner->mFN_Get_FirstHeader())
        , m_UnitSize(UnitSize)
        , m_bUse_Header_per_Unit(_Use_Header_per_Unit)
        , m_nTotalUnits(0)
        , m_pOwner(pOwner)
    {
    #if __X86
        _MACRO_STATIC_ASSERT(sizeof(m_ReferenceMask) == 4);
    #elif __X64
        _MACRO_STATIC_ASSERT(sizeof(m_ReferenceMask) == 8);
    #endif
    }
    TMemoryUnitsGroup::~TMemoryUnitsGroup()
    {
        _Assert(m_Lock.Query_isLocked());
    }
    void TMemoryUnitsGroup::sFN_ObjectPool_Initialize()
    {
    #define _LOCAL_MACRO_OBJECTPOOL_REFERENCE_ATTACH(x) \
        CObjectPool_Handle__UniquePool<TMemoryUnitsGroupChunk::_x##x>::Reference_Attach();\
        CObjectPool_Handle__UniquePool<TMemoryUnitsGroupChunk::_x##x>::GetPool()->mFN_Set_OnOff_CheckOverflow(FALSE)

        _LOCAL_MACRO_OBJECTPOOL_REFERENCE_ATTACH(1);
        _LOCAL_MACRO_OBJECTPOOL_REFERENCE_ATTACH(2);
        _LOCAL_MACRO_OBJECTPOOL_REFERENCE_ATTACH(4);
        _LOCAL_MACRO_OBJECTPOOL_REFERENCE_ATTACH(8);

    #undef _LOCAL_MACRO_OBJECTPOOL_REFERENCE_ATTACH
    }
    void TMemoryUnitsGroup::sFN_ObjectPool_Shotdown()
    {
    #define _LOCAL_MACRO_OBJECTPOOL_REFERENCE_DETACH(x) \
         CObjectPool_Handle__UniquePool<TMemoryUnitsGroupChunk::_x##x>::Reference_Detach()

        _LOCAL_MACRO_OBJECTPOOL_REFERENCE_DETACH(1);
        _LOCAL_MACRO_OBJECTPOOL_REFERENCE_DETACH(2);
        _LOCAL_MACRO_OBJECTPOOL_REFERENCE_DETACH(4);
        _LOCAL_MACRO_OBJECTPOOL_REFERENCE_DETACH(8);

    #undef _LOCAL_MACRO_OBJECTPOOL_REFERENCE_DETACH
    }
    namespace{
        // 빠른 접근을 위한 유닛그룹 할당/제거 함수포인터
        // n 배율은 정해진것만 사용하며, 나머지는 에러가 발생하도록 한다
    #define _LOCAL_MACRO_ALLOC(n) (BOOL(*)(TMemoryUnitsGroup**, size_t))CObjectPool_Handle__UniquePool<TMemoryUnitsGroupChunk::_x##n##>::Alloc_N
        BOOL (*gpFN_Array_MemoryUnitsGroup_Alloc[])(TMemoryUnitsGroup**, size_t) =
        {
            nullptr, //[0]
            _LOCAL_MACRO_ALLOC(1), _LOCAL_MACRO_ALLOC(2), nullptr, _LOCAL_MACRO_ALLOC(4),
            nullptr, nullptr, nullptr, _LOCAL_MACRO_ALLOC(8),
        };
    #define _LOCAL_MACRO_FREE(n) (void(*)(TMemoryUnitsGroup**, size_t))CObjectPool_Handle__UniquePool<TMemoryUnitsGroupChunk::_x##n##>::Free_N
        void (*gpFN_Array_MemoryUnitsGroup_Free[])(TMemoryUnitsGroup**, size_t) =
        {
            nullptr, //[0]
            _LOCAL_MACRO_FREE(1), _LOCAL_MACRO_FREE(2), nullptr, _LOCAL_MACRO_FREE(4),
            nullptr, nullptr, nullptr, _LOCAL_MACRO_FREE(8),
        };
    #undef _LOCAL_MACRO_ALLOC
    #undef _LOCAL_MACRO_FREE
    }

    TMemoryObject* TMemoryUnitsGroup::mFN_GetUnit_from_ReserveMemory()
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
    #pragma warning(disable: 28182)
        _Assert(m_pFirstDataBlockHeader);
        _Assert(m_pStartAddress);
        _Assert(m_Counting_Used_ReservedMemory < m_nTotalUnits);

        TMemoryObject* pUnit;

        if(!m_bUse_Header_per_Unit)
        {
            const size_t RealUnitSize = m_UnitSize;
            byte* pUnitB = m_pStartAddress + (RealUnitSize * (size_t)m_Counting_Used_ReservedMemory);

            pUnit = (TMemoryObject*)pUnitB;
        }
        else
        {
            const size_t RealUnitSize = m_UnitSize + sizeof(TDATA_BLOCK_HEADER);
            byte* pUnitB = m_pStartAddress + (RealUnitSize * (size_t)m_Counting_Used_ReservedMemory);

            TDATA_BLOCK_HEADER* pH = (TDATA_BLOCK_HEADER*)pUnitB;
            *pH = *m_pFirstDataBlockHeader;
            pUnit = (TMemoryObject*)(pH+1);
        }

        m_Counting_Used_ReservedMemory++;

        _Assert(m_pFirstDataBlockHeader->mFN_Query_ContainPTR(pUnit) && m_pFirstDataBlockHeader->mFN_Query_AlignedPTR(pUnit));
    #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
        MemoryPool_UTIL::sFN_Debug_Overflow_Set(pUnit+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
    #endif

        _Assert(pUnit != nullptr);
        return pUnit;
    #pragma warning(pop)
    }
    void TMemoryUnitsGroup::mFN_Sort_Units()
    {
        _Assert(m_Lock.Query_isLocked());
        _Assert(mFN_Query_isFull());
        if(!mFN_Query_isFull())
            return;

        m_Units.m_pUnit_F = m_Units.m_pUnit_B = nullptr;
        m_Counting_Used_ReservedMemory = 0;
    }

    BOOL TMemoryUnitsGroup::mFN_Query_isPossibleSafeDestroy() const
    {
        // 잠금을 확보한 상태여야 한다
        BOOL bSucceed = TRUE;
        if(!m_Lock.Query_isLocked())
            bSucceed = FALSE;
        else if(m_nUnits != m_nTotalUnits)
            bSucceed = FALSE;
        else if(mFN_Query_isManaged_from_Container())
            bSucceed = FALSE;
        else if(mFN_Query_isAttached_to_Processor())
            bSucceed = FALSE;

        if(!bSucceed)
            mFN_Report_Failed__Query_isPossibleSafeDestroy();

        return bSucceed;
    }
    BOOL TMemoryUnitsGroup::mFN_Query_isDamaged__CurrentStateNotManaged() const
    {
        // 메모리릭은 손상으로 판단하지 않는다
        BOOL bDamaged = FALSE;
        if(!m_Lock.Query_isLocked())
            bDamaged = TRUE;
        else if(m_nUnits > m_nTotalUnits)
            bDamaged = TRUE;
        else if(mFN_Query_isManaged_from_Container())
            bDamaged = TRUE;
        else if(mFN_Query_isAttached_to_Processor())
            bDamaged = TRUE;
        return bDamaged;
    }

    DECLSPEC_NOINLINE void TMemoryUnitsGroup::mFN_Report_Failed__Query_isPossibleSafeDestroy() const
    {
        // 메모리릭은 경고하지 않는다
        if(mFN_Query_isDamaged__CurrentStateNotManaged())
            MemoryPool_UTIL::sFN_Report_DamagedMemory(this, sizeof(*this));
    }

#pragma warning(push)
#pragma warning(disable: 6011)
#pragma warning(disable: 28182)
#if _DEF_USING_MEMORYPOOL_OPTIMIZE__UNITSGROUP_POP_UNITS_VER1
    UINT32 TMemoryUnitsGroup::mFN_Pop(TMemoryObject** _OUT_pF, TMemoryObject** _OUT_pL, UINT32 nMaxRequire)
    {
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(_OUT_pF && _OUT_pL && _OUT_pF != _OUT_pL && 0 < nMaxRequire);
        if(0 == nMaxRequire)
        {
            *_OUT_pF = *_OUT_pL = nullptr;
            return 0;
        }
        if(mFN_Query_isFull())
            m_pOwner->mFN_Counting_FullGroup_Decrement();

        UINT32 cnt = 0;
        TMemoryObject* pResultF;
        TMemoryObject* pResultL;
        // # 활성 메모리 우선
        if(m_Units.m_pUnit_F)
        {
            pResultF = m_Units.m_pUnit_F;
            TMemoryObject* pFocus = m_Units.m_pUnit_F;
            do
            {
                pResultL = pFocus;
                pFocus = pFocus->pNext;
                if(!pFocus)
                {
                    cnt++;
                    break;
                }
            }while(++cnt < nMaxRequire);
            if(!pFocus)
                m_Units.m_pUnit_F = m_Units.m_pUnit_B = nullptr;
            else
                m_Units.m_pUnit_F = pFocus;

            m_nUnits -= cnt;
            pResultL->pNext = nullptr;
            if(cnt == nMaxRequire)
            {
                *_OUT_pF = pResultF;
                *_OUT_pL = pResultL;
                return cnt;
            }
        }
        else
        {
            pResultF = pResultL = nullptr;
        }
        //----------------------------------------------------------------
        // # 예약 메모리 사용
        if(0 == m_nUnits)
        {
            *_OUT_pF = pResultF;
            *_OUT_pL = pResultL;
            return cnt;
        }

        _CompileHint(m_pFirstDataBlockHeader);
        _CompileHint(m_pStartAddress);
        _CompileHint(m_Counting_Used_ReservedMemory < m_nTotalUnits);

        const auto nReserved = m_nTotalUnits - m_Counting_Used_ReservedMemory;
        const auto nNeed = nMaxRequire - cnt;
        const auto nGet = min(nReserved, nNeed);
        _CompileHint(nReserved == m_nUnits);
        _CompileHint(0 < nGet);

        TMemoryObject* pReserved_F;
        TMemoryObject* pReserved_L;
        if(!m_bUse_Header_per_Unit)
        {
            const size_t RealUnitSize = m_UnitSize;
            byte* pFocus = m_pStartAddress + (RealUnitSize * (size_t)m_Counting_Used_ReservedMemory);
            pReserved_F = pReserved_L = (TMemoryObject*)pFocus;

            // N개 단위로 처리 기능(메모리 레이턴시 감소)
            const UINT32 xChunk = 8;
            const size_t ChunkSize = xChunk * RealUnitSize;
            const auto nChunk = nGet / xChunk;
            const auto nOther = nGet % xChunk;
            for(UINT32 i=0; i<nChunk; i++)
            {
                TMemoryObject* p0 = reinterpret_cast<TMemoryObject*>(pFocus+(RealUnitSize*0));
                TMemoryObject* p1 = reinterpret_cast<TMemoryObject*>(pFocus+(RealUnitSize*1));
                TMemoryObject* p2 = reinterpret_cast<TMemoryObject*>(pFocus+(RealUnitSize*2));
                TMemoryObject* p3 = reinterpret_cast<TMemoryObject*>(pFocus+(RealUnitSize*3));
                TMemoryObject* p4 = reinterpret_cast<TMemoryObject*>(pFocus+(RealUnitSize*4));
                TMemoryObject* p5 = reinterpret_cast<TMemoryObject*>(pFocus+(RealUnitSize*5));
                TMemoryObject* p6 = reinterpret_cast<TMemoryObject*>(pFocus+(RealUnitSize*6));
                TMemoryObject* p7 = reinterpret_cast<TMemoryObject*>(pFocus+(RealUnitSize*7));

                pFocus += ChunkSize;

                p0->pNext = p1;
                p1->pNext = p2;
                p2->pNext = p3;
                p3->pNext = p4;
                p4->pNext = p5;
                p5->pNext = p6;
                p6->pNext = p7;
                p7->pNext = (TMemoryObject*)pFocus;
                pReserved_L = p7;
            }
            for(UINT32 i=0; i<nOther; i++)
            {
                pReserved_L = (TMemoryObject*)pFocus; // current
                pFocus += RealUnitSize;
                pReserved_L->pNext = (TMemoryObject*)pFocus;
            }
        }
        else
        {
            const size_t HeaderSize = sizeof(TDATA_BLOCK_HEADER);
            const size_t UnitSize = m_UnitSize;
            const size_t RealUnitSize = HeaderSize + UnitSize;
            byte* pFocus = m_pStartAddress + (RealUnitSize * (size_t)m_Counting_Used_ReservedMemory);
            pReserved_F = pReserved_L = (TMemoryObject*)(pFocus+HeaderSize);

            // N개 단위로 처리 기능(메모리 레이턴시 감소)
            const UINT32 xChunk = 4;
            const size_t ChunkSize = xChunk * RealUnitSize;
            const auto nChunk = nGet / xChunk;
            const auto nOther = nGet % xChunk;
            for(UINT32 i=0; i<nChunk; i++)
            {
                TDATA_BLOCK_HEADER* pH0 = reinterpret_cast<TDATA_BLOCK_HEADER*>(pFocus+(RealUnitSize*0));
                TDATA_BLOCK_HEADER* pH1 = reinterpret_cast<TDATA_BLOCK_HEADER*>(pFocus+(RealUnitSize*1));
                TDATA_BLOCK_HEADER* pH2 = reinterpret_cast<TDATA_BLOCK_HEADER*>(pFocus+(RealUnitSize*2));
                TDATA_BLOCK_HEADER* pH3 = reinterpret_cast<TDATA_BLOCK_HEADER*>(pFocus+(RealUnitSize*3));
                *pH0 = *pH1 = *pH2 = *pH3 = *m_pFirstDataBlockHeader;

                TMemoryObject* p0 = reinterpret_cast<TMemoryObject*>(pH0+1);
                TMemoryObject* p1 = reinterpret_cast<TMemoryObject*>(pH1+1);
                TMemoryObject* p2 = reinterpret_cast<TMemoryObject*>(pH2+1);
                TMemoryObject* p3 = reinterpret_cast<TMemoryObject*>(pH3+1);

                pFocus += ChunkSize;

                p0->pNext = p1;
                p1->pNext = p2;
                p2->pNext = p3;
                p3->pNext = reinterpret_cast<TMemoryObject*>(pFocus+HeaderSize);
                pReserved_L = p3;
            }
            for(UINT32 i=0; i<nOther; i++)
            {
                TDATA_BLOCK_HEADER* pH = (TDATA_BLOCK_HEADER*)pFocus;
                *pH = *m_pFirstDataBlockHeader;

                pReserved_L = (TMemoryObject*)(pFocus + HeaderSize); // current
                pFocus += RealUnitSize;

                TMemoryObject* pNextUnit = (TMemoryObject*)(pFocus + HeaderSize);
                pReserved_L->pNext = pNextUnit;
            }
        }
        _CompileHint(pReserved_F && pReserved_L);
        pReserved_L->pNext = nullptr;
    #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
        for(TMemoryObject* p = pReserved_F; p; p = p->pNext)
        {
            MemoryPool_UTIL::sFN_Debug_Overflow_Set(p+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
        }
    #endif

        m_Counting_Used_ReservedMemory += nGet;
        m_nUnits -= nGet;
        cnt += nGet;

        if(pResultL)
        {
            pResultL->pNext = pReserved_F;
            *_OUT_pF = pResultF;
            *_OUT_pL = pReserved_L;
        }
        else
        {
            *_OUT_pF = pReserved_F;
            *_OUT_pL = pReserved_L;
        }
        return cnt;
    }
#else
    UINT32 TMemoryUnitsGroup::mFN_Pop(TMemoryObject** _OUT_pF, TMemoryObject** _OUT_pL, UINT32 nMaxRequire)
    {
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(_OUT_pF && _OUT_pL && _OUT_pF != _OUT_pL && 0 < nMaxRequire);
        if(0 == nMaxRequire)
        {
            *_OUT_pF = *_OUT_pL = nullptr;
            return 0;
        }
        if(mFN_Query_isFull())
            m_pOwner->mFN_Counting_FullGroup_Decrement();
        
        UINT32 cnt = 0;
        TMemoryObject* pResultF;
        TMemoryObject* pResultL;
        // # 활성 메모리 우선
        if(m_Units.m_pUnit_F)
        {
            pResultF = m_Units.m_pUnit_F;
            TMemoryObject* pFocus = m_Units.m_pUnit_F;
            do
            {
                pResultL = pFocus;
                pFocus = pFocus->pNext;
                if(!pFocus)
                {
                    cnt++;
                    break;
                }
            }while(++cnt < nMaxRequire);
            if(!pFocus)
                m_Units.m_pUnit_F = m_Units.m_pUnit_B = nullptr;
            else
                m_Units.m_pUnit_F = pFocus;

            m_nUnits -= cnt;
            pResultL->pNext = nullptr;
            if(cnt == nMaxRequire)
            {
                *_OUT_pF = pResultF;
                *_OUT_pL = pResultL;
                return cnt;
            }
        }
        else
        {
            pResultF = pResultL = nullptr;
        }
        //----------------------------------------------------------------
        // # 예약 메모리 사용
        if(0 == m_nUnits)
        {
            *_OUT_pF = pResultF;
            *_OUT_pL = pResultL;
            return cnt;
        }

        _CompileHint(m_pFirstDataBlockHeader);
        _CompileHint(m_pStartAddress);
        _CompileHint(m_Counting_Used_ReservedMemory < m_nTotalUnits);

        const auto nReserved = m_nTotalUnits - m_Counting_Used_ReservedMemory;
        const auto nNeed = nMaxRequire - cnt;
        const auto nGet = min(nReserved, nNeed);
        _CompileHint(nReserved == m_nUnits);
        _CompileHint(0 < nGet);

        TMemoryObject* pReserved_F;
        TMemoryObject* pReserved_L;
        if(!m_bUse_Header_per_Unit)
        {
            const size_t RealUnitSize = m_UnitSize;
            byte* pFocus = m_pStartAddress + (RealUnitSize * (size_t)m_Counting_Used_ReservedMemory);
            pReserved_F = pReserved_L = (TMemoryObject*)pFocus;
            for(UINT32 i=0; i<nGet; i++)
            {
                pReserved_L = (TMemoryObject*)pFocus; // current
                pFocus += RealUnitSize;
                pReserved_L->pNext = (TMemoryObject*)pFocus;
            }
        }
        else
        {
            const size_t HeaderSize = sizeof(TDATA_BLOCK_HEADER);
            const size_t UnitSize = m_UnitSize;
            const size_t RealUnitSize = HeaderSize + UnitSize;
            byte* pFocus = m_pStartAddress + (RealUnitSize * (size_t)m_Counting_Used_ReservedMemory);
            pReserved_F = pReserved_L = (TMemoryObject*)(pFocus+HeaderSize);
            for(UINT32 i=0; i<nGet; i++)
            {
                TDATA_BLOCK_HEADER* pH = (TDATA_BLOCK_HEADER*)pFocus;
                *pH = *m_pFirstDataBlockHeader;

                pReserved_L = (TMemoryObject*)(pFocus + HeaderSize); // current
                pFocus += RealUnitSize;

                TMemoryObject* pNextUnit = (TMemoryObject*)(pFocus + HeaderSize);
                pReserved_L->pNext = pNextUnit;
            }
        }
        _CompileHint(pReserved_F && pReserved_L);
        pReserved_L->pNext = nullptr;
    #if _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW
        for(TMemoryObject* p = pReserved_F; p; p = p->pNext)
        {
            MemoryPool_UTIL::sFN_Debug_Overflow_Set(p+1, m_UnitSize-sizeof(TMemoryObject), 0xDD);
        }
    #endif

        m_Counting_Used_ReservedMemory += nGet;
        m_nUnits -= nGet;
        cnt += nGet;

        if(pResultL)
        {
            pResultL->pNext = pReserved_F;
            *_OUT_pF = pResultF;
            *_OUT_pL = pReserved_L;
        }
        else
        {
            *_OUT_pF = pReserved_F;
            *_OUT_pL = pReserved_L;
        }
        return cnt;
    }
#endif
#pragma warning(pop)
    void TMemoryUnitsGroup::mFN_Push(TMemoryObject* pF, TMemoryObject* pL, UINT32 nUnits)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(pF && pL && !pL->pNext && 0 < nUnits);
        // pF, pL, nUnits 는 신뢰한다
        // 사용자는 가능한 pF ~ pL 를 주소 오름차순 정렬 시켜 반납해야 한다

        m_nUnits += nUnits;
        _Assert(m_nUnits <= m_nTotalUnits);
        if(mFN_Query_isFull())
        {
            mFN_Sort_Units();
            m_pOwner->mFN_Counting_FullGroup_Increment();
            return;
        }
        if(sc_bUse_Push_Front)
        {
            // Push Front
            pL->pNext = m_Units.m_pUnit_F;
            if(m_Units.m_pUnit_F)
            {
                m_Units.m_pUnit_F = pF;
            }
            else
            {
                m_Units.m_pUnit_F = pF;
                m_Units.m_pUnit_B = pL;
            }
        }
        else
        {
            // Push Back
            if(m_Units.m_pUnit_B)
            {
                m_Units.m_pUnit_B->pNext = pF;
                m_Units.m_pUnit_B = pL;
            }
            else
            {
                m_Units.m_pUnit_F = pF;
                m_Units.m_pUnit_B = pL;
            }
        }
    #pragma warning(pop)
    }
    const byte* TMemoryUnitsGroup::mFN_Query_Valid_Memory_Start() const
    {
        return m_pStartAddress;
    }
    const byte* TMemoryUnitsGroup::mFN_Query_Valid_Memory_End() const
    {
        const byte* pS = mFN_Query_Valid_Memory_Start();

        size_t RealUnitSize = m_UnitSize;
        if(m_bUse_Header_per_Unit)
            RealUnitSize += sizeof(TDATA_BLOCK_HEADER);
        const size_t TotalSize = RealUnitSize * m_nTotalUnits;
        const byte* pEndAddress = pS + TotalSize;

        return pEndAddress;
    }
    BOOL TMemoryUnitsGroup::mFN_Test_Ownership(TMemoryObject* pUnit) const
    {
        //테스트는 일반적으로 통과할 것으로 기대한다
        _Assert(pUnit);

        const byte* pS = mFN_Query_Valid_Memory_Start();
        const byte* pE = mFN_Query_Valid_Memory_End();
        
        const byte* pUnit_Focus = (const byte*)pUnit;
        return (pS <= pUnit_Focus) & (pUnit_Focus < pE);
    }
    void TMemoryUnitsGroup::mFN_DebugTest__IsNotFull() const
    {
    #if _DEF_USING_MEMORYPOOL_DEBUG
        _Assert(!mFN_Query_isFull() && m_nUnits < m_nTotalUnits);
        switch(m_State)
        {
        case TMemoryUnitsGroup::EState::E_Full:
            _Assert(mFN_Query_isFull());
            break;
        case TMemoryUnitsGroup::EState::E_Rest:
            break;
        case TMemoryUnitsGroup::EState::E_Empty:
            //_Assert(pCon->mFN_Query_isEmpty()); 최소한의 크기가 확보가 되어야 Empry -> Rest 전환
            break;
        case TMemoryUnitsGroup::EState::E_Busy:
            break;
        default:
            // Unknwon 상태에서는 항상 잠겨 있음
            _DebugBreak("무효한 상태");
        }
    #endif
    }


    TDATA_VMEM::TDATA_VMEM(void* pVirtualAddress, size_t AllocateSize)
        : m_nUnitsGroup(0)
        , m_pUnitsGroups(nullptr)
        , m_pStartAddress(pVirtualAddress)
        , m_SizeAllocate(AllocateSize)
        , m_nAllocatedUnits(0)
        , m_pPrevious(nullptr), m_pNext(nullptr)
        , m_pOwner(nullptr)
        //----------------------------------------------------------------
        , m_cntFullGroup(0)
        , m_IsManagedReservedMemory(FALSE)
        , m_pReservedMemoryManager(nullptr)
    {
    }
    TDATA_VMEM* TDATA_VMEM::sFN_New_VMEM(size_t AllocateSize)
    {
        _CompileHint(0 < AllocateSize);
        if(AllocateSize == GLOBAL::gc_minAllocationSize)
            return GLOBAL::g_VEM_ReserveMemory.mFN_Pop_VMEM();

        void* pStart = ::VirtualAlloc(0, AllocateSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if(pStart)
            return sFN_Create_VMEM(pStart, AllocateSize);
        return nullptr;
    }
    TDATA_VMEM* TDATA_VMEM::sFN_Create_VMEM(void* pAllocated, size_t AllocateSize)
    {
        _CompileHint(pAllocated && 0 < AllocateSize);

        TDATA_BLOCK_HEADER* pH = reinterpret_cast<TDATA_BLOCK_HEADER*>(pAllocated);
        TDATA_VMEM* p = reinterpret_cast<TDATA_VMEM*>(pH+1);

        _MACRO_CALL_CONSTRUCTOR(p, TDATA_VMEM(pAllocated, AllocateSize));
        return p;
    }
    void TDATA_VMEM::sFN_Delete_VMEM(TDATA_VMEM* pThis)
    {
    #pragma warning(push)
    #pragma warning(disable: 6387)
        // TDATA_VMEM::소멸자는 사용하지 않는다
        if(!pThis)
            return;
        _Assert(!pThis->m_pUnitsGroups && pThis->m_pStartAddress && pThis->m_SizeAllocate);

        if(pThis->m_IsManagedReservedMemory)
            GLOBAL::g_VEM_ReserveMemory.mFN_Push_VMEM(pThis);
        else
            ::VirtualFree(pThis->m_pStartAddress, 0, MEM_RELEASE);
        // pThis는 m_pStartAddress 의 부분이기 때문에 이후 사용해서는 안된다
        // pParent 소속이 있다면 처리
    #pragma warning(pop)
    }
    BOOL TDATA_VMEM::mFN_Lock_All_UnitsGroup(BOOL bCheck_FullUnits)
    {
        UINT32 iFailed=0;
        for(UINT32 i=0; i<m_nUnitsGroup; i++)
        {
            TMemoryUnitsGroup& u = m_pUnitsGroups[i];
            if(!u.m_Lock.Lock__NoInfinite(0xFFFF))
            {
                iFailed = i;
                break;
            }
            if(bCheck_FullUnits && !u.mFN_Query_isFull())
            {
                u.m_Lock.UnLock();
                iFailed = i;
                break;
            }
        }
        if(0 == iFailed)
            return TRUE;

        // 실패시 성공한 잠금을 모두 해제
        for(UINT32 i=0; i<iFailed; i++)
        {
            TMemoryUnitsGroup& u = m_pUnitsGroups[i];
            u.m_Lock.UnLock();
        }
        return FALSE;
    }
    void TDATA_VMEM::mFN_UnLock_All_UnitsGroup()
    {
        for(UINT32 i=0; i<m_nUnitsGroup; i++)
        {
            TMemoryUnitsGroup& u = m_pUnitsGroups[i];
            u.m_Lock.UnLock();
        }
    }


    size_t TDATA_VMEM::mFN_Query_CountUnits__Keeping()
    {
        if(!m_nUnitsGroup)
            return 0;

        size_t cntHave = 0;
        for(size_t i=0; i<m_nUnitsGroup; i++)
        {
            auto& ref = m_pUnitsGroups[i];
            const BOOL bKey = ref.m_Lock.Lock__NoInfinite(0xffff); // 만약 잠금을 실패한다면 그냥 값을 읽는다
            if(!bKey)
                _ReadWriteBarrier();

            cntHave += ref.m_nUnits;

            if(bKey)
                ref.m_Lock.UnLock();
        }
        return cntHave;
    }
    size_t TDATA_VMEM::mFN_Query_CountUnits__Using()
    {
        return m_nAllocatedUnits - mFN_Query_CountUnits__Keeping();
    }
    TDATA_BLOCK_HEADER* TDATA_VMEM::mFN_Get_FirstHeader()
    {
        return (TDATA_BLOCK_HEADER*)m_pStartAddress;
    }
    const TDATA_BLOCK_HEADER* TDATA_VMEM::mFN_Get_FirstHeader() const
    {
        return (TDATA_BLOCK_HEADER*)m_pStartAddress;
    }
    //----------------------------------------------------------------
    CMemoryBasket::CMemoryBasket(size_t _indexProcessor)
        : m_pAttached_Group(nullptr)
        , m_MaskID((size_t)1 << _indexProcessor)
        , m_MaskID_Inverse(~((size_t)1 << _indexProcessor))
        #if _DEF_USING_MEMORYPOOL_DEBUG
        , m_stats_cntGet(0)
        , m_stats_cntRet(0)
        , m_stats_cntCacheMiss_from_Get_Basket(0)
        , m_stats_cntRequestShareUnitsGroup(0)
        #endif
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();
    }
    BOOL CMemoryBasket::mFN_Attach(TMemoryUnitsGroup* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(m_MaskID <= ((size_t)1 << (GLOBAL::g_nProcessor-1)));

        _Assert(p);
        _Assert(p->m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(!p->mFN_Query_isManaged_from_Container());
        _Assert(p->m_State == TMemoryUnitsGroup::EState::E_Busy || p->m_State == TMemoryUnitsGroup::EState::E_Unknown);

        if(p->m_ReferenceMask & m_MaskID)
                return TRUE;

        BOOL bAttached;
        m_Lock.Lock__Busy();
        _Assert(m_pAttached_Group != p);
        if(!m_pAttached_Group)
        {
            _Assert(!p->mFN_Query_isAttached_to_Processor(m_MaskID));

            p->m_State = TMemoryUnitsGroup::EState::E_Busy;
            p->m_ReferenceMask |= m_MaskID;

            m_pAttached_Group = p;
            bAttached = TRUE;
        }
        else
        {
            bAttached = FALSE;
        }
        m_Lock.UnLock();

        return bAttached;
    #pragma warning(pop)
    #pragma warning(disable: 6011)
    }
    BOOL CMemoryBasket::mFN_Detach(TMemoryUnitsGroup* pCurrent)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(pCurrent);
        _Assert(pCurrent->m_Lock.Query_isLocked());

        if(0 == (pCurrent->m_ReferenceMask & m_MaskID))
                return FALSE;

        BOOL bDetached;
        m_Lock.Lock__Busy();
        _Assert(pCurrent->m_ReferenceMask & m_MaskID);
        if(m_pAttached_Group == pCurrent)
        {
            _Assert(pCurrent->mFN_Query_isAttached_to_Processor(m_MaskID));

            m_pAttached_Group = nullptr;

            pCurrent->m_ReferenceMask &= m_MaskID_Inverse;
            if(pCurrent->mFN_Query_isInvalidState())
                ;
            else if(!pCurrent->mFN_Query_isAttached_to_Processor())
                pCurrent->m_State = TMemoryUnitsGroup::EState::E_Unknown;

            bDetached = TRUE;
        }
        else
        {
            _DebugBreak(_T("Not Attached %p %p"), m_pAttached_Group, pCurrent);
            bDetached = FALSE;
            pCurrent->m_ReferenceMask &= m_MaskID_Inverse;
        }
        m_Lock.UnLock();

        return bDetached;
    #pragma warning(pop)
    }



#pragma region DATA_BLOCK and Table
    BOOL TDATA_BLOCK_HEADER::operator == (const TDATA_BLOCK_HEADER& r) const
    {
        if(this == &r)
            return TRUE;
        if(0 == ::memcmp(this, &r, sizeof(TDATA_BLOCK_HEADER)))
            return TRUE;
        else
            return FALSE;
    }
    BOOL TDATA_BLOCK_HEADER::operator != (const TDATA_BLOCK_HEADER& r) const
    {
        if(this == &r)
            return FALSE;
        if(0 == ::memcmp(this, &r, sizeof(TDATA_BLOCK_HEADER)))
            return FALSE;
        else
            return TRUE;
    }

    CTable_DataBlockHeaders::CTable_DataBlockHeaders()
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();

        CObjectPool_Handle_TSize<sc_MaxSlot_TableLow*sizeof(void*)>::Reference_Attach();
        CObjectPool_Handle<CTable_DataBlockHeaders::_TYPE_Link_Recycle_Slots>::Reference_Attach();
    }
    CTable_DataBlockHeaders::~CTable_DataBlockHeaders()
    {
        // 자원 삭제
        for(UINT32 i=0; i<m_cnt_Index_High; i++)
        {
            CObjectPool_Handle_TSize<sc_MaxSlot_TableLow*sizeof(void*)>::Free(m_ppTable[i]);
            m_ppTable[i] = nullptr;
        }

        // 빈슬롯 정보 삭제
        while(m_pRecycleSlots)
            mFN_Delete_Last_LinkFreeSlots();

        CObjectPool_Handle_TSize<sc_MaxSlot_TableLow*sizeof(void*)>::Reference_Detach();
        CObjectPool_Handle<CTable_DataBlockHeaders::_TYPE_Link_Recycle_Slots>::Reference_Detach();
    }
    BOOL CTable_DataBlockHeaders::mFN_Insert_Recycle_Slots(UINT16 h, UINT16 l)
    {
        if(!m_pRecycleSlots || m_pRecycleSlots->cnt == CTable_DataBlockHeaders::_TYPE_Link_Recycle_Slots::s_maxIndexes)
        {
            CTable_DataBlockHeaders::_TYPE_Link_Recycle_Slots* pN =
                (CTable_DataBlockHeaders::_TYPE_Link_Recycle_Slots*)CObjectPool_Handle<CTable_DataBlockHeaders::_TYPE_Link_Recycle_Slots>::Alloc();
            if(!pN) return FALSE;

            pN->cnt = 0;
            pN->pPrevious = m_pRecycleSlots;
            m_pRecycleSlots = pN;


            _MACRO_STATIC_ASSERT(sizeof(_TYPE_Link_Recycle_Slots) == _DEF_CACHELINE_SIZE);
            #if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
            if(pN->pPrevious)
                _mm_clflush(pN->pPrevious);
            #endif
        }
        auto& save = m_pRecycleSlots->m_Indexes[m_pRecycleSlots->cnt++];
        save.h = h;
        save.l = l;
        return TRUE;
    }
    void CTable_DataBlockHeaders::mFN_Delete_Last_LinkFreeSlots()
    {
        if(!m_pRecycleSlots)
            return;

        auto pDel = m_pRecycleSlots;
        m_pRecycleSlots = m_pRecycleSlots->pPrevious;

        CObjectPool_Handle<_TYPE_Link_Recycle_Slots>::Free(pDel);
    }
    // mFN_Register, mFN_UnRegister 주의사항
    // m_cnt_TotalSlot 는 마지막에 업데이트
    void CTable_DataBlockHeaders::mFN_Register(TDATA_BLOCK_HEADER* p)
    {
        m_Lock.Lock__Busy();
        {
            BOOL bIncrementCNT = FALSE;

            UINT32 iNowH, iNowL;
            if(m_pRecycleSlots && 0 < m_pRecycleSlots->cnt)
            {
                m_pRecycleSlots->cnt--;
                iNowH = m_pRecycleSlots->m_Indexes[m_pRecycleSlots->cnt].h;
                iNowL = m_pRecycleSlots->m_Indexes[m_pRecycleSlots->cnt].l;
                if(0 == m_pRecycleSlots->cnt)
                    mFN_Delete_Last_LinkFreeSlots();
            }
            else
            {
                bIncrementCNT = TRUE;

                const auto cntPrev = m_cnt_TotalSlot;
                iNowH = cntPrev / sc_MaxSlot_TableLow;
                iNowL = cntPrev % sc_MaxSlot_TableLow;
            }

            _Assert(m_cnt_Index_High >= iNowH);
            if(m_cnt_Index_High == iNowH)
            {
                m_ppTable[iNowH] = (TDATA_BLOCK_HEADER**)CObjectPool_Handle_TSize<sc_MaxSlot_TableLow*sizeof(void*)>::Alloc();
                ::memset(m_ppTable[iNowH], 0, sc_MaxSlot_TableLow*sizeof(void*));
                m_cnt_Index_High++;
            }

            _Assert(nullptr == m_ppTable[iNowH][iNowL]);
            m_ppTable[iNowH][iNowL] = p;
            p->m_Index.high = (UINT16)iNowH;
            p->m_Index.low  = (UINT16)iNowL;

            if(bIncrementCNT)
                m_cnt_TotalSlot++;
        }
        m_Lock.UnLock();
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CTable_DataBlockHeaders::mFN_UnRegister(TDATA_BLOCK_HEADER* p)
    {
        if(!mFN_Query_isRegistered(p))
            return;

        const auto h = p->m_Index.high;
        const auto l = p->m_Index.low;

        // p == pTrust
        // 제거하는 비용은 매우 비싸다
        m_Lock.Lock__Busy();
        {
            // 제거는 어떻게 할 것인가??
            //      전제:
            //          만들어진 헤더가 여기에 테이블에 주소로 단순 링크되어 지는데
            //          헤더의 링크 인덱스는 여기에 등록된 번호 이다
            //  방법1(불가능)
            //      여기서 만약 삭제하고 뒤에 있던것을 끼워 넣게 되면..
            //      다중스레드에서 문제가 된다(헤더의 전역 인덱스조차 잠금을 걸고 읽어야 한다)
            //
            //  방법2(가능)
            //      전역에 동적메모리를(오브젝트풀)을 이용하여 테이블(NormalSize가 아닌)의 카운팅보다 적은 앞의 사용가능한 인덱스들을 보관하여
            //      즉시 등록가능 하도록하는것
            //      문제점 : 추가적인 메모리의 소모
            //      mFN_Register 또한 빈슬롯이 있는 경우 사용하도록 수정해야 함
            //  -> 방법2를 택한다
            mFN_Insert_Recycle_Slots(h, l);
            m_ppTable[h][l] = nullptr;
        }
        m_Lock.UnLock();
    }
    BOOL CTable_DataBlockHeaders::mFN_Query_isRegistered(const TDATA_BLOCK_HEADER * p)
    {
        if(!p) return FALSE;
        const auto h = p->m_Index.high;
        const auto l = p->m_Index.low;
        if(sc_MaxSlot_TableHigh <= h || sc_MaxSlot_TableLow <= l)
        {
            _DebugBreak("Failed : CTable_DataBlockHeaders::mFN_UnRegister\nif(sc_MaxSlot_TableHigh <= high || sc_MaxSlot_TableLow <= low)");
            return FALSE;
        }
        if(m_cnt_TotalSlot <= h * sc_MaxSlot_TableLow + l)
        {
            _DebugBreak("Failed : CTable_DataBlockHeaders::mFN_UnRegister\nif(m_cnt_TotalSlot <= high * sc_MaxSlot_TableLow + low)");
            return FALSE;
        }

        auto& pTrust = m_ppTable[h][l];
        if(!pTrust || pTrust != p)
        {
            _DebugBreak("Failed : CTable_DataBlockHeaders::mFN_UnRegister\nif(!pTrust || pTrust != p)");
            return FALSE;
        }

        return TRUE;
    }
#pragma endregion

#pragma region CMemoryPool_Allocator_Virtual
    CMemoryPool_Allocator_Virtual::CMemoryPool_Allocator_Virtual(size_t _TypeSize, UINT32 _IndexThisPool, CMemoryPool* pPool, BOOL _bUse_Header_per_Unit)
        : m_pPool(pPool)
        , m_SETTING(sFN_Initialize_SETTING(_TypeSize, _IndexThisPool, pPool, _bUse_Header_per_Unit))
        //----------------------------------------------------------------
        , m_LastKnownValue_VMEM_CACHE__MaxStorageMemorySize(0)
        , m_nRecycle_UnitsGroupsSmall(0u)
        , m_nRecycle_UnitsGroupsBig(0u)
        //----------------------------------------------------------------
        , m_pVMEM_First(nullptr)
        , m_stats_cnt_Succeed_VirtualAlloc(0)
        , m_stats_cnt_Succeed_VirtualFree(0)
        , m_stats_size_TotalAllocated(0)
        , m_stats_size_TotalDeallocate(0)
        , m_stats_Current_AllocatedSize(0)
        , m_stats_Max_AllocatedSize(0)
        , m_pVMEM_CACHE__LastAccess(nullptr)
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();

        _MACRO_STATIC_ASSERT(CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_All_SlotSize > CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_All_AllocCount);
        _MACRO_STATIC_ASSERT(CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_All_SlotSize > CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_All_FreeCount);
        _MACRO_STATIC_ASSERT(CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_Small_SlotSize > CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_Small_AllocCount);
        _MACRO_STATIC_ASSERT(CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_Small_SlotSize > CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_Small_FreeCount);
        _MACRO_STATIC_ASSERT(CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_Big_SlotSize > CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_Big_AllocCount);
        _MACRO_STATIC_ASSERT(CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_Big_SlotSize > CMemoryPool_Allocator_Virtual::TRecycleSlot_UnitsGroups::sc_Big_FreeCount);

        ::memset(&m_RecycleSlot_UnitsGroups, 0, sizeof(m_RecycleSlot_UnitsGroups));

        // 할당단위별 유닛그룹수 유효성 확인
        _MACRO_STATIC_ASSERT(_MACRO_ARRAY_COUNT(gpFN_Array_MemoryUnitsGroup_Alloc) == _MACRO_ARRAY_COUNT(gpFN_Array_MemoryUnitsGroup_Free));
        {
            const auto nS = m_SETTING.m_VMEM_Small.m_nGroups_per_VMEM, nB = m_SETTING.m_VMEM_Big.m_nGroups_per_VMEM;
            _AssertRelease(nS < _MACRO_ARRAY_COUNT(gpFN_Array_MemoryUnitsGroup_Alloc));
            _AssertRelease(nB < _MACRO_ARRAY_COUNT(gpFN_Array_MemoryUnitsGroup_Alloc));
            const auto ppFN_Alloc = gpFN_Array_MemoryUnitsGroup_Alloc;
            const auto ppFN_Free = gpFN_Array_MemoryUnitsGroup_Free;
            _AssertRelease(ppFN_Alloc[nS] && ppFN_Free[nS]);
            _AssertRelease(ppFN_Alloc[nB] && ppFN_Free[nB]);
        }

        //const UTIL::ISystem_Information* pSys = CORE::g_instance_CORE.mFN_Get_System_Information();
        //const SYSTEM_INFO* pInfo = pSys->mFN_Get_SystemInfo();
        //m_ReserveUnitSize   = pInfo->dwAllocationGranularity;   // 예약단위 보통 64KB
        //m_AllocUnitSize     = pInfo->dwPageSize;                // 할당단위 보통 4KB
    }
    CMemoryPool_Allocator_Virtual::~CMemoryPool_Allocator_Virtual()
    {
        mFN_Release_All_VirtualMemory();
        mFN_RecycleSlot_All_Free();
    }
    void CMemoryPool_Allocator_Virtual::mFN_Release_All_VirtualMemory()
    {
        m_pPool->m_Lock_Pool.Begin_Write__INFINITE();
        while(m_pVMEM_First)
        {
            auto pDelete = m_pVMEM_First;

            mFN_UnRegister_VMEM(pDelete);
            
            const auto pUG = pDelete->m_pUnitsGroups;
            const auto nUG = pDelete->m_nUnitsGroup;
            pDelete->mFN_Lock_All_UnitsGroup(FALSE);
            if(!mFN_VMEM_Destroy_UnitsGroups_Step1__LockFree(*pDelete))
                continue;

            mFN_VMEM_Destroy_UnitsGroups_Step2(pUG, nUG);
            mFN_VMEM_Delete(pDelete);
        }
        m_pPool->m_Lock_Pool.End_Write();
    }
    void CMemoryPool_Allocator_Virtual::mFN_RecycleSlot_All_Free()
    {
        if(m_SETTING.m_bUse_Header_per_Unit)
        {
            if(0 < m_nRecycle_UnitsGroupsSmall)
            {
                const auto nUG = m_SETTING.m_VMEM_Small.m_nGroups_per_VMEM;
                gpFN_Array_MemoryUnitsGroup_Free[nUG](m_RecycleSlot_UnitsGroups.m_Small, m_nRecycle_UnitsGroupsSmall);
                m_nRecycle_UnitsGroupsSmall = 0;
            }
            if(0 < m_nRecycle_UnitsGroupsBig)
            {
                const auto nUG = m_SETTING.m_VMEM_Big.m_nGroups_per_VMEM;
                gpFN_Array_MemoryUnitsGroup_Free[nUG](m_RecycleSlot_UnitsGroups.m_Big, m_nRecycle_UnitsGroupsBig);
                m_nRecycle_UnitsGroupsBig = 0;
            }
        }
        else
        {
            if(0 < m_nRecycle_UnitsGroupsAll)
            {
                const auto nUG = m_SETTING.m_VMEM_Small.m_nGroups_per_VMEM;
                gpFN_Array_MemoryUnitsGroup_Free[nUG](m_RecycleSlot_UnitsGroups.m_All, m_nRecycle_UnitsGroupsAll);
                m_nRecycle_UnitsGroupsAll = 0;
            }
        }
        ::memset(&m_RecycleSlot_UnitsGroups, 0, sizeof(m_RecycleSlot_UnitsGroups));
    }
    DECLSPEC_NOINLINE size_t CMemoryPool_Allocator_Virtual::mFN_MemoryPredict_Add(size_t _Byte, BOOL bWriteLog)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        const size_t ret = mFN_Update__MaxAllocatedSize_Add(_Byte);

        if(bWriteLog)
            _LOG_DEBUG(_T("MemoryPool[%u] : User Request PredictAdd = %IuKB(%.2fMB)"), m_SETTING.m_UnitSize, _Byte/1024, _Byte/1024./1024.);

        return ret;
    }
    DECLSPEC_NOINLINE size_t CMemoryPool_Allocator_Virtual::mFN_MemoryPredict_Set(size_t _Byte, BOOL bWriteLog)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        const size_t ret = mFN_Update__MaxAllocatedSize_Set(_Byte);

        if(bWriteLog)
            _LOG_DEBUG(_T("MemoryPool[%u] : User Request PredictSet = %IuKB(%.2fMB)"), m_SETTING.m_UnitSize, _Byte/1024, _Byte/1024./1024.);

        return ret;
    }
    DECLSPEC_NOINLINE size_t CMemoryPool_Allocator_Virtual::mFN_MemoryPreAlloc_Add(size_t _Byte, BOOL bWriteLog)
    {
        _Assert(m_pPool->mFN_Query_isLocked());

        if(bWriteLog)
            _LOG_DEBUG(_T("MemoryPool[%u] : User Request PreAllocAdd = %uKB(%.2fMB)"), m_SETTING.m_UnitSize, _Byte/1024, _Byte/1024./1024.);

        return mFN_MemoryPreAlloc(_Byte, bWriteLog);
    }
    DECLSPEC_NOINLINE size_t CMemoryPool_Allocator_Virtual::mFN_MemoryPreAlloc_Set(size_t _Byte, BOOL bWriteLog)
    {
        _Assert(m_pPool->mFN_Query_isLocked());

        if(bWriteLog)
            _LOG_DEBUG(_T("MemoryPool[%u] : User Request PreAllocSet = %uKB(%.2fMB)"), m_pPool->m_UnitSize, _Byte/1024, _Byte/1024./1024.);

        if(m_stats_Current_AllocatedSize < _Byte)
            mFN_MemoryPreAlloc(_Byte - m_stats_Current_AllocatedSize, bWriteLog);
        else if(bWriteLog)
            _LOG_DEBUG(_T("MemoryPool[%u] : User Request PreAllocSet -> Invalid Request : Current AllocatedSize = %uKB(%.2fMB)")
                , m_pPool->m_UnitSize, m_stats_Current_AllocatedSize/1024, m_stats_Current_AllocatedSize/1024./1024.);

        return m_stats_Current_AllocatedSize;
    }
    TDATA_VMEM* CMemoryPool_Allocator_Virtual::mFN_Expansion()
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        _Assert(m_stats_Current_AllocatedSize <= m_stats_Max_AllocatedSize);

        // 할당 크기 결정
        const TMemoryPoolAllocator_Virtual__SettingVMEM* pSetting_VMEM; 
        if(!m_SETTING.m_bUse_Header_per_Unit)
        {
            pSetting_VMEM = &m_SETTING.m_VMEM_Small;
        }
        else
        {
            size_t SizeGoal = m_stats_Current_AllocatedSize + m_SETTING.m_VMEM_Big.m_MemoryExpansionSize;
            if(SizeGoal <= m_stats_Max_AllocatedSize)
                pSetting_VMEM = &m_SETTING.m_VMEM_Big;
            else
                pSetting_VMEM = &m_SETTING.m_VMEM_Small;
        }

        TDATA_VMEM* ret = mFN_Expansion_PRIVATE(*pSetting_VMEM);
        if(ret)
            return ret;

        if(m_SETTING.m_bUse_Header_per_Unit && pSetting_VMEM == &m_SETTING.m_VMEM_Big)
            ret = mFN_Expansion_PRIVATE(m_SETTING.m_VMEM_Small);

        return ret;
    }
    namespace{
        DECLSPEC_NOINLINE void gFN_Report_Failed_MemoryPoolExpansion(size_t UnitSize, size_t AllocSize, const TDATA_VMEM* pVMEM)
        {
        #if _DEF_USING_MEMORYPOOL_DEBUG
            if(GLOBAL::g_bDebug__Report_OutputDebug)
            {
                _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("0x%p = MemoryPool[%Iu] Expansion : Request(%IuKB %.3fMB) ThreadID(%u)\n"
                    "\tOut of Memory - Object Pool\n"
                    , pVMEM
                    , UnitSize
                    , AllocSize / 1024
                    , AllocSize / 1024. / 1024.
                    , ::GetCurrentThreadId());
            }
        #else
            UNREFERENCED_PARAMETER(UnitSize);
            UNREFERENCED_PARAMETER(AllocSize);
            UNREFERENCED_PARAMETER(pVMEM);
        #endif
        }
    }


    DECLSPEC_NOINLINE TDATA_VMEM* CMemoryPool_Allocator_Virtual::mFN_Expansion_PRIVATE(const TMemoryPoolAllocator_Virtual__SettingVMEM& setting)
    {
        const size_t _AllocSize = setting.m_MemoryExpansionSize;
        _Assert(m_SETTING.m_bUse_Header_per_Unit || (!m_SETTING.m_bUse_Header_per_Unit && _AllocSize == GLOBAL::gc_minAllocationSize));
        _Assert(_AllocSize > (sizeof(TDATA_BLOCK_HEADER) + sizeof(TDATA_VMEM)));

        TDATA_VMEM* pVMEM = nullptr;
        if(m_pVMEM_CACHE__LastAccess && _AllocSize == m_pVMEM_CACHE__LastAccess->mFN_Query__Size_per_VMEM())
        {
            // 옮바른 VMEM CACHE가 연결되어 있음
            pVMEM = m_pVMEM_CACHE__LastAccess->mFN_Pop();
        }
        else
        {
            m_pVMEM_CACHE__LastAccess = GLOBAL::g_VMEM_Recycle.mFN_Find_VMEM_CACHE(_AllocSize);
            if(m_pVMEM_CACHE__LastAccess)
                pVMEM = m_pVMEM_CACHE__LastAccess->mFN_Pop();
        }

        if(!pVMEM)
        {
            pVMEM = mFN_VMEM_New(_AllocSize);
            if(!pVMEM)
            {
                gFN_Report_Failed_MemoryPoolExpansion(m_SETTING.m_UnitSize, _AllocSize, nullptr);
                return nullptr;
            }
        }
        _Assert(_AllocSize == pVMEM->m_SizeAllocate);

        // 유닛수 계산(사이즈는 _Size 가 아닌 VMEM의 값을 사용한다(차후 기능 확장을 위해)
        if(!mFN_VMEM_Build_UnitsGroups(*pVMEM, setting))
        {
            gFN_Report_Failed_MemoryPoolExpansion(m_SETTING.m_UnitSize, _AllocSize, pVMEM);

            if(m_pVMEM_CACHE__LastAccess)
            {
                m_pVMEM_CACHE__LastAccess->mFN_Push(pVMEM);
            }
            else
            {
                mFN_VMEM_Delete(pVMEM);
            }
            return nullptr;
        }

        // Succeed
        mFN_Register_VMEM(pVMEM);
        return pVMEM;
    }
    DECLSPEC_NOINLINE size_t CMemoryPool_Allocator_Virtual::mFN_MemoryPreAlloc(size_t _Byte, BOOL bWriteLog)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        size_t TotalAllocated = 0;
        if(!m_SETTING.m_bUse_Header_per_Unit)
        {
            const size_t Chunk = m_SETTING.m_VMEM_Small.m_MemoryExpansionSize;
            for(::UTIL::NUMBER::TCounting_UT remainder = _Byte; 0 < remainder; )
            {
                auto pVMEM = mFN_Expansion_PRIVATE(m_SETTING.m_VMEM_Small);
                if(!pVMEM)
                    break;

                m_pPool->mFN_Request_Connect_UnitsGroup_Step1(nullptr, pVMEM->m_nAllocatedUnits);
                m_pPool->mFN_Request_Connect_UnitsGroup_Step2__LockFree(pVMEM->m_pUnitsGroups, pVMEM->m_nUnitsGroup);

                remainder -= Chunk;
                TotalAllocated += Chunk;
            }
        }
        else
        {
            const size_t Chunk_S = m_SETTING.m_VMEM_Small.m_MemoryExpansionSize;
            const size_t Chunk_B = m_SETTING.m_VMEM_Big.m_MemoryExpansionSize;;
            for(::UTIL::NUMBER::TCounting_UT remainder = _Byte; 0 < remainder; )
            {
                if(remainder >= Chunk_B)
                {
                    //# Chunk_B 시도
                    auto pVMEM = mFN_Expansion_PRIVATE(m_SETTING.m_VMEM_Big);
                    if(pVMEM)
                    {
                        m_pPool->mFN_Request_Connect_UnitsGroup_Step1(nullptr, pVMEM->m_nAllocatedUnits);
                        m_pPool->mFN_Request_Connect_UnitsGroup_Step2__LockFree(pVMEM->m_pUnitsGroups, pVMEM->m_nUnitsGroup);

                        remainder -= Chunk_B;
                        TotalAllocated += Chunk_B;
                        continue;
                    }
                    else
                    {
                        // Chunk_B가 실패한다면, Chunk_S로 시도한다
                        // 이런 상황의 원인은 x86의 메모리 주소 단편화가 유력하다
                    }
                }

                //# Chunk_S 시도
                auto pVMEM = mFN_Expansion_PRIVATE(m_SETTING.m_VMEM_Small);
                if(!pVMEM)
                    break;

                m_pPool->mFN_Request_Connect_UnitsGroup_Step1(nullptr, pVMEM->m_nAllocatedUnits);
                m_pPool->mFN_Request_Connect_UnitsGroup_Step2__LockFree(pVMEM->m_pUnitsGroups, pVMEM->m_nUnitsGroup);

                remainder -= Chunk_S;
                TotalAllocated += Chunk_S;
            }
        }

        if(bWriteLog)
            _LOG_DEBUG(_T("MemoryPool[%u] : PreAlloc Result -> Added Memory = %uKB(%.2fMB)"), m_pPool->m_UnitSize, TotalAllocated/1024, TotalAllocated/1024./1024.);

        return TotalAllocated;
    }
    void CMemoryPool_Allocator_Virtual::mFN_Register_VMEM(TDATA_VMEM* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(m_pPool->mFN_Query_isLocked());
        _Assert(p && !p->m_pOwner);

        p->m_pOwner = this;

        p->m_pPrevious = nullptr;
        p->m_pNext = m_pVMEM_First;

        if(m_pVMEM_First)
            m_pVMEM_First->m_pPrevious = p;

        m_pVMEM_First = p;


        m_stats_Current_AllocatedSize += p->m_SizeAllocate;
        mFN_Update__MaxAllocatedSize_Set(m_stats_Current_AllocatedSize);

        m_stats_size_TotalAllocated += p->m_SizeAllocate;

    #if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
        {
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("0x%p = MemoryPool[%Iu] Expansion : Request(%IuKB %.3fMB) G[%u] ThreadID(%u)\n"
                , p
                , m_SETTING.m_UnitSize
                , p->m_SizeAllocate / 1024
                , p->m_SizeAllocate / 1024. / 1024.
                , p->m_nUnitsGroup
                , ::GetCurrentThreadId());
        }
    #endif
    #pragma warning(pop)
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CMemoryPool_Allocator_Virtual::mFN_UnRegister_VMEM(TDATA_VMEM* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(m_pPool->mFN_Query_isLocked());
        _Assert(p && p->m_pOwner == this);

    #if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
        {
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("MemoryPool[%Iu] Reduction : 0x%p (%IuKB %.3fMB) ThreadID(%u)\n"
                , m_SETTING.m_UnitSize
                , p->m_pStartAddress
                , p->m_SizeAllocate / 1024
                , p->m_SizeAllocate / 1024. / 1024.
                , ::GetCurrentThreadId());
        }
    #endif

        p->m_pOwner = nullptr;

        auto pPrev = p->m_pPrevious;
        auto pNext = p->m_pNext;
        if(pPrev)
            pPrev->m_pNext = pNext;
        if(pNext)
            pNext->m_pPrevious = pPrev;

        if(p == m_pVMEM_First)
            m_pVMEM_First = pNext;


        _Assert(m_stats_Current_AllocatedSize >= p->m_SizeAllocate);
        m_stats_Current_AllocatedSize -= p->m_SizeAllocate;

        m_stats_size_TotalDeallocate += p->m_SizeAllocate;
    #pragma warning(pop)
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT TDATA_VMEM* CMemoryPool_Allocator_Virtual::mFN_VMEM_New(size_t _AllocSize)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        _Assert(0 < _AllocSize);

        TDATA_VMEM* pReturn = TDATA_VMEM::sFN_New_VMEM(_AllocSize);
        if(!pReturn)
            return mFN_VMEM_New__Slow(_AllocSize);

        m_stats_cnt_Succeed_VirtualAlloc++;
        return pReturn;
    }
    DECLSPEC_NOINLINE TDATA_VMEM* CMemoryPool_Allocator_Virtual::mFN_VMEM_New__Slow(size_t _AllocSize)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        _Assert(0 < _AllocSize);

        // 시스템에 메모리할당이 불가능할때(주소 또는 물리적메모리) 호출된다
        _DEF_COMPILE_MSG__FOCUS("통계 카운팅을 남길 것인가?");

        TDATA_VMEM* p;
        auto pRecycle = GLOBAL::g_VMEM_Recycle.mFN_Find_VMEM_CACHE(_AllocSize);
        for(;;)
        {
            if(pRecycle)
            {
                p = pRecycle->mFN_Pop();
                if(p)
                    break;
            }

            if(!GLOBAL::g_VMEM_Recycle.mFN_Free_AnythingHeapMemory(_AllocSize))
                return nullptr;

            p = TDATA_VMEM::sFN_New_VMEM(_AllocSize);
            if(p)
                break;
        }
        if(p)
            m_stats_cnt_Succeed_VirtualAlloc++;
        return p;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CMemoryPool_Allocator_Virtual::mFN_VMEM_Delete(TDATA_VMEM* p)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        _Assert(p && !p->m_pOwner);
        if(!p)
            return;

        TDATA_VMEM::sFN_Delete_VMEM(p);
        m_stats_cnt_Succeed_VirtualFree++;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CMemoryPool_Allocator_Virtual::mFN_VMEM_Close(TDATA_VMEM* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(m_pPool->mFN_Query_isLocked());
        _Assert(p && p->m_pOwner == this);

        mFN_UnRegister_VMEM(p);

        if(m_pVMEM_CACHE__LastAccess && p->m_SizeAllocate == m_pVMEM_CACHE__LastAccess->mFN_Query__Size_per_VMEM())
        {
            m_pVMEM_CACHE__LastAccess->mFN_Push(p);
        }
        else
        {
            m_pVMEM_CACHE__LastAccess = GLOBAL::g_VMEM_Recycle.mFN_Find_VMEM_CACHE(p->m_SizeAllocate);
            if(m_pVMEM_CACHE__LastAccess)
                m_pVMEM_CACHE__LastAccess->mFN_Push(p);
            else
                mFN_VMEM_Delete(p);
        }
    #pragma warning(pop)
    }

    _DEF_INLINE_TYPE__PROFILING__DEFAULT BOOL CMemoryPool_Allocator_Virtual::mFN_VMEM_Build_UnitsGroups(TDATA_VMEM& VMEM, const TMemoryPoolAllocator_Virtual__SettingVMEM& settingVMEM)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        const auto _Use_Header_per_Unit = m_SETTING.m_bUse_Header_per_Unit;
        const auto UnitSize = m_SETTING.m_UnitSize;
        const auto nGroups = settingVMEM.m_nGroups_per_VMEM;

        _Assert(1 <= nGroups && nGroups <= 8);
        _Assert(0 < UnitSize);
        _Assert(!VMEM.m_nUnitsGroup && !VMEM.m_pUnitsGroups);

        if(!VMEM.m_pStartAddress || !VMEM.m_SizeAllocate)
            return FALSE;

        //----------------------------------------------------------------
        // 유닛그룹 메모리 확보
        if(!_Use_Header_per_Unit)
        {
            VMEM.m_pUnitsGroups = mFN_Alloc_UnitsGroups_TypeAll();
        }
        else
        {
            if(&settingVMEM == &m_SETTING.m_VMEM_Small)
                VMEM.m_pUnitsGroups = mFN_Alloc_UnitsGroups_TypeSmall();
            else
                VMEM.m_pUnitsGroups = mFN_Alloc_UnitsGroups_TypeBig();
        }
        if(!VMEM.m_pUnitsGroups)
            return FALSE;
        for(UINT32 i=0; i<nGroups; i++)
            _MACRO_CALL_CONSTRUCTOR(VMEM.m_pUnitsGroups+i, TMemoryUnitsGroup(&VMEM, UnitSize, _Use_Header_per_Unit));
        //----------------------------------------------------------------
        // 분배
        VMEM.m_nUnitsGroup = (UINT32)nGroups;
        VMEM.m_cntFullGroup = (UINT32)nGroups;
        const BOOL bSucceed__Distribute_to_UnitsGroup = mFN_VMEM_Distribute_to_UnitsGroups(VMEM, settingVMEM);
        if(!bSucceed__Distribute_to_UnitsGroup)
        {
            const auto pUG = VMEM.m_pUnitsGroups;
            const BOOL bSucceed_Destroy_UnitsGroup = mFN_VMEM_Destroy_UnitsGroups_Step1__LockFree(VMEM);
            mFN_VMEM_Destroy_UnitsGroups_Step2(pUG, nGroups);
            _MACRO_OUTPUT_DEBUG_STRING_TRACE(_T("Failed TDATA_VMEM::FN_Distribute_to_UnitsGroups...\n")
                _T("\tUnitSize(%Iu) UnitsGroups(%Iu) Memory(%IuKB)\n")
                _T("\tDestroy Units Group(%s)\n")
                , UnitSize, nGroups, VMEM.m_SizeAllocate
                , bSucceed_Destroy_UnitsGroup? _T("SUCCEED") : _T("FAILED"));
            UNREFERENCED_PARAMETER(bSucceed_Destroy_UnitsGroup);
            return FALSE;
        }
        //----------------------------------------------------------------
        // 헤더 등록
        // 전역에 등록하는 일은 메모리풀 임계구역내에서 처리해야 한다
        // 첫번째 그룹을 메모리풀에 등록하는 순간, 즉시 사용이 시작되기 때문에
        GLOBAL::g_Table_BlockHeaders_Normal.mFN_Register(VMEM.mFN_Get_FirstHeader());

        return TRUE;
    }
    // 완료시 VMEM의 UnitsGroup , 관련 데이터는 초기화 된다
    // 사용자는 Step2 호출을 위해서 이 함수를 호출전, 다음 값을 보관해 두어야 한다
    // VMEM.m_pUnitsGroups VMEM.m_nUnitsGroup
    _DEF_INLINE_TYPE__PROFILING__DEFAULT BOOL CMemoryPool_Allocator_Virtual::mFN_VMEM_Destroy_UnitsGroups_Step1__LockFree(TDATA_VMEM& VMEM)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        if(!VMEM.m_nAllocatedUnits)
        {
            _Assert(!VMEM.m_nUnitsGroup && !VMEM.m_cntFullGroup && !VMEM.m_pUnitsGroups);
            // 만약 데이터가 손상되었다면 pUnitsGruops 는 메모리 릭
        }
        else
        {
            _Assert(VMEM.m_nUnitsGroup && VMEM.m_cntFullGroup && VMEM.m_pUnitsGroups);
            //----------------------------------------------------------------
            // 유닛그룹 파괴
            BOOL bDetedted_Failed = FALSE;
            const UINT32 nUG = VMEM.m_nUnitsGroup;
            for(UINT32 i=0; i<nUG; i++)
            {
                if(!VMEM.m_pUnitsGroups[i].mFN_Query_isPossibleSafeDestroy())
                    bDetedted_Failed = TRUE;
            }
            if(bDetedted_Failed)
                return FALSE;

            for(size_t i=0; i<nUG; i++)
            {
                auto pUG = VMEM.m_pUnitsGroups + i;

                pUG->m_State = TMemoryUnitsGroup::EState::E_Invalid;
                pUG->m_nUnits = pUG->m_nTotalUnits = 0;
                _MACRO_CALL_DESTRUCTOR(pUG);
            }
        }

        //----------------------------------------------------------------
        // 등록된 헤더 제거
        GLOBAL::g_Table_BlockHeaders_Normal.mFN_UnRegister(VMEM.mFN_Get_FirstHeader());
        //----------------------------------------------------------------
        // 유닛그룹 관련 데이터 초기화
        VMEM.m_nUnitsGroup = VMEM.m_cntFullGroup = 0;
        VMEM.m_pUnitsGroups = nullptr;
        VMEM.m_nAllocatedUnits = 0;
        return TRUE;
    #pragma warning(pop)
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CMemoryPool_Allocator_Virtual::mFN_VMEM_Destroy_UnitsGroups_Step2(TMemoryUnitsGroup* pUnitsGroups, UINT32 nGroups)
    {
        if(!pUnitsGroups)
            return;

        // 유닛그룹 메모리 반납
        if(!m_SETTING.m_bUse_Header_per_Unit)
        {
            if(nGroups == m_SETTING.m_VMEM_Small.m_nGroups_per_VMEM)
            {
                mFN_Free_UnitsGroups_TypeAll(pUnitsGroups);
                return;
            }
        }
        else
        {
            if(nGroups == m_SETTING.m_VMEM_Small.m_nGroups_per_VMEM)
            {
                mFN_Free_UnitsGroups_TypeSmall(pUnitsGroups);
                return;
            }
            else if(nGroups == m_SETTING.m_VMEM_Big.m_nGroups_per_VMEM)
            {
                mFN_Free_UnitsGroups_TypeBig(pUnitsGroups);
                return;
            }
        }

        _Error(_T("Damaged Memory Pool : Invalid Value(%u)"), nGroups);
    }

    _DEF_INLINE_TYPE__PROFILING__DEFAULT BOOL CMemoryPool_Allocator_Virtual::mFN_VMEM_Distribute_to_UnitsGroups(TDATA_VMEM& VMEM, const TMemoryPoolAllocator_Virtual__SettingVMEM& settingVMEM)
    {
        const size_t MemoryCost_Default = GLOBAL::SETTING::gc_SizeMemoryPoolAllocation_DefaultCost;
        byte* pStartAddress = (byte*)VMEM.m_pStartAddress + MemoryCost_Default;
        if(VMEM.m_SizeAllocate <= MemoryCost_Default)
            return FALSE;

        _Assert(VMEM.m_nUnitsGroup == settingVMEM.m_nGroups_per_VMEM);
        const auto UnitSize = m_SETTING.m_UnitSize;
        const auto nGroups = VMEM.m_nUnitsGroup;
        const auto nUnits = settingVMEM.m_nUnits_per_VMEM;
        const auto nUnits_per_Group = settingVMEM.m_nDistributeUnits_per_Group__VMEM;
        if(nUnits < nGroups){
            _DebugBreak(_T("Failed Distribute UnitsGroups...Units[%Iu] UnitsGroups[%Iu]"), nUnits, nGroups);
            return FALSE;
        }

        const size_t RealUnitSize = UnitSize + ((m_SETTING.m_bUse_Header_per_Unit)? sizeof(TDATA_BLOCK_HEADER) : 0);
        const size_t MemorySize_per_Group = nUnits_per_Group * RealUnitSize;
        _AssertReleaseMsg(nUnits_per_Group < MAXUINT32, _T("too many units : %Iu"), nUnits_per_Group);

        const auto iLastGroup = nGroups - 1;
        for(UINT32 iUG=0; iUG<nGroups; iUG++)
        {
            TMemoryUnitsGroup& UG = VMEM.m_pUnitsGroups[iUG];
            UG.m_Lock.Lock__Busy();

            _MACRO_STATIC_ASSERT(_MACRO_IS_SAME__VARIABLE_TYPE_TV(UINT32, UG.m_nUnits));
            _MACRO_STATIC_ASSERT(_MACRO_IS_SAME__VARIABLE_TYPE_TV(UINT32, UG.m_nTotalUnits));
            // 유닛그룹은 생성자에 의해 기본 초기화 되어 있는 상황
            if(iUG == iLastGroup)// 마지막 그룹
                UG.m_nUnits = UG.m_nTotalUnits = (nUnits - ((nGroups-1) * nUnits_per_Group));
            else// 0~ 마지막 이전 그룹 
                UG.m_nUnits = UG.m_nTotalUnits = (nUnits_per_Group);

            UG.m_pStartAddress = pStartAddress + ((size_t)iUG * MemorySize_per_Group);

            UG.m_State = TMemoryUnitsGroup::EState::E_Unknown;
        }
        VMEM.m_nAllocatedUnits = nUnits;

        // 헤더 생성
        TDATA_BLOCK_HEADER* pHF = VMEM.mFN_Get_FirstHeader();
        if(!m_SETTING.m_bUse_Header_per_Unit)
        {
            pHF->m_Type = TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_H1;

            pHF->m_User.m_pValidPTR_S = pStartAddress;
            pHF->m_User.m_pValidPTR_L = (byte*)pHF->m_User.m_pValidPTR_S + (nUnits * RealUnitSize) - UnitSize;

        }
        else
        {
            pHF->m_Type = TDATA_BLOCK_HEADER::_TYPE_UnitsAlign::E_Normal_HN;

            pHF->m_User.m_pValidPTR_S = pStartAddress + sizeof(TDATA_BLOCK_HEADER);
            pHF->m_User.m_pValidPTR_L = (byte*)pHF->m_User.m_pValidPTR_S + (nUnits * RealUnitSize) - UnitSize;
        }
        //pHF->m_Index.high = pHF->m_Index.low = ?;
        pHF->m_pFirstHeader = pHF;
        pHF->m_pPool = m_pPool;

        _MACRO_STATIC_ASSERT(_MACRO_IS_SAME__VARIABLE_TYPE(pHF->ParamNormal.m_RealUnitSize, pHF->ParamNormal.m_nUnitsGroups));
        typedef decltype(pHF->ParamNormal.m_RealUnitSize) _Type1;
        pHF->ParamNormal.m_RealUnitSize = (_Type1)RealUnitSize;
        pHF->ParamNormal.m_nUnitsGroups = (_Type1)VMEM.m_nUnitsGroup;
        pHF->ParamNormal.m_pUnitsGroups = VMEM.m_pUnitsGroups;
        pHF->ParamNormal.m_Size_per_Group = nUnits_per_Group * RealUnitSize;
        return TRUE;
    }
    //----------------------------------------------------------------
    _DEF_INLINE_TYPE__PROFILING__DEFAULT TMemoryUnitsGroup* CMemoryPool_Allocator_Virtual::mFN_Alloc_UnitsGroups_TypeAll()
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        const auto iFN = m_SETTING.m_VMEM_Small.m_nGroups_per_VMEM;
        auto& nUG = m_nRecycle_UnitsGroupsAll;
        TMemoryUnitsGroup** pSlot = m_RecycleSlot_UnitsGroups.m_All;
        if(!nUG)
        {
            const auto nRequest = m_RecycleSlot_UnitsGroups.sc_All_AllocCount;
            if(!gpFN_Array_MemoryUnitsGroup_Alloc[iFN](pSlot, nRequest))
                return nullptr;

            nUG += nRequest;
        }
        nUG--;
        TMemoryUnitsGroup* ret = pSlot[nUG];
        pSlot[nUG] = nullptr;
        return ret;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT TMemoryUnitsGroup* CMemoryPool_Allocator_Virtual::mFN_Alloc_UnitsGroups_TypeSmall()
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        const auto iFN = m_SETTING.m_VMEM_Small.m_nGroups_per_VMEM;
        auto& nUG = m_nRecycle_UnitsGroupsSmall;
        TMemoryUnitsGroup** pSlot = m_RecycleSlot_UnitsGroups.m_Small;
        if(!nUG)
        {
            const auto nRequest = m_RecycleSlot_UnitsGroups.sc_Small_AllocCount;
            if(!gpFN_Array_MemoryUnitsGroup_Alloc[iFN](pSlot, nRequest))
                return nullptr;

            nUG += nRequest;
        }
        nUG--;
        TMemoryUnitsGroup* ret = pSlot[nUG];
        pSlot[nUG] = nullptr;
        return ret;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT TMemoryUnitsGroup* CMemoryPool_Allocator_Virtual::mFN_Alloc_UnitsGroups_TypeBig()
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        const auto iFN = m_SETTING.m_VMEM_Big.m_nGroups_per_VMEM;
        auto& nUG = m_nRecycle_UnitsGroupsBig;
        TMemoryUnitsGroup** pSlot = m_RecycleSlot_UnitsGroups.m_Big;
        if(!nUG)
        {
            const auto nRequest = m_RecycleSlot_UnitsGroups.sc_Big_AllocCount;
            if(!gpFN_Array_MemoryUnitsGroup_Alloc[iFN](pSlot, nRequest))
                return nullptr;

            nUG += nRequest;
        }
        nUG--;
        TMemoryUnitsGroup* ret = pSlot[nUG];
        pSlot[nUG] = nullptr;
        return ret;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CMemoryPool_Allocator_Virtual::mFN_Free_UnitsGroups_TypeAll(TMemoryUnitsGroup* p)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        const auto iFN = m_SETTING.m_VMEM_Small.m_nGroups_per_VMEM;
        auto& nUG = m_nRecycle_UnitsGroupsAll;
        TMemoryUnitsGroup** pSlot = m_RecycleSlot_UnitsGroups.m_All;
        const auto SlotSize = m_RecycleSlot_UnitsGroups.sc_All_SlotSize;
        const auto FreeCount = m_RecycleSlot_UnitsGroups.sc_All_FreeCount;

        pSlot[nUG++] = p;
        if(nUG != SlotSize)
            return;
        // 가득찬 경우, 지정된 수량을 반납
        const auto nKeep = nUG - FreeCount;
        gpFN_Array_MemoryUnitsGroup_Free[iFN](pSlot+nKeep, FreeCount);
        nUG -= FreeCount;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CMemoryPool_Allocator_Virtual::mFN_Free_UnitsGroups_TypeSmall(TMemoryUnitsGroup* p)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        const auto iFN = m_SETTING.m_VMEM_Small.m_nGroups_per_VMEM;
        auto& nUG = m_nRecycle_UnitsGroupsSmall;
        TMemoryUnitsGroup** pSlot = m_RecycleSlot_UnitsGroups.m_Small;
        const auto SlotSize = m_RecycleSlot_UnitsGroups.sc_Small_SlotSize;
        const auto FreeCount = m_RecycleSlot_UnitsGroups.sc_Small_FreeCount;

        pSlot[nUG++] = p;
        if(nUG != SlotSize)
            return;
        // 가득찬 경우, 지정된 수량을 반납
        const auto nKeep = nUG - FreeCount;
        gpFN_Array_MemoryUnitsGroup_Free[iFN](pSlot+nKeep, FreeCount);
        nUG -= FreeCount;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CMemoryPool_Allocator_Virtual::mFN_Free_UnitsGroups_TypeBig(TMemoryUnitsGroup* p)
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        const auto iFN = m_SETTING.m_VMEM_Big.m_nGroups_per_VMEM;
        auto& nUG = m_nRecycle_UnitsGroupsBig;
        TMemoryUnitsGroup** pSlot = m_RecycleSlot_UnitsGroups.m_Big;
        const auto SlotSize = m_RecycleSlot_UnitsGroups.sc_Big_SlotSize;
        const auto FreeCount = m_RecycleSlot_UnitsGroups.sc_Big_FreeCount;

        pSlot[nUG++] = p;
        if(nUG != SlotSize)
            return;
        // 가득찬 경우, 지정된 수량을 반납
        const auto nKeep = nUG - FreeCount;
        gpFN_Array_MemoryUnitsGroup_Free[iFN](pSlot+nKeep, FreeCount);
        nUG -= FreeCount;
    }
    //----------------------------------------------------------------
    size_t CMemoryPool_Allocator_Virtual::mFN_Update__MaxAllocatedSize_Add(size_t _Byte)
    {
        _Assert(m_pPool->mFN_Query_isLocked());

        m_stats_Max_AllocatedSize += _Byte;
        const size_t ValidSize = mFN_Convert_Byte_to_ValidExpansionSize(m_stats_Max_AllocatedSize);
        if(ValidSize != m_stats_Max_AllocatedSize)
            m_stats_Max_AllocatedSize = ValidSize;

        if(m_stats_Max_AllocatedSize > m_LastKnownValue_VMEM_CACHE__MaxStorageMemorySize)
            mFN_Request__VMEM_CACHE__Expansion__MaxStorageMemorySize();

        return m_stats_Max_AllocatedSize;
    }
    size_t CMemoryPool_Allocator_Virtual::mFN_Update__MaxAllocatedSize_Set(size_t _Byte)
    {
        _Assert(m_pPool->mFN_Query_isLocked());

        if(m_stats_Max_AllocatedSize >= _Byte)
            return m_stats_Max_AllocatedSize;

        m_stats_Max_AllocatedSize = _Byte;
        const size_t ValidSize = mFN_Convert_Byte_to_ValidExpansionSize(m_stats_Max_AllocatedSize);
        if(ValidSize != m_stats_Max_AllocatedSize)
            m_stats_Max_AllocatedSize = ValidSize;

        if(m_stats_Max_AllocatedSize > m_LastKnownValue_VMEM_CACHE__MaxStorageMemorySize)
            mFN_Request__VMEM_CACHE__Expansion__MaxStorageMemorySize();

        return m_stats_Max_AllocatedSize;
    }
    size_t CMemoryPool_Allocator_Virtual::mFN_Convert_Byte_to_ValidExpansionSize(size_t _Byte)
    {
        if(_Byte == MAXSIZE_T)
            return MAXSIZE_T;

        size_t ret;

        // 할당단위의 배수가 되도록 수정한다
        if(!m_SETTING.m_bUse_Header_per_Unit)
        {
            const size_t Chunk = m_SETTING.m_VMEM_Small.m_MemoryExpansionSize;

            const size_t nChunk_u = _Byte / Chunk;
            const size_t nChunk_d = _Byte % Chunk;

            if(!nChunk_d)
                return _Byte;

            ret = (Chunk * nChunk_u) + Chunk;
        }
        else
        {
            const size_t Chunk_S = m_SETTING.m_VMEM_Small.m_MemoryExpansionSize;
            const size_t Chunk_B = m_SETTING.m_VMEM_Big.m_MemoryExpansionSize;
            _Assert(Chunk_S && Chunk_B);

            const size_t nChunk_u = _Byte / Chunk_B;
            const size_t nChunk_d = _Byte % Chunk_B;
            if(!nChunk_d)
                return _Byte;

            if(nChunk_d > Chunk_S)
                ret = (Chunk_B * nChunk_u) + Chunk_B;
            else
                ret = (Chunk_B * nChunk_u) + Chunk_S;
        }
        _Assert(ret >= _Byte);
        return ret;
    }
    DECLSPEC_NOINLINE void CMemoryPool_Allocator_Virtual::mFN_Request__VMEM_CACHE__Expansion__MaxStorageMemorySize()
    {
        _Assert(m_pPool->mFN_Query_isLocked());
        _Assert(m_stats_Max_AllocatedSize > m_LastKnownValue_VMEM_CACHE__MaxStorageMemorySize);

        const size_t c_MemoryExpansionSizeBig = m_SETTING.m_VMEM_Big.m_MemoryExpansionSize;
        _Assert(0 < c_MemoryExpansionSizeBig);

        auto pVMEM_CACHE = GLOBAL::g_VMEM_Recycle.mFN_Find_VMEM_CACHE(c_MemoryExpansionSizeBig);
        if(!pVMEM_CACHE)
        {
            m_LastKnownValue_VMEM_CACHE__MaxStorageMemorySize = MAXSIZE_T; // 이후 불필요한 테스트를 하지 않도록
            return;
        }

        pVMEM_CACHE->mFN_Set_MaxStorageMemorySize(m_stats_Max_AllocatedSize);
        const CVMEM_CACHE::TStats& refStats = pVMEM_CACHE->mFN_Get_Stats();
        _Assert(m_LastKnownValue_VMEM_CACHE__MaxStorageMemorySize <= refStats.m_Max_Storage_MemorySize);
        m_LastKnownValue_VMEM_CACHE__MaxStorageMemorySize = refStats.m_Max_Storage_MemorySize;
    }
    __forceinline size_t CMemoryPool_Allocator_Virtual::sFN_Calculate__VMEMMemory_to_Units(size_t _Byte, size_t _UnitSize, BOOL _bUse_Header_per_Unit)
    {
        const size_t MemoryCost_Default = GLOBAL::SETTING::gc_SizeMemoryPoolAllocation_DefaultCost;
        if(_Byte <= MemoryCost_Default)
            return 0;

        const size_t ValidSize = _Byte - MemoryCost_Default;
        if(!_bUse_Header_per_Unit)
            return ValidSize / _UnitSize;
        else
            return ValidSize / (_UnitSize + sizeof(TDATA_BLOCK_HEADER));
    }
    size_t CMemoryPool_Allocator_Virtual::mFN_Calculate__VMEMMemory_to_Units(size_t _Byte) const
    {
        return sFN_Calculate__VMEMMemory_to_Units(_Byte, m_SETTING.m_UnitSize, m_SETTING.m_bUse_Header_per_Unit);
    }
    size_t CMemoryPool_Allocator_Virtual::mFN_Calculate__Units_to_MemorySize(size_t _nUnits) const
    {
        if(!_nUnits)
            return 0;

        size_t ret;

        // 할당단위의 배수가 되도록 수정한다
        if(!m_SETTING.m_bUse_Header_per_Unit)
        {
            const size_t Chunk = m_SETTING.m_VMEM_Small.m_nUnits_per_VMEM;

            const size_t nChunk_u = _nUnits / Chunk;
            const size_t nChunk_d = _nUnits % Chunk;

            ret = nChunk_u * m_SETTING.m_VMEM_Small.m_MemoryExpansionSize;
            if(nChunk_d)
                ret += m_SETTING.m_VMEM_Small.m_MemoryExpansionSize;
        }
        else
        {
            const size_t Chunk_S = m_SETTING.m_VMEM_Small.m_nUnits_per_VMEM;
            const size_t Chunk_B = m_SETTING.m_VMEM_Big.m_nUnits_per_VMEM;
            _Assert(Chunk_S && Chunk_B);

            const size_t nChunk_u = _nUnits / Chunk_B;
            const size_t nChunk_d = _nUnits % Chunk_B;

            ret = nChunk_u * m_SETTING.m_VMEM_Big.m_MemoryExpansionSize;
            if(nChunk_d > Chunk_S)
                ret += m_SETTING.m_VMEM_Big.m_MemoryExpansionSize;
            else if(nChunk_d > 0)
                ret += m_SETTING.m_VMEM_Small.m_MemoryExpansionSize;
        }

        return ret;
    }
    TMemoryPoolAllocator_Virtual__Setting CMemoryPool_Allocator_Virtual::sFN_Initialize_SETTING(size_t _UnitSize, UINT32 _IndexThisPool, CMemoryPool* pPool, BOOL _bUse_Header_per_Unit)
    {
        UNREFERENCED_PARAMETER(pPool);

        TMemoryPoolAllocator_Virtual__Setting t;
        t.m_UnitSize = _UnitSize;
        t.m_bUse_Header_per_Unit = _bUse_Header_per_Unit;
        if(_bUse_Header_per_Unit)
        {
            t.m_VMEM_Small.m_MemoryExpansionSize = (size_t)1024 * GLOBAL::gFN_Precalculate_MemoryPoolExpansionSize__GetSmallSize(_IndexThisPool);
            t.m_VMEM_Big.m_MemoryExpansionSize = (size_t)1024 * GLOBAL::gFN_Precalculate_MemoryPoolExpansionSize__GetBigSize(_IndexThisPool);
        }
        else
        {
            t.m_VMEM_Small.m_MemoryExpansionSize = t.m_VMEM_Big.m_MemoryExpansionSize = GLOBAL::gc_minAllocationSize;
        }
        _Assert(0 < t.m_VMEM_Small.m_MemoryExpansionSize && 0 < t.m_VMEM_Big.m_MemoryExpansionSize);

        TMemoryPoolAllocator_Virtual__SettingVMEM* pPrecalculateVMEM[] = {&t.m_VMEM_Small, &t.m_VMEM_Big};
        for(size_t i=0; i<_MACRO_ARRAY_COUNT(pPrecalculateVMEM); i++)
        {
            TMemoryPoolAllocator_Virtual__SettingVMEM* p = pPrecalculateVMEM[i];
            p->m_nUnits_per_VMEM = decltype(p->m_nUnits_per_VMEM)(sFN_Calculate__VMEMMemory_to_Units(p->m_MemoryExpansionSize, _UnitSize, _bUse_Header_per_Unit));
            p->m_nGroups_per_VMEM = sFN_Calculate__nUnitsGroups_from_Units(_UnitSize, p->m_nUnits_per_VMEM);

            const auto nUnits = p->m_nUnits_per_VMEM;
            const auto nGroups = p->m_nGroups_per_VMEM;
            p->m_nDistributeUnits_per_Group__VMEM = (nUnits / nGroups) + ((nUnits % nGroups)? 1 : 0);
            if(2 < nGroups)
            {
                // 마지막 그룹의 유닛이 0개가 되는 일을 방지한다
                const auto sum = p->m_nDistributeUnits_per_Group__VMEM * (nGroups-1);
                if(sum == nUnits)
                    p->m_nDistributeUnits_per_Group__VMEM = nUnits / nGroups;
            }
        }
        // 유닛그룹 유닛 분배수 : 유닛수 / 그룹수 + Alpha
        // 마지막 인덱스의 유닛그룹은 분배하고 남은 것이 배치된다
        return t;
    }
    UINT32 CMemoryPool_Allocator_Virtual::sFN_Calculate__nUnitsGroups_from_Units(size_t _UnitSize, size_t _nUnits)
    {
        const size_t _MemorySizeUnits_UserSpace = _nUnits * _UnitSize;

        // 그룹수 계산
        size_t _nGroup;
        {
        #pragma warning(push)
        #pragma warning(disable: 4127)
            _MACRO_STATIC_ASSERT(0 < GLOBAL::gc_MinimumUnits_per_UnitsGroup);
            _Assert(256*1024 == GLOBAL::gc_minAllocationSize); // GLOBAL::gc_minAllocationSize가 변경되는 경우 이하 로직 수정 검토
        #pragma warning(pop)

            // 유닛그룹당 최소 요구 크기 설정(Byte)
            const size_t _DivisionSize = 16 * 1024;
            {
                // 설정으로 인한 최악의 낭비 예측(프로세서 저장소에 저장된채 사용되지 못하는 경우)
                const size_t _nTargetPools = 112;
                const size_t __Max_Waste_of_SizeMB__core2 = _nTargetPools * _DivisionSize * 1 / 1024 / 1024;
                const size_t __Max_Waste_of_SizeMB__core3 = _nTargetPools * _DivisionSize * 2 / 1024 / 1024;
                const size_t __Max_Waste_of_SizeMB__core4 = _nTargetPools * _DivisionSize * 3 / 1024 / 1024;
                const size_t __Max_Waste_of_SizeMB__core6 = _nTargetPools * _DivisionSize * 5 / 1024 / 1024;
                const size_t __Max_Waste_of_SizeMB__core8 = _nTargetPools * _DivisionSize * 7 / 1024 / 1024;
                const size_t __Max_Waste_of_SizeMB__core12 = _nTargetPools * _DivisionSize * 11 / 1024 / 1024;
            }
            size_t _Division = _MemorySizeUnits_UserSpace / _DivisionSize;
            if(_Division <= 1)
            {
                _nGroup = 1;
            }
            else
            {
                const size_t _nUnits_Per_Group = _nUnits / _Division;
                if(_nUnits_Per_Group >= GLOBAL::gc_MinimumUnits_per_UnitsGroup)
                {
                    // OK
                }
                else
                {
                    // 유닛단위 크기가 큰 경우이다
                    const size_t _Quotient = _nUnits / GLOBAL::gc_MinimumUnits_per_UnitsGroup;
                    const size_t _Remainder = _nUnits % GLOBAL::gc_MinimumUnits_per_UnitsGroup;
                    _Division = _Quotient + ((_Remainder)? 1 : 0);
                }
                if(8 <= _Division)
                    _nGroup = 8;
                else if(4 <= _Division)
                    _nGroup = 4;
                else if(2 <= _Division)
                    _nGroup = 2;
                else
                    _nGroup = 1;
            }
        }
        _Assert(1 <= _nGroup && _nGroup <= 8);
        return (UINT32)_nGroup;
    }
#pragma endregion
}
}
#endif