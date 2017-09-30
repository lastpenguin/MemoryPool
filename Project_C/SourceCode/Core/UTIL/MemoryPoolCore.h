#pragma once
#if _DEF_MEMORYPOOL_MAJORVERSION == 2
#include "./MemoryPool_v2/MemoryPoolCore_v2.h"
#elif _DEF_MEMORYPOOL_MAJORVERSION == 1
#include "./MemoryPool_Interface.h"
#include "../../BasisClass/LOCK/Lock.h"

#define _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS

#define _DEF_USING_REGISTER_SMALLUNIT__TO__DBH_TABLE 1
#define _DEF_USING_MEMORYPOOL_TEST_SMALLUNIT__FROM__DBH_TABLE 1

// 메모리 미리가져오기 & 사용하지 않는 메모리 캐시에서 제거
#define _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH 1

// Release 버전 성능 테스트 플래그
// 테스트에만 사용
// #define _DEF_USING_MEMORYPOOL_DEBUG 1



// 메모리풀 디버그
#if !defined(_DEF_USING_MEMORYPOOL_DEBUG)
  #if defined(_DEBUG)
    #define _DEF_USING_MEMORYPOOL_DEBUG 1
  #else
    #define _DEF_USING_MEMORYPOOL_DEBUG 0
  #endif
#endif

// 메모리풀 디버그 - 메모리 매직코드
#if !defined(_DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW)
  #if defined(_DEBUG) && _DEF_USING_MEMORYPOOL_DEBUG
    #define _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW 1
  #else
    #define _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW 0
  #endif
#endif


#pragma warning(push)
#pragma warning(disable: 4324)
#pragma warning(disable: 4310)
namespace UTIL{
namespace MEM{
    namespace GLOBAL{
    #pragma region 설정(빌드타이밍)
        extern const size_t gc_SmallUnitSize_Limit;
        extern const size_t gc_minPageUnitSize;
    #pragma endregion
    #pragma region 동적 설정
    #if _DEF_USING_DEBUG_MEMORY_LEAK
        extern BOOL g_bDebug__Trace_MemoryLeak;
    #endif
    #if _DEF_USING_MEMORYPOOL_DEBUG
        extern BOOL g_bDebug__Report_OutputDebug;
    #endif
    #pragma endregion
    }

    struct MemoryPool_UTIL{
        template<typename T>
        __forceinline static T* sFN_PopFront(T*& SourceF, T*& SourceB)
        {
            T* pReturn = SourceF;
            
            SourceF = SourceF->pNext;
            if(!SourceF)
                SourceB = nullptr;

            return pReturn;
        }

        template<typename T>
        __forceinline static void sFN_PushFront(T*& DestF, T*& DestB, T* p)
        {
            if(DestF)
            {
                p->pNext = DestF;
                DestF = p;
            }
            else
            {
                DestF = DestB = p;
                p->pNext = nullptr;
            }
        }
        template<typename T>
        __forceinline static void sFN_PushFrontAll(T*& DestF, T*& DestB, T*& SourceF, T*& SourceB)
        {
            if(DestB)
            {
                SourceB->pNext = DestF;
                DestF = SourceF;
            }
            else
            {
                DestF = SourceF;
                DestB = SourceB;
            }
            SourceF = nullptr;
            SourceB = nullptr;
        }

        // cnt 가 너무 많아서는 안됩니다
        template<typename T>
        __forceinline static void sFN_PushFrontN(T*& DestF, T*& DestB, T*& SourceF, T*& SourceB, size_t cnt)
        {
            // cnt 는 1이상일것
            _Assert(0 < cnt);

            T* pLast = SourceF;
            for(size_t i=1; i<cnt; i++)
                pLast = pLast->pNext;

            T* SourceF_After = pLast->pNext;

            pLast->pNext = DestF;
            DestF = SourceF;
            if(!DestB)
                DestB = pLast;

            SourceF = SourceF_After;
            if(!SourceF_After)
                SourceB = nullptr;
        }
        template<typename T>
        __forceinline static void sFN_PushBack(T*& DestF, T*& DestB, T* p)
        {
            if(DestB)
                DestB->pNext = p;
            else
                DestF = p;
            DestB = p;
            p->pNext = nullptr;
        }
        template<typename T>
        __forceinline static void sFN_PushBackN(T*& DestF, T*& DestB, T*& SourceF, T*& SourceB, size_t cnt)
        {
            // cnt 는 1이상일것
            _Assert(0 < cnt);

            T* pLast = SourceF;
            for(size_t i=1; i<cnt; i++)
                pLast = pLast->pNext;

            if(DestB)
            {
                DestB->pNext = SourceF;
                DestB = pLast;
            }
            else
            {
                DestF = SourceF;
                DestB = pLast;
            }

            SourceF = pLast->pNext;
            if(!SourceF)
                SourceB = nullptr;

            pLast->pNext = nullptr;
        }
        template<typename T>
        __forceinline static void sFN_PushBackAll(T*& DestF, T*& DestB, T*& SourceF, T*& SourceB)
        {
            if(DestB)
            {
                DestB->pNext = SourceF;
                DestB = SourceB;
            }
            else
            {
                DestF = SourceF;
                DestB = SourceB;
            }
            SourceF = nullptr;
            SourceB = nullptr;
        }
    };
}
}



