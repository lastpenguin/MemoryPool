#include "stdafx.h"
#if _DEF_MEMORYPOOL_MAJORVERSION == 2
#include "./VMEM_LIST.h"

#include "../LogWriter_Interface.h"
#include "../../Core.h"
#include "../ObjectPool.h"
#pragma warning(disable: 6011)
#pragma warning(disable: 6250)
#pragma warning(disable: 6385)
#pragma warning(disable: 6386)
#pragma warning(disable: 28182)

namespace UTIL{
namespace MEM{
    namespace GLOBAL{
        extern size_t gFN_Precalculate_MemoryPoolExpansionSize__CountingExpansionSize(UINT32 _Size);
    }
}
}
namespace UTIL{
namespace MEM{
    //----------------------------------------------------------------
    //
    //----------------------------------------------------------------
    _DEF_COMPILE_MSG__FOCUS("\n"
        "\t\t만약 CVMEM_ReservedMemory_Manager 잠금 경쟁이 심하다면...\n"
        "\t\tCVMEM_ReservedMemory_Manager::TVMEM_GROUP 단위 잠금을 추가, 직접 접근하도록 수정해야 함");

    CVMEM_ReservedMemory_Manager::CVMEM_ReservedMemory_Manager()
        : m_bInitialized(FALSE)
        , m_System_VirtualMemory_Minimal_ReservationSize(64*1024)
        , m_System_VirtualMemory_Minimal_CommitSize(4*1024)
        , m_VMEM_ReservationSize(64*1024)
        //----------------------------------------------------------------
        , m_pVMEM_Group_First(nullptr)
        , m_pVMEM_Group_Last(nullptr)
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();
        _MACRO_STATIC_ASSERT(1 == _MACRO_STATIC_BIT_COUNT(sc_VMEM_AllocationSize));

        CObjectPool_Handle<TVMEM_GROUP>::Reference_Attach();
    }
    CVMEM_ReservedMemory_Manager::~CVMEM_ReservedMemory_Manager()
    {
        // ※ 객체의 파괴 시점에 메모리풀은 존재하지 않는다
        _Assert(!m_pVMEM_Group_First && !m_pVMEM_Group_Last);

        CObjectPool_Handle<TVMEM_GROUP>::Reference_Detach();
    }

    BOOL CVMEM_ReservedMemory_Manager::mFN_Initialize()
    {
        auto pSysInfo = CORE::g_instance_CORE.mFN_Get_System_Information()->mFN_Get_SystemInfo();
        m_System_VirtualMemory_Minimal_ReservationSize = pSysInfo->dwAllocationGranularity;
        m_System_VirtualMemory_Minimal_CommitSize = pSysInfo->dwPageSize;

        m_VMEM_ReservationSize = sc_VMEM_TotalAllocationSize;
        if(sc_VMEM_AllocationSize > m_System_VirtualMemory_Minimal_ReservationSize)
        {
            _AssertRelease(sc_VMEM_AllocationSize % m_System_VirtualMemory_Minimal_ReservationSize == 0);
            auto rate = sc_VMEM_AllocationSize / m_System_VirtualMemory_Minimal_ReservationSize;
            rate--;
            m_VMEM_ReservationSize += (rate * m_System_VirtualMemory_Minimal_ReservationSize);
        }
        else if(sc_VMEM_AllocationSize < m_System_VirtualMemory_Minimal_ReservationSize)
        {
            _AssertRelease(m_System_VirtualMemory_Minimal_ReservationSize % sc_VMEM_AllocationSize == 0);
        }

        m_bInitialized = TRUE;
        return TRUE;
    }
    BOOL CVMEM_ReservedMemory_Manager::mFN_Shutdown()
    {
        m_Lock.Lock();
        // 모든 자원의 반납...
        TVMEM_GROUP* pBadPTR_F = nullptr;
        TVMEM_GROUP* pBadPTR_L = nullptr;
        for(auto p=m_pVMEM_Group_First; p;)
        {
            auto pNext = p->m_pNext;
            if(p->m_nVMEM == sc_Max_VMEM_Count)
            {
                mFN_Destroy_Chunk(p);
            }
            else
            {
                // 실패
                if(pBadPTR_L)
                {
                    pBadPTR_L->m_pNext = p;
                    p->m_pPrev = pBadPTR_L;
                    p->m_pNext = nullptr;
                }
                else
                {
                    pBadPTR_F = pBadPTR_L = p;
                    p->m_pPrev = p->m_pNext = nullptr;
                }
            }
            p = pNext;
        }
        m_pVMEM_Group_First = pBadPTR_F;
        m_pVMEM_Group_Last = pBadPTR_L;
        m_Lock.UnLock();
        return !m_pVMEM_Group_First;
    }

