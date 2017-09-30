#pragma once
#include "../MemoryPool_Interface.h"
#include "../../../BasisClass/LOCK/Lock.h"
#include "../Number.h"
//----------------------------------------------------------------
//  테스트 옵션(배포버전에 사용하지 않도록 주의)

// Release버전에서 테스트 ON
//#define _DEF_USING_MEMORYPOOL_DEBUG 1

// 프로파일링 ON
//#define _DEF_USING_MEMORYPOOL__PERFORMANCE_PROFILING 1
//----------------------------------------------------------------
//  프로파일링 관련 함수 매크로
//      ※ 컴파일러에 의해 인라인화를 방지
//         MACRO ReturnType FunctionName Parameter
//  _DEF_INLINE_TYPE__PROFILING__FORCEINLINE    // 프로파일링 미사용시 __forceinline
//  _DEF_INLINE_TYPE__PROFILING__DEFAULT        // 프로파일링 미사용시 컴파일러가 판단
//----------------------------------------------------------------
#define _DEF_USING_MEMORYPOOL_GETPROCESSORNUMBER_SIDT_TLS_CACHE _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS

#define _DEF_USING_MEMORYPOOL_TEST_SMALLUNIT__FROM__DBH_TABLE 1

// 메모리 미리가져오기 & 사용하지 않는 메모리 캐시에서 제거
#define _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH 1

// 데이터 병렬성 추가
#define _DEF_USING_MEMORYPOOL_OPTIMIZE__UNITSGROUP_POP_UNITS_VER1 1

// 메모리풀 디버그
#if !defined(_DEF_USING_MEMORYPOOL_DEBUG)
  #if defined(_DEBUG)
    #define _DEF_USING_MEMORYPOOL_DEBUG 1
  #else
    #define _DEF_USING_MEMORYPOOL_DEBUG 0
  #endif
#endif

// 메모리풀 디버그 - 메모리 상태확인
#if !defined(_DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW)
  #if defined(_DEBUG) && _DEF_USING_MEMORYPOOL_DEBUG
    #define _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW 1
  #else
    #define _DEF_USING_MEMORYPOOL_DEBUG__CHECK_OVERFLOW 0
  #endif
#endif
//----------------------------------------------------------------
#if _DEF_USING_MEMORYPOOL__PERFORMANCE_PROFILING
#define _DEF_INLINE_TYPE__PROFILING__FORCEINLINE __declspec(noinline)
#define _DEF_INLINE_TYPE__PROFILING__DEFAULT __declspec(noinline)
#else
#define _DEF_INLINE_TYPE__PROFILING__FORCEINLINE __forceinline
#define _DEF_INLINE_TYPE__PROFILING__DEFAULT
#endif
//----------------------------------------------------------------
#pragma warning(push)
#pragma warning(disable: 4324)
#pragma warning(disable: 4310)
#pragma warning(disable: 4201)
namespace UTIL{
namespace MEM{
    namespace GLOBAL{
    #pragma region 설정(빌드타이밍)
        namespace VERSION{
            // Major 0 ~
            // Minor 0000 ~ 9999
            const UINT16 gc_Version_Major = _DEF_MEMORYPOOL_MAJORVERSION;
            const UINT16 gc_Version_Minor = 1;
        }
        // 256KB
        // Windows 가상메모리 최소 예약단위 : 64KB
        const size_t gc_minAllocationSize = 256 * 1024;
    #pragma endregion
    }
}
}
namespace UTIL{
namespace MEM{
    namespace GLOBAL{
    #if _DEF_USING_DEBUG_MEMORY_LEAK
        extern BOOL g_bDebug__Trace_MemoryLeak;
    #endif
    #if _DEF_USING_MEMORYPOOL_DEBUG
        extern BOOL g_bDebug__Report_OutputDebug;
    #endif
    }
}
}

namespace UTIL{
namespace MEM{


    struct MemoryPool_UTIL{
        static void sFN_Debug_Overflow_Set(void* p, size_t size, BYTE code);
        static void sFN_Debug_Overflow_Check(const void* p, size_t size, BYTE code);
        static void sFN_Report_DamagedMemoryPool();
        static void sFN_Report_DamagedMemory(const void* p, size_t size);
        static void sFN_Report_Invalid_Deallocation(const void* p);
    };
    
    
    struct TMemoryObject : public CUnCopyAble{
        TMemoryObject* pNext;
        size_t mFN_Counting() const;
        //__forceinline TMemoryObject* GetNext(){ return pNext; }
    };
    _MACRO_STATIC_ASSERT(sizeof(TMemoryObject) == sizeof(void*));


