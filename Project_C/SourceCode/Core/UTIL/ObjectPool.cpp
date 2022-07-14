#include "stdafx.h"
#include "./ObjectPool.h"

#include "../Core.h"


#ifndef _DEF_USING_OBJECTPOOL_DEBUG
  #ifdef _DEBUG
    #define _DEF_USING_OBJECTPOOL_DEBUG 1
  #else
    #define _DEF_USING_OBJECTPOOL_DEBUG 0
  #endif
#elif _DEF_USING_OBJECTPOOL_DEBUG && !defined(_DEBUG)
  _DEF_COMPILE_MSG__WARNING("배포주의!! _DEF_USING_OBJECTPOOL_DEBUG : Release 버전에서 활성화")
  #ifdef _AssertRelease
    #undef _Assert
    #define _Assert _AssertRelease
  #endif
#endif

namespace UTIL{
namespace MEM{
    namespace OBJPOOLCACHE{
        namespace{
            #define _LOCAL_MACRO_P(s) CObjectPool_Handle_TSize<s>::GetPool()
            #define _LOCAL_MACRO_P4(s, step) _LOCAL_MACRO_P(s+step*0), _LOCAL_MACRO_P(s+step*1), _LOCAL_MACRO_P(s+step*2), _LOCAL_MACRO_P(s+step*3)
            #define _LOCAL_MACRO_P8(s, step) _LOCAL_MACRO_P4(s,step), _LOCAL_MACRO_P4(s+step*4,step)
            #define _LOCAL_MACRO_P12(s, step) _LOCAL_MACRO_P4(s,step), _LOCAL_MACRO_P4(s+step*4,step), _LOCAL_MACRO_P4(s+step*8,step)
            #define _LOCAL_MACRO_P16(s, step) _LOCAL_MACRO_P8(s,step), _LOCAL_MACRO_P8(s+step*8,step)
            // x64에서 유닛크기 4는 사용되지 않는다
            // 8 step(4)    4 8 12 16 20 24 28 32
            // 12 step(8)   40 48 56 64 72 80 88 96 104 112 120 128
            // 8 step(16)   144 160 176 192 208 224 240 256
            // 8 step(32)   288 320 352 384 416 448 480 512
            // 8 step(64)   576 640 704 768 832 896 960 1024
            // 16 step(64)  1088 1152 1216 1280 1344 1408 1472 1536 1600 1664 1728 1792 1856 1920 1984 2048
            // 16 step(128) 2176 2304 2432 2560 2688 2816 2944 3072 3200 3328 3456 3584 3712 3840 3968 4096
            CObjectPool* g_pObjectPoolCache[] =
            {
                _LOCAL_MACRO_P8(4, 4),      // 0 ~ 7
                _LOCAL_MACRO_P12(40, 8),    // 8 ~ 19
                _LOCAL_MACRO_P8(144, 16),   // 20 ~ 27
                _LOCAL_MACRO_P8(288, 32),   // 28 ~ 35
                _LOCAL_MACRO_P8(576, 64),   // 36 ~ 43
                _LOCAL_MACRO_P16(1088, 64), // 44 ~ 59
                _LOCAL_MACRO_P16(2176, 128),// 60 ~ 79
            };
            #undef _LOCAL_MACRO_P
            #undef _LOCAL_MACRO_P4
            #undef _LOCAL_MACRO_P8
            #undef _LOCAL_MACRO_P12
            #undef _LOCAL_MACRO_P16

