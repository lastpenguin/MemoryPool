#pragma once
#pragma pack(push, 4)
// 설명 작성은 다음 설정에서 작성 되었습니다
// Font      : 돋움체
// Font Size : 10 
/*----------------------------------------------------------------
/   메모리풀
/
/   테스트결과
/       무작위 가변크기 할당에서 TBBMalloc , LFH , TCMalloc 에 비해 빠른 성능을 보입니다
/       (이때 TBBMalloc와 비교하여 128 이하의 단위의 할당의 경우 낮은 성능을 보입니다)
/       2015 11월에 TBBMalloc TCMalloc 등과 비교
/----------------------------------------------------------------
/   사용 용어 / 정의
/       여기서 [프로세서]는 CPU의 1개 [코어]를 말합니다
/       IMemoryPool 에 질의하는 크기는 Commit 크기가 아닌 주소공간 예약 크기 입니다
/       실제 사용 메모리는 주소공간 에약 크기보다 작을 수 있습니다.
/----------------------------------------------------------------
/   초기 설정(※ 버전2 이상에서 사용하지 않습니다)
/       _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS 활성화의 경우
/           사용자는 메모리풀이 포함된 DLL에
/           사용하는 정적 라이브러리 또는 응용 프로그램에서
/           MEMORYPOOL TLS CACHE 접근 함수를 지정해야 합니다
/               IMemoryPool_Manager::mFN_Set_TlsCache_AccessFunction
/               ※ 이 메소드는 반드시 LIB 또는 exe 프로젝트에서 g_pMem 을 얻은후 실행해야 합니다.
/
/           기본형은 static TMemoryPool_TLS_CACHE& TMemoryPool_TLS_CACHE::sFN_Default_Function() 입니다
/           이 것은 흔히 DLL에서 TLS를 TlsAlloc, TlsFree, TlsSetValue, TlsGetValue 동적처리 하는 것을
/           정적처리하기 위한 트릭입니다.
/           (실제 TLS 의 주소는 이것을 사용하는 LIB 또는 exe 에 위치하도록 함)
/
/       ※ DLL에서 DLL을 사용하는 경우를 위하여 버전2에서는 동적방식을 취합니다.
/----------------------------------------------------------------
/   사용법 :
/       (다음의 방법들은 모두 혼용사용이 가능합니다)
/
/       #1) 메모리풀을 적용하려는 객체는 CMemoryPoolResource를 상속받습니다.
/           예:) class CTest1 : public CMemoryPoolResource
/           예:) struct TTest1 : CMemoryPoolResource
/           new delete 를 이용하여 사용합니다.
/           ※ 이미 작성된 코드에 쉽게 적용하는데 유리합니다.
/           ※ 주의 : 자식 클래스까지 모두 메모리풀의 영향을 받습니다.
/
/       #2) 객체단위에 대하여 만약 상속을 사용하지 않고 특정상황에만 사용하고 싶다면
/           다음의 매크로를 사용합니다.
/           (CMemoryPoolResource를 상속 받은 타입또한 가능합니다.)
/           ■ 매크로 버전 : 할당자 / 소멸자 비 호출
/               _MACRO_ALLOC__FROM_MEMORYPOOL(Address)
/               _MACRO_FREE__FROM_MEMORYPOOL(Address)
/           ■ 매크로 버전 : 할당자 / 소멸자 호출
/               _MACRO_NEW__FROM_MEMORYPOOL(Address, Constructor)
/               _MACRO_DELETE__FROM_MEMORYPOOL(Address)
/
/
/       #3) 만약 할당하려는 크기가 가변적(예를 들어 문자열버퍼) 이라면
/           메모리풀관리자에 직접 접근하여 다음의 메소드를 사용합니다
/           IMemoryPool_Manager::mFN_Get_Memory
/           IMemoryPool_Manager::mFN_Return_Memory
/           IMemoryPool_Manager::mFN_Get_Memory__AlignedCacheSize       // 캐시라인 정렬 버전
/           IMemoryPool_Manager::mFN_Return_Memory__AlignedCacheSize    // 캐시라인 정렬 버전
/           ■ 매크로 버전 : 할당자 / 소멸자 비 호출
/               _MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC
/               _MACRO_FREE__FROM_MEMORYPOOL_GENERIC
/           ※ malloc / free 를 대신하기에 적합한 방법입니다.
/
/
/       #M) 좀더 단계를 줄여 속도를 높이고 싶다면,
/           IMemoryPool_Manager::mFN_Get_MemoryPool 를 통해 메모리풀 인터페이스 참조를 계속 보관하며
/           직접접근하여 메모리를 Get / Return 할 수 있습니다.
/           ■ IMemoryPool 에 접근하는 방법
/               gFN_Get_MemoryPool
/               gFN_Get_MemoryPool__AlignedCacheSize
/               CMemoryPoolResource::__Get_MemoryPool<Type>
/               IMemoryPool_Manager::mFN_Get_MemoryPool
/           ※ Get / Return 은 종류에 따라 주의깊게 짝을 맞춰 사용해야.
/           ※ 생성자 소멸자 호출을 사용자가 직접 해야 합니다.
/
/       적용 편의성 : #1 > #2 > #3 > #M
/       성능        : #M > #2 > #1 = #3
/                   성능의 차는 할당시 메모리풀에 접근하는 속도의 차이입니다
/                   이것은 사용자가 접근 방식을 수정 또는 추가하여 개선이 가능합니다
/
/       ※ 캐시라인 정렬된 메모리를 원하마면 캐시라인크기의 배수를 요청하십시오.
/          만약 그럴 수 없다면 mFN_Get_Memory__AlignedCacheSize 를 사용하십시오.
/       ※ 방법 1)을 적용한 타입에 다른 방법을 사용해도 동작에는 문제가 없습니다
/
/       CMemoryPoolResource을 상속 받은 객체는 메모리풀 인터페이스를 다음과 같이 얻을 수 있습니다.
/           __Get_MemoryPool<T>() 또는 __Get_MemoryPool(sizeof(T))
/----------------------------------------------------------------
/   사용법(기타) :
/   STL Container 의 Allocator 교체 할 경우 다음을 사용
/       TAllocator
/           이것은 vector 같은 매우 가변적 크기 할당에 사용하지 않는 것이 좋습니다
/----------------------------------------------------------------
/   성능향상팁 :
/           안전하지 않은 반납 매소드 사용
/               mFN_Return_MemoryQ
/               이 매소드는 헤더의 손상을 제대로 확인하지 않고 반납된 주소를 받아들입니다.
/               만약 이 헤더가 손상된 경우 메모리풀은 오작동 하게 되며, 런타임 에러를 발생시킬 것 입니다
/               만약 사용하는 프로젝트에서 버퍼 오버플로우 실수가 있다면 사용해서는 안됩니다.
/               
/               반대로, 기본 매소드인 mFN_Return_Memory 의 경우...
/                   반납된 주소의 헤더를 최대한 확인하며,
/                   손상된 경우, 사용자의 반납을 무시합니다(이경우 메모리 릭이 발생하게 됩니다)
/                   ※ 하지만 디버그 버전이 아니라면 적당선 까지 테스트 합니다
/
/           멀티 코어에서 가짜 공유 방지
/               캐시라인 크기(보통 64바이트)의 배수에 객체 또는 요구 메모리 크기를 맞추십시오
/
/           메모리풀 확장시 블록크기 설정(※ 버전2 이상에서 사용하지 않습니다)
/               IMemoryPool::mFN_Set_ExpansionOption__BlockSize
/               다음을 설정할 수 있습니다.
/                   - 메모리풀 확장시, 생성되는 블럭수 제한
/                   - 블럭당 유닛 또는 크기 설정
/
/           필요한 메모리를 미리 예약
/               IMemoryPool::mFN_Reserve_Memory__nSize
/               IMemoryPool::mFN_Reserve_Memory__nUnits
/               초기에 예상되는 수요를 메모리풀에 미리 예약을 한다면,
/               원하지 않는 시기에 긴 대기시간을 방지할 수 있습니다(메모리풀이 보유메모리가 부족한 경우, 추가 확보 작업으로 인한)
/               또한 보통은 또한 일정 크기의 단위로 여러번에 걸쳐 메모리풀을 확장하는 경우,
/               할당해 사용하지 못하는 부분들이 생기는데, 이런 낭비를 줄일 수 있습니다
/               ※ STL Container Reserve 와는 달리 이미 보유한 메모리들은 계산에 포함하지 않습니다
/               ※ 메모리풀 V2 에서는 Reserve를 통해 확보한 메모리는 프로그램 종료전까지 OS에 반납하지 않습니다
/                  계속 유지할 필요한 크기만큼만 적용해야 합니다
/
/           필요한 메모리를 미리 예약 - 버전2에서 추가
/               IMemoryPool::mFN_Reserve_Memory__Max_nSize
/               IMemoryPool::mFN_Reserve_Memory__Max_nUnits
/               STL Container Reserve 의 목적과 유사합니다
/               (만약, 이미 사용중인 메모리 크기가 사용자 요구보다 크다면 사용자 명령은 무시합니다)
/
/           최대 크기 예측 - 버전2에서 추가
/               IMemoryPool::mFN_Hint_MaxMemorySize_Add
/               IMemoryPool::mFN_Hint_MaxMemorySize_Set
/               메모리를 미리 할당하는 기능과 달리, 이 기능은 실행시 메모리를 자원을 사용하지 않으며 즉시 처리됩니다
/                   OS에서 가상메모리에 실제메모리를 매핑하는 과정에 시간 소모가 있는데
/                   메모리풀은 이를 회피하기 위해 최대 수요를 계산하여, OS에 반납하지 않고 재활용합니다
/                   사용자가 요청하지 않더라도 이것은 항상 처리합니다
/               사용자는 이 기능을 이용해 최대 수요를 즉시 Add/Set 할 수 있습니다
/
/           스레드의 프로세서친화도 설정
/               이 메모리풀은 CPU의 코어들을 기준으로 임시 저장소를 가집니다(멀티 스레드 동작을 위하여)
/               이때, 문제는 다중코어 CPU는 코드상 1스레드로 동작하게 하더라도 실제로는 여러 프로세서로 분할하여 명령을 처리하는 경우가 있음
/               (스레드 친화도 설정을 하지 않았다면)
/               이런 경우 다음의 문제들이 발생할 가능성이 높습니다
/           1. 메모리 낭비 가능성
/               어떤 코어에 대한 저장소가 사용된 후 그후 사용되지 않는 경우 그 내부에 보관한 메모리의 낭비
/               (각 프로세서 전용 캐시는 보유하는 유닛을 최소한으로 유지하려 하기 때문에 낭비는 크지 않으며 필요시 공유합니다.)
/               최종 사용자 환경의 CPU 물리적 코어의 수를 고려하여, 이런 경우 다음의 식으로 여유분을 계산하십시오
/               = 코어수 * max(페이지크기(보통4KB) , Unit Size)
/           2. 메모리풀의 처리 속도 문제
/               예를 들면 CPU 코어0번의 저장소에 대하여 스레드A, B가 동시 접근하는 경우(공유 자원에 대한 경쟁)
/           ※ SetThreadAffinity 등으로 스레드별 프로세서 친화도를 설정하여 스레드가 동일 프로세서에서 동작할 가능성이 높다면
/              위 문제점을 개선할 수 있습니다
/
/           ※ 사용자의 접근하는 객체 크기단위 <60> <64> 등의 메모리풀은 별개의 메모리풀이 아닐 수 있습니다.
/              하나의 메모리풀은(a~b)크기를 관리하기 때문입니다
/----------------------------------------------------------------
/   제한사항:
/       #1) new[] 및 delete[]는 지원하지 않습니다.
/           기본동작을 합니다. ( :: operator new[], :: operator delete [] )
/
/       #2) 상속의 제한사항(CMemoryPoolResource를 상속받은 객체를 업캐스팅할때의 문제점)
/           예:) - class CBasis
/                  class CA : public CBasis, public CMemoryPoolResource
/                  CBasis p = new CA
/                  delete p;   // CMemoryPoolResource::operator delete 가 호출되지 않습니다.(런타임 에러가 발생하게 됩니다.)
/           -> 이것을 해결하기 위해서는 현재 객체와, 부모 객체가 모두 virtual table을 가지도록 합니다.
/              이경우 CObjectResource::operator delete 는 올바르게 호출되며, 위의 예제코드는 동작이 가능해집니다.
/       ※ CMemoryPoolResource 는 사용자가 객체의 멤버 데이터를 메모리에  정렬하는 경우에 대비하기 위해
/          가상 소멸자를 포함하지 않았습니다
/
/       #3) IMemoryPool::mFN_Reserve_Memory__nSize IMemoryPool::mFN_Reserve_Memory__nUnits 등 메모리 예약과 관련하여
/           사용자가 요청한 크기보다 실제로는 더 많은 크기가 처리될 수 있습니다
/           (주소공간의 낭비를 피하기 위해 일정크기 이상의 덩어리 단위로 처리하기 때문에)
/
/       #4) OMP등의 스레드풀 사용시 메모리 릭 발생 문제
/           ※ 메모리풀이 워커스레드들이 종료되기전에 먼저 언로드 되는 경우,
/              워커스레드들의 TLS에 메모리풀의 자원이 남아있다면 메모리 릭 경고가 발생합니다
/              각 워커스레드에서 ::UTIL::g_pMEM->mFN_Shutdown_CurrentThread_MemoryPoolTLS 를 실행하면 해결됩니다
/           예:) OMP 사용시
/               #pragma omp parallel num_threads(omp_get_max_threads())
/               {
/                   UTIL::g_pMem->mFN_Shutdown_CurrentThread_MemoryPoolTLS();
/               }
/----------------------------------------------------------------
/   버전2 이전의 버전은 이후 버전의 DLL과 호환되지 않습니다
/----------------------------------------------------------------
/
/   버  전: 2
/   작성자: lastpenguin83@gmail.com
/   작성일: 15-03-05 ~
/           기존 코드를 리팩토링
/           할당방식 변경, 인터페이스 수정(DLL EXPORT)
/   수정일: 15-07-27 ~
/           리펙토링
/           다중 스레드에서 성능 강화(다중 스레드에서 Baskets 접근이 빠르게 수정)
/           할당되는 메모리 절약
/
/           15-11-06
/           CMemoryPoolResource 를 통하지 않고
/           메모리풀 관리자에 직접 할당을 요청시 풀을 탐색하는 것을 캐시를 두어 개선함
/
/           15-11-28
/           속도개선
/----------------------------------------------------------------*/
#ifndef _MACRO_STATIC_ASSERT
#define _MACRO_STATIC_ASSERT _STATIC_ASSERT
#endif

