#pragma once
#pragma pack(push, 4)
// ���� �ۼ��� ���� �������� �ۼ� �Ǿ����ϴ�
// Font      : ����ü
// Font Size : 10 
/*----------------------------------------------------------------
/   �޸�Ǯ
/
/   �׽�Ʈ���
/       ������ ����ũ�� �Ҵ翡�� TBBMalloc , LFH , TCMalloc �� ���� ���� ������ ���Դϴ�
/       (�̶� TBBMalloc�� ���Ͽ� 128 ������ ������ �Ҵ��� ��� ���� ������ ���Դϴ�)
/       2015 11���� TBBMalloc TCMalloc ��� ��
/----------------------------------------------------------------
/   ��� ��� / ����
/       ���⼭ [���μ���]�� CPU�� 1�� [�ھ�]�� ���մϴ�
/       IMemoryPool �� �����ϴ� ũ��� Commit ũ�Ⱑ �ƴ� �ּҰ��� ���� ũ�� �Դϴ�
/       ���� ��� �޸𸮴� �ּҰ��� ���� ũ�⺸�� ���� �� �ֽ��ϴ�.
/----------------------------------------------------------------
/   �ʱ� ����(�� ����2 �̻󿡼� ������� �ʽ��ϴ�)
/       _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS Ȱ��ȭ�� ���
/           ����ڴ� �޸�Ǯ�� ���Ե� DLL��
/           ����ϴ� ���� ���̺귯�� �Ǵ� ���� ���α׷�����
/           MEMORYPOOL TLS CACHE ���� �Լ��� �����ؾ� �մϴ�
/               IMemoryPool_Manager::mFN_Set_TlsCache_AccessFunction
/               �� �� �޼ҵ�� �ݵ�� LIB �Ǵ� exe ������Ʈ���� g_pMem �� ������ �����ؾ� �մϴ�.
/
/           �⺻���� static TMemoryPool_TLS_CACHE& TMemoryPool_TLS_CACHE::sFN_Default_Function() �Դϴ�
/           �� ���� ���� DLL���� TLS�� TlsAlloc, TlsFree, TlsSetValue, TlsGetValue ����ó�� �ϴ� ����
/           ����ó���ϱ� ���� Ʈ���Դϴ�.
/           (���� TLS �� �ּҴ� �̰��� ����ϴ� LIB �Ǵ� exe �� ��ġ�ϵ��� ��)
/
/       �� DLL���� DLL�� ����ϴ� ��츦 ���Ͽ� ����2������ ��������� ���մϴ�.
/----------------------------------------------------------------
/   ���� :
/       (������ ������� ��� ȥ������ �����մϴ�)
/
/       #1) �޸�Ǯ�� �����Ϸ��� ��ü�� CMemoryPoolResource�� ��ӹ޽��ϴ�.
/           ��:) class CTest1 : public CMemoryPoolResource
/           ��:) struct TTest1 : CMemoryPoolResource
/           new delete �� �̿��Ͽ� ����մϴ�.
/           �� �̹� �ۼ��� �ڵ忡 ���� �����ϴµ� �����մϴ�.
/           �� ���� : �ڽ� Ŭ�������� ��� �޸�Ǯ�� ������ �޽��ϴ�.
/
/       #2) ��ü������ ���Ͽ� ���� ����� ������� �ʰ� Ư����Ȳ���� ����ϰ� �ʹٸ�
/           ������ ��ũ�θ� ����մϴ�.
/           (CMemoryPoolResource�� ��� ���� Ÿ�Զ��� �����մϴ�.)
/           �� ��ũ�� ���� : �Ҵ��� / �Ҹ��� �� ȣ��
/               _MACRO_ALLOC__FROM_MEMORYPOOL(Address)
/               _MACRO_FREE__FROM_MEMORYPOOL(Address)
/           �� ��ũ�� ���� : �Ҵ��� / �Ҹ��� ȣ��
/               _MACRO_NEW__FROM_MEMORYPOOL(Address, Constructor)
/               _MACRO_DELETE__FROM_MEMORYPOOL(Address)
/
/
/       #3) ���� �Ҵ��Ϸ��� ũ�Ⱑ ������(���� ��� ���ڿ�����) �̶��
/           �޸�Ǯ�����ڿ� ���� �����Ͽ� ������ �޼ҵ带 ����մϴ�
/           IMemoryPool_Manager::mFN_Get_Memory
/           IMemoryPool_Manager::mFN_Return_Memory
/           IMemoryPool_Manager::mFN_Get_Memory__AlignedCacheSize       // ĳ�ö��� ���� ����
/           IMemoryPool_Manager::mFN_Return_Memory__AlignedCacheSize    // ĳ�ö��� ���� ����
/           �� ��ũ�� ���� : �Ҵ��� / �Ҹ��� �� ȣ��
/               _MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC
/               _MACRO_FREE__FROM_MEMORYPOOL_GENERIC
/           �� malloc / free �� ����ϱ⿡ ������ ����Դϴ�.
/
/
/       #M) ���� �ܰ踦 �ٿ� �ӵ��� ���̰� �ʹٸ�,
/           IMemoryPool_Manager::mFN_Get_MemoryPool �� ���� �޸�Ǯ �������̽� ������ ��� �����ϸ�
/           ���������Ͽ� �޸𸮸� Get / Return �� �� �ֽ��ϴ�.
/           �� IMemoryPool �� �����ϴ� ���
/               gFN_Get_MemoryPool
/               gFN_Get_MemoryPool__AlignedCacheSize
/               CMemoryPoolResource::__Get_MemoryPool<Type>
/               IMemoryPool_Manager::mFN_Get_MemoryPool
/           �� Get / Return �� ������ ���� ���Ǳ�� ¦�� ���� ����ؾ�.
/           �� ������ �Ҹ��� ȣ���� ����ڰ� ���� �ؾ� �մϴ�.
/
/       ���� ���Ǽ� : #1 > #2 > #3 > #M
/       ����        : #M > #2 > #1 = #3
/                   ������ ���� �Ҵ�� �޸�Ǯ�� �����ϴ� �ӵ��� �����Դϴ�
/                   �̰��� ����ڰ� ���� ����� ���� �Ǵ� �߰��Ͽ� ������ �����մϴ�
/
/       �� ĳ�ö��� ���ĵ� �޸𸮸� ���ϸ��� ĳ�ö���ũ���� ����� ��û�Ͻʽÿ�.
/          ���� �׷� �� ���ٸ� mFN_Get_Memory__AlignedCacheSize �� ����Ͻʽÿ�.
/       �� ��� 1)�� ������ Ÿ�Կ� �ٸ� ����� ����ص� ���ۿ��� ������ �����ϴ�
/
/       CMemoryPoolResource�� ��� ���� ��ü�� �޸�Ǯ �������̽��� ������ ���� ���� �� �ֽ��ϴ�.
/           __Get_MemoryPool<T>() �Ǵ� __Get_MemoryPool(sizeof(T))
/----------------------------------------------------------------
/   ����(��Ÿ) :
/   STL Container �� Allocator ��ü �� ��� ������ ���
/       TAllocator
/           �̰��� vector ���� �ſ� ������ ũ�� �Ҵ翡 ������� �ʴ� ���� �����ϴ�
/----------------------------------------------------------------
/   ��������� :
/           �������� ���� �ݳ� �żҵ� ���
/               mFN_Return_MemoryQ
/               �� �żҵ�� ����� �ջ��� ����� Ȯ������ �ʰ� �ݳ��� �ּҸ� �޾Ƶ��Դϴ�.
/               ���� �� ����� �ջ�� ��� �޸�Ǯ�� ���۵� �ϰ� �Ǹ�, ��Ÿ�� ������ �߻���ų �� �Դϴ�
/               ���� ����ϴ� ������Ʈ���� ���� �����÷ο� �Ǽ��� �ִٸ� ����ؼ��� �ȵ˴ϴ�.
/               
/               �ݴ��, �⺻ �żҵ��� mFN_Return_Memory �� ���...
/                   �ݳ��� �ּ��� ����� �ִ��� Ȯ���ϸ�,
/                   �ջ�� ���, ������� �ݳ��� �����մϴ�(�̰�� �޸� ���� �߻��ϰ� �˴ϴ�)
/                   �� ������ ����� ������ �ƴ϶�� ���缱 ���� �׽�Ʈ �մϴ�
/
/           ��Ƽ �ھ�� ��¥ ���� ����
/               ĳ�ö��� ũ��(���� 64����Ʈ)�� ����� ��ü �Ǵ� �䱸 �޸� ũ�⸦ ���߽ʽÿ�
/
/           �޸�Ǯ Ȯ��� ���ũ�� ����(�� ����2 �̻󿡼� ������� �ʽ��ϴ�)
/               IMemoryPool::mFN_Set_ExpansionOption__BlockSize
/               ������ ������ �� �ֽ��ϴ�.
/                   - �޸�Ǯ Ȯ���, �����Ǵ� ���� ����
/                   - ���� ���� �Ǵ� ũ�� ����
/
/           �ʿ��� �޸𸮸� �̸� ����
/               IMemoryPool::mFN_Reserve_Memory__nSize
/               IMemoryPool::mFN_Reserve_Memory__nUnits
/               �ʱ⿡ ����Ǵ� ���並 �޸�Ǯ�� �̸� ������ �Ѵٸ�,
/               ������ �ʴ� �ñ⿡ �� ���ð��� ������ �� �ֽ��ϴ�(�޸�Ǯ�� �����޸𸮰� ������ ���, �߰� Ȯ�� �۾����� ����)
/               ���� ������ ���� ���� ũ���� ������ �������� ���� �޸�Ǯ�� Ȯ���ϴ� ���,
/               �Ҵ��� ������� ���ϴ� �κе��� ����µ�, �̷� ���� ���� �� �ֽ��ϴ�
/               �� STL Container Reserve �ʹ� �޸� �̹� ������ �޸𸮵��� ��꿡 �������� �ʽ��ϴ�
/               �� �޸�Ǯ V2 ������ Reserve�� ���� Ȯ���� �޸𸮴� ���α׷� ���������� OS�� �ݳ����� �ʽ��ϴ�
/                  ��� ������ �ʿ��� ũ�⸸ŭ�� �����ؾ� �մϴ�
/
/           �ʿ��� �޸𸮸� �̸� ���� - ����2���� �߰�
/               IMemoryPool::mFN_Reserve_Memory__Max_nSize
/               IMemoryPool::mFN_Reserve_Memory__Max_nUnits
/               STL Container Reserve �� ������ �����մϴ�
/               (����, �̹� ������� �޸� ũ�Ⱑ ����� �䱸���� ũ�ٸ� ����� ����� �����մϴ�)
/
/           �ִ� ũ�� ���� - ����2���� �߰�
/               IMemoryPool::mFN_Hint_MaxMemorySize_Add
/               IMemoryPool::mFN_Hint_MaxMemorySize_Set
/               �޸𸮸� �̸� �Ҵ��ϴ� ��ɰ� �޸�, �� ����� ����� �޸𸮸� �ڿ��� ������� ������ ��� ó���˴ϴ�
/                   OS���� ����޸𸮿� �����޸𸮸� �����ϴ� ������ �ð� �Ҹ� �ִµ�
/                   �޸�Ǯ�� �̸� ȸ���ϱ� ���� �ִ� ���並 ����Ͽ�, OS�� �ݳ����� �ʰ� ��Ȱ���մϴ�
/                   ����ڰ� ��û���� �ʴ��� �̰��� �׻� ó���մϴ�
/               ����ڴ� �� ����� �̿��� �ִ� ���並 ��� Add/Set �� �� �ֽ��ϴ�
/
/           �������� ���μ���ģȭ�� ����
/               �� �޸�Ǯ�� CPU�� �ھ���� �������� �ӽ� ����Ҹ� �����ϴ�(��Ƽ ������ ������ ���Ͽ�)
/               �̶�, ������ �����ھ� CPU�� �ڵ�� 1������� �����ϰ� �ϴ��� �����δ� ���� ���μ����� �����Ͽ� ����� ó���ϴ� ��찡 ����
/               (������ ģȭ�� ������ ���� �ʾҴٸ�)
/               �̷� ��� ������ �������� �߻��� ���ɼ��� �����ϴ�
/           1. �޸� ���� ���ɼ�
/               � �ھ ���� ����Ұ� ���� �� ���� ������ �ʴ� ��� �� ���ο� ������ �޸��� ����
/               (�� ���μ��� ���� ĳ�ô� �����ϴ� ������ �ּ������� �����Ϸ� �ϱ� ������ ����� ũ�� ������ �ʿ�� �����մϴ�.)
/               ���� ����� ȯ���� CPU ������ �ھ��� ���� ����Ͽ�, �̷� ��� ������ ������ �������� ����Ͻʽÿ�
/               = �ھ�� * max(������ũ��(����4KB) , Unit Size)
/           2. �޸�Ǯ�� ó�� �ӵ� ����
/               ���� ��� CPU �ھ�0���� ����ҿ� ���Ͽ� ������A, B�� ���� �����ϴ� ���(���� �ڿ��� ���� ����)
/           �� SetThreadAffinity ������ �����庰 ���μ��� ģȭ���� �����Ͽ� �����尡 ���� ���μ������� ������ ���ɼ��� ���ٸ�
/              �� �������� ������ �� �ֽ��ϴ�
/
/           �� ������� �����ϴ� ��ü ũ����� <60> <64> ���� �޸�Ǯ�� ������ �޸�Ǯ�� �ƴ� �� �ֽ��ϴ�.
/              �ϳ��� �޸�Ǯ��(a~b)ũ�⸦ �����ϱ� �����Դϴ�
/----------------------------------------------------------------
/   ���ѻ���:
/       #1) new[] �� delete[]�� �������� �ʽ��ϴ�.
/           �⺻������ �մϴ�. ( :: operator new[], :: operator delete [] )
/
/       #2) ����� ���ѻ���(CMemoryPoolResource�� ��ӹ��� ��ü�� ��ĳ�����Ҷ��� ������)
/           ��:) - class CBasis
/                  class CA : public CBasis, public CMemoryPoolResource
/                  CBasis p = new CA
/                  delete p;   // CMemoryPoolResource::operator delete �� ȣ����� �ʽ��ϴ�.(��Ÿ�� ������ �߻��ϰ� �˴ϴ�.)
/           -> �̰��� �ذ��ϱ� ���ؼ��� ���� ��ü��, �θ� ��ü�� ��� virtual table�� �������� �մϴ�.
/              �̰�� CObjectResource::operator delete �� �ùٸ��� ȣ��Ǹ�, ���� �����ڵ�� ������ ���������ϴ�.
/       �� CMemoryPoolResource �� ����ڰ� ��ü�� ��� �����͸� �޸𸮿�  �����ϴ� ��쿡 ����ϱ� ����
/          ���� �Ҹ��ڸ� �������� �ʾҽ��ϴ�
/
/       #3) IMemoryPool::mFN_Reserve_Memory__nSize IMemoryPool::mFN_Reserve_Memory__nUnits �� �޸� ����� �����Ͽ�
/           ����ڰ� ��û�� ũ�⺸�� �����δ� �� ���� ũ�Ⱑ ó���� �� �ֽ��ϴ�
/           (�ּҰ����� ���� ���ϱ� ���� ����ũ�� �̻��� ��� ������ ó���ϱ� ������)
/
/       #4) OMP���� ������Ǯ ���� �޸� �� �߻� ����
/           �� �޸�Ǯ�� ��Ŀ��������� ����Ǳ����� ���� ��ε� �Ǵ� ���,
/              ��Ŀ��������� TLS�� �޸�Ǯ�� �ڿ��� �����ִٸ� �޸� �� ��� �߻��մϴ�
/              �� ��Ŀ�����忡�� ::UTIL::g_pMEM->mFN_Shutdown_CurrentThread_MemoryPoolTLS �� �����ϸ� �ذ�˴ϴ�
/           ��:) OMP ����
/               #pragma omp parallel num_threads(omp_get_max_threads())
/               {
/                   UTIL::g_pMem->mFN_Shutdown_CurrentThread_MemoryPoolTLS();
/               }
/----------------------------------------------------------------
/   ����2 ������ ������ ���� ������ DLL�� ȣȯ���� �ʽ��ϴ�
/----------------------------------------------------------------
/
/   ��  ��: 2
/   �ۼ���: lastpenguin83@gmail.com
/   �ۼ���: 15-03-05 ~
/           ���� �ڵ带 �����丵
/           �Ҵ��� ����, �������̽� ����(DLL EXPORT)
/   ������: 15-07-27 ~
/           �����丵
/           ���� �����忡�� ���� ��ȭ(���� �����忡�� Baskets ������ ������ ����)
/           �Ҵ�Ǵ� �޸� ����
/
/           15-11-06
/           CMemoryPoolResource �� ������ �ʰ�
/           �޸�Ǯ �����ڿ� ���� �Ҵ��� ��û�� Ǯ�� Ž���ϴ� ���� ĳ�ø� �ξ� ������
/
/           15-11-28
/           �ӵ�����
/----------------------------------------------------------------*/
#ifndef _MACRO_STATIC_ASSERT
#define _MACRO_STATIC_ASSERT _STATIC_ASSERT
#endif