    TDATA_VMEM* CVMEM_ReservedMemory_Manager::mFN_Pop_VMEM()
    {
        void* pAllocated = nullptr;
        TVMEM_GROUP* pVG;
        m_Lock.Lock();
        {
            pVG = m_pVMEM_Group_First;
            BOOL bNeedExpansion;
            if(!pVG)
                bNeedExpansion = TRUE;
            else if(0 == pVG->m_nVMEM)
                bNeedExpansion = TRUE;
            else
                bNeedExpansion = FALSE;

            if(bNeedExpansion)
                pVG = mFN_Make_Chunk();

            if(pVG)
            {
                const auto i = --m_pVMEM_Group_First->m_nVMEM;
                auto p = m_pVMEM_Group_First->m_Slot[i];
                _AssertRelease(p);
                m_pVMEM_Group_First->m_Slot[i] = nullptr;

                pAllocated = ::VirtualAlloc(p, sc_VMEM_AllocationSize, MEM_COMMIT, PAGE_READWRITE);
                if(!pAllocated)
                {
                    m_pVMEM_Group_First->m_Slot[i] = p;
                    m_pVMEM_Group_First->m_nVMEM++;
                }
                else if(i == 0) // m_pVMEM_Group_First->m_nVMEM == 0
                {
                    mFN_Move_to_BackEnd__VMEM_GROUP(pVG);
                }
            }
        }
        m_Lock.UnLock();
        if(pAllocated)
        {
            _Assert(pVG);
            auto pVMEM = TDATA_VMEM::sFN_Create_VMEM(pAllocated, sc_VMEM_AllocationSize);
            pVMEM->m_IsManagedReservedMemory = TRUE;
            pVMEM->m_pReservedMemoryManager = pVG;
            return pVMEM;
        }
        return nullptr;
    }
    void CVMEM_ReservedMemory_Manager::mFN_Push_VMEM(TDATA_VMEM* p)
    {
        _Assert(p && p->m_IsManagedReservedMemory && p->m_pReservedMemoryManager);

        TVMEM_GROUP* pVG = (TVMEM_GROUP*)p->m_pReservedMemoryManager;
        void* pStart = p->m_pStartAddress;
        if(!mFN_Test_VMEM(pStart, pVG))
            return;

        if(!::VirtualFree(pStart, sc_VMEM_AllocationSize, MEM_DECOMMIT))
        {
            // 중복해서 해제를 시도했거나 시작주소가 손상
            // 결과적으로 메모리 릭이 발생하게 된다
            return;
        }
        m_Lock.Lock();
        {
            _Assert(pVG->m_nVMEM < sc_Max_VMEM_Count);
            pVG->m_Slot[pVG->m_nVMEM++] = pStart;
            switch(pVG->m_nVMEM)
            {
            case sc_Max_VMEM_Count:
                if(m_pVMEM_Group_First != m_pVMEM_Group_Last)
                    mFN_Destroy_Chunk(pVG);// 그룹이 2개 이상인 경우에만 제거
                break;

            case 1:
                //mFN_Request_Move_to_Front__VMEM_GROUP(pVG);
                mFN_Move_to_FrontEnd__VMEM_GROUP(pVG);
                break;
            }
        }
        m_Lock.UnLock();
    }