// 다음의 매크로는
//  DLL 프로젝트와 일치해야 합니다
//  _DEF_MEMORYPOOL_MAJORVERSION
//  _DEF_USING_DEBUG_MEMORY_LEAK
//  _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A
//  _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS



#if !defined(_DEF_USING_DEBUG_MEMORY_LEAK)
  #if defined(_DEBUG)
    #define _DEF_USING_DEBUG_MEMORY_LEAK    1
  #else
    #define _DEF_USING_DEBUG_MEMORY_LEAK    0
  #endif
#endif

#define _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A    1

#if _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS
  #if !_DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A
    #error _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS 는 _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A 를 필요료 합니다
  #endif
#elif !defined(_DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS)
  #if _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A
    #define _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS 1
  #else
    #define _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS 0
  #endif
#endif


namespace UTIL{
namespace MEM{
    __interface IMemoryPool;
    __interface IMemoryPool_Manager;
    class CMemoryPoolResource;
#if _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS
    struct TMemoryPool_TLS_CACHE;
#endif
}
}
namespace UTIL{
    extern MEM::IMemoryPool_Manager* g_pMem;
}


/*----------------------------------------------------------------
/   메모리풀 Interface
----------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    // 통계
    struct TMemoryPool_Stats{
        inline UINT64 Calculate_KeepingUnits(){ return Base.nUnits_Created - Base.nUnits_Using; }
        struct{
            UINT64 nUnits_Created;      // 생성된 유닛수
            UINT64 nUnits_Using;        // 사용자가 사용중

            UINT64 nCount_Expansion;    // 메모리 확장 성공수
            UINT64 nCount_Reduction;    // 메모리 축소 성공수
            UINT64 nTotalSize_Expansion;// 메모리 확장크기 합계
            UINT64 nTotalSize_Reduction;// 메모리 축소크기 합계

            UINT64 nCurrentSize_Pool;
            UINT64 nMaxSize_Pool;
        }Base;
        struct{
        #if _DEF_MEMORYPOOL_MAJORVERSION == 1
            // 1개의 메모리풀이 a~b 사이즈의 객체에 공유되기 때문에
            // 사용자의 ExpanstionOption 변경이 여러번에 걸쳐 요청되는 경우를 찾는 용도로 사용할 수 있습니다.

            // 이하 사용자의 mFN_Set_ExpansionOption__BlockSize 요청수 기록
            UINT64 ExpansionOption_OrderCount_UnitsPerBlock;
            UINT64 ExpansionOption_OrderCount_LimitBlocksPerExpansion;

            // 이하 사용자의 Reserve 요청 기록 
            //      다음의 두 값은 사용자가 호출한 함수에 따라 별개로 카운팅
            //          Reserve_Order_TotalUnits
            //          Reserve_Order_TotalSize
            UINT64 Reserve_OrderCount_Size;
            UINT64 Reserve_OrderCount_Units;
            UINT64 Reserve_OrderSum_TotalSize;
            UINT64 Reserve_OrderSum_TotalUnits;
            // 이하 사용자의 Reserve 요청의 실제 결과
            UINT64 Reserve_result_TotalSize;
            UINT64 Reserve_result_TotalUnits;
        #elif _DEF_MEMORYPOOL_MAJORVERSION == 2
            // 사용자 요청수
            UINT64 OrderCount_Reserve_nSize;
            UINT64 OrderCount_Reserve_nUnits;
            UINT64 OrderCount_Reserve_Max_nSize;
            UINT64 OrderCount_Reserve_Max_nUnits;
            // 사용자 요청중 가장 큰 요구
            UINT64 MaxRequest_nSize;
            UINT64 MaxRequest_nUnits;
            // 사용자 요청 실제 결과
            UINT64 Result_Total_nSize;
            UINT64 Result_Total_nUnits;
        #endif
        }UserOrder;
        UINT64 __FreeSlot[16];
    };

    __interface IMemoryPool{
        #if _DEF_USING_DEBUG_MEMORY_LEAK
        void* mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line); // Memory Alloc : Debug Memory Leak
        #else
        void* mFN_Get_Memory(size_t _Size); // Memory Alloc
        #endif
        void mFN_Return_Memory(void* pAddress); // Memory Free
        void mFN_Return_MemoryQ(void* pAddress); // Memory Free : Unsafe


        //----------------------------------------------------------------
        // 질의 : 취급하는 최대 유닛 크기(0 : 지원하지 않는 크기를 처리하는 풀)
        size_t mFN_Query_This_UnitSize() const;
    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        // 질의 : 취급하는 유닛 크기 범위
        BOOL mFN_Query_This_UnitsSizeLimit(size_t* pOUT_Min, size_t* pOUT_Max) const;
        // 질의 : 취급하지 않는 크기를 처리하는 메모리풀(느린 동작)
        BOOL mFN_Query_isSlowMemoryPool() const;
        // 질의 : 캐시라인 정렬 확인
        BOOL mFN_Query_MemoryUnit_isAligned(size_t AlignSize) const;
    #endif
        // 질의 : 통계
        BOOL mFN_Query_Stats(TMemoryPool_Stats* pOUT);
    #if _DEF_MEMORYPOOL_MAJORVERSION == 1
        // 질의 : 확장당 블록수제한
        size_t mFN_Query_LimitBlocks_per_Expansion();
        // 질의 : 블록당 유닛수
        size_t mFN_Query_Units_per_Block();
        // 질의 : 블록당 크기(메모리)
        size_t mFN_Query_MemorySize_per_Block();
    #endif

    #if _DEF_MEMORYPOOL_MAJORVERSION == 1
        //----------------------------------------------------------------
        //  유닛 예약 질의
        //----------------------------------------------------------------
        // 질의 : 유닛예약시 실제 사용될 메모리
        size_t mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(size_t nUnits);
        // 질의 : 유닛예약시 실제 생성될 유닛수
        size_t mFN_Query_PreCalculate_ReserveUnits_to_Units(size_t nUnits);
        // 질의 : 메모리예약시 실제 사용될 메모리
        size_t mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(size_t nByte);
        // 질의 : 메모리예약시 실제 생성될 유닛수
        size_t mFN_Query_PreCalculate_ReserveMemorySize_to_Units(size_t nByte);
    #endif

        //----------------------------------------------------------------
        //  유닛 예약 : 지정크기 만큼 최대 크기 추가 확장
        //----------------------------------------------------------------
        // 예약 : 메모리 크기 기준
        // 리턴 : 실제 생성된 메모리(0 : 실패)
        size_t mFN_Reserve_Memory__nSize(size_t nByte, BOOL bWriteLog = TRUE);
        // 예약 : 유닛수 기준
        // 리턴 : 실제 생성된 유닛수(0 : 실패)
        size_t mFN_Reserve_Memory__nUnits(size_t nUnits, BOOL bWriteLog = TRUE);
    #if _DEF_MEMORYPOOL_MAJORVERSION == 2
        //----------------------------------------------------------------
        //  유닛 예약 : 이미 사용된 용량을 포함하여 최대 크기 확장
        //----------------------------------------------------------------
        // 예약 : 메모리 크기 기준
        // 리턴 : 메모리 풀에서 관리하는 전체 메모리
        size_t mFN_Reserve_Memory__Max_nSize(size_t nByte, BOOL bWriteLog = TRUE);
        // 예약 : 유닛수 기준
        // 리턴 : 메모리 풀에서 관리하는 전채 유닛
        size_t mFN_Reserve_Memory__Max_nUnits(size_t nUnits, BOOL bWriteLog = TRUE);
        //----------------------------------------------------------------
        //  최대사용 메모리 예측
        //  mFN_Reserve_Memory__ ... 계열은 이 메소드의 기능을 포함하여 처리
        //  만약 과도하게 설정시 메모리가 부족할 수 있음
        //  ※ 이 메소드는 통계를 집계하지 않음
        //----------------------------------------------------------------
        // 리턴 : 메모리 풀의 예상된 최대크기(설정된)
        size_t mFN_Hint_MaxMemorySize_Add(size_t nByte, BOOL bWriteLog = TRUE);
        // 리턴 : 메모리 풀의 예상된 최대크기(설정된)
        size_t mFN_Hint_MaxMemorySize_Set(size_t nByte, BOOL bWriteLog = TRUE);
        //----------------------------------------------------------------
        //  설정 변경
        //----------------------------------------------------------------
        // 프로세서별 캐시 사용
        BOOL mFN_Set_OnOff_ProcessorCACHE(BOOL _On);
        BOOL mFN_Query_OnOff_ProcessorCACHE() const;
    #endif

    #if _DEF_MEMORYPOOL_MAJORVERSION == 1
        // 확장방식 설정 변경
        //      nUnits_per_Block            : 블럭당 유닛수 설정 1~ (0 = 변경하지 않음)
        //      nLimitBlocks_per_Expansion  : 확장당 블록수제한  1~? (0 = 변경하지 않음)
        //          ※ nLimitBlocks_per_Expansion 는 확장실패시 감소할 수 있습니다.
        // 리턴 : 실제 적용된 블럭당 유닛수 설정(0 : 실패)
        size_t mFN_Set_ExpansionOption__BlockSize(size_t nUnits_per_Block, size_t nLimitBlocks_per_Expansion = 0, BOOL bWriteLog = TRUE);
        size_t mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const;
    #endif
        //----------------------------------------------------------------
        void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On); // Default On
    };
}
}

/*----------------------------------------------------------------
/   메모리풀 관리자 Interface
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    // 버전
    struct TMemoryPool_Virsion{
        UINT16 Major; // 0 ~ 
        UINT16 Minor; // 0000 ~ 9999
    };

    __interface IMemoryPool_Manager{
    #if _DEF_USING_DEBUG_MEMORY_LEAK
        void* mFN_Get_Memory(size_t _Size, const char* _FileName, int _Line); // Memory Alloc : Debug Memory Leak
    #else
        void* mFN_Get_Memory(size_t _Size); // Memory Alloc
    #endif
        void mFN_Return_Memory(void* pAddress); // Memory Free
        void mFN_Return_MemoryQ(void* pAddress); // Memory Free : Unsafe

    #if _DEF_USING_DEBUG_MEMORY_LEAK
        void* mFN_Get_Memory__AlignedCacheSize(size_t _Size, const char* _FileName, int _Line);
    #else
        void* mFN_Get_Memory__AlignedCacheSize(size_t _Size);
    #endif
        void mFN_Return_Memory__AlignedCacheSize(void* pAddress);


        IMemoryPool* mFN_Get_MemoryPool(size_t _Size);
    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        IMemoryPool* mFN_Get_MemoryPool(void* pUserValidMemoryUnit);
    #endif


    #if _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS && _DEF_MEMORYPOOL_MAJORVERSION == 1
        /*----------------------------------------------------------------
        /   TLS CACHE 접근 합수 지정
        /---------------------------------------------------------------*/
        void mFN_Set_TlsCache_AccessFunction(TMemoryPool_TLS_CACHE&(*pFN)(void));
    #endif
    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        // 가능한 메모리풀 수
        size_t mFN_Query_Possible_MemoryPools_Num() const;
        // 가능한 메모리풀들 조사
        BOOL mFN_Query_Possible_MemoryPools_Size(size_t* _OUT_Array) const;

        size_t mFN_Query_Limit_MaxUnitSize_Default() const;
        size_t mFN_Query_Limit_MaxUnitSize_UserSetting() const;
        // 메모리풀 단위유닛 최대 크기 제한
        BOOL mFN_Set_Limit_MaxUnitSize(size_t _MaxUnitSize_Byte, size_t* _OUT_Result);

        // 힙 메모리 확보(관리하는 힙 메모리중 아무것이나 제거)
        BOOL mFN_Reduce_AnythingHeapMemory();
    #endif

        // 관리자, 모든 메모리풀에 대하여
        void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On); // Default On

        // 메모리 누수지점 추적(DEBUG 모드에서만 의미가 있음)
        void mFN_Set_OnOff_Trace_MemoryLeak(BOOL _On); // Default Off
        // 메모리풀 동작 자세히 추적(DEBUG 모드에서만 의미가 있음)
        void mFN_Set_OnOff_ReportOutputDebugString(BOOL _On); // Default On

    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        BOOL mFN_Set_CurrentThread_Affinity(size_t _Index_CpuCore);
        void mFN_Reset_CurrentThread_Affinity();

        // 현재 스레드의 메모리풀 관련 TLS 정리
        //      스레드풀(예: OMP)의 각 스레드 스택에 메모리풀 TLS가 정리되지 않은 상태에서,
        //      메모리풀이 먼저 소멸되는 경우, 대량의 메모리릭 보고가 발생할 수 있습니다
        //      (연결된 로그파일이 있다면 기록 됩니다)
        //      메모리풀 라이브러리 종료전 각 워커스레드에서 호출하면 이 문제는 해결됩니다.
        //      ※ 이 함수는 여러번 호출해도 정상동작 합니다
        //      ※ TLS를 정리후 메모리풀에 접근시 TLS가 다시 생성될 수 있습니다.
        void mFN_Shutdown_CurrentThread_MemoryPoolTLS();

        TMemoryPool_Virsion mFN_Query_Version() const;
    #endif
    };
}
}


