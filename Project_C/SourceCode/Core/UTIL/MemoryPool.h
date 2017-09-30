#pragma once
#if _DEF_MEMORYPOOL_MAJORVERSION == 2
#include "./MemoryPool_v2/MemoryPool_v2.h"
#elif _DEF_MEMORYPOOL_MAJORVERSION == 1
#include "./MemoryPoolCore.h"
#include "./ObjectPool.h"

#if _DEF_USING_DEBUG_MEMORY_LEAK
#pragma push_macro("_HAS_ITERATOR_DEBUGGING")
#undef _HAS_ITERATOR_DEBUGGING
#define _HAS_ITERATOR_DEBUGGING 0
#include <unordered_map>
#pragma pop_macro("_HAS_ITERATOR_DEBUGGING")
#endif

#pragma warning(push)
#pragma warning(disable: 4324)
/*----------------------------------------------------------------
/   메모리풀
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    class _DEF_CACHE_ALIGN CMemoryPool__BadSize : public IMemoryPool, CUnCopyAble{
        friend class CMemoryPool_Manager;
    public:
        CMemoryPool__BadSize();
        virtual ~CMemoryPool__BadSize();

    public:
        #if _DEF_USING_DEBUG_MEMORY_LEAK
        virtual void* mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line) override;
        #else
        virtual void* mFN_Get_Memory(size_t _Size) override;
        #endif
        virtual void mFN_Return_Memory(void* pAddress) override;
        virtual void mFN_Return_MemoryQ(void* pAddress) override;

    public:
        virtual size_t mFN_Query_This_UnitSize() const override;
        virtual BOOL mFN_Query_Stats(TMemoryPool_Stats* pOUT) override;
        virtual size_t mFN_Query_LimitBlocks_per_Expansion() override;
        virtual size_t mFN_Query_Units_per_Block() override;
        virtual size_t mFN_Query_MemorySize_per_Block() override;
        //----------------------------------------------------------------
        virtual size_t mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(size_t nUnits) override;
        virtual size_t mFN_Query_PreCalculate_ReserveUnits_to_Units(size_t nUnits) override;
        virtual size_t mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(size_t nByte) override;
        virtual size_t mFN_Query_PreCalculate_ReserveMemorySize_to_Units(size_t nByte) override;

        virtual size_t mFN_Reserve_Memory__nSize(size_t nByte, BOOL bWriteLog = TRUE) override;
        virtual size_t mFN_Reserve_Memory__nUnits(size_t nUnits, BOOL bWriteLog = TRUE) override;

        virtual size_t mFN_Set_ExpansionOption__BlockSize(size_t nUnits_per_Block, size_t nLimitBlocks_per_Expansion, BOOL bWriteLog = TRUE) override;
        virtual size_t mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const override;
        //----------------------------------------------------------------
        virtual void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On) override;

    private:
        void mFN_Return_Memory_Process(TMemoryObject* pAddress);
        BOOL mFN_TestHeader(TMemoryObject* pAddress);

        size_t mFN_Calculate_UnitSize(size_t size) const;
        void mFN_Register(TDATA_BLOCK_HEADER* p, size_t size);
        void mFN_UnRegister(TDATA_BLOCK_HEADER* p);

        void mFN_Debug_Report();

    private:
        // size_t virtual table ptr
        volatile size_t m_UsingSize;
        volatile size_t m_UsingCounting;
        BOOL m_bWriteStats_to_LogFile;
        UINT32 _FreeSlog1;

        volatile size_t m_stats_Maximum_AllocatedSize;
        size_t _FreeSlot2;

        volatile UINT64 m_stats_Counting_Free_BadPTR;
    };


    class _DEF_CACHE_ALIGN CMemoryPool : public IMemoryPool, CUnCopyAble{
        friend class CMemoryPool_Manager;
        friend class CMemoryPool_Allocator_Virtual;
        template<typename T> friend void UTIL::PRIVATE::gFN_Call_Destructor(T* p);
    private:
        CMemoryPool(size_t _TypeSize);
        virtual ~CMemoryPool();

    public:
        #if _DEF_USING_DEBUG_MEMORY_LEAK
        virtual void* mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line) override;
        #else
        virtual void* mFN_Get_Memory(size_t _Size) override;
        #endif
        virtual void mFN_Return_Memory(void* pAddress) override;
        virtual void mFN_Return_MemoryQ(void* pAddress) override;

    public:
        // 질의 : 취급하는 최대 유닛 크기
        virtual size_t mFN_Query_This_UnitSize() const override;
        // 질의 : 통계
        virtual BOOL mFN_Query_Stats(TMemoryPool_Stats* pOUT) override;
        // 질의 : 확장당 블록수제한
        virtual size_t mFN_Query_LimitBlocks_per_Expansion() override;
        // 질의 : 블록당 유닛수
        virtual size_t mFN_Query_Units_per_Block() override;
        // 질의 : 블록당 크기(메모리)
        virtual size_t mFN_Query_MemorySize_per_Block() override;

        //----------------------------------------------------------------
        //  유닛 예약은 [확장블록당 유닛]을 기준으로 이보다 작게 할당될 수 없습니다
        //----------------------------------------------------------------

        // 질의 : 유닛예약시 실제 사용될 메모리
        virtual size_t mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(size_t nUnits) override;
        // 질의 : 유닛예약시 실제 생성될 유닛수
        virtual size_t mFN_Query_PreCalculate_ReserveUnits_to_Units(size_t nUnits) override;
        // 질의 : 메모리예약시 실제 사용될 메모리
        virtual size_t mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(size_t nByte) override;
        // 질의 : 메모리예약시 실제 생성될 유닛수
        virtual size_t mFN_Query_PreCalculate_ReserveMemorySize_to_Units(size_t nByte) override;

        // 예약 : 메모리 크기 기준
        // 리턴 : 실제 생성된 메모리(0 : 실패)
        virtual size_t mFN_Reserve_Memory__nSize(size_t nByte, BOOL bWriteLog = TRUE) override;
        // 예약 : 유닛수 기준
        // 리턴 : 실제 생성된 유닛수(0 : 실패)
        virtual size_t mFN_Reserve_Memory__nUnits(size_t nUnits, BOOL bWriteLog = TRUE) override;

        // 확장방식 설정 변경
        //      nUnits_per_Block            : 블럭당 유닛수 설정 1~ (0 = 변경하지 않음)
        //      nLimitBlocks_per_Expansion  : 확장당 블록수제한  1~? (0 = 변경하지 않음)
        //          ※ nLimitBlocks_per_Expansion 는 확장실패시 감소할 수 있습니다.
        // 리턴 : 실제 적용된 블럭당 유닛수 설정(0 : 실패)
        virtual size_t mFN_Set_ExpansionOption__BlockSize(size_t nUnits_per_Block, size_t nLimitBlocks_per_Expansion, BOOL bWriteLog = TRUE) override;
        virtual size_t mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const override;
        //----------------------------------------------------------------
        virtual void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On) override;

    private:
        /*----------------------------------------------------------------
        /   데이터 - 읽기
        ----------------------------------------------------------------*/
        // size_t virtual table ptr
        size_t m_UnitSize;

        size_t m_UnitSize_per_IncrementDemand;
        size_t m_nLimit_Basket_KeepMax;
        size_t m_nLimit_Cache_KeepMax;
        size_t m_nLimit_CacheShare_KeepMax;

        TMemoryBasket*          m_pBaskets;         // n : g_nBaskets( == CPU 코어수)
        TMemoryBasket_CACHE*    m_pBasket__CACHE;   // n : g_nBasket__CACHE

        /*----------------------------------------------------------------
        /   데이터 - 기타
        ----------------------------------------------------------------*/
        _DEF_CACHE_ALIGN size_t     m_nUnit;        // 32bit의 경우 램4G의 한계로 표현범위의 문제가 없다
        TMemoryObject* m_pUnit_F;
        TMemoryObject* m_pUnit_B;

        size_t m_stats_Units_Allocated;

        size_t m_stats_OrderCount__ReserveSize;
        size_t m_stats_OrderCount__ReserveUnits;
        size_t m_stats_OrderSum__ReserveSize;
        size_t m_stats_OrderSum__ReserveUnits;
        // 64 경계
        size_t m_stats_OrderResult__Reserve_Size;
        size_t m_stats_OrderResult__Reserve_Units;
        size_t m_stats_OrderCount__ExpansionOption_UnitsPerBlock;
        size_t m_stats_OrderCount__ExpansionOption_LimitBlocksPerExpansion;
        size_t m_stats_Counting_Free_BadPTR;
        size_t __FreeSlot[2];
        BOOL   m_bWriteStats_to_LogFile;

        _DEF_CACHE_ALIGN CMemoryPool_Allocator_Virtual  m_Allocator;
        _DEF_CACHE_ALIGN UTIL::LOCK::CSpinLockRW        m_Lock;


