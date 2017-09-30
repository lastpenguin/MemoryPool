#include "stdafx.h"
#if _DEF_MEMORYPOOL_MAJORVERSION == 1
#include "./MemoryPoolCore.h"

#include "./MemoryPool_Interface.h"
#include "./MemoryPool.h"
#include "./ObjectPool.h"

#include "../Core.h"

#pragma message("MemoryPoolCore.cpp 수정시 유의사항 : 코드내에서 다음의 매크로를 이용한 조건부 컴파일 금지")
#pragma message("_DEF_USING_DEBUG_MEMORY_LEAK")
#pragma message("_DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT")
#pragma message("_DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE")

#if _DEF_USING_MEMORYPOOL_DEBUG && !defined(_DEBUG)
_DEF_COMPILE_MSG__WARNING("배포주의!! Release 버전에 _DEF_USING_MEMORYPOOL_DEBUG가 사용되었습니다====")
#endif

namespace UTIL{
namespace MEM{


#pragma region TMemoryBasket
    TMemoryBasket::TMemoryBasket(size_t _index_CACHE)
        : m_index_CACHE(_index_CACHE)
        , m_nDemand(0)
    #if _DEF_USING_MEMORYPOOL_DEBUG
        , m_cnt_Get(0), m_cnt_Ret(0)
        , m_cnt_CacheMiss_from_Get_Basket(0)
    #endif
    {
    }
    TMemoryBasket::~TMemoryBasket()
    {
    }
#pragma endregion


#pragma region DATA_BLOCK and Table
    CTable_DataBlockHeaders::CTable_DataBlockHeaders()
    {
        CObjectPool_Handle_TSize<s_MaxSlot_Table*sizeof(void*)>::Reference_Attach();
        CObjectPool_Handle<CTable_DataBlockHeaders::_TYPE_Link_Recycle_Slots>::Reference_Attach();
    }
    CTable_DataBlockHeaders::~CTable_DataBlockHeaders()
    {
        // 자원 삭제
        for(UINT32 i=0; i<m_cnt_Index_High; i++)
        {
            CObjectPool_Handle_TSize<s_MaxSlot_Table*sizeof(void*)>::Free(m_ppTable[i]);
            m_ppTable[i] = nullptr;
        }

        // 빈슬롯 정보 삭제
        while(m_pRecycleSlots)
            mFN_Delete_Last_LinkFreeSlots();

        CObjectPool_Handle_TSize<s_MaxSlot_Table*sizeof(void*)>::Reference_Detach();
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
                iNowH = cntPrev / s_MaxSlot_Table;
                iNowL = cntPrev % s_MaxSlot_Table;
            }