/*----------------------------------------------------------------
/   사용자 Interface
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    /*----------------------------------------------------------------
    /-----------------------------------------------------------------
    /   메모리풀 고속 접근 인터페이스
    /       각 사이즈별 미리 찾은 메모리풀을 사용
    /-----------------------------------------------------------------
    ----------------------------------------------------------------*/
    template<size_t Size>
    IMemoryPool* gFN_Get_MemoryPool()
    {
        static IMemoryPool* s_pPool(nullptr);
        if(!s_pPool)
            s_pPool = g_pMem->mFN_Get_MemoryPool(Size);
        return s_pPool;
    }
    template<typename T>
    inline IMemoryPool* gFN_Get_MemoryPool()
    {
        return gFN_Get_MemoryPool<sizeof(T)>();
    }
    template<typename T>
    inline IMemoryPool* gFN_Get_MemoryPool__AlignedCacheSize()
    {
        _MACRO_STATIC_ASSERT(sizeof(T) % _DEF_CACHELINE_SIZE == 0);
        return gFN_Get_MemoryPool<sizeof(T)>();
    }
    /*----------------------------------------------------------------
    /-----------------------------------------------------------------
    /   객체 단위 메모리 자원
    /       이 객체를 상속받아 메모리풀을 적용합니다
    /-----------------------------------------------------------------
    ----------------------------------------------------------------*/