    struct _DEF_CACHE_ALIGN TMemoryUnitsGroup : public CUnCopyAble{
        //----------------------------------------------------------------
        // 설정
        static const BOOL sc_bUse_Push_Front = TRUE;
        //----------------------------------------------------------------
        // 타입선언
        // ※ EState 순서가 변경되지 않도록 주의(0 부터 순서대로 채움)
        //    컨테이너 형태로 관리할 타입들은 E_Count_Element 앞에 순서대로 정의
        enum struct EState : UINT32 {
            E_Full = 0,
            E_Rest = 1,
            E_Count_Element, // 이상은 소속 컨테이너에 따른 식별값
            
            E_Empty = 0x40000000,
            E_Busy  = 0x80000000,
            E_Unknown = 0x90000000,
            E_Invalid = 0xFFFFFFFF,
        };

        //--------------------------------
        //  데이터 그룹1
        //      size_t 크기만큼의 첫번째 공간은, 이 객체의 해제후 접근하는 경우 접근하지 않을 가능성이 있는 것으로 한정
        //--------------------------------
        struct{
            TMemoryObject* m_pUnit_F = nullptr;
            TMemoryObject* m_pUnit_B = nullptr;
        }m_Units;

        ::UTIL::LOCK::CSpinLockQ m_Lock;

        // ※ 보유 유닛수 UINT32
        //      1개 그룹에 최대 유닛수는 42억개, 8B 유닛단위 기준 그룹최대 메모리 용량은 약32GB (x86 에서는 문제 없음)
        //      그룹 분할시 수량을 체크하지 않는다. 문제가 되면 추가한다
        UINT32 m_nUnits;
        EState m_State;

        size_t m_ReferenceMask; // 참조한 프로세서 Mask

        void* m_pContainerChunkFocus;
        UINT32 m_iContainerChunk_Detail_Index; // 무효값 : UINT32_MAX

        UINT32 m_Counting_Used_ReservedMemory;
    #if __X64
        UINT64 : 64;
    #endif
        //--------------------------------
        //  데이터 그룹2
        //  X64에서 그룹1과 다른 캐시라인에 정렬되도록
        //--------------------------------
    #if __X86
        byte* m_pStartAddress;

        const struct TDATA_BLOCK_HEADER* m_pFirstDataBlockHeader;
        const size_t m_UnitSize;
        const BOOL   m_bUse_Header_per_Unit;
        UINT32       m_nTotalUnits;

        struct TDATA_VMEM* const m_pOwner;
    #elif __X64
        _DEF_CACHE_ALIGN byte* m_pStartAddress;
        
        const struct TDATA_BLOCK_HEADER* m_pFirstDataBlockHeader;
        const size_t m_UnitSize;
        const BOOL   m_bUse_Header_per_Unit;
        UINT32       m_nTotalUnits;
        
        struct TDATA_VMEM* const m_pOwner;
    #endif
        //----------------------------------------------------------------
        static const size_t sc_InitialValue_ReferenceMask = 0;
        static const UINT32 sc_InitialValue_iContainerChunk_Detail_Index = UINT32_MAX;

        //----------------------------------------------------------------
        TMemoryUnitsGroup(struct TDATA_VMEM* pOwner, size_t UnitSize, BOOL _Use_Header_per_Unit);
        ~TMemoryUnitsGroup();

        static void sFN_ObjectPool_Initialize();
        static void sFN_ObjectPool_Shotdown();

        TMemoryObject* mFN_GetUnit_from_ReserveMemory(); // 외부 사용 금지
        void mFN_Sort_Units(); // 외부 사용 금지

        BOOL mFN_Query_isPossibleSafeDestroy() const;
        BOOL mFN_Query_isDamaged__CurrentStateNotManaged() const;
        void mFN_Report_Failed__Query_isPossibleSafeDestroy() const;
            