    CVMEM_ReservedMemory_Manager::TVMEM_GROUP* CVMEM_ReservedMemory_Manager::mFN_Make_Chunk()
    {
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());
        //----------------------------------------------------------------
        // #1 대형 메모리 확보
        byte* pReserveS = (byte*)::VirtualAlloc(0, m_VMEM_ReservationSize, MEM_RESERVE, PAGE_NOACCESS);
        byte* pReserveE = pReserveS + m_VMEM_ReservationSize;
        if(!pReserveS)
            return nullptr;

        //----------------------------------------------------------------
        // #2 Build
        TVMEM_GROUP* pGroup = (TVMEM_GROUP*)CObjectPool_Handle<TVMEM_GROUP>::Alloc();
        if(!pGroup)
        {
            ::VirtualFree(pReserveS, 0, MEM_RELEASE);
            return nullptr;
        }
        pGroup->m_pOwner = nullptr;
        pGroup->m_pPrev = nullptr;
        pGroup->m_pNext = nullptr;

        byte* pAlignedS = _MACRO_POINTER_GET_ALIGNED(pReserveS, sc_VMEM_AllocationSize);
        if(pAlignedS < pReserveS)
            pAlignedS += sc_VMEM_AllocationSize;
        byte* pAlignedE = pAlignedS + sc_VMEM_TotalAllocationSize;

        pGroup->m_nVMEM = sc_Max_VMEM_Count;
        pGroup->m_pValidAddressS = pAlignedS;
        pGroup->m_pValidAddressE = pAlignedE;
        pGroup->m_pReservedAddress = pReserveS;
        
        // 잉여공간 앞
        if(pReserveS < pAlignedS)
        {
            pGroup->m_NotAligned.m_pFront = pReserveS;
            pGroup->m_NotAligned.m_SizeFront = pAlignedS - pReserveS;
        }
        else
        {
            pGroup->m_NotAligned.m_pFront = nullptr;
            pGroup->m_NotAligned.m_SizeFront = 0;
        }
        // 잉여공간 뒤
        if(pAlignedE < pReserveE)
        {
            pGroup->m_NotAligned.m_pBack = pAlignedE;
            pGroup->m_NotAligned.m_SizeBack = pReserveE - pAlignedE;
        }
        else
        {
            pGroup->m_NotAligned.m_pBack = nullptr;
            pGroup->m_NotAligned.m_SizeBack = 0;
        }