#if _DEF_USING_DEBUG_MEMORY_LEAK
        struct TTrace_SourceCode{
            TTrace_SourceCode(LPCSTR name, int line): m_File(name), m_Line(line){}
            LPCSTR  m_File;
            int     m_Line;
        };
        _DEF_CACHE_ALIGN std::unordered_map<TMemoryObject*, TTrace_SourceCode, std::hash<TMemoryObject*>, std::equal_to<TMemoryObject*>, TAllocatorOBJ<std::pair<TMemoryObject*, TTrace_SourceCode>>> m_map_Lend_MemoryUnits;
        _DEF_CACHE_ALIGN LOCK::CCompositeLock m_Lock_Debug__Lend_MemoryUnits;

        volatile size_t m_Debug_stats_UsingCounting;
#endif

    public:
        void mFN_KeepingUnit_from_AllocatorS(byte* pStart, size_t size);
        void mFN_KeepingUnit_from_AllocatorS_With_Register(byte* pStart, size_t size);
        void mFN_KeepingUnit_from_AllocatorN_With_Register(byte* pStart, size_t size);

    protected:
        /*----------------------------------------------------------------
        /   비공개 인터페이스
        /       잠금 정책
        /           잠금 단계
        /               mb - cache - pool
        /   역순 잠금 불가
        /       cache - mb / pool - cache / pool - mb 모두 불가능
        ----------------------------------------------------------------*/
        TMemoryBasket& mFN_Get_Basket();

        TMemoryObject* mFN_Get_Memory_Process();
        TMemoryObject* mFN_Get_Memory_Process_Default();
        TMemoryObject* mFN_Get_Memory_Process_DirectCACHE();

        void mFN_Return_Memory_Process(TMemoryObject* pAddress);
        void mFN_Return_Memory_Process_Default(TMemoryObject* pAddress);
        void mFN_Return_Memory_Process_DirectCACHE(TMemoryObject* pAddress);
        BOOL mFN_TestHeader(TMemoryObject* pAddress);

        // 이하 함수들은 mb을 잠긴 상태라고 가정함
        TMemoryObject* mFN_Fill_Basket_andRET1(TMemoryBasket& mb, TMemoryObject*& F, TMemoryObject*& B, size_t& nSource);
        TMemoryObject* mFN_Fill_CACHE_andRET1(TMemoryBasket_CACHE& cache, TMemoryObject*& F, TMemoryObject*& B, size_t& nSource);
        TMemoryObject* mFN_Fill_Basket_from_ThisCache_andRET1(TMemoryBasket& mb);
        TMemoryObject* mFN_Fill_Basket_from_Pool_andRET1(TMemoryBasket& mb); // Pool 잠금 보호 없음
        TMemoryObject* mFN_Fill_CACHE_from_Pool_andRET1(TMemoryBasket_CACHE& cache); // Pool 잠금 보호 없음
        TMemoryObject* mFN_Fill_Basket_from_AllOtherBasketCache_andRET1(TMemoryBasket& mb);
        TMemoryObject* mFN_Fill_CACHE_from_AllOtherBasketCache_andRET1(TMemoryBasket_CACHE& cache);
        TMemoryObject* mFN_Fill_Basket_from_AllOtherBasketLocalStorage_andRET1(TMemoryBasket& mb);

        void mFN_Fill_Pool_from_Other(TUnits* pUnits);

        void mFN_BasketDemand_Expansion(TMemoryBasket& mb);
        void mFN_BasketDemand_Reduction(TMemoryBasket& mb);
        void mFN_BasektCacheDemand_Expansion(TMemoryBasket_CACHE& cache);
        void mFN_BasektCacheDemand_Reduction(TMemoryBasket_CACHE& cache);


        void mFN_Debug_Overflow_Set(void* p, size_t size, BYTE code);
        void mFN_Debug_Overflow_Check(const void* p, size_t size, BYTE code) const;

        void mFN_Debug_Report();
        size_t mFN_Counting_KeepingUnits();

        void mFN_all_Basket_Lock();
        void mFN_all_Basket_UnLock();
        void mFN_all_BasketCACHE_Lock();
        void mFN_all_BasketCACHE_UnLock();
    };
}
}