// ������ ��ũ�δ�
//  DLL ������Ʈ�� ��ġ�ؾ� �մϴ�
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
    #error _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A_TLS �� _DEF_USING_MEMORYPOOL_UPGRADE_MULTIPROCESSING__A �� �ʿ�� �մϴ�
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
/   �޸�Ǯ Interface
----------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    // ���
    struct TMemoryPool_Stats{
        inline UINT64 Calculate_KeepingUnits(){ return Base.nUnits_Created - Base.nUnits_Using; }
        struct{
            UINT64 nUnits_Created;      // ������ ���ּ�
            UINT64 nUnits_Using;        // ����ڰ� �����

            UINT64 nCount_Expansion;    // �޸� Ȯ�� ������
            UINT64 nCount_Reduction;    // �޸� ��� ������
            UINT64 nTotalSize_Expansion;// �޸� Ȯ��ũ�� �հ�
            UINT64 nTotalSize_Reduction;// �޸� ���ũ�� �հ�

            UINT64 nCurrentSize_Pool;
            UINT64 nMaxSize_Pool;
        }Base;
        struct{
        #if _DEF_MEMORYPOOL_MAJORVERSION == 1
            // 1���� �޸�Ǯ�� a~b �������� ��ü�� �����Ǳ� ������
            // ������� ExpanstionOption ������ �������� ���� ��û�Ǵ� ��츦 ã�� �뵵�� ����� �� �ֽ��ϴ�.

            // ���� ������� mFN_Set_ExpansionOption__BlockSize ��û�� ���
            UINT64 ExpansionOption_OrderCount_UnitsPerBlock;
            UINT64 ExpansionOption_OrderCount_LimitBlocksPerExpansion;

            // ���� ������� Reserve ��û ��� 
            //      ������ �� ���� ����ڰ� ȣ���� �Լ��� ���� ������ ī����
            //          Reserve_Order_TotalUnits
            //          Reserve_Order_TotalSize
            UINT64 Reserve_OrderCount_Size;
            UINT64 Reserve_OrderCount_Units;
            UINT64 Reserve_OrderSum_TotalSize;
            UINT64 Reserve_OrderSum_TotalUnits;
            // ���� ������� Reserve ��û�� ���� ���
            UINT64 Reserve_result_TotalSize;
            UINT64 Reserve_result_TotalUnits;
        #elif _DEF_MEMORYPOOL_MAJORVERSION == 2
            // ����� ��û��
            UINT64 OrderCount_Reserve_nSize;
            UINT64 OrderCount_Reserve_nUnits;
            UINT64 OrderCount_Reserve_Max_nSize;
            UINT64 OrderCount_Reserve_Max_nUnits;
            // ����� ��û�� ���� ū �䱸
            UINT64 MaxRequest_nSize;
            UINT64 MaxRequest_nUnits;
            // ����� ��û ���� ���
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
        // ���� : ����ϴ� �ִ� ���� ũ��(0 : �������� �ʴ� ũ�⸦ ó���ϴ� Ǯ)
        size_t mFN_Query_This_UnitSize() const;
    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        // ���� : ����ϴ� ���� ũ�� ����
        BOOL mFN_Query_This_UnitsSizeLimit(size_t* pOUT_Min, size_t* pOUT_Max) const;
        // ���� : ������� �ʴ� ũ�⸦ ó���ϴ� �޸�Ǯ(���� ����)
        BOOL mFN_Query_isSlowMemoryPool() const;
        // ���� : ĳ�ö��� ���� Ȯ��
        BOOL mFN_Query_MemoryUnit_isAligned(size_t AlignSize) const;
    #endif
        // ���� : ���
        BOOL mFN_Query_Stats(TMemoryPool_Stats* pOUT);
    #if _DEF_MEMORYPOOL_MAJORVERSION == 1
        // ���� : Ȯ��� ��ϼ�����
        size_t mFN_Query_LimitBlocks_per_Expansion();
        // ���� : ��ϴ� ���ּ�
        size_t mFN_Query_Units_per_Block();
        // ���� : ��ϴ� ũ��(�޸�)
        size_t mFN_Query_MemorySize_per_Block();
    #endif

    #if _DEF_MEMORYPOOL_MAJORVERSION == 1
        //----------------------------------------------------------------
        //  ���� ���� ����
        //----------------------------------------------------------------
        // ���� : ���ֿ���� ���� ���� �޸�
        size_t mFN_Query_PreCalculate_ReserveUnits_to_MemorySize(size_t nUnits);
        // ���� : ���ֿ���� ���� ������ ���ּ�
        size_t mFN_Query_PreCalculate_ReserveUnits_to_Units(size_t nUnits);
        // ���� : �޸𸮿���� ���� ���� �޸�
        size_t mFN_Query_PreCalculate_ReserveMemorySize_to_MemorySize(size_t nByte);
        // ���� : �޸𸮿���� ���� ������ ���ּ�
        size_t mFN_Query_PreCalculate_ReserveMemorySize_to_Units(size_t nByte);
    #endif

        //----------------------------------------------------------------
        //  ���� ���� : ����ũ�� ��ŭ �ִ� ũ�� �߰� Ȯ��
        //----------------------------------------------------------------
        // ���� : �޸� ũ�� ����
        // ���� : ���� ������ �޸�(0 : ����)
        size_t mFN_Reserve_Memory__nSize(size_t nByte, BOOL bWriteLog = TRUE);
        // ���� : ���ּ� ����
        // ���� : ���� ������ ���ּ�(0 : ����)
        size_t mFN_Reserve_Memory__nUnits(size_t nUnits, BOOL bWriteLog = TRUE);
    #if _DEF_MEMORYPOOL_MAJORVERSION == 2
        //----------------------------------------------------------------
        //  ���� ���� : �̹� ���� �뷮�� �����Ͽ� �ִ� ũ�� Ȯ��
        //----------------------------------------------------------------
        // ���� : �޸� ũ�� ����
        // ���� : �޸� Ǯ���� �����ϴ� ��ü �޸�
        size_t mFN_Reserve_Memory__Max_nSize(size_t nByte, BOOL bWriteLog = TRUE);
        // ���� : ���ּ� ����
        // ���� : �޸� Ǯ���� �����ϴ� ��ä ����
        size_t mFN_Reserve_Memory__Max_nUnits(size_t nUnits, BOOL bWriteLog = TRUE);
        //----------------------------------------------------------------
        //  �ִ��� �޸� ����
        //  mFN_Reserve_Memory__ ... �迭�� �� �޼ҵ��� ����� �����Ͽ� ó��
        //  ���� �����ϰ� ������ �޸𸮰� ������ �� ����
        //  �� �� �޼ҵ�� ��踦 �������� ����
        //----------------------------------------------------------------
        // ���� : �޸� Ǯ�� ����� �ִ�ũ��(������)
        size_t mFN_Hint_MaxMemorySize_Add(size_t nByte, BOOL bWriteLog = TRUE);
        // ���� : �޸� Ǯ�� ����� �ִ�ũ��(������)
        size_t mFN_Hint_MaxMemorySize_Set(size_t nByte, BOOL bWriteLog = TRUE);
        //----------------------------------------------------------------
        //  ���� ����
        //----------------------------------------------------------------
        // ���μ����� ĳ�� ���
        BOOL mFN_Set_OnOff_ProcessorCACHE(BOOL _On);
        BOOL mFN_Query_OnOff_ProcessorCACHE() const;
    #endif

    #if _DEF_MEMORYPOOL_MAJORVERSION == 1
        // Ȯ���� ���� ����
        //      nUnits_per_Block            : ���� ���ּ� ���� 1~ (0 = �������� ����)
        //      nLimitBlocks_per_Expansion  : Ȯ��� ��ϼ�����  1~? (0 = �������� ����)
        //          �� nLimitBlocks_per_Expansion �� Ȯ����н� ������ �� �ֽ��ϴ�.
        // ���� : ���� ����� ���� ���ּ� ����(0 : ����)
        size_t mFN_Set_ExpansionOption__BlockSize(size_t nUnits_per_Block, size_t nLimitBlocks_per_Expansion = 0, BOOL bWriteLog = TRUE);
        size_t mFN_Query_ExpansionOption__MaxLimitBlocks_per_Expantion() const;
    #endif
        //----------------------------------------------------------------
        void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On); // Default On
    };
}
}