        // 슬롯에 분배
        // 사용시 빠른 주소를 먼저 사용할 수 있도록 한다
        // 거꾸로 삽입
        {
            byte* p = pAlignedE;
            for(size_t i=0; i<sc_Max_VMEM_Count; i++)
            {
                p -= sc_VMEM_AllocationSize;
                pGroup->m_Slot[i] = (TDATA_VMEM*)p;
            }
        }
        mFN_Push__VMEM_GROUP(pGroup);
        return pGroup;
    }
    void CVMEM_ReservedMemory_Manager::mFN_Destroy_Chunk(TVMEM_GROUP* p)
    {
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(p && p->m_nVMEM == sc_Max_VMEM_Count);

        mFN_Pop__VMEM_GROUP(p);
        ::VirtualFree(p->m_pReservedAddress, 0, MEM_RELEASE);
        CObjectPool_Handle<TVMEM_GROUP>::Free(p);
    }

    void CVMEM_ReservedMemory_Manager::mFN_Push__VMEM_GROUP(TVMEM_GROUP* pGroup)
    {
        _Assert(pGroup && m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(!pGroup->m_pPrev && !pGroup->m_pNext && !pGroup->m_pOwner);
        pGroup->m_pOwner = this;
        if(!m_pVMEM_Group_First)
        {
            pGroup->m_pPrev = pGroup->m_pNext = nullptr;
            m_pVMEM_Group_First = m_pVMEM_Group_Last = pGroup;
            return;
        }
        _CompileHint(m_pVMEM_Group_First && m_pVMEM_Group_Last);
        if(0 == pGroup->m_nVMEM)
        {
            // Push Back
            pGroup->m_pPrev = m_pVMEM_Group_Last;
            pGroup->m_pNext = nullptr;
            m_pVMEM_Group_Last->m_pNext = pGroup;
            m_pVMEM_Group_Last = pGroup;
        }
        else
        {
            // Push Front
            pGroup->m_pPrev = nullptr;
            pGroup->m_pNext = m_pVMEM_Group_First;
            m_pVMEM_Group_First->m_pPrev = pGroup;
            m_pVMEM_Group_First = pGroup;
        }
    }
    void CVMEM_ReservedMemory_Manager::mFN_Pop__VMEM_GROUP(TVMEM_GROUP* pGroup)
    {
        _Assert(pGroup && m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(pGroup->m_pOwner == this);
        _Assert(m_pVMEM_Group_First && m_pVMEM_Group_Last);
        auto pPrev = pGroup->m_pPrev;
        auto pNext = pGroup->m_pNext;
        if(pPrev)
            pPrev->m_pNext = pNext;
        else
            m_pVMEM_Group_First = pNext;
        if(pNext)
            pNext->m_pPrev = pPrev;
        else
            m_pVMEM_Group_Last = pPrev;

        pGroup->m_pPrev = pGroup->m_pNext = nullptr;
        pGroup->m_pOwner = nullptr;
    }
    void CVMEM_ReservedMemory_Manager::mFN_PushFront__VMEM_GROUP(TVMEM_GROUP* pDest, TVMEM_GROUP* pGroup)
    {
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(pDest && pDest->m_pOwner == this);
        _Assert(pGroup && !pGroup->m_pOwner);

        auto pPrev = pDest->m_pPrev;
        pDest->m_pPrev = pGroup;
        if(pPrev)
            pPrev->m_pNext = pGroup;
        else
            m_pVMEM_Group_First = pGroup;

        pGroup->m_pPrev = pPrev;
        pGroup->m_pNext = pDest;
        pGroup->m_pOwner = this;
    }
    void CVMEM_ReservedMemory_Manager::mFN_PushBack__VMEM_GROUP(TVMEM_GROUP* pDest, TVMEM_GROUP* pGroup)
    {
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(pDest && pDest->m_pOwner == this);
        _Assert(pGroup && !pGroup->m_pOwner);

        auto pNext = pDest->m_pNext;
        pDest->m_pNext = pGroup;
        if(pNext)
            pNext->m_pPrev = pGroup;
        else
            m_pVMEM_Group_Last = pGroup;

        pGroup->m_pPrev = pDest;
        pGroup->m_pNext = pNext;
        pGroup->m_pOwner = this;
    }

    void CVMEM_ReservedMemory_Manager::mFN_Request_Move_to_Front__VMEM_GROUP(TVMEM_GROUP* pGroup)
    {
        _Assert(pGroup && m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(pGroup->m_pOwner == this);
        if(!pGroup->m_nVMEM)
            return;
        if(!pGroup->m_pPrev)
            return;
        if(pGroup->m_pPrev->m_nVMEM)
            return;

        TVMEM_GROUP* pDest = nullptr;
        for(pDest = m_pVMEM_Group_First; pDest != pGroup; pDest = pDest->m_pNext)
        {
            if(0 == pDest->m_nVMEM)
                break;
        }
        if(!pDest)
            return;
        if(pDest == pGroup)
            return;

        mFN_Pop__VMEM_GROUP(pGroup);
        mFN_PushFront__VMEM_GROUP(pDest, pGroup);
    }
    void CVMEM_ReservedMemory_Manager::mFN_Move_to_FrontEnd__VMEM_GROUP(TVMEM_GROUP* pGroup)
    {
        _Assert(pGroup && m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(pGroup->m_pOwner == this);
        if(pGroup->m_pOwner != this)
            return;
        if(m_pVMEM_Group_First == pGroup)
            return; // 이미 처리되어 있다면 리턴(인자의 그룹 하나만 존재하는 경우도 포함)
        _CompileHint(m_pVMEM_Group_First != m_pVMEM_Group_Last);

        auto pPrev = pGroup->m_pPrev;
        auto pNext = pGroup->m_pNext;
        if(pPrev)
            pPrev->m_pNext = pNext;
        else
            m_pVMEM_Group_First = pNext;
        if(pNext)
            pNext->m_pPrev = pPrev;
        else
            m_pVMEM_Group_Last = pPrev;

        m_pVMEM_Group_First->m_pPrev = pGroup;
        pGroup->m_pPrev = nullptr;
        pGroup->m_pNext = m_pVMEM_Group_First;
        m_pVMEM_Group_First = pGroup;
    }
    void CVMEM_ReservedMemory_Manager::mFN_Move_to_BackEnd__VMEM_GROUP(TVMEM_GROUP* pGroup)
    {
        _Assert(pGroup && m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(pGroup->m_pOwner == this);
        if(pGroup->m_pOwner != this)
            return;
        if(m_pVMEM_Group_Last == pGroup)
            return; // 이미 처리되어 있다면 리턴(인자의 그룹 하나만 존재하는 경우도 포함)
        _CompileHint(m_pVMEM_Group_First != m_pVMEM_Group_Last);

        auto pPrev = pGroup->m_pPrev;
        auto pNext = pGroup->m_pNext;
        if(pPrev)
            pPrev->m_pNext = pNext;
        else
            m_pVMEM_Group_First = pNext;
        if(pNext)
            pNext->m_pPrev = pPrev;
        else
            m_pVMEM_Group_Last = pPrev;

        m_pVMEM_Group_Last->m_pNext = pGroup;
        pGroup->m_pPrev = m_pVMEM_Group_Last;
        pGroup->m_pNext = nullptr;
        m_pVMEM_Group_Last = pGroup;
    }
    BOOL CVMEM_ReservedMemory_Manager::mFN_Test_VMEM(const void* p, const TVMEM_GROUP* pOwner)
    {
        BOOL bFailed = FALSE;
        
        if(this != pOwner->m_pOwner)
            bFailed = TRUE;
        if(pOwner->m_nVMEM >= sc_Max_VMEM_Count)
            bFailed = TRUE;

        // 주소 정렬 확인
        if((size_t)p & (sc_VMEM_AllocationSize-1))
            bFailed = TRUE;
        // 주소 범위 확인
        if(p < pOwner->m_pValidAddressS || pOwner->m_pValidAddressE <= p)
            bFailed = TRUE;

        if(bFailed)
            mFN_ExceptionProcess_from_mFN_Test_VMEM(p);
        return !bFailed;
    }
    DECLSPEC_NOINLINE void CVMEM_ReservedMemory_Manager::mFN_ExceptionProcess_from_mFN_Test_VMEM(const void* p)
    {
        _LOG_DEBUG__WITH__OUTPUTDEBUGSTR(TRUE, _T("Damaged MemoryPool VMEM 0x%p ~ %IuByte"), p, sizeof(TDATA_BLOCK_HEADER) + sizeof(TDATA_VMEM));
    }
    //----------------------------------------------------------------
    //
    //----------------------------------------------------------------
    CVMEM_CACHE::CVMEM_CACHE(size_t sizeKB_per_VMEM)
        : mc_Size_per_VMEM(sizeKB_per_VMEM*1024)
        , m_pFirst(nullptr), m_cnt_VMEM(0)
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();
    }
    CVMEM_CACHE::~CVMEM_CACHE()
    {
        // ※ 객체의 파괴 시점에 메모리풀은 존재하지 않는다
        _Assert(!m_pFirst && !m_cnt_VMEM);
    }
    BOOL CVMEM_CACHE::mFN_Shutdown()
    {
        if(!m_Lock.Lock__NoInfinite__Lazy(0xFFFF))
        {
            _LOG_DEBUG__WITH__TIME("Damaged CVMEM_CACHE[%IuKB] : Lock", mFN_Query__SizeKB_per_VMEM());
            return FALSE;
        }

        // 캐시에 남은 VEMM 제거
        size_t cntDelete_VMEM = 0;
        for(auto pVMEM=m_pFirst; pVMEM;)
        {
            auto pNext = pVMEM->m_pNext;
            m_stats.m_Current_MemorySize -= pVMEM->m_SizeAllocate;

            pVMEM->sFN_Delete_VMEM(pVMEM);
            cntDelete_VMEM++;

            pVMEM = pNext;
        }

        BOOL bSucceed = TRUE;
        if(cntDelete_VMEM != m_cnt_VMEM)
        {
            bSucceed = FALSE;
            _LOG_DEBUG__WITH__TIME("Damaged CVMEM_CACHE[%IuKB] : m_cnt_VMEM[%Iu] Count Delete[%Iu]", mFN_Query__SizeKB_per_VMEM()
                , m_cnt_VMEM, cntDelete_VMEM);
        }
        if(0 != m_stats.m_Current_MemorySize)
        {
            bSucceed = FALSE;
            _LOG_DEBUG__WITH__TIME("Damaged CVMEM_CACHE[%IuKB] : m_Current_MemorySize[%Iu]", mFN_Query__SizeKB_per_VMEM(), m_stats.m_Current_MemorySize);
        }

        m_pFirst   = nullptr;
        m_cnt_VMEM = 0;

        m_Lock.UnLock();
        return bSucceed;
    }
    size_t CVMEM_CACHE::mFN_Query__Size_per_VMEM() const
    {
        return mc_Size_per_VMEM;
    }
    size_t CVMEM_CACHE::mFN_Query__SizeKB_per_VMEM() const
    {
        return mc_Size_per_VMEM / 1024;
    }
    const CVMEM_CACHE::TStats& CVMEM_CACHE::mFN_Get_Stats() const
    {
        return m_stats;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT BOOL CVMEM_CACHE::mFN_Set_MaxStorageMemorySize(size_t Size)
    {
        if(!Size)
            return FALSE;

        // 나머지는 올림
        const size_t nChunk = (Size + mc_Size_per_VMEM - 1) / mc_Size_per_VMEM;
        size_t ValidSize = nChunk * mc_Size_per_VMEM;
        if(ValidSize / nChunk != mc_Size_per_VMEM)
        {
            _MACRO_OUTPUT_DEBUG_STRING_TRACE(_T("Overflow!! CVMEM_CACHE::mFN_Set_MaxStorageMemorySize\n\t%Iu * %Iu\n"), nChunk, mc_Size_per_VMEM);
            ValidSize = MAXSIZE_T;
        }
        if(m_stats.m_Max_Storage_MemorySize >= ValidSize)
            return FALSE;

        BOOL bRet;
        m_Lock.Lock__Busy();
        {
            if(m_stats.m_Max_Storage_MemorySize < ValidSize)
            {
                m_stats.m_Max_Storage_MemorySize = ValidSize;
                bRet = TRUE;
            }
            else
            {
                bRet = FALSE;
            }
        }
        m_Lock.UnLock();
        return bRet;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT size_t CVMEM_CACHE::mFN_Add_MaxStorageMemorySize(size_t Size)
    {
        if(!Size)
            return 0;

        const size_t nChunk = (Size + mc_Size_per_VMEM - 1) / mc_Size_per_VMEM;
        const size_t ValidSize = nChunk * mc_Size_per_VMEM;
        size_t sizeRet = ValidSize;
        m_Lock.Lock__Busy();
        {
            // 나머지는 올림
            const size_t ResultSize = m_stats.m_Max_Storage_MemorySize + ValidSize;
            if(ResultSize > m_stats.m_Max_Storage_MemorySize)
            {
                m_stats.m_Max_Storage_MemorySize = ResultSize;
            }
            else // 오버플로우 대비
            {
                m_stats.m_Max_Storage_MemorySize = MAXSIZE_T;
                sizeRet = MAXSIZE_T - m_stats.m_Max_Storage_MemorySize;
            }
        }
        m_Lock.UnLock();
        return sizeRet;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT TDATA_VMEM * CVMEM_CACHE::mFN_Pop()
    {
        TDATA_VMEM* ret;

    #if _MSC_VER <= 1900 // ~ vc 2015
        ::std::_Atomic_thread_fence(std::memory_order::memory_order_consume);
    #else
        ::std::atomic_thread_fence(std::memory_order::memory_order_consume);
    #endif
        if(!m_cnt_VMEM)
            return nullptr;
        m_Lock.Lock__Lazy();
        {
            ret = m_pFirst;
            if(ret)
            {
                m_pFirst = m_pFirst->m_pNext;
                --m_cnt_VMEM;

                _Assert(m_stats.m_Current_MemorySize >= ret->m_SizeAllocate);
                m_stats.m_Current_MemorySize -= ret->m_SizeAllocate;
            }
        }
        m_Lock.UnLock();

        if(ret)
        {
            _Assert(ret->m_pOwner == this);
            ret->m_pOwner = nullptr;

            ret->m_pNext = nullptr;
        }
        return ret;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT void CVMEM_CACHE::mFN_Push(TDATA_VMEM * p)
    {
        _Assert(p && 0 == p->m_nUnitsGroup && !p->m_pOwner && nullptr == p->m_pUnitsGroups);
        _Assert(0 < p->m_SizeAllocate);
        const size_t sizeVMEM = p->m_SizeAllocate;

        BOOL bSucceed_Push = FALSE;
        // TDATA_VMEM p가 비어있는 더미가 있을수 없다면 오버플로우는 체크 할 필요 없다
    #if _MSC_VER <= 1900 // ~ vc 2015
        std::_Atomic_thread_fence(std::memory_order::memory_order_consume);
    #else
        std::atomic_thread_fence(std::memory_order::memory_order_consume);
    #endif
        if(m_stats.m_Max_Storage_MemorySize >= (m_stats.m_Current_MemorySize + sizeVMEM))
        {
            m_Lock.Lock__Busy();
            // 2차 확인
            if(m_stats.m_Max_Storage_MemorySize >= (m_stats.m_Current_MemorySize + sizeVMEM))
            {
                p->m_pOwner = this;

                p->m_pNext = m_pFirst;
                m_pFirst = p;

                ++m_cnt_VMEM;
                ++m_stats.m_Counting_VMEM_Push;
                if(m_stats.m_Max_cnt_VMEM < m_cnt_VMEM)
                    m_stats.m_Max_cnt_VMEM = m_cnt_VMEM;

                m_stats.m_Current_MemorySize += sizeVMEM;

                bSucceed_Push = TRUE;
            }
            m_Lock.UnLock();
        }

        if(!bSucceed_Push)
            TDATA_VMEM::sFN_Delete_VMEM(p);
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT BOOL CVMEM_CACHE::mFN_Free_AnythingHeapMemory()
    {
        auto pVMEM = mFN_Pop();
        if(!pVMEM)
            return FALSE;

        TDATA_VMEM::sFN_Delete_VMEM(pVMEM);
        return TRUE;
    }
    //----------------------------------------------------------------
    //
    //----------------------------------------------------------------
    TVMEM_CACHE_LIST::TVMEM_CACHE_LIST()
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();

        // CVMEM_CACHE 최대 보관 크기 결정
        //----------------------------------------------------------------
        // 모든 CVMEM_CACHE 를 합쳐 n크기를 분할
        //----------------------------------------------------------------
        const size_t c_TotalStorageMemorySize = 24 * 1024 * 1024;
        size_t cntTotal = 0;
        size_t cnt256KB = 0;
        size_t cnt512KB = 0;
        size_t cnt1024KB = 0;
        size_t cnt2048KB = 0;
        size_t cnt4096KB = 0;

    #define _LOCAL_MACRO_CHECK(x) cnt##x##KB = ::UTIL::MEM::GLOBAL::gFN_Precalculate_MemoryPoolExpansionSize__CountingExpansionSize(x);\
        cntTotal += cnt##x##KB

        _LOCAL_MACRO_CHECK(256);
        _LOCAL_MACRO_CHECK(512);
        _LOCAL_MACRO_CHECK(1024);
        _LOCAL_MACRO_CHECK(2048);
        _LOCAL_MACRO_CHECK(4096);
    #undef _LOCAL_MACRO_CHECK

    #define _LOCAL_MACRO_PROCESS(x) { const size_t _size = c_TotalStorageMemorySize * cnt##x##KB / cntTotal;\
        m_##x##KB.mFN_Set_MaxStorageMemorySize(_size); }

        _LOCAL_MACRO_PROCESS(256);
        _LOCAL_MACRO_PROCESS(512);
        _LOCAL_MACRO_PROCESS(1024);
        _LOCAL_MACRO_PROCESS(2048);
        _LOCAL_MACRO_PROCESS(4096);
    #undef _LOCAL_MACRO_PROCESS
    }
    BOOL TVMEM_CACHE_LIST::mFN_Shutdown()
    {
        BOOL bSucceed = TRUE;
    #define _LOCAL_MACRO_SHUTDOWN_CACHE(x) if(!m_##x.mFN_Shutdown()) bSucceed = FALSE
        _LOCAL_MACRO_SHUTDOWN_CACHE(256KB);
        _LOCAL_MACRO_SHUTDOWN_CACHE(512KB);
        _LOCAL_MACRO_SHUTDOWN_CACHE(1024KB);
        _LOCAL_MACRO_SHUTDOWN_CACHE(2048KB);
        _LOCAL_MACRO_SHUTDOWN_CACHE(4096KB);
    #undef _LOCAL_MACRO_SHUTDOWN_CACHE
        return bSucceed;
    }
    CVMEM_CACHE* TVMEM_CACHE_LIST::mFN_Find_VMEM_CACHE(size_t Size_per_VMEM)
    {
    #define _LOCAL_MACRO_FIND_VMEMCACHE(x) if(m_##x.mFN_Query__Size_per_VMEM() == Size_per_VMEM) return &m_##x
        _LOCAL_MACRO_FIND_VMEMCACHE(256KB);
        _LOCAL_MACRO_FIND_VMEMCACHE(512KB);
        _LOCAL_MACRO_FIND_VMEMCACHE(1024KB);
        _LOCAL_MACRO_FIND_VMEMCACHE(2048KB);
        _LOCAL_MACRO_FIND_VMEMCACHE(4096KB);
    #undef _LOCAL_MACRO_FIND_VMEMCACHE
        return nullptr;
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT BOOL TVMEM_CACHE_LIST::mFN_Free_AnythingHeapMemory()
    {
        // 가능한 작은 단위의 메모리를 해제
    #define _LOCAL_MACRO_FREE_VMEMCACHE(x) if(m_##x.mFN_Free_AnythingHeapMemory()) return TRUE
        _LOCAL_MACRO_FREE_VMEMCACHE(256KB);
        _LOCAL_MACRO_FREE_VMEMCACHE(512KB);
        _LOCAL_MACRO_FREE_VMEMCACHE(1024KB);
        _LOCAL_MACRO_FREE_VMEMCACHE(2048KB);
        _LOCAL_MACRO_FREE_VMEMCACHE(4096KB);
        return FALSE;
    #undef _LOCAL_MACRO_FREE_VMEMCACHE
    }
    _DEF_INLINE_TYPE__PROFILING__DEFAULT BOOL TVMEM_CACHE_LIST::mFN_Free_AnythingHeapMemory(size_t ProtectedSize)
    {
        const size_t size = ProtectedSize;
    #define _LOCAL_MACRO(x) if(m_##x##KB.mFN_Query__Size_per_VMEM() != size) { if(m_##x##KB.mFN_Free_AnythingHeapMemory()) return TRUE; }
        _LOCAL_MACRO(4096);
        _LOCAL_MACRO(2048);
        _LOCAL_MACRO(1024);
        _LOCAL_MACRO(512);
        _LOCAL_MACRO(256);
        return FALSE;
    #undef _LOCAL_MACRO
    }
}
}

namespace UTIL{
namespace MEM{
    namespace GLOBAL{
        _DEF_CACHE_ALIGN CVMEM_ReservedMemory_Manager g_VEM_ReserveMemory;
        _DEF_CACHE_ALIGN TVMEM_CACHE_LIST g_VMEM_Recycle;
    }
}
}
#endif