            size_t g_nObjectPoolCache = _MACRO_ARRAY_COUNT(g_pObjectPoolCache);
            size_t gFN_Size_to_Index__max1KB(size_t size)
            {
                _Assert(1 <= size);

                if(size <= 32)
                {
                    const size_t nMinUnitSize = sizeof(void*);
                    #if __X64
                    const size_t nMinUnit_Index = 1;
                    #elif __X86
                    const size_t nMinUnit_Index = 0;
                    #endif
                    if(size <= nMinUnitSize)
                        return nMinUnit_Index;
                    else
                        return 0 + (size-1) / 4;
                }
                if(size <= 128)
                    return 8 + (size-32-1) / 8;
                if(size <= 256)
                    return 20 + (size-128-1) / 16;
                if(size <= 512)
                    return 28 + (size-256-1) / 32;
                if(size <= 1024)
                    return 36 + (size-512-1) / 64;

                return MAXSIZE_T;
            }
            size_t gFN_Size_to_Index__max4KB(size_t size)
            {
                _Assert(1 <= size);

                if(size <= 32)
                {
                    const size_t nMinUnitSize = sizeof(void*);
                    #if __X64
                    const size_t nMinUnit_Index = 1;
                    #elif __X86
                    const size_t nMinUnit_Index = 0;
                    #endif
                    if(size <= nMinUnitSize)
                        return nMinUnit_Index;
                    else
                        return 0 + (size-1) / 4;
                }
                if(size <= 128)
                    return 8 + (size-32-1) / 8;
                if(size <= 256)
                    return 20 + (size-128-1) / 16;
                if(size <= 512)
                    return 28 + (size-256-1) / 32;
                //if(size <= 1024)
                //    return 36 + (size-512-1) / 64;
                if(size <= 2048)
                    return 36 + (size-512-1) / 64;
                if(size <= 4096)
                    return 60 + (size-2048-1) / 128;

                return MAXSIZE_T;
            }
        }
        void* gFN_Alloc_from_ObjectPool__Shared__Max1KB(size_t size)
        {
            if(size > 1024)
                return ::_aligned_malloc(size, _DEF_CACHELINE_SIZE);

            size_t i = gFN_Size_to_Index__max1KB(size);
            _Assert(i < g_nObjectPoolCache);
            return g_pObjectPoolCache[i]->mFN_Alloc();
        }
        void gFN_Free_from_ObjectPool__Shared__Max1KB(void* p, size_t size)
        {
            if(size > 1024)
            {
                ::_aligned_free(p);
                return;
            }

            size_t i = gFN_Size_to_Index__max1KB(size);
            _Assert(i < g_nObjectPoolCache);
            g_pObjectPoolCache[i]->mFN_Free(p);
        }
        void* gFN_Alloc_from_ObjectPool__Shared(size_t size)
        {
            if(size > 1024 * 4)
                return ::_aligned_malloc(size, _DEF_CACHELINE_SIZE);

            size_t i = gFN_Size_to_Index__max4KB(size);
            _Assert(i < g_nObjectPoolCache);
            return g_pObjectPoolCache[i]->mFN_Alloc();
        }
        void gFN_Free_from_ObjectPool__Shared(void* p, size_t size)
        {
            if(size > 1024 * 4)
            {
                ::_aligned_free(p);
                return;
            }
            size_t i = gFN_Size_to_Index__max4KB(size);
            _Assert(i < g_nObjectPoolCache);
            return g_pObjectPoolCache[i]->mFN_Free(p);
        }
    }
}
}

namespace UTIL{
namespace MEM{
    // 이 객체는 다른 객체에 대한 의존성이 있어서는 안된다
    CObjectPool::CObjectPool(size_t _UnitSize, BOOL _Unique)
        : m_UnitSize(static_cast<UINT32>(max(sizeof(TPTR_LINK), _UnitSize)))
        , m_UseLinkEX(TRUE)
        //, m_DemandSize_Per_Alloc(?)
        //, m_Units_Per_Alloc(?)
    #if _DEF_USING_OBJECTPOOL_DEBUG
        , m_bCheckOverflow(TRUE)
    #else
        , m_bCheckOverflow(FALSE)
    #endif
        , m_bUniqueObjectPool(_Unique)
        //----------------------------------------------------------------
        , m_nAllocatedUnitss(0)
        , m_pVMEM_First(nullptr)
        , m_nRemaining_Unist_List(0)
        , m_pUnit_First(nullptr)
        , m_nRemaining_Units_ReseervedMemory(0)
        , m_pReservedMemory(nullptr)
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();

        _AssertRelease(_UnitSize < MAXUINT32);
        _MACRO_STATIC_ASSERT(sizeof(void*) <= _DEF_CACHELINE_SIZE);

        //const UTIL::ISystem_Information* pSys = CORE::g_instance_CORE.mFN_Get_System_Information();
        //const SYSTEM_INFO* pInfo = pSys->mFN_Get_SystemInfo();
        //const size_t minimum_DemandSize_Per_Alloc = pInfo->dwAllocationGranularity;
        const size_t minimum_DemandSize_Per_Alloc = 64 * 1024;
        //const size_t minimum_MemoryUnitSize = 4 * 1024; // 실제 사용 주소 단위는 예약단위인 64KB에 맞춰진다

        const size_t minimum_cnt_per_Alloc = 16; // 사용자 설정
        size_t minimum_Units_Per_Alloc = m_UnitSize * minimum_cnt_per_Alloc;
        // 할당단위크기 배율에 맞춘다
        {
            size_t a = minimum_Units_Per_Alloc / minimum_DemandSize_Per_Alloc;
            if(minimum_Units_Per_Alloc % minimum_DemandSize_Per_Alloc)
                a++;
            minimum_Units_Per_Alloc = a * minimum_DemandSize_Per_Alloc;
        }