/*----------------------------------------------------------------
/   �޸�Ǯ ������ Interface
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    // ����
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
        /   TLS CACHE ���� �ռ� ����
        /---------------------------------------------------------------*/
        void mFN_Set_TlsCache_AccessFunction(TMemoryPool_TLS_CACHE&(*pFN)(void));
    #endif
    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        // ������ �޸�Ǯ ��
        size_t mFN_Query_Possible_MemoryPools_Num() const;
        // ������ �޸�Ǯ�� ����
        BOOL mFN_Query_Possible_MemoryPools_Size(size_t* _OUT_Array) const;

        size_t mFN_Query_Limit_MaxUnitSize_Default() const;
        size_t mFN_Query_Limit_MaxUnitSize_UserSetting() const;
        // �޸�Ǯ �������� �ִ� ũ�� ����
        BOOL mFN_Set_Limit_MaxUnitSize(size_t _MaxUnitSize_Byte, size_t* _OUT_Result);

        // �� �޸� Ȯ��(�����ϴ� �� �޸��� �ƹ����̳� ����)
        BOOL mFN_Reduce_AnythingHeapMemory();
    #endif

        // ������, ��� �޸�Ǯ�� ���Ͽ�
        void mFN_Set_OnOff_WriteStats_to_LogFile(BOOL _On); // Default On

        // �޸� �������� ����(DEBUG ��忡���� �ǹ̰� ����)
        void mFN_Set_OnOff_Trace_MemoryLeak(BOOL _On); // Default Off
        // �޸�Ǯ ���� �ڼ��� ����(DEBUG ��忡���� �ǹ̰� ����)
        void mFN_Set_OnOff_ReportOutputDebugString(BOOL _On); // Default On

    #if _DEF_MEMORYPOOL_MAJORVERSION > 1
        BOOL mFN_Set_CurrentThread_Affinity(size_t _Index_CpuCore);
        void mFN_Reset_CurrentThread_Affinity();

        // ���� �������� �޸�Ǯ ���� TLS ����
        //      ������Ǯ(��: OMP)�� �� ������ ���ÿ� �޸�Ǯ TLS�� �������� ���� ���¿���,
        //      �޸�Ǯ�� ���� �Ҹ�Ǵ� ���, �뷮�� �޸𸮸� ���� �߻��� �� �ֽ��ϴ�
        //      (����� �α������� �ִٸ� ��� �˴ϴ�)
        //      �޸�Ǯ ���̺귯�� ������ �� ��Ŀ�����忡�� ȣ���ϸ� �� ������ �ذ�˴ϴ�.
        //      �� �� �Լ��� ������ ȣ���ص� ������ �մϴ�
        //      �� TLS�� ������ �޸�Ǯ�� ���ٽ� TLS�� �ٽ� ������ �� �ֽ��ϴ�.
        void mFN_Shutdown_CurrentThread_MemoryPoolTLS();

        TMemoryPool_Virsion mFN_Query_Version() const;
    #endif
    };
}
}