namespace UTIL{
namespace MEM{

#pragma region TMemoryObject
    struct TMemoryObject{
        TMemoryObject* pNext;
        __forceinline TMemoryObject* GetNext(){ return pNext; }
    };
    _MACRO_STATIC_ASSERT(sizeof(TMemoryObject) == sizeof(void*));
#pragma endregion

#pragma region TMemoryBasket and CACHE
#pragma pack(push, 8)
    struct TUnits{
        size_t         m_nUnit = 0;
        TMemoryObject* m_pUnit_F = nullptr;
        TMemoryObject* m_pUnit_B = nullptr;
    };
    struct _DEF_CACHE_ALIGN TMemoryBasket final : public CUnCopyAble{
        TMemoryBasket(size_t _index_CACHE);
        ~TMemoryBasket();

        inline void Lock(){ m_Lock.Lock__Busy(); }
        inline void UnLock(){ m_Lock.UnLock(); }

        __forceinline TMemoryObject* TMemoryBasket::mFN_Get_Object()
        {
            // Pop Front
            if(0 == m_Units.m_nUnit)
                return nullptr;

            m_Units.m_nUnit--;
        #if _DEF_USING_MEMORYPOOL_DEBUG
            if(m_cnt_Get < c_Debug_MaxCallCounting)
                m_cnt_Get++;
        #endif

            return MemoryPool_UTIL::sFN_PopFront(m_Units.m_pUnit_F, m_Units.m_pUnit_B);
        }
        __forceinline void TMemoryBasket::mFN_Return_Object(TMemoryObject* pObj)
        {
            // Push Front
            _Assert(pObj != nullptr);
            m_Units.m_nUnit++;

        #if _DEF_USING_MEMORYPOOL_DEBUG
            if(m_cnt_Ret < c_Debug_MaxCallCounting)
                m_cnt_Ret++;
        #endif

            MemoryPool_UTIL::sFN_PushBack(m_Units.m_pUnit_F, m_Units.m_pUnit_B, pObj);
        }

        // 64에 맞춘다... 8B * 8
        size_t  m_index_CACHE;

        ::UTIL::LOCK::CSpinLockQ m_Lock;

        size_t m_nDemand;
        TUnits m_Units;

#if _DEF_USING_MEMORYPOOL_DEBUG
        static const UINT32 c_Debug_MaxCallCounting = UINT32_MAX;

        UINT32 m_cnt_Get;
        UINT32 m_cnt_Ret;
        UINT64 m_cnt_CacheMiss_from_Get_Basket;
#else
        UINT32 __FreeSlot1;
        UINT32 __FreeSlot2;
        UINT64 __FreeSlot3;
#endif
    };

    struct _DEF_CACHE_ALIGN TMemoryBasket_CACHE{
        TMemoryBasket_CACHE()
        {
            m_nMax_Keep = 0;

            m_Keep.m_nUnit = 0;
            m_Keep.m_pUnit_F = m_Keep.m_pUnit_B = nullptr;

            m_Share.m_nUnit = 0;
            m_Share.m_pUnit_F = m_Share.m_pUnit_B = nullptr;
        }
        inline void Lock(){ m_Lock.Lock__Busy(); }
        inline void UnLock(){ m_Lock.UnLock(); }

        // 64B
        ::UTIL::LOCK::CSpinLockQ m_Lock;
        size_t m_nMax_Keep;