#pragma region class CMemoryPool Resource
    class CMemoryPoolResource{
    public:
        // Debug - 메모리 풀로부터 메모리를 할당받습니다.
        void* operator new(size_t _Size, const char* _FileName=__FILE__, int _Line=__LINE__)
        {
            IMemoryPool* pPool = __Get_MemoryPool(_Size);
        #if _DEF_USING_DEBUG_MEMORY_LEAK
            return pPool->mFN_Get_Memory(_Size, _FileName, _Line);
        #else
            UNREFERENCED_PARAMETER(_FileName);
            UNREFERENCED_PARAMETER(_Line);
            return pPool->mFN_Get_Memory(_Size);
        #endif
        }

        // 메모리 풀에 메모리를 반납합니다.
        void operator delete(void* pAddress, const char* _FileName, int _Line)
        {
            UNREFERENCED_PARAMETER(_FileName);
            UNREFERENCED_PARAMETER(_Line);

            g_pMem->mFN_Return_Memory(pAddress);
        }
        // 메모리 풀에 메모리를 반납합니다.
        void operator delete(void* pAddress)
        {
            g_pMem->mFN_Return_Memory(pAddress);
        }

    public:
        __forceinline static IMemoryPool* __Get_MemoryPool(size_t TypeSize)
        {
            return g_pMem->mFN_Get_MemoryPool(TypeSize);
        }
        template<size_t Size>
        __forceinline static IMemoryPool* __Get_MemoryPool()
        {
            return gFN_Get_MemoryPool<Size>();
        }
        template<typename QueryType>
        __forceinline static IMemoryPool* __Get_MemoryPool()
        {
            return gFN_Get_MemoryPool<QueryType>();
        }
    };