/*----------------------------------------------------------------
/   ����� Interface
/---------------------------------------------------------------*/
namespace UTIL{
namespace MEM{
    /*----------------------------------------------------------------
    /-----------------------------------------------------------------
    /   �޸�Ǯ ��� ���� �������̽�
    /       �� ����� �̸� ã�� �޸�Ǯ�� ���
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
    /   ��ü ���� �޸� �ڿ�
    /       �� ��ü�� ��ӹ޾� �޸�Ǯ�� �����մϴ�
    /-----------------------------------------------------------------
    ----------------------------------------------------------------*/
#pragma region class CMemoryPool Resource
    class CMemoryPoolResource{
    public:
        // Debug - �޸� Ǯ�κ��� �޸𸮸� �Ҵ�޽��ϴ�.
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

        // �޸� Ǯ�� �޸𸮸� �ݳ��մϴ�.
        void operator delete(void* pAddress, const char* _FileName, int _Line)
        {
            UNREFERENCED_PARAMETER(_FileName);
            UNREFERENCED_PARAMETER(_Line);

            g_pMem->mFN_Return_Memory(pAddress);
        }
        // �޸� Ǯ�� �޸𸮸� �ݳ��մϴ�.
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
        size_t m_ID = MAXSIZE_T; // �ʱⰪ ��� ��Ʈ ON

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
    /   �ܺ� �������̽�
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


