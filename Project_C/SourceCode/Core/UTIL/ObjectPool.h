#pragma once
#include "../../BasisClass/LOCK/Lock.h"
/*----------------------------------------------------------------
    ���� ������Ʈ Ǯ(���� ����ũ�Ⱓ ������Ʈ Ǯ�� ����)
        - ���� ũ����� ����
            gFN_Alloc_from_ObjectPool__Shared / gFN_Free_from_ObjectPool__Shared
            gFN_Alloc_from_ObjectPool__Shared__Max1KB / gFN_Free_from_ObjectPool__Shared__Max1KB
        - ���� ����ũ�� ����
            CObjectPool_Handle_TSize<size_t>
            CObjectPool_Handle<Type>

    ����ũ ������Ʈ Ǯ(���� ����ũ�Ⱓ ������Ʈ Ǯ�� �������� ����)
    �� ������ ��쿡 ���
        ��ü������ Free �Ǵ���, �����Ͱ� �����Ǳ⸦ �ٶ�� ���(�̶� ù 4B(x86) �Ǵ� 8B(x64)�� �������� ����)
        ���� ũ������� �Ҵ�/������ �ټ��� ������ƮǮ�� ����Ͽ� ������ƮǮ ��� ������ ���̰� ������
        - ���� ����ũ�� ����
            CObjectPool_Handle__UniquePool<Type>
----------------------------------------------------------------*/

#pragma warning(push)
#pragma warning(disable: 4324)
namespace UTIL{
namespace MEM{
    namespace OBJPOOLCACHE{
        /*----------------------------------------------------------------
        /   �ִ� n KB ������Ʈ Ǯ�� ���, �ʰ��� ���������� _aligned_malloc / _aligned_free ����
        /       �ڵ����� Ȯ��Ǵ� �迭����(std::vector ����)�� ��� ������ ����ũ�Ⱑ ���ǵ��� �ؾ� ��
        /       �׷��� ������ ���ʿ��� ����ũ���� ������ƮǮ�� �ʹ� ���� ������
        ----------------------------------------------------------------*/
        void* gFN_Alloc_from_ObjectPool__Shared(size_t size);
        void gFN_Free_from_ObjectPool__Shared(void* p, size_t size);

        // 1KB ������ Ȯ��Ǵ� �迭����(std::vector ����)�� �����
        // �̰��� �����̳ʿ��� ����ϰ� �� �Ҵ��� TAllocatorOBJ���� ȣ���
        void* gFN_Alloc_from_ObjectPool__Shared__Max1KB(size_t size);
        void gFN_Free_from_ObjectPool__Shared__Max1KB(void* p, size_t size);
    }
}
}


namespace UTIL{
namespace MEM{
    class _DEF_CACHE_ALIGN CObjectPool{
        template<size_t TSize, typename TGroup, BOOL UniqueObjectPool> friend class CObjectPool_Handle_Basis;
        template<size_t TSize> friend class CObjectPool_Handle_TSize;
        struct TSharedObjectPool{};// ���� Ÿ�� �ĺ���

        struct TPTR_LINK{
            TPTR_LINK* pNext = nullptr;
        };
        struct TPTR_LINK_EX : TPTR_LINK{
        #if __X86
            static const size_t c_ThisSize = _DEF_CACHELINE_SIZE;
        #elif __X64
            static const size_t c_ThisSize = _DEF_CACHELINE_SIZE * 2;
        #endif

            size_t nCount = 0;


            static const size_t c_MaxSlot = (c_ThisSize - sizeof(TPTR_LINK) - sizeof(size_t)) / sizeof(void*);
            void* pSlot[c_MaxSlot];
        };
        _MACRO_STATIC_ASSERT(sizeof(TPTR_LINK_EX) == TPTR_LINK_EX::c_ThisSize);


    protected:
        CObjectPool(size_t _UnitSize, BOOL _Unique);
        ~CObjectPool();

        BOOL mFN_Expansion();

        // ���ϰ� : Ȯ���� ������Ʈ ��
        size_t mFN_Alloc_from_List(void** ppOUT, size_t nRequireCount);
        size_t mFN_Alloc_from_ReservedMemory(void** ppOUT, size_t nRequireCount); // �ʿ�� ���ο��� Expansion
        void mFN_Free_to_List(void** pp, size_t Count);

        void mFN_Debug_Overflow_Check_InternalCode(void** ppUnits, size_t nUnits);
        void mFN_Debug_Overflow_Set_InternalCode(void** ppUnits, size_t nUnits);
        void mFN_Debug_Overflow_Set_UserCode(void** ppUnits, size_t nUnits);