        TUnits m_Keep;
        TUnits m_Share;
    };

    _MACRO_STATIC_ASSERT(64 == _DEF_CACHELINE_SIZE && sizeof(void*) == sizeof(size_t));
    _MACRO_STATIC_ASSERT(sizeof(::UTIL::LOCK::CSpinLock) == 8);
    _MACRO_STATIC_ASSERT(sizeof(TMemoryBasket) % _DEF_CACHELINE_SIZE == 0);
    _MACRO_STATIC_ASSERT(sizeof(TMemoryBasket_CACHE) % _DEF_CACHELINE_SIZE == 0);
#pragma pack(pop)
#pragma endregion

#pragma region DATA_BLOCK and Table
    struct _DEF_CACHE_ALIGN TDATA_BLOCK_HEADER{
        enum struct _TYPE_Units_SizeType : UINT32{
            // 가능한 다른 메모리패턴과 겹치지 않도록 유니크 하도록
            E_Invalid    = 0, // 무효
            E_SmallSize  = 0x012E98F5,
            E_NormalSize = 0x3739440C,
            E_OtherSize  = 0x3617DCF9,
        };
        // 32B ~ 필수 데이터
        _TYPE_Units_SizeType    m_Type;
        struct{
            UINT16 high;    //(0 ~(64KB / sizeof(void*))
            UINT16 low;     //(0 ~(64KB / sizeof(void*))
        }m_Index;

        void* m_pUserValidPTR_S; //사용자 영역 시작
        void* m_pUserValidPTR_L; //사용자 영역 마지막

        IMemoryPool* m_pPool;
        // 필수 데이터 ~ 32B 
        
        

        // 나머지 영역 32B ~
        TDATA_BLOCK_HEADER* m_pGroupFront; // 할당단위의 그룹중 첫번째 해더
        union{
            size_t __FreeSlot[3];
            struct{
            }ParamNormal;
            struct{
                size_t SizeThisUnit;
            }ParamBad;
        };
        __forceinline BOOL mFN_Query_ContainPTR(void* p) const
        {
            if(p < m_pUserValidPTR_S || m_pUserValidPTR_L < p)
                return FALSE;
            return TRUE;
        }
        __forceinline void* mFN_Get_AllocatedStartPTR() const
        {
            return (byte*)m_pUserValidPTR_S - sizeof(TDATA_BLOCK_HEADER);
        }
    };
    _MACRO_STATIC_ASSERT(sizeof(TDATA_BLOCK_HEADER) == _DEF_CACHELINE_SIZE);

    class _DEF_CACHE_ALIGN CTable_DataBlockHeaders{
    #pragma pack(push, 4)
        struct _DEF_CACHE_ALIGN _TYPE_Link_Recycle_Slots{
            struct TIndex{
                UINT16 h, l;
            };

            _TYPE_Link_Recycle_Slots* pPrevious;
            size_t cnt;

            static const size_t s_maxIndexes = (_DEF_CACHELINE_SIZE - sizeof(void*) - sizeof(size_t)) / sizeof(TIndex);
            TIndex m_Indexes[s_maxIndexes];
        };
        _MACRO_STATIC_ASSERT(sizeof(_TYPE_Link_Recycle_Slots) == _DEF_CACHELINE_SIZE);
    #pragma pack(pop)

        static const UINT32 s_MaxSlot_Table = 64 * 1024 / sizeof(void*);
        _MACRO_STATIC_ASSERT((UINT32)(s_MaxSlot_Table*s_MaxSlot_Table) <= UINT32_MAX);
    
    public:
        CTable_DataBlockHeaders();
        ~CTable_DataBlockHeaders();

    private:
        TDATA_BLOCK_HEADER** m_ppTable[s_MaxSlot_Table] = {0}; // 64KB

        // 이하 64B에 정렬된 시작주소...
        volatile UINT32 m_cnt_Index_High    = 0;
        volatile UINT32 m_cnt_TotalSlot     = 0;

        _TYPE_Link_Recycle_Slots* m_pRecycleSlots = nullptr;;

        UINT32 __FreeSlot[10];
        
        ::UTIL::LOCK::CSpinLockQ m_Lock;

    private:
        BOOL mFN_Insert_Recycle_Slots(UINT16 h, UINT16 l); // 잠금 보호가 필요함
        void mFN_Delete_Last_LinkFreeSlots(); // 잠금 보호가 필요함