        inline BOOL mFN_Query_isManaged_from_Container() const
        {
            const BOOL bTRUE = (nullptr != m_pContainerChunkFocus);
        #if _DEF_USING_MEMORYPOOL_DEBUG
            if(bTRUE)
            {
                switch(m_State)
                {
                case EState::E_Rest:
                case EState::E_Full:
                    break;
                default:
                    _Error(_T("Damaged UnitsGoup[0x%p]"), this);
                }
            }
            else
            {
                _Assert(!m_pContainerChunkFocus && UINT32_MAX == m_iContainerChunk_Detail_Index);
                switch(m_State)
                {
                case EState::E_Rest:
                case EState::E_Full:
                    _Error(_T("Damaged UnitsGoup[0x%p]"), this);
                }
            }
        #endif
            return bTRUE;
        }
        inline BOOL mFN_Query_isFull() const
        {
            return m_nUnits == m_nTotalUnits;
        }
        inline BOOL mFN_Query_isEmpty() const
        {
            if(m_nUnits == 0)
            {
                _Assert(!m_Units.m_pUnit_F && !m_Units.m_pUnit_B && m_Counting_Used_ReservedMemory == m_nTotalUnits);
                return TRUE;
            }
            else
            {
                _Assert(m_Units.m_pUnit_F || m_Counting_Used_ReservedMemory < m_nTotalUnits);
                return FALSE;
            }
        }
        inline BOOL mFN_Query_isInvalidState() const
        {
            return m_State == EState::E_Invalid;
        }
        inline BOOL mFN_Query_isAttached_to_Processor() const
        {
            return (0 != m_ReferenceMask);
        }
        inline BOOL mFN_Query_isAttached_to_Processor(size_t _ProcessorMaskID) const
        {
            _Assert(_ProcessorMaskID);
            return (0 != (m_ReferenceMask & _ProcessorMaskID));
        }
        inline size_t mFN_Query_CountAttached_to_Processor() const
        {
            return _MACRO_BIT_COUNT(m_ReferenceMask);
        }
        inline TMemoryObject* mFN_Pop_Front();
        inline void mFN_Push_Front(TMemoryObject* p);
        inline void mFN_Push_Back(TMemoryObject* p);
        inline void mFN_Push(TMemoryObject* p);

        UINT32 mFN_Pop(TMemoryObject** _OUT_pF, TMemoryObject** _OUT_pL, UINT32 nMaxRequire); // 리턴 : 확보한 유닛수
        void mFN_Push(TMemoryObject* pF, TMemoryObject* pL, UINT32 nUnits);

        const byte* mFN_Query_Valid_Memory_Start() const;
        const byte* mFN_Query_Valid_Memory_End() const;
        BOOL mFN_Test_Ownership(TMemoryObject* pUnit) const;
        void mFN_DebugTest__IsNotFull() const;
    };
#if __X86
    _MACRO_STATIC_ASSERT(sizeof(TMemoryUnitsGroup) == _DEF_CACHELINE_SIZE);
#elif __X64
    _MACRO_STATIC_ASSERT(sizeof(TMemoryUnitsGroup) == _DEF_CACHELINE_SIZE * 2);
#endif
    struct TMemoryUnitsGroupChunk{
        struct _x1{
            TMemoryUnitsGroup m;
        };_MACRO_STATIC_ASSERT(sizeof(_x1) == sizeof(TMemoryUnitsGroup)*1);
        struct _x2{
            TMemoryUnitsGroup m[2];
        };_MACRO_STATIC_ASSERT(sizeof(_x2) == sizeof(TMemoryUnitsGroup)*2);
        struct _x4{
            TMemoryUnitsGroup m[4];
        };_MACRO_STATIC_ASSERT(sizeof(_x4) == sizeof(TMemoryUnitsGroup)*4);
        struct _x8{
            TMemoryUnitsGroup m[8];
        };_MACRO_STATIC_ASSERT(sizeof(_x8) == sizeof(TMemoryUnitsGroup)*8);

    };

    // 이 객체의 잠금 단위는 이 객체를 소유한 객체에서 이루어져야 함
    struct _DEF_CACHE_ALIGN TDATA_VMEM final : public CUnCopyAble{
        TDATA_VMEM(void* pVirtualAddress, size_t AllocateSize);
        // 소멸자는 사용하지 않음

        static TDATA_VMEM* sFN_New_VMEM(size_t AllocateSize);
        static TDATA_VMEM* sFN_Create_VMEM(void* pAllocated, size_t AllocateSize);
        static void sFN_Delete_VMEM(TDATA_VMEM* pThis);

        BOOL mFN_Lock_All_UnitsGroup(BOOL bCheck_FullUnits); // 소유 유닛그룹 모두 잠금(실패시 잠금 해제)
        void mFN_UnLock_All_UnitsGroup();

        size_t mFN_Query_CountUnits__Keeping();
        size_t mFN_Query_CountUnits__Using();

        struct TDATA_BLOCK_HEADER* mFN_Get_FirstHeader();
        const struct TDATA_BLOCK_HEADER* mFN_Get_FirstHeader() const;

    #ifdef _DEBUG
        inline void mFN_Counting_FullGroup_Increment(){ _Assert(m_nUnitsGroup != InterlockedExchangeAdd(&m_cntFullGroup, 1)); }
        inline void mFN_Counting_FullGroup_Decrement(){ _Assert(0 != InterlockedExchangeSubtract(&m_cntFullGroup, 1)); }
    #else
        inline void mFN_Counting_FullGroup_Increment(){ InterlockedExchangeAdd(&m_cntFullGroup, 1); }
        inline void mFN_Counting_FullGroup_Decrement(){ InterlockedExchangeSubtract(&m_cntFullGroup, 1); }
    #endif
        //----------------------------------------------------------------
        // 사용중 변하지 않는 값
        //----------------------------------------------------------------
        UINT32 m_nUnitsGroup; // ~64(코어수 기준)... 매우 큰 크기가 아니라면, 작게 분할
        UINT32 __FreeSlot1;
        TMemoryUnitsGroup* m_pUnitsGroups;