    public:
        void* mFN_Alloc();
        void mFN_Free(void* p);

        BOOL mFN_Alloc(void** ppOUT, size_t nCount);
        void mFN_Free(void** pp, size_t nCount);

        void mFN_Set_OnOff_CheckOverflow(BOOL bCheck);

    protected:
        /*----------------------------------------------------------------
        /   ����
        ----------------------------------------------------------------*/
        const UINT32 m_UnitSize;
        BOOL m_UseLinkEX;

        size_t m_DemandSize_Per_Alloc;
        size_t m_Units_Per_Alloc;

        size_t __FreeSlot1[4];

        BOOL m_bCheckOverflow;
        BOOL m_bUniqueObjectPool;
        /*----------------------------------------------------------------
        /   ����
        ----------------------------------------------------------------*/
        _DEF_CACHE_ALIGN ::UTIL::LOCK::CSpinLockQ m_Lock;

        size_t m_nAllocatedUnitss;
        TPTR_LINK* m_pVMEM_First;

        size_t m_nRemaining_Unist_List;
        TPTR_LINK* m_pUnit_First;

        // ���Ḯ��Ʈ�� �������� �ʰ� �ִ� ��ü��
        size_t m_nRemaining_Units_ReseervedMemory;
        byte* m_pReservedMemory;

        size_t __FreeSlot3;
    };
    _MACRO_STATIC_ASSERT(sizeof(CObjectPool) == _DEF_CACHELINE_SIZE*2);
    class _DEF_CACHE_ALIGN CObjectPool_Unique : public CObjectPool{
        CObjectPool_Unique();
        ~CObjectPool_Unique();
    };
    // �� ���� ������� ��ȿ�� �ּ� �ݳ�(������ �ƴ�, ���ĵ��� ����)�� Ȯ���Ϸ���
    // mFN_Free_to_List �� ������ �����Ͽ��� �Ѵ�
}
}


namespace UTIL{
namespace MEM{
    template<size_t TSize, typename TGroup, BOOL UniqueObjectPool>
    class CObjectPool_Handle_Basis : public CSingleton_ManagedDeleteTiming<CObjectPool_Handle_Basis<TSize, TGroup, UniqueObjectPool>>{
        _DEF_FRIEND_SINGLETON
    private:
        CObjectPool_Handle_Basis()
            : m_ObjPool(TSize, UniqueObjectPool)
        {
        }
        // �����Ҵ翡�� ����ڴ� ������ũ�� ���� ���� ũ�⸦ �䱸 �� ���� �ִ�
        // ���� ��� std::unordered_map ��� key �� int �϶� int ���� 1���� �Ҵ��û �ϱ⵵ �Ѵ�
        _MACRO_STATIC_ASSERT(TSize >= 1);

    protected:
        static void* Alloc()
        {
            auto _This = Get_Instance();
            return _This->m_ObjPool.mFN_Alloc();
        }
        static void Free(void* p)
        {
            auto _This = Get_Instance();
            _This->m_ObjPool.mFN_Free(p);
        }
        static BOOL Alloc_N(void** ppOUT, size_t nCount)
        {
            auto _This = Get_Instance();
            return _This->m_ObjPool.mFN_Alloc(ppOUT, nCount);
        }
        static void Free_N(void** pp, size_t nCount)
        {
            auto _This = Get_Instance();
            _This->m_ObjPool.mFN_Free(pp, nCount);
        }

        static CObjectPool* GetPool()
        {
            auto _This = Get_Instance();
            return &_This->m_ObjPool;
        }

    private:
        CObjectPool m_ObjPool;
    };

    template<size_t TSize>
    class CObjectPool_Handle_TSize : public CObjectPool_Handle_Basis<TSize, CObjectPool::TSharedObjectPool, FALSE>{
    public:
        // ���ڸ������� ������� �ʴ� ������ �ִ�
        //using CObjectPool_Handle_Basis::Alloc;
        //using CObjectPool_Handle_Basis::Free;
        //using CObjectPool_Handle_Basis::Alloc_N;
        //using CObjectPool_Handle_Basis::Free_N;
        //using CObjectPool_Handle_Basis::GetPool;
        static void* Alloc(){ return CObjectPool_Handle_Basis<TSize, CObjectPool::TSharedObjectPool, FALSE>::Alloc(); }
        static void Free(void* p){ CObjectPool_Handle_Basis<TSize, CObjectPool::TSharedObjectPool, FALSE>::Free(p); }
        static BOOL Alloc_N(void** ppOUT, size_t nCount){ return CObjectPool_Handle_Basis<TSize, CObjectPool::TSharedObjectPool, FALSE>::Alloc_N(ppOUT, nCount); }
        static void Free_N(void** pp, size_t nCount){ CObjectPool_Handle_Basis<TSize, CObjectPool::TSharedObjectPool, FALSE>::Free_N(pp, nCount); }
        static CObjectPool* GetPool(){ return CObjectPool_Handle_Basis<TSize, CObjectPool::TSharedObjectPool, FALSE>::GetPool(); }
    };