#if _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS && _DEF_MEMORYPOOL_MAJORVERSION == 1
    struct TMemoryPool_TLS_CACHE{
        size_t m_Code = 0;
        size_t m_ID = MAXSIZE_T; // 초기값 모든 비트 ON

        static TMemoryPool_TLS_CACHE& sFN_Default_Function()
        {
            static __declspec(thread) TMemoryPool_TLS_CACHE _Instance;
            return _Instance;
        };
    };
    _MACRO_STATIC_ASSERT(sizeof(TMemoryPool_TLS_CACHE) == sizeof(size_t)*2);
#endif

#pragma endregion






    /*----------------------------------------------------------------
    /-----------------------------------------------------------------
    /   외부 인터페이스
    /-----------------------------------------------------------------
    ----------------------------------------------------------------*/
#pragma warning(push)
#pragma warning(disable: 4127)

#if _DEF_USING_DEBUG_MEMORY_LEAK
    template<typename T>
    inline void gFN_Alloc_from_MemoryPool(T*& pRef, const char* _FileName, int _Line)
    {
        _Assert(nullptr == pRef);

        void* p;
        p = gFN_Get_MemoryPool<T>()->mFN_Get_Memory(sizeof(T), _FileName, _Line);
        pRef = reinterpret_cast<T*>(p);
    }
    template<typename T>
    inline void gFN_Alloc_from_MemoryPool_Aligned(T*& pRef, const char* _FileName, int _Line)
    {
        _Assert(nullptr == pRef);

        void* p;
        p = gFN_Get_MemoryPool__AlignedCacheSize<T>()->mFN_Get_Memory(sizeof(T), _FileName, _Line);
        pRef = reinterpret_cast<T*>(p);
    }