        void* m_pStartAddress;
        size_t m_SizeAllocate;
        size_t m_nAllocatedUnits;

        TDATA_VMEM* m_pPrevious;
        TDATA_VMEM* m_pNext;

        void* m_pOwner; // 현재 소유자
        //----------------------------------------------------------------
        //  m_cntFullGroup , 거의 접근하지 않는 값
        //----------------------------------------------------------------
        _DEF_CACHE_ALIGN volatile UINT32 m_cntFullGroup;
        BOOL m_IsManagedReservedMemory;
        void* m_pReservedMemoryManager;

    };
    _MACRO_STATIC_ASSERT(sizeof(BOOL) == 4);
    _MACRO_STATIC_ASSERT(sizeof(TDATA_VMEM) == _DEF_CACHELINE_SIZE*2);


    class _DEF_CACHE_ALIGN CMemoryBasket : public CUnCopyAble{
    public:
        CMemoryBasket(size_t _indexProcessor);

        // Attach / Detach 는 호출전에 __assume 를 이용하여 최대한 최적화가 되도록 한다
        inline TMemoryUnitsGroup* mFN_Get_UnitsGroup(){ return m_pAttached_Group; }
        BOOL mFN_Attach(TMemoryUnitsGroup* p);
        BOOL mFN_Detach(TMemoryUnitsGroup* pCurrent);

    private:
        TMemoryUnitsGroup* m_pAttached_Group;

    public:
        ::UTIL::LOCK::CSpinLockQ m_Lock;

        const size_t m_MaskID;
        const size_t m_MaskID_Inverse;
        size_t __FreeSlot1;

        #if _DEF_USING_MEMORYPOOL_DEBUG
        ::UTIL::NUMBER::TCounting_LockFree_UINT64 m_stats_cntGet;
        ::UTIL::NUMBER::TCounting_LockFree_UINT64 m_stats_cntRet;
        ::UTIL::NUMBER::TCounting_LockFree_UINT32 m_stats_cntCacheMiss_from_Get_Basket;
        ::UTIL::NUMBER::TCounting_LockFree_UINT32 m_stats_cntRequestShareUnitsGroup;
        //----------------------------------------------------------------
        static const UINT64 sc_Debug_MaxCountingUINT32 = UINT32_MAX;
        static const UINT64 sc_Debug_MaxCountingUINT64 = UINT64_MAX;
        #endif
    };
    _MACRO_STATIC_ASSERT(sizeof(CMemoryBasket) == _DEF_CACHELINE_SIZE);





    #pragma region DATA_BLOCK and Table
    struct _DEF_CACHE_ALIGN TDATA_BLOCK_HEADER{
        // 멤버 데이터는 8유닛(1유닛당 8Byte)로 한정
        enum struct _TYPE_UnitsAlign : UINT32{
            // 가능한 다른 메모리패턴과 겹치지 않도록 유니크 하도록
            E_Invalid   = 0, // 무효
            E_Normal_H1 = 0x012E98F5,
            E_Normal_HN = 0x3739440C,
            E_Other_HN  = 0x3617DCF9,
        };
        //----------------------------------------------------------------
        // 1슬롯
        //----------------------------------------------------------------
        _TYPE_UnitsAlign    m_Type;
        struct{
            UINT16 high;    //(0 ~(64KB / sizeof(void*))
            UINT16 low;     //(0 ~(64KB / sizeof(void*))
        }m_Index;
        //----------------------------------------------------------------
        // 2슬롯
        //----------------------------------------------------------------
        struct{
            void* m_pValidPTR_S; //사용자 영역 시작
            void* m_pValidPTR_L; //사용자 영역 마지막
        }m_User;
        //----------------------------------------------------------------
        // 2슬롯
        //----------------------------------------------------------------
        TDATA_BLOCK_HEADER* m_pFirstHeader;
        IMemoryPool* m_pPool;
        //----------------------------------------------------------------        
        // 3슬롯
        //----------------------------------------------------------------
        union{
            struct{
                UINT32 m_RealUnitSize; // 1 유닛이 사용한 실제 공간
                UINT32 m_nUnitsGroups;
                TMemoryUnitsGroup* m_pUnitsGroups;
                size_t m_Size_per_Group; // 그룹별 분배된 메모리 크기(가장 마지막 그룹은 이보다 적을 수 있다)
            }ParamNormal;

            struct{
                size_t m_RealUnitSize;
                size_t __FreeSlot[2];
            }ParamBad;
        };