            _Assert(m_cnt_Index_High >= iNowH);
            if(m_cnt_Index_High == iNowH)
            {
                m_ppTable[iNowH] = (TDATA_BLOCK_HEADER**)CObjectPool_Handle_TSize<s_MaxSlot_Table*sizeof(void*)>::Alloc();
                ::memset(m_ppTable[iNowH], 0, s_MaxSlot_Table*sizeof(void*));
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
    void CTable_DataBlockHeaders::mFN_UnRegister(TDATA_BLOCK_HEADER * p)
    {
        // 유효성 부터 확인
        if(!p) return;
        const auto h = p->m_Index.high;
        const auto l = p->m_Index.low;
        if(s_MaxSlot_Table <= h || s_MaxSlot_Table <= l)
            return;
        if(m_cnt_TotalSlot <= s_MaxSlot_Table * h + l)
            return;

        auto& pTrust = m_ppTable[h][l];
        if(!pTrust || pTrust != p)
            return;

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
#pragma endregion

#pragma region CMemoryPool_Allocator_Virtual
    CMemoryPool_Allocator_Virtual::CMemoryPool_Allocator_Virtual(CMemoryPool* pPool)
        : m_pPool(pPool)
        , m_isSmallUnitSize(m_pPool->m_UnitSize <= GLOBAL::gc_SmallUnitSize_Limit)
        //, m_nUnits_per_Page(?)
        //, m_PageSize(?)
        //, m_PagePerBlock(?)
        , m_nLimitBlocks_per_Expansion(8)
        , m_stats_cnt_Succeed_VirtualAlloc(0)
        , m_stats_size_TotalAllocated(0)
        , m_pVMem_F(nullptr)
    {
        CObjectPool_Handle<TDATA_VMEM_USED>::Reference_Attach();


        //const UTIL::ISystem_Information* pSys = CORE::g_instance_CORE.mFN_Get_System_Information();
        //const SYSTEM_INFO* pInfo = pSys->mFN_Get_SystemInfo();
        //m_ReserveUnitSize   = pInfo->dwAllocationGranularity;   // 예약단위 보통 64KB
        //m_AllocUnitSize     = pInfo->dwPageSize;                // 할당단위 보통 4KB

        m_PageSize = GLOBAL::gc_minPageUnitSize;
        if(m_isSmallUnitSize)
        {
            const size_t mul = m_PageSize / GLOBAL::gc_minPageUnitSize;
            const size_t s = GLOBAL::gc_minPageUnitSize - sizeof(TDATA_BLOCK_HEADER);
            const size_t n = s / m_pPool->m_UnitSize;
            m_nUnits_per_Page = (UINT32)(n * mul);
        }
        else
        {
            const size_t t = m_pPool->m_UnitSize + sizeof(TDATA_BLOCK_HEADER);
            m_nUnits_per_Page = (UINT32)(m_PageSize / t);
        }

        const size_t cnt_temp = mFN_Convert__Size_to_Units(m_PageSize);
        m_PagePerBlock = mFN_Convert__Units_to_PageCount(max(cnt_temp, CORE::SETTING::gc_MemoryPool_minUnits_per_Alloc));
    }
    CMemoryPool_Allocator_Virtual::~CMemoryPool_Allocator_Virtual()
    {
        for(TDATA_VMEM_USED* p = m_pVMem_F;p;)
        {
            const size_t end = p->m_cntUsedSlot;
            for(size_t i=0; i<end; i++)
            {
                void*& pDelete = p->m_Array_pUsedPTR[i];

                _Assert(pDelete != nullptr);
                if(pDelete)
                {
                    ::VirtualFree(pDelete, 0, MEM_RELEASE);
                    pDelete = nullptr;
                }
            }

            TDATA_VMEM_USED* pDelete = p;
            p = p->m_pNext;

            CObjectPool_Handle<TDATA_VMEM_USED>::Free(pDelete);
        }

        CObjectPool_Handle<TDATA_VMEM_USED>::Reference_Detach();
    }

    BOOL CMemoryPool_Allocator_Virtual::mFN_Set_ExpansionOption__Units_per_Block(size_t nUnits_per_Block, BOOL bWriteLog)
    {
        if(!nUnits_per_Block)
            return FALSE;

        // 최소 할당수는 무시한다 : CORE::SETTING::gc_MemoryPool_minUnits_per_Alloc
        //      사용자가 매우큰 유닛 단위의 메모리풀을 1개단위로 확장하도록 설정할 수 있다
        m_PagePerBlock = max(1, mFN_Convert__Units_to_PageCount(nUnits_per_Block));

        if(bWriteLog)
        {
            _LOG_DEBUG(_T("MemoryPool[%u] : Set Expansion Option(Units per Block) = %u"), m_pPool->m_UnitSize, nUnits_per_Block);
        }
        return TRUE;
    }
    namespace{
        const size_t gc_MaxLimitBlocks_per_Expansion = 64;
    }
    BOOL CMemoryPool_Allocator_Virtual::mFN_Set_ExpansionOption__LimitBlocks_per_Expansion(size_t nLimitBlocks_per_Expansion, BOOL bWriteLog)
    {
        if(!nLimitBlocks_per_Expansion || gc_MaxLimitBlocks_per_Expansion < nLimitBlocks_per_Expansion)
            return FALSE;

        m_nLimitBlocks_per_Expansion = nLimitBlocks_per_Expansion;

        if(bWriteLog)
        {
            _LOG_DEBUG(_T("MemoryPool[%u] : Set Expansion Option(LimitBlocks per Expansion) = %u"), m_pPool->m_UnitSize, nLimitBlocks_per_Expansion);
        }
        return TRUE;
    }

    size_t CMemoryPool_Allocator_Virtual::mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const
    {
        return gc_MaxLimitBlocks_per_Expansion;
    }

    DECLSPEC_NOINLINE size_t CMemoryPool_Allocator_Virtual::mFN_Add_ElementSize(size_t _Byte, BOOL bWriteLog)
    {
        if(_Byte < mFN_Calculate__Size_per_Block())
            return 0;

        size_t nPage;

        const size_t nNeedUnits = _Byte / m_pPool->m_UnitSize;
        // 사용자의 의도는 요청크기 = 유닛크기 * 원하는 수량
        // 헤더는 포함하지 않고 요청하기 때문에 다시 계산
        if(m_isSmallUnitSize)
        {
            const size_t nUnitsPerPage = (m_PageSize - sizeof(TDATA_BLOCK_HEADER)) / m_pPool->m_UnitSize;
            const size_t quotient  = nNeedUnits / nUnitsPerPage;
            const size_t remainder = nNeedUnits % nUnitsPerPage;

            nPage = quotient + ((remainder)? 1 : 0);
        }
        else
        {
            const size_t RealUnitSize = m_pPool->m_UnitSize + sizeof(TDATA_BLOCK_HEADER);
            const size_t RealTotalSize = nNeedUnits * RealUnitSize;

            nPage = mFN_Convert__Size_to_PageCount(RealTotalSize);
        }

        if(mFN_Expansion_PRIVATE(nPage))
        {
            size_t _Byte_Allocated = nPage * m_PageSize;
            if(bWriteLog)
                _LOG_DEBUG(_T("MemoryPool[%u] : Reserve = %uKB(%.2fMB)"), m_pPool->m_UnitSize, _Byte_Allocated/1024, _Byte_Allocated/1024./1024.);

            return _Byte_Allocated;
        }

        return 0;
    }
    DECLSPEC_NOINLINE BOOL CMemoryPool_Allocator_Virtual::mFN_Expansion()
    {
        size_t _ratePage;
        if(1 < m_nLimitBlocks_per_Expansion)
        {
            _ratePage = 1 + (m_stats_cnt_Succeed_VirtualAlloc / 8);
            _ratePage = min(_ratePage, m_nLimitBlocks_per_Expansion);
        }
        else
        {
            _ratePage = 1;
        }

        if(!mFN_Expansion_PRIVATE(_ratePage * m_PagePerBlock))
        {
            if(1 == _ratePage)
                return FALSE;

            m_nLimitBlocks_per_Expansion = 0;
            if(mFN_Expansion_PRIVATE(1 * m_PagePerBlock))
                return TRUE;

            return FALSE;
        }
        return TRUE;
    }

    DECLSPEC_NOINLINE BOOL CMemoryPool_Allocator_Virtual::mFN_Expansion_PRIVATE(size_t _cntPage)
    {
        // 예약되는 크기 - 실제 커밋크기 에서 낭비가 있다
        // 가장 치명적인 것은 x86의 경우 주소공간의 낭비
        // 방법1) 남는 여분을 다른 작은 유닛단위에 사용하도록 한다. 기존 메모리 주소 예약단위인 64KB 단위로 할당한다
        //      -> 작은유닛 / 일반유닛은 헤더의 위치를 얻는 식이 다르다, 다시 말해 여분 재활용을 작은 유닛단위에 재활용이 불가능하다
        //      -> 이 경우 메모리풀 종료시 메모리풀이 소유한 메모리 접근이 문제를 일으킬 수 있다(다른 메모리풀의 해제된 메모리일 가능성)
        //         메모리풀의 소멸자에서 처리가 아닌 for(Pools) 통계보고 , for(Pools) delete pool 의 식으로 우회 가능하다
        //      -> 조각난 여분의 활용은 통계에 있어 어떻게 계산할 것인가?
        // 방법2) 할당단위인(4KB)배수로 계산하면, 낭비를 줄일 수 있다(하지만 주소공간의 낭비는 줄일수 없다)
        //      -> 통계나 사용자 질의 부분을 수정해야 한다
        // ■ 방법 2를 실행한다
        size_t _AllocSize;
        if(!m_isSmallUnitSize/* && m_pPool->m_UnitSize > 4096-sizeof(TDATA_BLOCK_HEADER)*/)
        {
            // 유닛 크기 + 헤더(64) 가 4KB를 초과하는  경우 낭비가 있을 수 있다
            const size_t RealUnitSize = m_pPool->m_UnitSize + sizeof(TDATA_BLOCK_HEADER);

            size_t t = _cntPage * m_PageSize;
            const size_t nUnit = t / RealUnitSize;
            _AllocSize = nUnit * RealUnitSize;
            
            const size_t mast4KB = 1024*4 - 1;
            if(_AllocSize & mast4KB)
                _AllocSize = (_AllocSize & ~mast4KB) + (1024*4);
        }
        else
        {
            _AllocSize = _cntPage * m_PageSize;
        }


        void* pNewMEM = ::VirtualAlloc(nullptr, _AllocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if(!pNewMEM)
        {
            return FALSE;
        }

        m_stats_cnt_Succeed_VirtualAlloc++;
        m_stats_size_TotalAllocated += _AllocSize;

#if _DEF_USING_MEMORYPOOL_DEBUG
        if(GLOBAL::g_bDebug__Report_OutputDebug)
        {
            //_MACRO_OUTPUT_DEBUG_STRING_ALWAYS("0x%p = VirtualAlloc(Commit) %uByte : Request(%uKB %4.4fMB)\n"
            //    , pNewMEM
            //    , m_pPool->m_UnitSize
            //    , _AllocSize / 1024
            //    , _AllocSize / 1024 / 1024.f);
            _MACRO_OUTPUT_DEBUG_STRING_ALWAYS("0x%p = MemoryPool[%u] Expansion : Request(%uKB %4.4fMB)\n"
                , pNewMEM
                , m_pPool->m_UnitSize
                , _AllocSize / 1024
                , _AllocSize / 1024 / 1024.f);
        }
#endif

        mFN_Take_VirtualMem(pNewMEM);

        // 보관
        if(m_isSmallUnitSize)
        {
        #if _DEF_USING_REGISTER_SMALLUNIT__TO__DBH_TABLE
            m_pPool->mFN_KeepingUnit_from_AllocatorS_With_Register((byte*)pNewMEM, _AllocSize);
        #else
            m_pPool->mFN_KeepingUnit_from_AllocatorS((byte*)pNewMEM, _AllocSize);
        #endif
        }
        else
        {
            m_pPool->mFN_KeepingUnit_from_AllocatorN_With_Register((byte*)pNewMEM, _AllocSize);
        }
        return TRUE;
    }
    void CMemoryPool_Allocator_Virtual::mFN_Take_VirtualMem(void* _pVMem)
    {
        BOOL bRequire_New_TDATA_VMEM_USED = FALSE;
        if(!m_pVMem_F)
            bRequire_New_TDATA_VMEM_USED = TRUE;
        else if(m_pVMem_F->m_cntUsedSlot == m_pVMem_F->c_cntSlot_Max)
            bRequire_New_TDATA_VMEM_USED = TRUE;

        TDATA_VMEM_USED* p;
        if(bRequire_New_TDATA_VMEM_USED)
        {
            p = (TDATA_VMEM_USED*)CObjectPool_Handle<TDATA_VMEM_USED>::Alloc();
            p->m_cntUsedSlot    = 0;
            p->m_pNext          = m_pVMem_F;
          #ifdef _DEBUG
            ::memset(p->m_Array_pUsedPTR, 0xcd, sizeof(TDATA_VMEM_USED::m_Array_pUsedPTR));
          #endif

            m_pVMem_F = p;

            #if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
            // 가득찬 그룹은 더 이상 접근할 일이 없다
            if(p->m_pNext)
                _mm_clflush(p->m_pNext);
            #endif
        }
        else
        {
            p = m_pVMem_F;
        }

        auto& refCNT = p->m_cntUsedSlot;
        p->m_Array_pUsedPTR[refCNT++] = _pVMem;
    }
    size_t CMemoryPool_Allocator_Virtual::mFN_Convert__PageCount_to_Size(size_t _Page) const
    {
        return _Page * m_PageSize;
    }
    size_t CMemoryPool_Allocator_Virtual::mFN_Convert__Size_to_PageCount(size_t _Byte) const
    {
        const size_t a = _Byte / m_PageSize;
        const size_t b = _Byte % m_PageSize;
        size_t r = a + ((b)? 1 : 0);
        return r;
    }
    size_t CMemoryPool_Allocator_Virtual::mFN_Convert__Units_to_PageCount(size_t _Units) const
    {
        if(m_isSmallUnitSize)
        {
            const size_t a = _Units / m_nUnits_per_Page;
            const size_t b = _Units % m_nUnits_per_Page;
            const size_t r = a + ((b)? 1 : 0);
            return r;
        }
        else
        {
            const size_t t = m_pPool->m_UnitSize + sizeof(TDATA_BLOCK_HEADER);
            return mFN_Convert__Size_to_PageCount(_Units * t);
        }
    }
    // _Byte는 페이지 크기의 배수
    size_t CMemoryPool_Allocator_Virtual::mFN_Convert__Size_to_Units(size_t _Byte) const
    {
        if(m_isSmallUnitSize)
        {
            const size_t nPage = _Byte / m_PageSize;
            return m_nUnits_per_Page * nPage;
        }
        else
        {
            const size_t t = m_pPool->m_UnitSize + sizeof(TDATA_BLOCK_HEADER);
            return _Byte / t;
        }
    }
    size_t CMemoryPool_Allocator_Virtual::mFN_Calculate__Size_per_Block() const
    {
        return m_PagePerBlock * m_PageSize;
    }
    size_t CMemoryPool_Allocator_Virtual::mFN_Calculate__Units_per_Block() const
    {
        const size_t totalsize = mFN_Calculate__Size_per_Block();
        return mFN_Convert__Size_to_Units(totalsize);
    }
#pragma endregion
}
}
#endif