#else
    template<typename T>
    inline void gFN_Alloc_from_MemoryPool(T*& pRef)
    {
        _Assert(nullptr == pRef);

        void* p;
        p = gFN_Get_MemoryPool<T>()->mFN_Get_Memory(sizeof(T));
        pRef = reinterpret_cast<T*>(p);
    }
    template<typename T>
    inline void gFN_Alloc_from_MemoryPool_Aligned(T*& pRef)
    {
        _Assert(nullptr == pRef);

        void* p;
        p = gFN_Get_MemoryPool__AlignedCacheSize<T>()->mFN_Get_Memory(sizeof(T));
        pRef = reinterpret_cast<T*>(p);
    }
#endif
    inline void gFN_Free_from_MemoryPool(void* p)
    {
        g_pMem->mFN_Return_Memory(p);
    }
    
#if _DEF_USING_DEBUG_MEMORY_LEAK
    #define _MACRO_ALLOC__FROM_MEMORYPOOL(Address)  ::UTIL::MEM::gFN_Alloc_from_MemoryPool(Address,  __FILE__, __LINE__)
#else
    #define _MACRO_ALLOC__FROM_MEMORYPOOL(Address)  ::UTIL::MEM::gFN_Alloc_from_MemoryPool(Address)
#endif
    #define _MACRO_FREE__FROM_MEMORYPOOL(Address)   ::UTIL::MEM::gFN_Free_from_MemoryPool(Address)

    #define _MACRO_NEW__FROM_MEMORYPOOL(Address, Constructor)       \
    {                                                               \
        _MACRO_ALLOC__FROM_MEMORYPOOL(Address);                     \
        if(Address) _MACRO_CALL_CONSTRUCTOR(Address, Constructor);  \
    }
    #define _MACRO_DELETE__FROM_MEMORYPOOL(Address)                 \
    {                                                               \
        _MACRO_CALL_DESTRUCTOR(Address);                            \
        _MACRO_FREE__FROM_MEMORYPOOL(Address);                      \
    }