        m_DemandSize_Per_Alloc = max(minimum_Units_Per_Alloc, minimum_DemandSize_Per_Alloc);
        m_Units_Per_Alloc = m_DemandSize_Per_Alloc / m_UnitSize;

        size_t remainingMem = m_DemandSize_Per_Alloc - (m_Units_Per_Alloc * m_UnitSize);
        if(remainingMem >= _DEF_CACHELINE_SIZE)
            m_UseLinkEX = FALSE;

        _MACRO_OUTPUT_DEBUG_STRING(_T(" - ObjectPool[%u]%s::Create\n"), m_UnitSize
            , (m_bUniqueObjectPool)? _T("[Unique]") : _T(""));
    }
    CObjectPool::~CObjectPool()
    {
        const size_t c_cntUsingObject = m_nAllocatedUnitss - m_nRemaining_Unist_List - m_nRemaining_Units_ReseervedMemory;
        _Assert(0 == c_cntUsingObject);

        _MACRO_OUTPUT_DEBUG_STRING(_T(" - ObjectPool[%u]%s::Destroy Allocated Object[%Iu] MemorySize[%Iu]KB\n")
            , m_UnitSize
            , (m_bUniqueObjectPool)? _T("[Unique]") : _T("")
            , m_nAllocatedUnitss, m_nAllocatedUnitss / m_Units_Per_Alloc * m_DemandSize_Per_Alloc / 1024);
        if(0 < c_cntUsingObject)
            _LOG_DEBUG__WITH__TRACE(FALSE, _T("\t[Warning] ObjectPool[%u] : Memory Leak(Count:%Iu , TotalSize:%IuKB)"), m_UnitSize, c_cntUsingObject, c_cntUsingObject * m_UnitSize / 1024);

        if(!m_UseLinkEX)
        {
            TPTR_LINK* pVMEM = m_pVMEM_First;
            while(pVMEM)
            {
                TPTR_LINK* pNext = pVMEM->pNext;
                ::VirtualFree(pVMEM, 0, MEM_RELEASE);

                pVMEM = pNext;
            }
        }
        else
        {
            TPTR_LINK_EX* pVMEM = static_cast<TPTR_LINK_EX*>(m_pVMEM_First);
            while(pVMEM)
            {
                TPTR_LINK_EX* pNext = static_cast<TPTR_LINK_EX*>(pVMEM->pNext);
                for(size_t i=0; i<pVMEM->nCount; i++)
                    ::VirtualFree(pVMEM->pSlot[i], 0, MEM_RELEASE);

                ::_aligned_free(pVMEM);

                pVMEM = pNext;
            }
        }
        m_pVMEM_First = nullptr;
    }
    DECLSPEC_NOINLINE BOOL CObjectPool::mFN_Expansion()
    {
        _Assert(m_Lock.Query_isLocked());
        _Assert(!m_nRemaining_Units_ReseervedMemory && !m_pReservedMemory);

        byte* p = (byte*)::VirtualAlloc(0, m_DemandSize_Per_Alloc, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if(!p)
        {
            _Error_OutOfMemory();
            return FALSE;
        }

        // 시작주소 기록
        if(!m_UseLinkEX)
        {
            TPTR_LINK* pHead = (TPTR_LINK*)p;
            pHead->pNext = m_pVMEM_First;
            m_pVMEM_First = pHead;

            if(pHead->pNext)
                _mm_clflush(pHead->pNext);

            p += _DEF_CACHELINE_SIZE;
        }
        else
        {
            TPTR_LINK_EX* pVMEM = static_cast<TPTR_LINK_EX*>(m_pVMEM_First);
            if(!pVMEM || pVMEM->nCount == pVMEM->c_MaxSlot)
            {
                pVMEM = (TPTR_LINK_EX*)::_aligned_malloc(sizeof(TPTR_LINK_EX), sizeof(TPTR_LINK_EX));
                if(!pVMEM)
                {
                    ::VirtualFree(p, 0, MEM_RELEASE);
                    _Error_OutOfMemory();
                    return FALSE;
                }
                _MACRO_CALL_CONSTRUCTOR(pVMEM, TPTR_LINK_EX);
                pVMEM->nCount = 0;
                pVMEM->pNext = m_pVMEM_First;
                m_pVMEM_First = pVMEM;

                if(pVMEM->pNext)
                    _mm_clflush(pVMEM->pNext);
            }
            pVMEM->pSlot[pVMEM->nCount++] = p;
        }


        // 보관
        m_nAllocatedUnitss += m_Units_Per_Alloc;

        m_nRemaining_Units_ReseervedMemory += m_Units_Per_Alloc;
        m_pReservedMemory = p;
        return TRUE;
    }

    //----------------------------------------------------------------
    __forceinline size_t CObjectPool::mFN_Alloc_from_List(void** ppOUT, size_t nRequireCount)
    {
        _Assert(m_Lock.Query_isLocked());
        if(!m_nRemaining_Unist_List)
            return 0;

        const size_t c_Count = min(nRequireCount, m_nRemaining_Unist_List);
        auto pUnit = m_pUnit_First;
        for(size_t i=0; i<c_Count; i++)
        {
            ppOUT[i] = pUnit;
            pUnit = pUnit->pNext;
        }
        m_nRemaining_Unist_List -= c_Count;
        m_pUnit_First = pUnit;
        return c_Count;
    }
    __forceinline size_t CObjectPool::mFN_Alloc_from_ReservedMemory(void** ppOUT, size_t nRequireCount)
    {
        _Assert(m_Lock.Query_isLocked());
        if(!m_nRemaining_Units_ReseervedMemory)
        {
            if(!mFN_Expansion())
                return 0;
        }

        const size_t c_Count = min(nRequireCount, m_nRemaining_Units_ReseervedMemory);
        byte* pPTR = m_pReservedMemory;
        for(size_t i=0; i<c_Count; i++)
        {
            ppOUT[i] = pPTR;
            pPTR += m_UnitSize;
        }
        m_nRemaining_Units_ReseervedMemory -= c_Count;
        if(m_nRemaining_Units_ReseervedMemory)
            m_pReservedMemory = pPTR;
        else
            m_pReservedMemory = nullptr;
        return c_Count;
    }
    __forceinline void CObjectPool::mFN_Free_to_List(void** pp, size_t Count)
    {
        _Assert(m_Lock.Query_isLocked());
        m_nRemaining_Unist_List += Count;
        for(size_t i=0; i<Count; i++)
        {
            TPTR_LINK* p = (TPTR_LINK*)pp[i];

            p->pNext = m_pUnit_First;
            m_pUnit_First = p;
        }
        // ※ 반납 주소의 유효성을 확인하려면 m_pVMEM_First 리스트를 확인해야 한다
    }
    void CObjectPool::mFN_Debug_Overflow_Check_InternalCode(void** ppUnits, size_t nUnits)
    {
    #if _DEF_USING_OBJECTPOOL_DEBUG
        if(!m_bCheckOverflow || !nUnits || !ppUnits)
            return;

        const auto sizeHeader = sizeof(TPTR_LINK);
        const auto sizeBody = m_UnitSize - sizeHeader;
        if(!sizeBody)
            return;
        for(size_t i=0; i<nUnits; i++)
        {
            const TPTR_LINK* const pUnit = (TPTR_LINK*)ppUnits[i];
            if(!g_pUtil->mFN_MemoryCompareSSE2(pUnit+1, sizeBody, 0xDD))
            {
                _DebugBreak(_T("Detected Overflow\n")
                    _T("Address: 0x%p\n")
                    _T("Size(Byte) : %u")
                    , pUnit
                    , m_UnitSize);
            }
        }
    #endif
        UNREFERENCED_PARAMETER(ppUnits);
        UNREFERENCED_PARAMETER(nUnits);
    }
    void CObjectPool::mFN_Debug_Overflow_Set_InternalCode(void** ppUnits, size_t nUnits)
    {
    #if _DEF_USING_OBJECTPOOL_DEBUG
        if(!m_bCheckOverflow || !nUnits || !ppUnits)
            return;

        const auto sizeHeader = sizeof(TPTR_LINK);
        const auto sizeBody = m_UnitSize - sizeHeader;
        if(!sizeBody)
            return;
        for(size_t i=0; i<nUnits; i++)
        {
            TPTR_LINK* const pUnit = (TPTR_LINK*)ppUnits[i];
            ::memset(pUnit+1, 0xDD, sizeBody);
        }
    #endif
        UNREFERENCED_PARAMETER(ppUnits);
        UNREFERENCED_PARAMETER(nUnits);
    }
    void CObjectPool::mFN_Debug_Overflow_Set_UserCode(void** ppUnits, size_t nUnits)
    {
    #if _DEF_USING_OBJECTPOOL_DEBUG
        if(!m_bCheckOverflow || !nUnits || !ppUnits)
            return;

        const auto sizeHeader = sizeof(TPTR_LINK);
        const auto sizeBody = m_UnitSize - sizeHeader;
        if(!sizeBody)
            return;
        for(size_t i=0; i<nUnits; i++)
        {
            TPTR_LINK* const pUnit = (TPTR_LINK*)ppUnits[i];
            ::memset(pUnit+1, 0xCD, sizeBody);
        }
    #endif
        UNREFERENCED_PARAMETER(ppUnits);
        UNREFERENCED_PARAMETER(nUnits);
    }
    //----------------------------------------------------------------
    void* CObjectPool::mFN_Alloc()
    {
    #if _DEF_USING_OBJECTPOOL_DEBUG
        BOOL bAlloc_from_List = TRUE;
    #endif
        void* pOUT;
        m_Lock.Lock__Busy();
        {
            if(!mFN_Alloc_from_List(&pOUT, 1))
            {
                if(!mFN_Alloc_from_ReservedMemory(&pOUT, 1))
                    pOUT = nullptr;
            #if _DEF_USING_OBJECTPOOL_DEBUG
                bAlloc_from_List = FALSE;
            #endif
            }
        }
        m_Lock.UnLock();

    #if _DEF_USING_OBJECTPOOL_DEBUG
        if(pOUT)
        {
            if(bAlloc_from_List)
                mFN_Debug_Overflow_Check_InternalCode(&pOUT, 1);
            mFN_Debug_Overflow_Set_UserCode(&pOUT, 1);
        }
    #endif
        return pOUT;
    }
    void CObjectPool::mFN_Free(void* p)
    {
        _Assert(p);
        if(!p)
            return;
    #if _DEF_USING_OBJECTPOOL_DEBUG
        mFN_Debug_Overflow_Set_InternalCode(&p, 1);
    #endif

        m_Lock.Lock__Busy();
        {
            mFN_Free_to_List(&p, 1);
        }
        m_Lock.UnLock();
    }
    //----------------------------------------------------------------
    BOOL CObjectPool::mFN_Alloc(void** ppOUT, size_t nCount)
    {
    #pragma warning(push)
    #pragma warning(disable: 28182)
        _CompileHint(nullptr != ppOUT && 0< nCount);
    #if _DEF_USING_OBJECTPOOL_DEBUG
        size_t cntAlloc_from_List;
    #endif

        void** const ppOUT_Original = ppOUT;
        const size_t nCount_Original = nCount;
        m_Lock.Lock__Busy();
        {
            const size_t nGets = mFN_Alloc_from_List(ppOUT, nCount);
            nCount -= nGets;
            ppOUT += nGets;
        #if _DEF_USING_OBJECTPOOL_DEBUG
            cntAlloc_from_List = nGets;
        #endif
        }
        for(; 0 < nCount;)
        {
            const size_t nGets = mFN_Alloc_from_ReservedMemory(ppOUT, nCount);
            nCount -= nGets;
            ppOUT += nGets;
            if(!nGets)
            {
                // 실패시 모두 다시 반납
                const size_t cntAlloc = nCount_Original - nCount;
                mFN_Free(ppOUT_Original, cntAlloc);
                for(size_t i=0; i<cntAlloc; i++)
                    ppOUT_Original[i] = nullptr;

                break;
            }
        }
        m_Lock.UnLock();

        const BOOL bsucceed = (0 == nCount);
    #if _DEF_USING_OBJECTPOOL_DEBUG
        if(bsucceed)
        {
            mFN_Debug_Overflow_Check_InternalCode(ppOUT_Original, cntAlloc_from_List);
            mFN_Debug_Overflow_Set_UserCode(ppOUT_Original, nCount_Original);
        }
    #endif
        return bsucceed;
    #pragma warning(pop)
    }
    void CObjectPool::mFN_Free(void** pp, size_t nCount)
    {
        _Assert(pp && nCount);
        if(!pp || !nCount)
            return;
    #if _DEF_USING_OBJECTPOOL_DEBUG
        mFN_Debug_Overflow_Set_InternalCode(pp, nCount);
    #endif

        m_Lock.Lock__Busy();
        {
            mFN_Free_to_List(pp, nCount);
        }
        m_Lock.UnLock();
    }
    void CObjectPool::mFN_Set_OnOff_CheckOverflow(BOOL bCheck)
    {
        m_bCheckOverflow = bCheck;
    #if _MSC_VER <= 1900 // ~ vc 2015
        std::_Atomic_thread_fence(std::memory_order::memory_order_release);
    #else
        std::atomic_thread_fence(std::memory_order::memory_order_release);
    #endif
    }

}
}