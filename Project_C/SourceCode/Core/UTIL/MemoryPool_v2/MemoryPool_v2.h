#pragma once
#include "./MemoryPoolCore_v2.h"
#include "../ObjectPool.h"

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
/   설정
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{

    extern const size_t gc_KB;
    extern const size_t gc_MB;

    namespace GLOBAL{
        namespace SETTING{
            const size_t gc_Num_MemoryPool = 258;
            const size_t gc_SizeMemoryPoolAllocation_DefaultCost = sizeof(TDATA_BLOCK_HEADER) + sizeof(TDATA_VMEM);

            extern const size_t gc_Max_ThreadAccessKeys;// 실행속도의 하락을 예상하는 너무 많은 CPU 수(실제 동시 스레드)

            // 유닛 크기 계산에 사용하는 환경변수
            extern const size_t gc_Array_Limit[];
            extern const size_t gc_Array_MinUnit[];
            extern const size_t gc_Array_Limit__Count;
            extern const size_t gc_Array_MinUnit__Count;

            extern const size_t gc_minSize_of_MemoryPoolUnitSize; // 취급 유닛 크기 :가장 작은 메모리풀 
            extern const size_t gc_maxSize_of_MemoryPoolUnitSize; // 취급 유닛 크기 :가장 큰 메모리풀
            
            extern const UINT32 gc_Array_MemoryPoolUnitSize[gc_Num_MemoryPool];
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
        struct TQuickLinkMemoryPool{
            class CMemoryPool* m_pPool;
            UINT32 m_index_Pool;
        };
        // 메모리풀 바로가기 테이블
        extern const size_t gc_Num_QuickLink_MemoryPool;
        extern TQuickLinkMemoryPool g_Array_QuickLink_MemoryPool[];

        // 무결성 확인용 테이블
        extern CTable_DataBlockHeaders g_Table_BlockHeaders_Normal;
        extern CTable_DataBlockHeaders g_Table_BlockHeaders_Other;

        extern size_t g_nProcessor;
        extern BOOL g_bDebug__Trace_MemoryLeak;
        extern BOOL g_bDebug__Report_OutputDebug;
    }
}
}


namespace UTIL{
namespace MEM{
    /*----------------------------------------------------------------
    /   메모리풀 : 예외크기
    /---------------------------------------------------------------*/
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
        virtual BOOL mFN_Query_This_UnitsSizeLimit(size_t* pOUT_Min, size_t* pOUT_Max) const override;
        virtual BOOL mFN_Query_isSlowMemoryPool() const override;
        virtual BOOL mFN_Query_MemoryUnit_isAligned(size_t AlignSize) const override;
        virtual BOOL mFN_Query_Stats(TMemoryPool_Stats* pOUT) override;

        //----------------------------------------------------------------
        virtual size_t mFN_Reserve_Memory__nSize(size_t nByte, BOOL bWriteLog = TRUE) override;
        virtual size_t mFN_Reserve_Memory__nUnits(size_t nUnits, BOOL bWriteLog = TRUE) override;
        virtual size_t mFN_Reserve_Memory__Max_nSize(size_t nByte, BOOL bWriteLog = TRUE) override;
        virtual size_t mFN_Reserve_Memory__Max_nUnits(size_t nUnits, BOOL bWriteLog = TRUE) override;
        virtual size_t mFN_Hint_MaxMemorySize_Add(size_t nByte, BOOL bWriteLog = TRUE) override;
        virtual size_t mFN_Hint_MaxMemorySize_Set(size_t nByte, BOOL bWriteLog = TRUE) override;
        //----------------------------------------------------------------
        // 프로세서별 캐시 사용
        virtual BOOL mFN_Set_OnOff_ProcessorCACHE(BOOL _ON) override;
        virtual BOOL mFN_Query_OnOff_ProcessorCACHE() const override;
        //----------------------------------------------------------------
        virtual void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On) override;

    private:
        void mFN_Return_Memory_Process(TMemoryObject* pAddress);
        BOOL mFN_TestHeader(TMemoryObject* pAddress);

        size_t mFN_Calculate_UnitSize(size_t size) const;
        void mFN_Register(TDATA_BLOCK_HEADER* p, size_t size);
        void mFN_UnRegister(TDATA_BLOCK_HEADER* p);

        void mFN_Debug_Report();