/*----------------------------------------------------------------
/   메모리풀 관리자
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    class _DEF_CACHE_ALIGN CMemoryPool_Manager: public IMemoryPool_Manager{
        _DEF_FRIEND_SINGLETON;
        friend CMemoryPool;
    protected:
        CMemoryPool_Manager();
        virtual ~CMemoryPool_Manager();

        void mFN_Test_Environment_Variable();
        void mFN_Initialize_MemoryPool_Shared_Environment_Variable1();
        void mFN_Initialize_MemoryPool_Shared_Environment_Variable2();

        void mFN_Destroy_MemoryPool_Shared_Environment_Variable();

    protected:
#pragma region Define TYPE
        struct TLinkPool{
            size_t          m_UnitSize;
            CMemoryPool*    m_pPool;
            bool operator < (const TLinkPool& r) const{
                return (m_UnitSize < r.m_UnitSize);
            }
        };
#pragma endregion
        /*----------------------------------------------------------------
        /   데이터
        ----------------------------------------------------------------*/
        _DEF_CACHE_ALIGN LOCK::CSpinLockRW      m_Lock__set_Pool;

        _DEF_CACHE_ALIGN std::set<TLinkPool>    m_set_Pool; // 잠금으로 보호

        _DEF_CACHE_ALIGN CMemoryPool__BadSize   m_SlowPool;
        
        // 설정, 통계
        BOOL   m_bWriteStats_to_LogFile;
        size_t m_stats_Counting_Free_BadPTR;




    public:
#if _DEF_USING_DEBUG_MEMORY_LEAK
        virtual void* mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line) override;
#else
        virtual void* mFN_Get_Memory(size_t _Size) override;
#endif
        virtual void mFN_Return_Memory(void* pAddress) override;
        virtual void mFN_Return_MemoryQ(void* pAddress) override;

    #if _DEF_USING_DEBUG_MEMORY_LEAK
        virtual void* mFN_Get_Memory__AlignedCacheSize(size_t _Size, const char* _FileName, int _Line) override;
#else
        virtual void* mFN_Get_Memory__AlignedCacheSize(size_t _Size) override;
#endif
        virtual void mFN_Return_Memory__AlignedCacheSize(void* pAddress) override;


        virtual IMemoryPool* mFN_Get_MemoryPool(size_t _Size) override;


#if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE
        /*----------------------------------------------------------------
        /   TLS CACHE 접근 합수 지정
        /---------------------------------------------------------------*/
        virtual void mFN_Set_TlsCache_AccessFunction(TMemoryPool_TLS_CACHE&(*pFN)(void)) override;
    #endif

        virtual void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On) override;

        virtual void mFN_Set_OnOff_Trace_MemoryLeak(BOOL _On) override;
        virtual void mFN_Set_OnOff_ReportOutputDebugString(BOOL _On) override;


    protected:
        CMemoryPool* _mFN_Prohibit__CreatePool_and_SetMemoryPoolTable(size_t iTable, size_t UnitSize);

    public:
        static IMemoryPool* sFN_Get_MemoryPool_SmallObjectSize(void* p);
        static IMemoryPool* sFN_Get_MemoryPool_NormalObjectSize(void* p);
        static IMemoryPool* sFN_Get_MemoryPool_fromPointer(void* p, TDATA_BLOCK_HEADER::_TYPE_Units_SizeType& _out_Type);
        static IMemoryPool* sFN_Get_MemoryPool_fromPointerQ(void* p, TDATA_BLOCK_HEADER::_TYPE_Units_SizeType& _out_Type);

        void mFN_Report_Environment_Variable();
        void mFN_Debug_Report();
    };
}
}
#pragma warning(pop)
#endif