    template<typename T>
    class CObjectPool_Handle : public CObjectPool_Handle_TSize<sizeof(T)>{
    public:
        static void CallConstructor(T* p)
        {
            _MACRO_CALL_CONSTRUCTOR(p, T());
        }
        static void CallDestructor(T* p)
        {
            _MACRO_CALL_DESTRUCTOR(p);
        }
        static T* Alloc_and_CallConstructor()
        {
            T* p = (T*)Alloc();
            if(p)
                CallConstructor(p);
            return p;
        }
        static void Free_and_CallDestructor(T* p)
        {
            if(!p)
                return;
            CallDestructor(p);
            Free(p);
        }
    };
    _MACRO_STATIC_ASSERT(sizeof(CObjectPool_Handle<void*>) == sizeof(CObjectPool_Handle_TSize<sizeof(void*)>));

    template<typename T>
    class CObjectPool_Handle__UniquePool : public CObjectPool_Handle_Basis<sizeof(T), T, TRUE>{
    public:
        // ���ڸ������� ������� �ʴ� ������ �ִ�
        //using CObjectPool_Handle_Basis::Alloc;
        //using CObjectPool_Handle_Basis::Free;
        //using CObjectPool_Handle_Basis::Alloc_N;
        //using CObjectPool_Handle_Basis::Free_N;
        //using CObjectPool_Handle_Basis::GetPool;
        static T* Alloc(){ return (T*)CObjectPool_Handle_Basis<sizeof(T), T, TRUE>::Alloc(); }
        static void Free(T* p){ CObjectPool_Handle_Basis<sizeof(T), T, TRUE>::Free(p); }
        static BOOL Alloc_N(T** ppOUT, size_t nCount){ return CObjectPool_Handle_Basis<sizeof(T), T, TRUE>::Alloc_N((void**)ppOUT, nCount); }
        static void Free_N(T** pp, size_t nCount){ CObjectPool_Handle_Basis<sizeof(T), T, TRUE>::Free_N((void**)pp, nCount); }
        static CObjectPool* GetPool(){ return CObjectPool_Handle_Basis<sizeof(T), T, TRUE>::GetPool(); }

    public:
        static void CallConstructor(T* p)
        {
            _MACRO_CALL_CONSTRUCTOR(p, T());
        }
        static void CallDestructor(T* p)
        {
            _MACRO_CALL_DESTRUCTOR(p);
        }
        static T* Alloc_and_CallConstructor()
        {
            T* p = (T*)Alloc();
            if(p)
                CallConstructor(p);
            return p;
        }
        static void Free_and_CallDestructor(T* p)
        {
            if(!p)
                return;
            CallDestructor(p);
            Free(p);
        }
    };

    template<class _Ty>
    struct TAllocatorOBJ
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
            _Assert(1 <= _Count);

            return (pointer)OBJPOOLCACHE::gFN_Alloc_from_ObjectPool__Shared__Max1KB(sizeof(_Ty)*_Count);
        }
        void _Deallocate(pointer _Ptr, size_type _Count)
        {
            _Assert(nullptr != _Ptr && 1 <= _Count);

            OBJPOOLCACHE::gFN_Free_from_ObjectPool__Shared__Max1KB(_Ptr, sizeof(_Ty)*_Count);
        }
        TAllocatorOBJ() noexcept
        {
        }

        TAllocatorOBJ(const TAllocatorOBJ<_Ty>&) noexcept
        {
        }

        template<class _Other>
        TAllocatorOBJ(const TAllocatorOBJ<_Other>&) noexcept
        {
        }
        ~TAllocatorOBJ()
        {
        }

        template<class _Other>
        TAllocatorOBJ<_Ty>& operator=(const TAllocatorOBJ<_Other>&)
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
#pragma warning(pop)