        //----------------------------------------------------------------
    private:
       static const UINT32 sc_bUse_Header_per_Unit = TRUE;
       static const size_t sc_AlignSize = _DEF_CACHELINE_SIZE;

       //----------------------------------------------------------------
    private:
        // size_t virtual table ptr
        volatile size_t m_UsingSize;
        volatile size_t m_UsingCounting;
        BOOL m_bWriteStats_to_LogFile;
        UINT32 _FreeSlog1;

        volatile size_t m_stats_Maximum_AllocatedSize;
        size_t _FreeSlot2;

        ::UTIL::NUMBER::TCounting_LockFree_UT m_stats_Counting_Free_BadPTR;
    };

    /*----------------------------------------------------------------
    /   유닛그룹 보관 컨테이너
    /       형태는 캐시라인단위에 맞춘 느슨한 큐
    /       추가적으로 빠른 삭제를 위한 기능을 포함
    /---------------------------------------------------------------*/
    class _DEF_CACHE_ALIGN CMemoryUnitsGroup_List final : CUnCopyAble{
    public:
        struct _DEF_CACHE_ALIGN TChunk{
            ~TChunk(){}

        #if __X86
            static const UINT32 sc_SizeThisStruct = _DEF_CACHELINE_SIZE;
        #elif __X64
            static const UINT32 sc_SizeThisStruct = _DEF_CACHELINE_SIZE * 2;
        #endif
            static const UINT32 sc_MaxSlot = (sc_SizeThisStruct - (sizeof(TChunk*)*2 + sizeof(UINT32)*2)) / sizeof(TMemoryUnitsGroup*);
            //----------------------------------------------------------------

            TChunk* m_pPrev = nullptr;
            TChunk* m_pNext = nullptr;

            UINT32  m_iSlotPop = 0;
            UINT32  m_iSlotPush = 0; // 0 : 모든 슬롯이 비어 있음
            TMemoryUnitsGroup* m_Slot[sc_MaxSlot] = {nullptr};

            BOOL mFN_Query_isEmpty() const{return 0 == m_iSlotPush;}
        };
        _MACRO_STATIC_ASSERT(sizeof(TChunk) == TChunk::sc_SizeThisStruct);

    public:
        CMemoryUnitsGroup_List(TMemoryUnitsGroup::EState _this_list_state);
        ~CMemoryUnitsGroup_List();

        BOOL mFN_Query_IsEmpty() const; // 무잠금(사용시 std::_Atomic_thread_fence 를 사용)

        // 지정한 유닛그룹(이미 잠금을 확보한 상태의)을 리스트에서 제거
        UINT32 mFN_Pop(TMemoryUnitsGroup* p);
        // 지정한 유닛그룹(이미 잠금을 확보한 상태의)을 리스트에서 제거
        // ※ 연속된 다수의 유닛그룹의 처리는 한번에 처리하는 것이 잠금-잠금해제 수를 줄일 수 있다
        UINT32 mFN_Pop(TMemoryUnitsGroup* p, UINT32 n);
        
        // 유닛그룹을 리턴(잠금확보후)
        TMemoryUnitsGroup* mFN_PopFront();
        
        // 지정한 유닛그룹(잠금을 확보한 상태의)을 이 컨테이너에 소유권을 넘김
        void mFN_Push(TMemoryUnitsGroup* p);
        // 지정한 유닛그룹(잠금을 확보한 상태의)을 이 컨테이너에 소유권을 넘김
        void mFN_Push(TMemoryUnitsGroup* p, UINT32 n);

    private:
        UTIL::LOCK::CSpinLockQ m_Lock;
        TChunk* m_pF;
        TChunk* m_pL;

        UINT32 m_cntUnitsGroup;
        UINT32 m_cntRecycleChunk;

        TChunk* m_pRecycleF;
        TChunk* m_pRecycleL;


        const TMemoryUnitsGroup::EState mc_GroupState;
        UINT32 _FreeSlot_4B_b;

        UINT64 _FreeSlot_8B_a;
        //----------------------------------------------------------------
        _MACRO_STATIC_ASSERT(sizeof(TMemoryUnitsGroup::EState) == sizeof(UINT32));
        // 사용하지 않는 TChunk 에서 최대 n 개의 TChunk를 오브젝풀에 반납하지 않고 보관하여 재활용한다
        // 관련 데이터 m_cntRecycleChunk / m_pRecycleF / m_pRecycleㅣ
        static const UINT32 sc_MaxRecycleStorage = 4u;
        //----------------------------------------------------------------
    private:
        BOOL mFN_Private__PopFullSeach(TMemoryUnitsGroup* p);
        BOOL mFN_Private__Pop(TMemoryUnitsGroup* p);
        void mFN_Private__Push(TMemoryUnitsGroup* p);
        void mFN_Chunk_UpdatePop(TChunk* pChunk, UINT32 index);

        TChunk* mFN_Chunk_Add();            // Chunk <- Memory(Dynamic) / RecycleSlot
        void mFN_Chunk_Remove(TChunk* p);   // Chunk -> Memory(Dynamic) / RecycleSlot
        TChunk* mFN_Chunk_AllocMemory();        // Chunk <- Memory(Dynamic)
        void mFN_Chunk_FreeMemory(TChunk* p);   // Chunk -> Memory(Dynamic)
    };
    _MACRO_STATIC_ASSERT(sizeof(CMemoryUnitsGroup_List) == _DEF_CACHELINE_SIZE);


    /*----------------------------------------------------------------
    /   메모리풀 : 기본형
    /---------------------------------------------------------------*/
    class _DEF_CACHE_ALIGN CMemoryPool : public IMemoryPool, CUnCopyAble{
        friend class CMemoryPool_Manager;
        friend class CMemoryPool_Allocator_Virtual;
        template<typename T> friend void UTIL::PRIVATE::gFN_Call_Destructor(T* p);
    private:
        CMemoryPool(size_t _TypeSize, UINT32 _IndexThisPool);
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
        // 질의 : 취급하는 유닛 크기 범위
        virtual BOOL mFN_Query_This_UnitsSizeLimit(size_t* pOUT_Min, size_t* pOUT_Max) const override;
        // 질의 : 취급하지 않는 크기를 처리하는 메모리풀(느린 동작)
        virtual BOOL mFN_Query_isSlowMemoryPool() const override;
        // 질의 : 캐시라인 정렬 확인
        virtual BOOL mFN_Query_MemoryUnit_isAligned(size_t AlignSize) const override;
        // 질의 : 통계
        virtual BOOL mFN_Query_Stats(TMemoryPool_Stats* pOUT) override;

        //----------------------------------------------------------------
        //  유닛 예약 : 지정크기 만큼 최대 크기 추가 확장
        //----------------------------------------------------------------
        // 예약 : 메모리 크기 기준
        // 리턴 : 실제 생성된 메모리(0 : 실패)
        virtual size_t mFN_Reserve_Memory__nSize(size_t nByte, BOOL bWriteLog = TRUE) override;
        // 예약 : 유닛수 기준
        // 리턴 : 실제 생성된 유닛수(0 : 실패)
        virtual size_t mFN_Reserve_Memory__nUnits(size_t nUnits, BOOL bWriteLog = TRUE) override;
        //----------------------------------------------------------------
        //  유닛 예약 : 이미 사용된 용량을 포함하여 최대 크기 확장
        //----------------------------------------------------------------
        // 예약 : 메모리 크기 기준
        // 리턴 : 메모리 풀에서 관리하는 전체 메모리
        virtual size_t mFN_Reserve_Memory__Max_nSize(size_t nByte, BOOL bWriteLog = TRUE) override;
        // 예약 : 유닛수 기준
        // 리턴 : 메모리 풀에서 관리하는 전채 유닛
        virtual size_t mFN_Reserve_Memory__Max_nUnits(size_t nUnits, BOOL bWriteLog = TRUE) override;
        //----------------------------------------------------------------
        //  최대사용 메모리 예측
        //  mFN_Reserve_Memory__ ... 계열은 이 메소드의 기능을 포함하여 처리
        //  만약 과도하게 설정시 메모리가 부족할 수 있음
        //  ※ 이 메소드는 통계를 집계하지 않음
        //----------------------------------------------------------------
        // 리턴 : 메모리 풀의 예상된 최대크기(설정된)
        virtual size_t mFN_Hint_MaxMemorySize_Add(size_t nByte, BOOL bWriteLog = TRUE) override;
        // 리턴 : 메모리 풀의 예상된 최대크기(설정된)
        virtual size_t mFN_Hint_MaxMemorySize_Set(size_t nByte, BOOL bWriteLog = TRUE) override;
        //----------------------------------------------------------------
        // 프로세서별 캐시 사용
        virtual BOOL mFN_Set_OnOff_ProcessorCACHE(BOOL _On) override;
        virtual BOOL mFN_Query_OnOff_ProcessorCACHE() const override;
        //----------------------------------------------------------------
        virtual void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On) override;

    private:
        /*----------------------------------------------------------------
        /   데이터 - 읽기
        ----------------------------------------------------------------*/
        // size_t virtual table ptr
        union{
            const size_t m_UnitSize;
            const size_t m_UnitSizeMax;
        };// size_t
        const size_t m_UnitSizeMin;

        const UINT32 m_Index_ThisPool;
        const BOOL m_bUse_Header_per_Unit;

        const BOOL m_bUse_TLS_BIN;
        const BOOL : 32;

        const BOOL m_bShared_UnitsGroup_from_MultiProcessor;
        BOOL m_bUse_OnlyBasket_0;
        // ▲ 문제점
        //      Basket 의 유닛그룹에 접근시 무잠금으로 접근하는 로직으로 인하여
        //      다수의 스레드에서 문제가 될 수 있다
        //      -> 접근한 유닛이 무효화되어 재접근해야 하는 문제가 있을 수 있다
        //      -> 동시 접근성이 높지 않다면 별 문제가 되지 않는다

        // 유닛그룹의 공유 프로세서 가능수 = 유닛그룹의 유닛수 / m_nMinUnitsShare_per_Processor_from_UnitsGroup
        UINT32 m_nMinUnitsShare_per_Processor_from_UnitsGroup;
        // 유닛그룹의 상태 Empty 에서 다른 상태로 변환하기 위한 보유 유닛수량 요구
        UINT32 m_nMinUnits_UnitGroupStateChange_Empty_to_Other;

        CMemoryBasket* m_pBaskets; // n : g_nBaskets( == CPU 코어수)

        /*----------------------------------------------------------------
        /   데이터 - 기타
        ----------------------------------------------------------------*/
        _DEF_CACHE_ALIGN UTIL::LOCK::CSpinLockRW m_Lock_Pool;
        CMemoryBasket* m_pExpansion_from_Basket;
        size_t __FreeSlot_ExpansionMem[6];

        _DEF_CACHE_ALIGN CMemoryPool_Allocator_Virtual m_Allocator;

        _DEF_CACHE_ALIGN CMemoryUnitsGroup_List m_ListUnitGroup_Rest;
        _DEF_CACHE_ALIGN CMemoryUnitsGroup_List m_ListUnitGroup_Full;
        //----------------------------------------------------------------
        _DEF_CACHE_ALIGN size_t m_stats_Units_Allocated; // current

        // 사용자 요청 카운팅
        ::UTIL::NUMBER::TCounting_UINT64 m_stats_OrderCount_Reserve_nSize;
        ::UTIL::NUMBER::TCounting_UINT64 m_stats_OrderCount_Reserve_nUnits;
        ::UTIL::NUMBER::TCounting_UINT64 m_stats_OrderCount_Reserve_Max_nSize;
        ::UTIL::NUMBER::TCounting_UINT64 m_stats_OrderCount_Reserve_Max_nUnits;

        // 사용자 요청 실제 결과
        ::UTIL::NUMBER::TCounting_UINT64 m_stats_Order_Result_Total_nSize;
        ::UTIL::NUMBER::TCounting_UINT64 m_stats_Order_Result_Total_nUnits;

        size_t __FreeSlot_a;
        //----------------------------------------------------------------
        // 사용자 요청중 가장 큰 요구
        UINT64 m_stats_Order_MaxRequest_nSize;
        UINT64 m_stats_Order_MaxRequest_nUnits;
        UINT64 __FreeSlot_b[2];
        
        ::UTIL::NUMBER::TCounting_LockFree_UT m_stats_Counting_Free_BadPTR;
        BOOL   m_bWriteStats_to_LogFile;
        UINT32 __FreeSlot_c;
        UINT64 __FreeSlot_d[2];

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
        void mFN_GiveBack_Unit(TMemoryUnitsGroup* pChunk, TMemoryObject* pUnit);
        void mFN_GiveBack_Units(TMemoryUnitsGroup* pChunk,TMemoryObject* pUnitsF, TMemoryObject* pUnitsL, UINT32 cnt);
        void mFN_GiveBack_PostProcess(TMemoryUnitsGroup* pChunk);

        static BOOL sFN_TLS_InitializeIndex();
        static BOOL sFN_TLS_ShutdownIndex();
        static void* sFN_TLS_Get();
        static void* sFN_TLS_Create();
        static void sFN_TLS_Destroy();

        inline BOOL mFN_Query_UsingHeader_per_Unit() const { return m_bUse_Header_per_Unit; }
        inline BOOL mFN_Query_isLocked() const {return m_Lock_Pool.Query_isLockedWrite();}

    protected:
        CMemoryBasket& mFN_Get_Basket(TMemoryPool_TLS_CACHE* pTLS);
        BOOL mFN_ExpansionMemory(CMemoryBasket& MB);

        TMemoryObject* mFN_Get_Memory_Process();
        TMemoryObject* mFN_Get_Memory_Process__Internal();
        TMemoryObject* mFN_Get_Memory_Process__Internal__Default();
        TMemoryObject* mFN_Get_Memory_Process__Internal__TLS();
        TMemoryUnitsGroup* mFN_Get_Avaliable_UnitsGroup_from_ProcessorStorageBin(CMemoryBasket& mb);
        TMemoryUnitsGroup* mFN_Get_Avaliable_UnitsGroup(CMemoryBasket& mb);

        void mFN_Return_Memory_Process(TMemoryObject* pAddress, const TDATA_BLOCK_HEADER* pH);
        void mFN_Return_Memory_Process__Internal(TMemoryObject* pAddress, const TDATA_BLOCK_HEADER* pH);
        void mFN_Return_Memory_Process__Internal__Default(TMemoryObject* pAddress, const TDATA_BLOCK_HEADER* pH);
        void mFN_Return_Memory_Process__Internal__TLS(TMemoryObject* pAddress);

        void mFN_Request_Connect_UnitsGroup_Step1(TMemoryUnitsGroup* pFirst__Attach_to_Basket, size_t nTotalAllocatedUnits);
        void mFN_Request_Connect_UnitsGroup_Step2__LockFree(TMemoryUnitsGroup* pOther, UINT32 nGroups);

        BOOL mFN_Request_Disconnect_UnitsGroup(TMemoryUnitsGroup* pChunk);
        BOOL mFN_Request_Disconnect_UnitsGroup_Step1(TMemoryUnitsGroup* pChunk);
        BOOL mFN_Request_Disconnect_UnitsGroup_Step2(TMemoryUnitsGroup* pChunk);
        BOOL mFN_Request_Disconnect_VMEM(TDATA_VMEM* pVMEM);

        const TDATA_BLOCK_HEADER* mFN_GetHeader_Safe(TMemoryObject* pAddress) const;
        const TDATA_BLOCK_HEADER* mFN_GetHeader(TMemoryObject* pAddress) const;

        TMemoryObject* mFN_AttachUnitGroup_and_GetUnit(CMemoryBasket& mb, TMemoryUnitsGroup* pChunk);
        void mFN_UnitsGroup_Relocation(TMemoryUnitsGroup* p);
        void mFN_UnitsGroup_Detach_from_All_Processor(TMemoryUnitsGroup* p);

        void mFN_Debug_Report();
        size_t mFN_Counting_KeepingUnits();

        TMemoryUnitsGroup* mFN_Take_UnitsGroup_from_OtherProcessor();

        inline CMemoryUnitsGroup_List& mFN_Get_ListUnitsGroup_Rest(){return m_ListUnitGroup_Rest;}
        inline CMemoryUnitsGroup_List& mFN_Get_ListUnitsGroup_Full(){return m_ListUnitGroup_Full;}
        __declspec(property(get = mFN_Get_ListUnitsGroup_Rest)) CMemoryUnitsGroup_List& ListUnitGroup_Rest;
        __declspec(property(get = mFN_Get_ListUnitsGroup_Full)) CMemoryUnitsGroup_List& ListUnitGroup_Full;

        void mFN_Debug_Counting__Get(CMemoryBasket* pMB);
        void mFN_Debug_Counting__Ret(CMemoryBasket* pMB);
        
        void mFN_ExceptionProcess_InvalidAddress_Deallocation(const void* p);
        void mFN_ExceptionProcess_from_mFN_Request_Disconnect_VMEM(TDATA_VMEM* pVMEM, int iCase);
        static void sFN_ExceptionProcess_from_sFN_TLS_InitializeIndex();
        static void sFN_ExceptionProcess_from_sFN_TLS_ShutdownIndex();
    };
}
}