    public:
        __forceinline TDATA_BLOCK_HEADER* mFN_Get_Link(UINT32 h, UINT32 l)
        {
            auto n = h * s_MaxSlot_Table + l;
            if(m_cnt_TotalSlot <= n)
                return nullptr;

            return m_ppTable[h][l];
        }
        __forceinline const TDATA_BLOCK_HEADER* mFN_Get_Link(UINT32 h, UINT32 l) const
        {
            auto n = h * s_MaxSlot_Table + l;
            if(m_cnt_TotalSlot <= n)
                return nullptr;

            return m_ppTable[h][l];
        }
        void mFN_Register(TDATA_BLOCK_HEADER* p);
        void mFN_UnRegister(TDATA_BLOCK_HEADER *p);
    };
#pragma endregion

#pragma region CMemoryPool_Allocator_Virtual
    class _DEF_CACHE_ALIGN CMemoryPool_Allocator_Virtual final : public CUnCopyAble{
        friend class CMemoryPool;
    public:
        CMemoryPool_Allocator_Virtual(CMemoryPool* pPool);
        ~CMemoryPool_Allocator_Virtual();

        /*----------------------------------------------------------------
        /   내부 사용객체 정의
        ----------------------------------------------------------------*/
        struct _DEF_CACHE_ALIGN TDATA_VMEM_USED{
            static const size_t c_cntSlot_Max = (_DEF_CACHELINE_SIZE - sizeof(size_t) - sizeof(TDATA_VMEM_USED*)) / sizeof(void*);

            size_t              m_cntUsedSlot;
            TDATA_VMEM_USED*    m_pNext;

            void* m_Array_pUsedPTR[c_cntSlot_Max];

            _MACRO_STATIC_ASSERT(c_cntSlot_Max >= 6);
        };

    private:
        /*----------------------------------------------------------------
        /   데이터
        ----------------------------------------------------------------*/
        CMemoryPool* m_pPool;
        BOOL    m_isSmallUnitSize;
        UINT32  m_nUnits_per_Page; // m_isSmallUnitSize ON 일때 신뢰할수 있는 수치

        size_t m_PageSize;      // GLOBAL::gc_minPageUnitSize 의 배수
        size_t m_PagePerBlock;


        // 확장시 한번에 할당되는 블록수 제한
        // 만약 이 값이 0이라면 메모리 할당이 더는 불가능한 상태이다
        size_t m_nLimitBlocks_per_Expansion;

        size_t m_stats_cnt_Succeed_VirtualAlloc;
        size_t m_stats_size_TotalAllocated;         // 할당된 메모리 합계
        
        TDATA_VMEM_USED*    m_pVMem_F;

    public:
        BOOL mFN_Set_ExpansionOption__Units_per_Block(size_t nUnits_per_Block, BOOL bWriteLog);
        BOOL mFN_Set_ExpansionOption__LimitBlocks_per_Expansion(size_t nLimitBlocks_per_Expansion, BOOL bWriteLog);
        size_t mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const;

        size_t mFN_Add_ElementSize(size_t _Byte, BOOL bWriteLog);
        BOOL mFN_Expansion();

    private:
        BOOL mFN_Expansion_PRIVATE(size_t _cntPage);
        void mFN_Take_VirtualMem(void* _pVMem);

        /*----------------------------------------------------------------
        /   이하 const 함수이지만, 사용자의 입장에서
        /   다른 스레드가 끼어들어 이 객체의 내용을 수정하면 결과가 틀려지기 때문에
        /   잠금을 걸고 사용해야 한다
        ----------------------------------------------------------------*/
        size_t mFN_Convert__PageCount_to_Size(size_t _Page) const;
        size_t mFN_Convert__Size_to_PageCount(size_t _Byte) const;
        size_t mFN_Convert__Units_to_PageCount(size_t _Units) const ;
        size_t mFN_Convert__Size_to_Units(size_t _Byte) const; // _Byte는 페이지 크기의 배수
        size_t mFN_Calculate__Size_per_Block() const;
        size_t mFN_Calculate__Units_per_Block() const;
    };
    _MACRO_STATIC_ASSERT(sizeof(CMemoryPool_Allocator_Virtual) == _DEF_CACHELINE_SIZE);
#pragma endregion


#pragma warning(pop)
}
}
#endif