        __forceinline BOOL mFN_Query_ContainPTR(const void* p) const
        {
            if(p < m_User.m_pValidPTR_S || m_User.m_pValidPTR_L < p)
                return FALSE;
            return TRUE;
        }
        __forceinline BOOL mFN_Query_AlignedPTR(const void* p) const
        {
            _Assert(m_Type == _TYPE_UnitsAlign::E_Normal_H1 || m_Type == _TYPE_UnitsAlign::E_Normal_HN);
            _Assert((size_t)m_User.m_pValidPTR_S <= (size_t)p);
            const size_t offset = (size_t)p - (size_t)m_User.m_pValidPTR_S;
            if(offset % ParamNormal.m_RealUnitSize)
                return FALSE;
            return TRUE;
        }
        __forceinline size_t mFN_Calculate_UnitsGroupIndex(const void* p) const
        {
            _Assert(m_Type == _TYPE_UnitsAlign::E_Normal_H1 || m_Type == _TYPE_UnitsAlign::E_Normal_HN);
            _Assert(p && ParamNormal.m_pUnitsGroups && ParamNormal.m_Size_per_Group);
            const size_t offset = (size_t)p - (size_t)m_User.m_pValidPTR_S;
            const size_t index = offset / ParamNormal.m_Size_per_Group;
            _AssertRelease(index < ParamNormal.m_nUnitsGroups);
            return index;
        }
        __forceinline TMemoryUnitsGroup* mFN_Get_UnitsGroup(size_t index)
        {
            _Assert(ParamNormal.m_pUnitsGroups && index < ParamNormal.m_nUnitsGroups);
            return ParamNormal.m_pUnitsGroups+index;
        }
        __forceinline const TMemoryUnitsGroup* mFN_Get_UnitsGroup(size_t index) const
        {
            _Assert(ParamNormal.m_pUnitsGroups && index < ParamNormal.m_nUnitsGroups);
            return ParamNormal.m_pUnitsGroups+index;
        }
        BOOL operator == (const TDATA_BLOCK_HEADER& r) const;
        BOOL operator != (const TDATA_BLOCK_HEADER& r) const;
    };
    _MACRO_STATIC_ASSERT(sizeof(TDATA_BLOCK_HEADER) == _DEF_CACHELINE_SIZE); // 반드시 캐시라인 크기 또는 배수에 맞춰야 한다

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

        //MaxSlot High / Low는 각각 Max 16BIT(Slot 16,384)
        // 슬롯과 VMEM 는 1:1
        // VMEM 최소 크기 단위 : GLOBAL::gc_minAllocationSize(최소 64KB)
        static const size_t sc_Rate_minAllocationSize = GLOBAL::gc_minAllocationSize / 64 / 1024;
    #if __X86
        // ~ 4GB
        static const UINT32 sc_MaxSlot_TableHigh = 256 / sizeof(void*);
        static const UINT32 sc_MaxSlot_TableLow = 4 * 1024 / sizeof(void*) / sc_Rate_minAllocationSize;
    #elif __X64
        // ~ 2TB
        static const UINT32 sc_MaxSlot_TableHigh = 64 * 1024 / sizeof(void*);
        static const UINT32 sc_MaxSlot_TableLow = 32 * 1024 / sizeof(void*) / sc_Rate_minAllocationSize;
    #endif
        static const UINT64 sc_MaxValid_ManagedMemorySize__BYTE = (UINT64)GLOBAL::gc_minAllocationSize * sc_MaxSlot_TableHigh * sc_MaxSlot_TableLow;
        static const UINT64 sc_MaxValid_ManagedMemorySize__KB = sc_MaxValid_ManagedMemorySize__BYTE / 1024;
        static const UINT64 sc_MaxValid_ManagedMemorySize__MB = sc_MaxValid_ManagedMemorySize__KB / 1024;
        static const UINT64 sc_MaxValid_ManagedMemorySize__GB = sc_MaxValid_ManagedMemorySize__MB / 1024;
        static const UINT64 sc_MaxValid_ManagedMemorySize__TB = sc_MaxValid_ManagedMemorySize__GB / 1024;
        // sc_MaxSlot_TableLow 크기에 따른 최대 관리 크기 계산
        //      조건 가정
        //      sc_MaxSlot_TableHigh 64KB
        //          x86[16,384 Slot] , x64[8,192 Slot]
        //      메모리풀 최소 할당단위 64KB
        // TableLowSize x86(Slot    MaxAlloc)   x64(Slot    maxAlloc)
        // 1KB              256     256GB           128     64GB
        // 2KB              512     512GB           256     128GB
        // 4KB              1,024   1,024GB         512     256GB
        // 8KB              2,048   2,048GB         1,024   512GB
        // 16KB             4,096   4,096GB         2,048   1,024GB
        // 32KB             8,192   8,192GB         4,096   2,048GB
        // 64KB             16,384  16,384GB        8,192   4,096GB
        // ※ x86 4GB x64 2TB 까지 지원 : https://blogs.technet.microsoft.com/sankim/2009/11/03/windows-7-w2k8-server-r2version-6-1-32bit-64bit/
        _MACRO_STATIC_ASSERT(1 < sc_MaxSlot_TableHigh && sc_MaxSlot_TableHigh <= UINT16_MAX);
        _MACRO_STATIC_ASSERT(1 < sc_MaxSlot_TableLow && sc_MaxSlot_TableLow <= UINT16_MAX);