namespace UTIL{
namespace MEM{
    /*----------------------------------------------------------------
    /   메모리풀 관리자
    /---------------------------------------------------------------*/
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

        BOOL mFN_Initialize_MemoryPool_Shared_Variable();
        BOOL mFN_Destroy_MemoryPool_Shared_Variable();

    protected:
        /*----------------------------------------------------------------
        /   데이터 - 읽기
        ----------------------------------------------------------------*/
        // size_t virtual table ptr
        size_t m_MaxSize_of_MemoryPoolUnitSize;

        size_t _FreeSlot[6];

        /*----------------------------------------------------------------
        /   데이터
        ----------------------------------------------------------------*/
        _DEF_CACHE_ALIGN LOCK::CSpinLockRW      m_Lock__PoolManager;
        _DEF_CACHE_ALIGN CMemoryPool*           m_Array_Pool[GLOBAL::SETTING::gc_Num_MemoryPool] = {nullptr};
        _DEF_CACHE_ALIGN CMemoryPool__BadSize   m_SlowPool;
        _MACRO_STATIC_ASSERT(sizeof(CMemoryPool__BadSize) % _DEF_CACHELINE_SIZE == 0);
        
        // 설정, 통계
        BOOL   m_bWriteStats_to_LogFile;
        ::UTIL::NUMBER::TCounting_LockFree_UT m_stats_Counting_Free_BadPTR;

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
    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        virtual IMemoryPool* mFN_Get_MemoryPool(void* pUserValidMemoryUnit) override;
    #endif