    // xmemory0 �� template<class _Ty> class allocator �� ȣȯ �ǵ��� �����
    // ȥ�� ������ ���� �̸��� �޸� �Ѵ�
    // �� �Ҵ� ����
    //      1�� ����           list, set, map
    //      n�� ����(��������) queue, deque
    //      n�� ����(�����Ұ�) vector
    //          : �Ҵ� ���� 1 2 3 4 6 9 13 19 28 42 63 94 141 211 316 474 711 1066...
    //            �̴� stl::allocator �� ������ ���� �ٸ� �� �ִ�
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

        // 1�� ���� �Ҵ� ������� �޸�Ǯ�� ���� ������� �����Ѵ�
        pointer _Allocate(size_type _Count, pointer)
        {
            // xmemory0 _Allocate�� �� ũ�Ⱑ 4KB�� ������ �ּҸ� 32�� �����Ͽ� ������
            // ������ �� �޸�Ǯ�� 512B �ʰ��� 32, 1024B �ʰ��� 64�� �����ϱ� ������ �Ű澵 �ʿ� ����

            _Assert(1 <= _Count);
            // ���� ���
            // 1�� ������ �ƴϸ鼭, 64KB�� �ʰ� �ϴ� ��� ���(����κ� ����)
            // _Count ������ �������� �ʰ� �� ������ �������� ���Ѵٸ� ���� �Ҵ��ڸ� ����ϴ� ���� ����
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