    public:
        CTable_DataBlockHeaders();
        ~CTable_DataBlockHeaders();

    private:
        TDATA_BLOCK_HEADER** m_ppTable[sc_MaxSlot_TableHigh] = {0}; // 64KB

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
        //__forceinline TDATA_BLOCK_HEADER* mFN_Get_Link(UINT32 h, UINT32 l)
        //{
        //    auto n = h * sc_MaxSlot_TableLow + l;
        //    if(m_cnt_TotalSlot <= n)
        //        return nullptr;

        //    return m_ppTable[h][l];
        //}
        __forceinline const TDATA_BLOCK_HEADER* mFN_Get_Link(UINT32 h, UINT32 l) const
        {
            auto n = h * sc_MaxSlot_TableLow + l;
            if(m_cnt_TotalSlot <= n)
                return nullptr;

            return m_ppTable[h][l];
        }
        void mFN_Register(TDATA_BLOCK_HEADER* p);
        void mFN_UnRegister(TDATA_BLOCK_HEADER* p);
        BOOL mFN_Query_isRegistered(const TDATA_BLOCK_HEADER* p);
    };
    _MACRO_STATIC_ASSERT(sizeof(CTable_DataBlockHeaders) % _DEF_CACHELINE_SIZE == 0);
#pragma endregion



#pragma region CMemoryPool_Allocator_Virtual
#pragma pack(push, 4)
    struct TMemoryPoolAllocator_Virtual__SettingVMEM{
        size_t m_MemoryExpansionSize; // VMEM 할당 크기
        UINT32 m_nUnits_per_VMEM; // VMEM 당 유닛수
        UINT32 m_nGroups_per_VMEM; // VMEM 당 유닛그룹 분할수
        UINT32 m_nDistributeUnits_per_Group__VMEM; // 유닛그룹에 분배하는 유닛수(마지막 유닛그룹을 제외한: 마지막 유닛그룹은 나머지 유닛을 분배)
    };
    struct TMemoryPoolAllocator_Virtual__Setting{
        size_t m_UnitSize;

        BOOL m_bUse_Header_per_Unit;
        BOOL _FreeSlot;

        TMemoryPoolAllocator_Virtual__SettingVMEM m_VMEM_Small, m_VMEM_Big;
    };
    _MACRO_STATIC_ASSERT(sizeof(TMemoryPoolAllocator_Virtual__SettingVMEM) % 4 == 0);
    _MACRO_STATIC_ASSERT(sizeof(TMemoryPoolAllocator_Virtual__Setting) % 4 == 0);
#pragma pack(pop)
    class _DEF_CACHE_ALIGN CMemoryPool_Allocator_Virtual final : public CUnCopyAble{
        friend class CMemoryPool;
    public:
        CMemoryPool_Allocator_Virtual(size_t _TypeSize, UINT32 _IndexThisPool, CMemoryPool* pPool, BOOL _bUse_Header_per_Unit);
        ~CMemoryPool_Allocator_Virtual();

    private:
        void mFN_Release_All_VirtualMemory();
        void mFN_RecycleSlot_All_Free();

    private:
        /*----------------------------------------------------------------
        /   데이터 - 정적
        ----------------------------------------------------------------*/
        CMemoryPool* m_pPool;
        const TMemoryPoolAllocator_Virtual__Setting m_SETTING;
        _MACRO_STATIC_ASSERT(sizeof(TMemoryPoolAllocator_Virtual__Setting) <= 56);
        /*----------------------------------------------------------------
        /   데이터 - 동적
        /   x86의 경우 가능한 작게
        ----------------------------------------------------------------*/
        _DEF_CACHE_ALIGN size_t m_LastKnownValue_VMEM_CACHE__MaxStorageMemorySize; // 마지막에 확인한 VMEM LIST(CACHE)의 최대크기
        
        union{
            UINT32 m_nRecycle_UnitsGroupsAll;
            UINT32 m_nRecycle_UnitsGroupsSmall;
        };
        UINT32 m_nRecycle_UnitsGroupsBig;