    #if _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE && _DEF_MEMORYPOOL_MAJORVERSION == 1
        /*----------------------------------------------------------------
        /   TLS CACHE 접근 합수 지정
        /---------------------------------------------------------------*/
        virtual void mFN_Set_TlsCache_AccessFunction(TMemoryPool_TLS_CACHE&(*pFN)(void)) override;
    #endif
    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        virtual size_t mFN_Query_Possible_MemoryPools_Num() const override;
        virtual BOOL mFN_Query_Possible_MemoryPools_Size(size_t* _OUT_Array) const override;

        virtual size_t mFN_Query_Limit_MaxUnitSize_Default() const override;
        virtual size_t mFN_Query_Limit_MaxUnitSize_UserSetting() const override;
        virtual BOOL mFN_Set_Limit_MaxUnitSize(size_t _MaxUnitSize_Byte, size_t* _OUT_Result) override;
        virtual BOOL mFN_Reduce_AnythingHeapMemory() override;
    #endif

        virtual void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On) override;

        virtual void mFN_Set_OnOff_Trace_MemoryLeak(BOOL _On) override;
        virtual void mFN_Set_OnOff_ReportOutputDebugString(BOOL _On) override;

    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        virtual BOOL mFN_Set_CurrentThread_Affinity(size_t _Index_CpuCore) override;
        virtual void mFN_Reset_CurrentThread_Affinity() override;