#if _DEF_USING_DEBUG_MEMORY_LEAK
    #define _MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC(Size)     ::UTIL::g_pMem->mFN_Get_Memory(Size, __FILE__, __LINE__)
#else
    #define _MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC(Size)     ::UTIL::g_pMem->mFN_Get_Memory(Size)
#endif

    #define _MACRO_FREE__FROM_MEMORYPOOL_GENERIC(Address)   ::UTIL::g_pMem->mFN_Return_Memory(Address)
    #define _MACRO_FREEQ__FROM_MEMORYPOOL_GENERIC(Address)  ::UTIL::g_pMem->mFN_Return_MemoryQ(Address)
#pragma warning(pop)


    // xmemory0 의 template<class _Ty> class allocator 에 호환 되도록 만든다
    // 혼란 방지를 위해 이름은 달리 한다
    // ※ 할당 패턴
    //      1개 단위           list, set, map
    //      n개 단위(일정패턴) queue, deque
    //      n개 단위(예측불가) vector
    //          : 할당 패턴 1 2 3 4 6 9 13 19 28 42 63 94 141 211 316 474 711 1066...
    //            이는 stl::allocator 의 버전에 따라 다를 수 있다
    template<class _Ty>
    struct TAllocator
    {
        typedef _Ty value_type;

        typedef value_type *pointer;
        typedef const value_type *const_pointer;

        typedef value_type& reference;
        typedef const value_type& const_reference;

        typedef size_t size_type;
        typedef ptrdiff_t difference_type;

        // 1개 단위 할당 해제라면 메모리풀에 좀더 고속으로 접근한다
        pointer _Allocate(size_type _Count, pointer)
        {
            // xmemory0 _Allocate는 총 크기가 4KB가 넘으면 주소를 32에 정렬하여 리턴함
            // 하지만 이 메모리풀은 512B 초과시 32, 1024B 초과시 64에 정렬하기 때문에 신경쓸 필요 없음

            _Assert(1 <= _Count);
            // 성능 경고
            // 1개 단위가 아니면서, 64KB를 초과 하는 경우 경고(헤더부분 제외)
            // _Count 패턴이 일정하지 않고 이 조건을 만족하지 못한다면 전용 할당자를 사용하는 것이 좋음
            _Assert(1 == _Count || (sizeof(_Ty)*_Count <= 64*1024-64));

            #if _DEF_USING_DEBUG_MEMORY_LEAK
            if(1 == _Count)
                return (pointer)gFN_Get_MemoryPool<sizeof(_Ty)>()->mFN_Get_Memory(sizeof(_Ty), __FILE__, __LINE__);
            else
                return (pointer)::UTIL::g_pMem->mFN_Get_Memory(sizeof(_Ty)*_Count, __FILE__, __LINE__);
            #else
            if(1 == _Count)
                return (pointer)gFN_Get_MemoryPool<sizeof(_Ty)>()->mFN_Get_Memory(sizeof(_Ty));
            else
                return (pointer)::UTIL::g_pMem->mFN_Get_Memory(sizeof(_Ty)*_Count);
            #endif
        }
        void _Deallocate(pointer _Ptr, size_type _Count)
        {
            _Assert(nullptr != _Ptr && 1 <= _Count);

            if(1 == _Count)
                return gFN_Get_MemoryPool<sizeof(_Ty)>()->mFN_Return_Memory(_Ptr);
            else
                return ::UTIL::g_pMem->mFN_Return_Memory(_Ptr);
        }

        TAllocator() noexcept
        {	// construct default allocator (do nothing)
        }

        TAllocator(const TAllocator<_Ty>&) noexcept
        {	// construct by copying (do nothing)
        }

        template<class _Other>
        TAllocator(const TAllocator<_Other>&) noexcept
        {	// construct from a related allocator (do nothing)
        }

        template<class _Other>
        TAllocator<_Ty>& operator=(const TAllocator<_Other>&)
        {	// assign from a related allocator (do nothing)
            return (*this);
        }

        void deallocate(pointer _Ptr, size_type _Count)
        {	// deallocate object at _Ptr
            _Deallocate(_Ptr, _Count);
        }

        /*_DECLSPEC_ALLOCATOR*/ pointer allocate(size_type _Count)
        {	// allocate array of _Count elements
            return (_Allocate(_Count, (pointer)0));
        }

        /*_DECLSPEC_ALLOCATOR*/ pointer allocate(size_type _Count, const void *)
        {	// allocate array of _Count elements, ignore hint
            return (allocate(_Count));
        }

        template<class _Objty,
        class... _Types>
            void construct(_Objty *_Ptr, _Types&&... _Args)
        {	// construct _Objty(_Types...) at _Ptr
            ::new ((void *)_Ptr) _Objty(_STD forward<_Types>(_Args)...);
        }

        template<class _Uty>
        void destroy(_Uty *_Ptr)
        {	// destroy object at _Ptr
            UNREFERENCED_PARAMETER(_Ptr);
            _Ptr->~_Uty();
        }

        size_t max_size() const noexcept
        {	// estimate maximum array size
            return ((size_t)(-1) / sizeof (_Ty));
        }
    };

}
}

#pragma pack(pop)