        _DEF_CACHE_ALIGN struct TRecycleSlot_UnitsGroups{
        #pragma region 설정
            static const UINT32 sc_Small_SlotSize = 8;
            static const UINT32 sc_Big_SlotSize = 8;
            static const UINT32 sc_All_SlotSize = sc_Small_SlotSize + sc_Big_SlotSize;
            
            static const UINT32 sc_Small_AllocCount = 4;
            static const UINT32 sc_Big_AllocCount = 4;
            static const UINT32 sc_All_AllocCount = 4;
            static const UINT32 sc_Small_FreeCount = 4;
            static const UINT32 sc_Big_FreeCount = 4;
            static const UINT32 sc_All_FreeCount = 8;
        #pragma endregion
            
            union{
                TMemoryUnitsGroup* m_All[sc_All_SlotSize];
                struct{
                    TMemoryUnitsGroup* m_Small[sc_Small_SlotSize];
                    TMemoryUnitsGroup* m_Big[sc_All_SlotSize];
                };
            };
        }m_RecycleSlot_UnitsGroups; // x86(64B) x64(128B) : 합계 16슬롯 사용시

        /*----------------------------------------------------------------
        /   데이터 - 동적
        ----------------------------------------------------------------*/
        _DEF_CACHE_ALIGN TDATA_VMEM* m_pVMEM_First; // 양방향 연결 유지

        ::UTIL::NUMBER::TCounting_UT m_stats_cnt_Succeed_VirtualAlloc;
        ::UTIL::NUMBER::TCounting_UT m_stats_cnt_Succeed_VirtualFree;
        ::UTIL::NUMBER::TCounting_UINT64 m_stats_size_TotalAllocated;  // 합계 : 할당크기
        ::UTIL::NUMBER::TCounting_UINT64 m_stats_size_TotalDeallocate; // 합계 : 해제크기

        size_t m_stats_Current_AllocatedSize;
        ::UTIL::NUMBER::TCounting_UT m_stats_Max_AllocatedSize;

        struct CVMEM_CACHE* m_pVMEM_CACHE__LastAccess; // 빠른 VMEM CACHE 접근 PTR

    public:
        size_t mFN_MemoryPredict_Add(size_t _Byte, BOOL bWriteLog);
        size_t mFN_MemoryPredict_Set(size_t _Byte, BOOL bWriteLog);
        size_t mFN_MemoryPreAlloc_Add(size_t _Byte, BOOL bWriteLog);
        size_t mFN_MemoryPreAlloc_Set(size_t _Byte, BOOL bWriteLog);
        struct TDATA_VMEM* mFN_Expansion();

    private:
        struct TDATA_VMEM* mFN_Expansion_PRIVATE(const TMemoryPoolAllocator_Virtual__SettingVMEM& setting);
        size_t mFN_MemoryPreAlloc(size_t _Byte, BOOL bWriteLog);

        void mFN_Register_VMEM(TDATA_VMEM* p);
        void mFN_UnRegister_VMEM(TDATA_VMEM* p);

        TDATA_VMEM* mFN_VMEM_New(size_t _AllocSize);
        TDATA_VMEM* mFN_VMEM_New__Slow(size_t _AllocSize);
        void mFN_VMEM_Delete(TDATA_VMEM* p);

        void mFN_VMEM_Close(TDATA_VMEM* p);
        BOOL mFN_VMEM_Build_UnitsGroups(TDATA_VMEM& VMEM, const TMemoryPoolAllocator_Virtual__SettingVMEM& settingVMEM);
        BOOL mFN_VMEM_Destroy_UnitsGroups_Step1__LockFree(TDATA_VMEM& VMEM);
        void mFN_VMEM_Destroy_UnitsGroups_Step2(TMemoryUnitsGroup* pUnitsGroups, UINT32 nGroups);

        BOOL mFN_VMEM_Distribute_to_UnitsGroups(TDATA_VMEM& VMEM, const TMemoryPoolAllocator_Virtual__SettingVMEM& settingVMEM);
        //----------------------------------------------------------------
        TMemoryUnitsGroup* mFN_Alloc_UnitsGroups_TypeAll();
        TMemoryUnitsGroup* mFN_Alloc_UnitsGroups_TypeSmall();
        TMemoryUnitsGroup* mFN_Alloc_UnitsGroups_TypeBig();
        void mFN_Free_UnitsGroups_TypeAll(TMemoryUnitsGroup* p);
        void mFN_Free_UnitsGroups_TypeSmall(TMemoryUnitsGroup* p);
        void mFN_Free_UnitsGroups_TypeBig(TMemoryUnitsGroup* p);
        //----------------------------------------------------------------
        size_t mFN_Update__MaxAllocatedSize_Add(size_t _Byte);
        size_t mFN_Update__MaxAllocatedSize_Set(size_t _Byte);
        size_t mFN_Convert_Byte_to_ValidExpansionSize(size_t _Byte);
        void mFN_Request__VMEM_CACHE__Expansion__MaxStorageMemorySize();
        //----------------------------------------------------------------
        static size_t sFN_Calculate__VMEMMemory_to_Units(size_t _Byte, size_t _UnitSize, BOOL _bUse_Header_per_Unit);
        size_t mFN_Calculate__VMEMMemory_to_Units(size_t _Byte) const;
        size_t mFN_Calculate__Units_to_MemorySize(size_t _nUnits) const;
        //----------------------------------------------------------------
        static TMemoryPoolAllocator_Virtual__Setting sFN_Initialize_SETTING(size_t _UnitSize, UINT32 _IndexThisPool, CMemoryPool* pPool, BOOL _bUse_Header_per_Unit);
        static UINT32 sFN_Calculate__nUnitsGroups_from_Units(size_t _UnitSize, size_t _nUnits);
    };
    _MACRO_STATIC_ASSERT(sizeof(CMemoryPool_Allocator_Virtual) % _DEF_CACHELINE_SIZE == 0);
    #pragma endregion


#pragma warning(pop)
}
}