        virtual void mFN_Shutdown_CurrentThread_MemoryPoolTLS() override;

        virtual TMemoryPool_Virsion mFN_Query_Version() const override;
    #endif

    protected:
        CMemoryPool* mFN_CreatePool_and_SetMemoryPoolTable(size_t iTable, size_t UnitSize);

    public:
        CMemoryPool* mFN_Get_MemoryPool_Internal(size_t _Size);
        CMemoryPool* mFN_Get_MemoryPool_QuickAccess(UINT32 _Index);

        static const TDATA_BLOCK_HEADER* sFN_Get_DataBlockHeader_SmallObjectSize(const void* p);
        static const TDATA_BLOCK_HEADER* sFN_Get_DataBlockHeader_NormalObjectSize(const void* p);
        static const TDATA_BLOCK_HEADER* sFN_Get_DataBlockHeader_fromPointer(const void* p);
        static const TDATA_BLOCK_HEADER* sFN_Get_DataBlockHeader_fromPointerQ(const void* p);

        void mFN_Report_Environment_Variable();
        void mFN_Debug_Report();

        static BOOL sFN_Callback_from_DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved);

    protected:
        void mFN_ExceptionProcess_InvalidAddress_Deallocation(const void* p);
    };
}
}
#pragma warning(pop)