namespace UTIL{
namespace MEM{

    TMemoryObject* TMemoryUnitsGroup::mFN_Pop_Front()
    {
        _Assert(m_Lock.Query_isLocked_from_CurrentThread());
        _Assert(0 < m_nTotalUnits);

        TMemoryObject* pRet;

        if(m_Units.m_pUnit_F)
        {
            _Assert(0 < m_nUnits);

            pRet = m_Units.m_pUnit_F;
            if(m_Units.m_pUnit_F->pNext)
            {
                _CompileHint(m_Units.m_pUnit_F != m_Units.m_pUnit_F->pNext);
                m_Units.m_pUnit_F = m_Units.m_pUnit_F->pNext;

            #if _DEF_USING_MEMORYPOOL_OPTIMIZE__CACHELINE_PREFETCH_AND_FLUSH
                //prefetch하지 않는 편이 더 성능이 좋음
                //_mm_prefetch((const char*)&m_Units.m_pUnit_F, _MM_HINT_T0);
            #endif
            }
            else
            {
                m_Units.m_pUnit_F = m_Units.m_pUnit_B = nullptr;
            }
        }
        else if(m_Counting_Used_ReservedMemory < m_nTotalUnits)
        {
            _Assert(0 < m_nUnits);

            pRet = mFN_GetUnit_from_ReserveMemory();
        }
        else
        {
            return nullptr;
        }

        const BOOL bNeedDecrementFullGroupCounting = mFN_Query_isFull();

        m_nUnits--;

        if(bNeedDecrementFullGroupCounting)
            m_pOwner->mFN_Counting_FullGroup_Decrement();

        return pRet;
    }
    void TMemoryUnitsGroup::mFN_Push_Front(TMemoryObject* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(m_Lock.Query_isLocked() && p != nullptr);
        _Assert(0 < m_nTotalUnits);
        _Assert(m_nUnits < m_nTotalUnits);
        _CompileHint(!mFN_Query_isFull() && m_Units.m_pUnit_F != p && m_Units.m_pUnit_B != p);

        m_nUnits++;
        if(mFN_Query_isFull())
        {
            mFN_Sort_Units();
            m_pOwner->mFN_Counting_FullGroup_Increment();
            return;
        }

        p->pNext = m_Units.m_pUnit_F;
        if(m_Units.m_pUnit_F)
            m_Units.m_pUnit_F = p;
        else
            m_Units.m_pUnit_F = m_Units.m_pUnit_B = p;
    #pragma warning(pop)
    }
    void TMemoryUnitsGroup::mFN_Push_Back(TMemoryObject* p)
    {
    #pragma warning(push)
    #pragma warning(disable: 6011)
        _Assert(m_Lock.Query_isLocked() && p != nullptr);
        _Assert(0 < m_nTotalUnits);
        _Assert(m_nUnits < m_nTotalUnits);
        _CompileHint(!mFN_Query_isFull() && m_Units.m_pUnit_F != p && m_Units.m_pUnit_B != p);

        m_nUnits++;
        if(mFN_Query_isFull())
        {
            mFN_Sort_Units();
            m_pOwner->mFN_Counting_FullGroup_Increment();
            return;
        }

        if(m_Units.m_pUnit_B)
            m_Units.m_pUnit_B->pNext = p;
        else
            m_Units.m_pUnit_F = p;

        p->pNext = nullptr;
        m_Units.m_pUnit_B = p;
    #pragma warning(pop)
    }
    void TMemoryUnitsGroup::mFN_Push(TMemoryObject* p)
    {
        if(sc_bUse_Push_Front)
            mFN_Push_Front(p);
        else
            mFN_Push_Back(p);
    }
}
}