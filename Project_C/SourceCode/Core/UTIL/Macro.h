#pragma once
#include "./DebugMemory.h"
#include <functional> 
/*----------------------------------------------------------------
/   [�Ϲݿ�]
/   ��ü �޸� ����
/   _DEF_CACHELINE_SIZE             // ĳ�ö��� ũ��
/   _DEF_CACHE_ALIGN                // ĳ�ö��ο� ����
/   _DEF_CACHE_ALIGNED_TEST__THIS   // ĳ�ö��ο� ���� �׽�Ʈ this 
/   _DEF_CACHE_ALIGNED_TEST__PTR    // ĳ�ö��ο� ���� �׽�Ʈ ptr
/
/   �Ϲ� ��ũ��
/       _MACRO_ARRAY_COUNT
/       _MACRO_STATIC_BIT_COUNT
/       _MACRO_BIT_COUNT
/       _MACRO_BIT_COUNT_MEM
/       _MACRO_POINTER_IS_ALIGNED
/       _MACRO_POINTER_GET_ALIGNED
/       _MACRO_TYPE_SIZE
/       _MACRO_CALL_CONSTRUCTOR
/       _MACRO_CALL_DESTRUCTOR
/       _DEF_VARIABLE_UNIQUE_NAME
/       _MACRO_CREATE__FUNCTION_OBJECT_FINALLY
/       _MACRO_CREATE__FUNCTION_OBJECT_FINALLYQ
/       _MACRO_SAFE_LOCAL_ALLOCA
/
/   SAFE �ø���
/       _SAFE_NEW
/       _SAFE_DELETE
/       _SAFE_NEW_ARRAY
/       _SAFE_DELETE_ARRAY
/
/       _SAFE_NEW_ALIGNED_CACHELINE
/       _SAFE_DELETE_ALIGNED_CACHELINE
/       _SAFE_NEW_ARRAY_ALIGNED_CACHELINE
/       _SAFE_DELETE_ARRAY_ALIGNED_CACHELINE
/
/       _SAFE_RELEASE
/       _SAFE_CLOSE_HANDLE
/
/   ��ġ ��(���� ����)    ==  <   <=  >   >=
/       UTIL::MATH::Compare__
/
/   �������ϸ�
/       _MACRO_PROFILING_BEGIN
/       _MACRO_PROFILING_END
/       _MACRO_PROFILING_TIME
/
/   Windows / Thread / Process
/       UTIL::TUTIL::mFN_GetRootWindow
/       UTIL::TUTIL::mFN_Set_ClientSize
/       UTIL::TUTIL::mFN_Change_WinProc
/       UTIL::TUTIL::mFN_SetThreadName
/       UTIL::TUTIL::sFN_SwithToThread
/
/   ������ �޼����ڽ�(���� ���ڿ�)
/       _MACRO_MESSAGEBOX
/       _MACRO_MESSAGEBOX__OK
/       _MACRO_MESSAGEBOX__WARNING
/       _MACRO_MESSAGEBOX__ERROR
/       _MACRO_MESSAGEBOX__WITH__OUTPUTDEBUGSTR_ALWAYS
/
/   ��Ÿ
/       UTIL::TUTIL::mFN_GetCurrentProjectCompileTime
/       UTIL::TUTIL::mFN_BitCount
/       UTIL::TUTIL::mFN_MemoryCompare
/       UTIL::TUTIL::mFN_MemoryCompareSSE2
/       UTIL::TUTIL::mFN_MemoryScan
/       UTIL::TUTIL::mFN_MemoryScanCompareAB
/       _Error                                      ������ ����� ���� ������ �����߻�
/       _Error_OutOfMemory                          ������ ����� ���� ������ �����߻�(�޸� ����)
/       _AssertRelease(���� �� �޼���)              �������� ���н� ����(No Debug ����)
/       _AssertReleaseMsg(����, �޼���)             �������� ���н� ����(No Debug ����)
/       _CompileHint(����)
/
/   ���ڿ�
/       UTIL::TUTIL::mFN_strlen                                 ���ڿ� ����
/       UTIL::TUTIL::mFN_Make_Clone__String                     ���ο� ���ڿ� ���۸� �Ҵ� �� ����
/       UTIL::TUTIL::mFN_Make_Clone__String__From_MemoryPool    ���ο� ���ڿ� ���۸� �Ҵ� �� ����(�޸�Ǯ ����)
/
/       _MACRO_SPRINTF              sprintf_s CHAR / WCHAR
/       _MACRO_SPRINTF_TRACE        sprintf_s CHAR / WCHAR + FileName(Line) :
/
/
/   ��ó���� ��ũ��
/       _DEF_MAKE_GET_METHOD
/       _DEF_MAKE_GETSET_METHOD
/       _DEF_LOCAL_STRING_RETURN__BY_CASE
/       _DEF_STR_AB                                 �̸� ����
/       _DEF_STR_ABC                                �̸� ����
/       _DEF_NUM_TO_STR                             ���ڸ� ���ڷ� ��ȯ
/       _DEF_STR_SOURCECODE_FOCUS                   "����ҽ��ڵ�(�������) : "
/       ������ ����
/           _DEF_COMPILE_MSG                ����� �޼���
/           _DEF_COMPILE_MSG__FOCUS         ���ϸ�(line) : ����� �޼���
/           _DEF_COMPILE_MSG__WARNING       ���ϸ�(line) : warning :����� �޼���
/
/
/*----------------------------------------------------------------
/   [������]
/
/   ������ Ÿ�� ������
/       _MACRO_STATIC_ASSERT(���� �� �޼���)
/   ������ Ÿ�� Ÿ�Ժ�
/       _MACRO_IS_SAME__VARIABLE_TYPE(����1, ����2)
/       _MACRO_IS_SAME__VARIABLE_TYPE_TV(Ÿ��, ����)
/       _MACRO_IS_SAME__VARIABLE_TYPE_TT(Ÿ��, Ÿ��)
/
/   ��Ÿ�� �ߴ��� �߻���
/       _DebugBreak(�޼���)
/       _Assert(���� �� �޼���)
/       _AssertMsg(����, �޼���)
/
/   ����� ���â
/       _MACRO_OUTPUT_DEBUG_STRING_ALWAYS           // DEBUG RELEASE ���о��� OutputDebugString
/       _MACRO_OUTPUT_DEBUG_STRING_TRACE_ALWAYS     // DEBUG RELEASE ���о��� OutputDebugString
/       _MACRO_OUTPUT_DEBUG_STRING
/       _MACRO_OUTPUT_DEBUG_STRING_TRACE
/
/   �޸� ���� ���
/       _MACRO_MEMORY_SCAN_OUTPUT_DEBUG_STRING
/       _MACRO_MEMORY_SCAN_COMPARE_AB_OUTPUT_DEBUG_STRING
/
----------------------------------------------------------------*/
#pragma region Object Memory Align
#if __X86 || __X64
#define _DEF_CACHELINE_SIZE SYSTEM_CACHE_ALIGNMENT_SIZE 
#define _DEF_CACHE_ALIGN __declspec(align(_DEF_CACHELINE_SIZE))
// _DEF_CACHE_ALIGN �� ������ ��ü�� _DEF_CACHE_ALIGNED_TEST.. �� �̿��Ͽ� �׽�Ʈ�ؾ� ��
#define _DEF_CACHE_ALIGNED_TEST__THIS() {if(!_MACRO_POINTER_IS_ALIGNED(this, _DEF_CACHELINE_SIZE)) _Error(_T("Not Aligned PTR: 0x%p"), this);}
#define _DEF_CACHE_ALIGNED_TEST__PTR(ptr) {if(!_MACRO_POINTER_IS_ALIGNED(ptr, _DEF_CACHELINE_SIZE)) _Error(_T("Not Aligned PTR: 0x%p"), ptr);}
#else
#error Not Defined
#endif
#pragma endregion ~ Object Memory Align

#pragma region Generic Macro
//----------------------------------------------------------------
//  �������� �͵��� �Լ���, ��ũ�� ó�� ���̹���
//----------------------------------------------------------------
#if defined(ARRAYSIZE)
#define _MACRO_ARRAY_COUNT(_Array)                  ARRAYSIZE(_Array)
#else
#define _MACRO_ARRAY_COUNT(_Array)                  (sizeof(_Array) / sizeof(_Array[0]))
#endif

namespace UTIL{
    namespace PRIVATE{
        template<UINT64 val>
        struct TBIT_COUNTING_STATIC{
            static const byte value = (val & 0x01) + TBIT_COUNTING_STATIC<(val>>1)>::value;
        };
        template<>
        struct TBIT_COUNTING_STATIC<0>{
            static const byte value = 0;
        };
        template<size_t Size>struct TBIT_COUNTING{
            template<typename T> static size_t Counting(T val){ return ::UTIL::g_pUtil->mFN_BitCount((UINT8*)ptr, Size); }
        };
        template<>struct TBIT_COUNTING<1>{
            template<typename T> static size_t Counting(T val){ return ::UTIL::g_pUtil->mFN_BitCount((UINT8)val); }
        };
        template<>struct TBIT_COUNTING<2>{
            template<typename T> static size_t Counting(T val){ return ::UTIL::g_pUtil->mFN_BitCount((UINT16)val); }
        };
        template<>struct TBIT_COUNTING<4>{
            template<typename T> static size_t Counting(T val){ return ::UTIL::g_pUtil->mFN_BitCount((UINT32)val); }
        };
        template<>struct TBIT_COUNTING<8>{
            template<typename T> static size_t Counting(T val){ return ::UTIL::g_pUtil->mFN_BitCount((UINT64)val); }
        };

        template<size_t _nByte>
        __forceinline BOOL isAlignedPointer(const void* p){
            if((size_t)p & (size_t)(_nByte-1))
                return FALSE;
            return TRUE;
        }
        template<size_t _nByte, typename T>
        __forceinline T* PointerAlign(T* p){
           const size_t ret = (size_t)p & ~(_nByte-1);
           return (T*)ret;
        }
    }
}
#define _MACRO_STATIC_BIT_COUNT(val)     (::UTIL::PRIVATE::TBIT_COUNTING_STATIC<val>::value)
#define _MACRO_BIT_COUNT(val)            (::UTIL::PRIVATE::TBIT_COUNTING<sizeof(val)>::Counting(val))
#define _MACRO_BIT_COUNT_MEM(p, nByte)   (::UTIL::g_pUtil->mFN_BitCount((UINT8*)p, nByte))
#define _MACRO_POINTER_IS_ALIGNED(p, nByte)  (::UTIL::PRIVATE::isAlignedPointer<nByte>(p))
#define _MACRO_POINTER_GET_ALIGNED(p, nByte) (::UTIL::PRIVATE::PointerAlign<nByte>(p))


template<typename T> size_t _MACRO_TYPE_SIZE(){return sizeof(T);}
template<typename T> size_t _MACRO_TYPE_SIZE(const T& ref){return sizeof(T);}
template<typename T> size_t _MACRO_TYPE_SIZE(const T* p){return sizeof(T);}

#pragma region CALL CONSTRUCTOR / CALL DESTRUCTOR
namespace UTIL{
namespace PRIVATE{
    template<typename T>
    void gFN_Call_Destructor(T* p){
        p->~T();
    }
}
}
// ������ ���� ȣ��
#define _MACRO_CALL_CONSTRUCTOR(Address, Constructor) new(Address) Constructor

// �Ҹ��� ���� ȣ��
#define _MACRO_CALL_DESTRUCTOR(Address)         UTIL::PRIVATE::gFN_Call_Destructor(Address)
#pragma endregion ~ CALL CONSTRUCTOR / CALL DESTRUCTOR

#pragma region FUNCTION FINALLY OBJECT
namespace UTIL{
    namespace PRIVATE{
        // Ver1
        class CCallFinishFunction{
        public:
            CCallFinishFunction(std::function<void()>& _function)
                : m_FN(_function)
            {
            }
            CCallFinishFunction(std::function<void()>&& _function)
                : m_FN(std::move(_function))
            {
            }
            ~CCallFinishFunction()
            {
                m_FN();
            }
        private:
            CCallFinishFunction(const CCallFinishFunction&);
            CCallFinishFunction& operator = (const CCallFinishFunction&);

            std::function<void()> m_FN;
        };
        // Ver2 std::function ���� ���� ������
        template<typename TFunction>
        class CCallFinishFunctionQ{
        public:
            CCallFinishFunctionQ(TFunction& _pFunction)
                : m_ppFunction(&_pFunction)
            {
            }
            ~CCallFinishFunctionQ()
            {
                if(m_ppFunction)
                    (*m_ppFunction)();
            }
        private:
            CCallFinishFunctionQ(const CCallFinishFunctionQ&);
            CCallFinishFunctionQ& operator = (const CCallFinishFunctionQ&);

            TFunction* m_ppFunction = nullptr;
        };
    }
}
#ifdef __COUNTER__
#define _DEF_VARIABLE_UNIQUE_NAME(Prefix) _DEF_STR_ABC(Prefix, _, __COUNTER__)
#else
#define _DEF_VARIABLE_UNIQUE_NAME(Prefix) _DEF_STR_ABC(Prefix, _, __LINE__)
#endif
#define _MACRO_CREATE__FUNCTION_OBJECT_FINALLY(pFunction) ::UTIL::PRIVATE::CCallFinishFunction _DEF_VARIABLE_UNIQUE_NAME(__Local_HiddenObject)(pFunction)
#define _MACRO_CREATE__FUNCTION_OBJECT_FINALLYQ(pFunction) ::UTIL::PRIVATE::CCallFinishFunctionQ<decltype(pFunction)> _DEF_VARIABLE_UNIQUE_NAME(__Local_HiddenObject)(pFunction)
#pragma endregion ~ FUNCTION FINALLY OBJECT

#pragma region _MACRO_SAFE_LOCAL_ALLOCA
namespace UTIL{
    namespace PRIVATE{
        class CLocalTemporaryMemory{
        public:
            CLocalTemporaryMemory()
                : m_pMemory(nullptr)
                , m_bUseAlloca(FALSE)
            {
            }
            CLocalTemporaryMemory(void* p, BOOL bUseAlloca)
                : m_pMemory(p)
                , m_bUseAlloca(bUseAlloca)
            {
            }
            ~CLocalTemporaryMemory()
            {
                FreeMemory();
            }
            void SetMemory(void* p, BOOL bUseAlloca)
            {
                FreeMemory();
                m_pMemory = p, m_bUseAlloca = bUseAlloca;
            }
            void FreeMemory()
            {
                if(!m_pMemory)
                    return;
                if(!m_bUseAlloca)
                    free(m_pMemory);
                m_pMemory = nullptr;
            }
            void* GetMemory()
            {
                return m_pMemory;
            }
            void* operator () ()
            {
                return m_pMemory;
            }

        private:
            void* m_pMemory;
            BOOL m_bUseAlloca;
        };
    }
}
#define _MACRO_SAFE_LOCAL_ALLOCA(pResult, Size, RemainingStackSize) \
    _Assert(0 < Size && 0 <= RemainingStackSize); \
    ::UTIL::PRIVATE::CLocalTemporaryMemory __LocalTemporaryMemory__##pResult; \
    __pragma(warning(push)) \
    __pragma(warning(disable: 6255)) \
    __pragma(warning(disable: 6263)) \
    if(Size <= RemainingStackSize) __LocalTemporaryMemory__##pResult.SetMemory(_alloca(Size), TRUE); \
    else __LocalTemporaryMemory__##pResult.SetMemory(malloc(Size), FALSE); \
    __pragma(warning(pop)) \
    pResult = decltype(pResult)(__LocalTemporaryMemory__##pResult.GetMemory())
#pragma endregion ~ _MACRO_SAFE_LOCAL_ALLOCA

#pragma endregion ~ Generic Macro

#pragma region Simple Profiling
#define _MACRO_PROFILING_BEGIN(name)  DWORD _timeSimpleProfile##name = timeGetTime();
#define _MACRO_PROFILING_END(name)    _timeSimpleProfile##name = timeGetTime() - _timeSimpleProfile##name;
#define _MACRO_PROFILING_TIME(name)   (_timeSimpleProfile##name)
#pragma endregion

#pragma region SAFE NEW / SAFE DELETE / SAFE RELEASE / SAFE CLOSE HANDLE
#define _SAFE_NEW(Address, Constructor)             {if(Address)delete Address; Address = NEW Constructor;}
#define _SAFE_DELETE(Address)                       {if(Address)delete Address; Address = NULL;}
#define _SAFE_NEW_ARRAY(Address, Type, Num)         {if(Address)delete[] Address; Address = NEW Type[Num];}
#define _SAFE_DELETE_ARRAY(Address)                 {if(Address)delete[] Address; Address = NULL;}

#pragma region ALIGNED(NEW / DELETE)
namespace UTIL{
namespace PRIVATE{
    template<typename T>
    void gFN_aligned_malloc(T*& p, size_t cnt, size_t alignment)
    {
        p = (T*)::_aligned_malloc(sizeof(T) * cnt, alignment);
    }
    template<typename T>
    void gFN_aligned_malloc_dbg(T*& p, size_t cnt, size_t alignment, LPCSTR _F, int _L)
    {
        p = (T*)::_aligned_malloc_dbg(sizeof(T) * cnt, alignment, _F, _L);
    }
    template<typename T>
    void gFN_aligned_free(T*& p)
    {
        _aligned_free(p);
    }
    template<typename T>
    void gFN_aligned_free_dbg(T*& p)
    {
        _aligned_free_dbg(p);
    }
}
}

#if _DEF_USING_DEBUG_MEMORY_LEAK
#define _SAFE_NEW_ALIGNED_CACHELINE(Address, Constructor)                   \
{                                                                           \
    if(Address) _MACRO_CALL_DESTRUCTOR(Address);                            \
    else UTIL::PRIVATE::gFN_aligned_malloc_dbg(Address, 1,  _DEF_CACHELINE_SIZE, __FILE__, __LINE__);\
                                                                            \
    if(Address) _MACRO_CALL_CONSTRUCTOR(Address, Constructor);              \
}
#define _SAFE_DELETE_ALIGNED_CACHELINE(Address)                             \
{                                                                           \
    if(Address){                                                            \
        _MACRO_CALL_DESTRUCTOR(Address);                                    \
        UTIL::PRIVATE::gFN_aligned_free_dbg(Address);                       \
        Address = NULL;                                                     \
    }                                                                       \
}
#define _SAFE_NEW_ARRAY_ALIGNED_CACHELINE(Address, Constructor, Num)        \
{                                                                           \
    if(Address){                                                            \
        _Error("�̹� �Ҵ�ȼ��� ������ �� �� ����");                      \
    }                                                                       \
    else{                                                                   \
        UTIL::PRIVATE::gFN_aligned_malloc_dbg(Address, Num,  _DEF_CACHELINE_SIZE, __FILE__, __LINE__);\
    }                                                                       \
                                                                            \
    if(Address){                                                            \
        for(size_t __i=0; __i<Num; __i++)_MACRO_CALL_CONSTRUCTOR(Address+__i, Constructor);\
    }                                                                       \
}
#define _SAFE_DELETE_ARRAY_ALIGNED_CACHELINE(Address, Num)                  \
{                                                                           \
    if(Address && 0 < Num){                                                 \
        for(size_t __i=0; __i<Num; __i++)_MACRO_CALL_DESTRUCTOR(Address+__i);\
        UTIL::PRIVATE::gFN_aligned_free_dbg(Address);                       \
        Address = NULL;                                                     \
    }                                                                       \
}
#else
#define _SAFE_NEW_ALIGNED_CACHELINE(Address, Constructor)                   \
{                                                                           \
    if(Address) _MACRO_CALL_DESTRUCTOR(Address);                            \
    else UTIL::PRIVATE::gFN_aligned_malloc(Address, 1, _DEF_CACHELINE_SIZE);    \
                                                                            \
    if(Address) _MACRO_CALL_CONSTRUCTOR(Address, Constructor);              \
}
#define _SAFE_DELETE_ALIGNED_CACHELINE(Address)                             \
{                                                                           \
    if(Address){                                                            \
        _MACRO_CALL_DESTRUCTOR(Address);                                    \
        UTIL::PRIVATE::gFN_aligned_free(Address);                           \
        Address = NULL;                                                     \
    }                                                                       \
}
#define _SAFE_NEW_ARRAY_ALIGNED_CACHELINE(Address, Constructor, Num)        \
{                                                                           \
    if(Address){                                                            \
        _Error("�̹� �Ҵ�ȼ��� ������ �� �� ����");                      \
    }                                                                       \
    else{                                                                   \
        UTIL::PRIVATE::gFN_aligned_malloc(Address, Num, _DEF_CACHELINE_SIZE);   \
    }                                                                       \
                                                                            \
    if(Address){                                                            \
        for(size_t __i=0; __i<Num; __i++)_MACRO_CALL_CONSTRUCTOR(Address+__i, Constructor);\
    }                                                                       \
}
#define _SAFE_DELETE_ARRAY_ALIGNED_CACHELINE(Address, Num)                  \
{                                                                           \
    if(Address && 0 < Num){                                                 \
        for(size_t __i=0; __i<Num; __i++)_MACRO_CALL_DESTRUCTOR(Address+__i);\
        UTIL::PRIVATE::gFN_aligned_free(Address);                           \
        Address = NULL;                                                     \
    }                                                                       \
}
#endif
#pragma endregion


#define _SAFE_RELEASE(Data)                 {if(Data){Data->Release(); Data = NULL;}}
#define _SAFE_CLOSE_HANDLE(Data)            {if(Data){::CloseHandle(Data); Data = NULL;}}
#pragma endregion


#pragma region ��ó���� ��ũ��
#define _DEF_MAKE_GET_METHOD(_Name, _Type, _MemberData)                 \
    _Type   mFN_Get_##_Name##() const       { return _MemberData;     }

#define _DEF_MAKE_GETSET_METHOD(_Name, _Type, _MemberData)              \
    _Type   mFN_Get_##_Name##() const       { return _MemberData;     } \
    void    mFN_Set_##_Name##(_Type _Value) { _MemberData = _Value;   }
#define _DEF_LOCAL_STRING_RETURN__BY_CASE(code)     case code: return _T(#code);

#pragma region internal
#define _DEF_STR_AB__internal(A, B) A##B
#define _DEF_STR_ABC__internal(A, B, C) A##B##C
#define _DEF_NUM_TO_STR__internal(Num) #Num
#pragma endregion // ~ internal

#define _DEF_STR_AB(A, B)       _DEF_STR_AB__internal(A, B)
#define _DEF_STR_ABC(A, B, C)   _DEF_STR_ABC__internal(A, B, C)
#define _DEF_NUM_TO_STR(Num)    _DEF_NUM_TO_STR__internal(Num)
#define _DEF_STR_SOURCECODE_FOCUS __FILE__##"("##_DEF_NUM_TO_STR(__LINE__)##") : "
#define _DEF_COMPILE_MSG(msg)          __pragma(message(msg))
#define _DEF_COMPILE_MSG__FOCUS(msg)   __pragma(message(_DEF_STR_SOURCECODE_FOCUS##msg))
#define _DEF_COMPILE_MSG__WARNING(msg) __pragma(message(_DEF_STR_SOURCECODE_FOCUS##" warning : "##msg))
#pragma endregion // ~ ��ó���� ��ũ��

#pragma region ��Ÿ
#define _MACRO_THROW_EXCEPTION__OUTOFMEMORY ::RaiseException((DWORD)E_OUTOFMEMORY, EXCEPTION_NONCONTINUABLE, 0, NULL)

#ifdef _DEBUG
#define _DebugBreak(...)                                                                    \
    {                                                                                       \
        static BOOL _bIgnoreBreak = FALSE;                                                  \
        static int  _CountBreak = 0;                                                        \
        if( !_bIgnoreBreak )                                                                \
        {                                                                                   \
            ++_CountBreak;                                                                  \
            switch(UTIL::TUTIL::sFN_DebugBreakMessage(_CountBreak, __FILE__, __LINE__, __VA_ARGS__))\
            {                                                                               \
            case IDABORT:                                                                   \
                __debugbreak();                                                             \
                break;                                                                      \
            case IDIGNORE:                                                                  \
                _bIgnoreBreak = TRUE;                                                       \
                break;                                                                      \
            case IDRETRY:                                                                   \
                break;                                                                      \
            }                                                                               \
        }                                                                                   \
    }
#define _Error( ...)                    _DebugBreak(__VA_ARGS__)
#define _Error_OutOfMemory()            _MACRO_THROW_EXCEPTION__OUTOFMEMORY
#define _AssertRelease(Expr)            _AssertReleaseMsg(Expr, _T(#Expr))
#define _AssertReleaseMsg(Expr, ...)    {if(!(Expr))_DebugBreak(__VA_ARGS__);}
#define _CompileHint(Expr)              {if(!(Expr))_DebugBreak(_T(#Expr));}
#else
#define _DebugBreak(...)                __noop
#define _Error(...)                     *((UINT32*)NULL) = 0x003BBA4A
#define _Error_OutOfMemory()            _MACRO_THROW_EXCEPTION__OUTOFMEMORY
#define _AssertRelease(Expr)            _AssertReleaseMsg(Expr, Expr)
#define _AssertReleaseMsg(Expr, ...)    {if(!(Expr))_Error(0);}
#define _CompileHint(Expr)              __assume(Expr)
#endif
#pragma endregion

#define _MACRO_MESSAGEBOX(Type, szTitle, ...)    ::UTIL::TUTIL::sFN_Messagebox((Type), szTitle, __VA_ARGS__)
#define _MACRO_MESSAGEBOX__OK(szTitle, ...)      ::UTIL::TUTIL::sFN_Messagebox(MB_OK, szTitle, __VA_ARGS__)
#define _MACRO_MESSAGEBOX__WARNING(szTitle, ...) ::UTIL::TUTIL::sFN_Messagebox(MB_ICONWARNING, szTitle, __VA_ARGS__)
#define _MACRO_MESSAGEBOX__ERROR(szTitle, ...)   ::UTIL::TUTIL::sFN_Messagebox(MB_ICONERROR, szTitle, __VA_ARGS__)
#define _MACRO_MESSAGEBOX__WITH__OUTPUTDEBUGSTR_ALWAYS(Type, szTitle, ...) ::UTIL::TUTIL::sFN_Messagebox_With_OutputDebugStr((Type), szTitle, __VA_ARGS__)

namespace UTIL{

    struct TSystemTime{
        TSystemTime(): Year(-1), Month(-1), Day(-1), Hour(-1), Minute(-1), Second(-1){}
        TSystemTime(int dy, int dm, int dd, int th, int tm, int ts) :Year(dy), Month(dm), Day(dd), Hour(th), Minute(tm), Second(ts){}
        int Year, Month, Day, Hour, Minute, Second;
    };

    struct TUTIL{
#pragma region ���ڿ� ó��
        template<typename _WordType>
        static BOOL sFN_isMultiByte(_WordType)
        {
            return (sizeof(_WordType) == 1);
        }
        template<typename _WordType>
        static BOOL sFN_isMultiByte(_WordType*)
        {
            return (sizeof(_WordType) == 1);
        }

        virtual size_t mFN_strlen(LPCSTR _str);
        virtual size_t mFN_strlen(LPCWSTR _str);
        /*----------------------------------------------------------------
        /       ���ڿ��� ī���մϴ�.
        /       Dest�� ���۸� �Ҵ���, ���ڿ��� �����մϴ�.
        ----------------------------------------------------------------*/
        virtual void mFN_Make_Clone__String(LPSTR&  _pDest, LPCSTR  _pSorce);
        virtual void mFN_Make_Clone__String(LPSTR&  _pDest, LPCWSTR _pSorce);
        virtual void mFN_Make_Clone__String(LPWSTR& _pDest, LPCSTR  _pSorce);
        virtual void mFN_Make_Clone__String(LPWSTR& _pDest, LPCWSTR _pSorce);
        /*----------------------------------------------------------------
        /       ���ڿ��� ī���մϴ�.(�޸�Ǯ ����)
        ----------------------------------------------------------------*/
        virtual void mFN_Make_Clone__String__From_MemoryPool(LPSTR&  _pDest, LPCSTR  _pSorce);
        virtual void mFN_Make_Clone__String__From_MemoryPool(LPSTR&  _pDest, LPCWSTR _pSorce);
        virtual void mFN_Make_Clone__String__From_MemoryPool(LPWSTR& _pDest, LPCSTR  _pSorce);
        virtual void mFN_Make_Clone__String__From_MemoryPool(LPWSTR& _pDest, LPCWSTR _pSorce);

        static inline int sFN_Sprintf(LPSTR _pDest, size_t _nDest, LPCSTR fmt, va_list vaArglist)
        {
            if((int)_nDest <= _vscprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return 0;
            }
            return vsprintf_s(_pDest, _nDest, fmt, vaArglist);
        }
        static int sFN_Sprintf(LPSTR _pDest, size_t _nDest, LPCSTR fmt, ...)
        {
            va_list	vaArglist;
            va_start(vaArglist, fmt);
            int r = sFN_Sprintf(_pDest, _nDest, fmt, vaArglist);
            va_end(vaArglist);

            return r;
        }
        static inline int sFN_Sprintf(LPWSTR _pDest, size_t _nDest, LPCWSTR fmt, va_list vaArglist)
        {
            if((int)_nDest < _vscwprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return 0;
            }
            return vswprintf_s(_pDest, _nDest, fmt, vaArglist);
        }
        static int sFN_Sprintf(LPWSTR _pDest, size_t _nDest, LPCWSTR fmt, ...)
        {
            va_list	vaArglist;
            va_start(vaArglist, fmt);
            int r = sFN_Sprintf(_pDest, _nDest, fmt, vaArglist);
            va_end(vaArglist);

            return r;
        }
        static inline int sFN_Sprintf_With_Trace(LPCSTR _szFileName, int _Line, LPSTR _pDest, size_t _nDest, LPCSTR fmt, va_list vaArglist)
        {
            int n = sprintf_s(_pDest, _nDest, "%s(%d): ", _szFileName, _Line);
            if(n == -1)
                return 0;

            if((int)_nDest-n <= _vscprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return 0;
            }

            return n + vsprintf_s(_pDest+n, _nDest-n, fmt, vaArglist);
        }
        static int sFN_Sprintf_With_Trace(LPCSTR _szFileName, int _Line, LPSTR _pDest, size_t _nDest, LPCSTR fmt, ...)
        {

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            int r = sFN_Sprintf_With_Trace(_szFileName, _Line, _pDest, _nDest, fmt, vaArglist);
            va_end(vaArglist);
            
            return r;
        }
        static inline int sFN_Sprintf_With_Trace(LPCSTR _szFileName, int _Line, LPWSTR _pDest, size_t _nDest, LPCWSTR fmt, va_list vaArglist)
        {
            int n, r=0;
            // ���ϰ� Ȯ��
            n = MultiByteToWideChar(CP_ACP, 0, _szFileName, -1, _pDest, (int)_nDest);
            if(n==0)
                return 0;
            n--; // MultiByteToWideChar �� \0�� �����Ͽ� ī�����Ѵ�
            _pDest += n;
            _nDest -= n;
            r += n;

            n = swprintf_s(_pDest, _nDest, L"(%d): ", _Line);
            if(n == -1)
                return 0;
            _pDest += n;
            _nDest -= n;
            r += n;

            if((int)_nDest < _vscwprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return 0;
            }
            return r + vswprintf_s(_pDest, _nDest, fmt, vaArglist);
        }
        static int sFN_Sprintf_With_Trace(LPCSTR _szFileName, int _Line, LPWSTR _pDest, size_t _nDest, LPCWSTR fmt, ...)
        {
            va_list	vaArglist;
            va_start(vaArglist, fmt);
            int r = sFN_Sprintf_With_Trace(_szFileName, _Line, _pDest, _nDest, fmt, vaArglist);
            va_end(vaArglist);

            return r; 
        }
        template<typename _TypeWord, size_t _Size>
        inline static int sFN_Sprintf(_TypeWord(&_pDest)[_Size], const _TypeWord* fmt, va_list vaArglist)
        {
            return sFN_Sprintf(_pDest, _Size, fmt, vaArglist);
        }
        template<typename _TypeWord, size_t _Size>
        inline static int sFN_Sprintf(_TypeWord(&_pDest)[_Size], const _TypeWord* fmt, ...)
        {
            va_list vaArglist;
            va_start(vaArglist, fmt);
            int r = sFN_Sprintf(_pDest, _Size, fmt, vaArglist);
            va_end(vaArglist);

            return r;
        }
        template<typename _TypeWord, size_t _Size>
        inline static int sFN_Sprintf_With_Trace(LPCSTR _szFileName, int _Line, _TypeWord(&_pDest)[_Size], const _TypeWord* fmt, va_list vaArglist)
        {
            return sFN_Sprintf_With_Trace(_szFileName, _Line, _pDest, _Size, fmt, vaArglist);
        }
        template<typename _TypeWord, size_t _Size>
        inline static int sFN_Sprintf_With_Trace(LPCSTR _szFileName, int _Line, _TypeWord (&_pDest)[_Size], const _TypeWord* fmt, ...)
        {
            va_list vaArglist;
            va_start(vaArglist, fmt);
            int r = sFN_Sprintf_With_Trace(_szFileName, _Line, _pDest, _Size, fmt, vaArglist);
            va_end(vaArglist);

            return r;
        }
        #define _MACRO_SPRINTF(pDest, LenDest, ...)         UTIL::TUTIL::sFN_Sprintf(pDest, LenDest, __VA_ARGS__)
        #define _MACRO_SPRINTF_TRACE(pDest, LenDest, ...)   ::UTIL::TUTIL::sFN_Sprintf_With_Trace(__FILE__, __LINE__, pDest, LenDest, __VA_ARGS__)
#pragma endregion

#pragma region Windows / Thread
        /*----------------------------------------------------------------
        /   ��Ʈ �������ڵ��� ���ϴ� �Լ�
        /       ���ϵ� �Ӽ��� ������ �ʴ� ���� ����� ������ �θ������츦 ã���ϴ�.
        /       _bNonChild  - TRUE  : ���ϵ�Ӽ��� �ƴ� ����� ���� ������
        /                   - FALSE : �ֻ����� ������
        ----------------------------------------------------------------*/
        virtual HWND mFN_GetRootWindow(HWND _hWnd, BOOL _bNonChild = FALSE);
        /*----------------------------------------------------------------
        /   �������� Ŭ���̾�Ʈ ���� ũ�⸦ �����մϴ�.
        ----------------------------------------------------------------*/
        virtual BOOL mFN_Set_ClientSize(HWND _hWnd, LONG _Width, LONG _Height);
        /*----------------------------------------------------------------
        /   WinProc�� �����մϴ�. (Return: ���� / ����)
        /       _pFN_After      : ������ ������ WINPROC
        /       _pFN_Before     : ������ ���Ǵ� WINPROC ����
        ----------------------------------------------------------------*/
        virtual BOOL mFN_Change_WinProc(HWND _hWnd, WNDPROC _pFN_After, WNDPROC* _pFN_Before = NULL);
        // ������ �̸�����
        virtual void mFN_SetThreadName(DWORD _ThreadID, LPCSTR _ThreadName);
        // ������ ��ȯ
        __forceinline static BOOL sFN_SwithToThread()
        {
        #if (_WIN32 || _WIN64)
            return ::SwitchToThread();
        #else
            #error ���ǵ��� ����
        #endif
        }
#pragma endregion // ~ Windows / Thread

#pragma region ��Ÿ
        // ������Ʈ ������ �ð��� ��´�(���峯¥ �ð�)
        virtual TSystemTime mFN_GetCurrentProjectCompileTime(LPCSTR in__DATE__, LPCSTR in_TIME__);
        // on bit counting
        virtual UINT32 mFN_BitCount(const UINT8* pData, size_t nByte) const;
        virtual UINT32 mFN_BitCount(UINT8 val) const;
        virtual UINT32 mFN_BitCount(UINT16 val) const;
        virtual UINT32 mFN_BitCount(UINT32 val) const;
        virtual UINT32 mFN_BitCount(UINT64 val) const;
        // �޸� ��
        virtual BOOL mFN_MemoryCompare(const void* pSource, size_t SizeSource, unsigned char code) const;
        virtual BOOL mFN_MemoryCompareSSE2(const void* pSource, size_t SizeSource, unsigned char code) const;
        /*----------------------------------------------------------------
        /   ������ �޸� ���� ���ڿ����
        /       pOutputSTR ��� ���۰� ���� ���� �ʴ� ��� �ʿ��� ���� ũ�� ����
        /       ComareAB�� �޸� A B �� ��
        ----------------------------------------------------------------*/
#pragma region MemoryScan
        virtual size_t mFN_MemoryScan(LPSTR pOutputSTR, size_t nOutputLen
            , void* pObj, size_t sizeObj, int nColumn, int sizeUnit=4);
        virtual size_t mFN_MemoryScan(LPWSTR pOutputSTR, size_t nOutputLen
            , void* pObj, size_t sizeObj, int nColumn, int sizeUnit=4);
        virtual size_t mFN_MemoryScanCompareAB(LPSTR pOutputSTR, size_t nOutputLen
            , void* pObjA, size_t sizeObjA, void* pObjB, size_t sizeObjB
            , BOOL bShowDifference, int nColumn, int sizeUnit=4);
        virtual size_t mFN_MemoryScanCompareAB(LPWSTR pOutputSTR, size_t nOutputLen
            , void* pObjA, size_t sizeObjA, void* pObjB, size_t sizeObjB
            , BOOL bShowDifference, int nColumn, int sizeUnit=4);
        /*----------------------------------------------------------------
        /   ������ �޸� ���� ���ڿ���� : ����� ���â
        ----------------------------------------------------------------*/
        template<typename T>
        void mFN_MemoryScan__OutputDebug(T* pObj, int nColumn=4)
        {
            if(!pObj) return;
            const TCHAR szTitle[]   = _T("- Memory Scan : ");
            const size_t LenTitle   = _tcslen(szTitle);
            const size_t LenPointer = sizeof(void*)*2;
            const size_t LenSymbol  = _tcslen(_T("0x "));
            const size_t LenValue   = mFN_MemoryScan(pObj, sizeof(*pObj), nColumn, 4);
            if(!LenValue) return;
            const size_t Len = LenValue + LenTitle + LenPointer + LenSymbol;
            LPTSTR szBuff = (LPTSTR)_alloca(sizeof(TCHAR) * Len);
            LPTSTR pSTR   = szBuff + _stprintf_s(szBuff, Len, _T("%s0x%p\n"), szTitle, pObj);
            if(LenValue == mFN_MemoryScan(pSTR, LenValue, pObj, sizeof(*pObj), nColumn, 4))
                OutputDebugString(szBuff);
        }
        template<typename T1, typename T2>
        void mFN_MemoryScanCompareAB__OutputDebug(T1* pObj1, T2* pObj2, BOOL bShowDifference, int nColumn=4)
        {
            if(!pObj1 || !pObj2) return;
            const TCHAR szTitle[]   = _T("- Memory Scan Compare AB : ");
            const size_t LenTitle   = _tcslen(szTitle);
            const size_t LenPointer = 2 * sizeof(void*)*2;
            const size_t LenSymbol  = 2 * _tcslen(_T("0x "));
            const size_t LenValue   = mFN_MemoryScanCompareAB(pObj1, sizeof(*pObj1), pObj2, sizeof(*pObj2), bShowDifference, nColumn, 4);
            if(!LenValue) return;
            const size_t Len = LenValue + LenTitle + LenPointer + LenSymbol;
            LPTSTR szBuff = (LPTSTR)_alloca(sizeof(TCHAR) * Len);
            LPTSTR pSTR   = szBuff + _stprintf_s(szBuff, Len, _T("%s0x%p 0x%p\n"), szTitle, pObj1, pObj2);
            if(LenValue == mFN_MemoryScanCompareAB(pSTR, LenValue, pObj1, sizeof(*pObj1), pObj2, sizeof(*pObj2), nColumn, 4))
                OutputDebugString(szBuff);
        }
#if defined(_DEBUG) || defined(_DEF_NOWARNING__MACRO_OUTPUT_DEBUG_STRING__MEMORY)
#define _MACRO_OUTPUT_DEBUG_STRING__MEMORY__SUB1
#else
#define _MACRO_OUTPUT_DEBUG_STRING__MEMORY__SUB1   _DEF_MSG_WARNING("OutputDebug__Memory : RELEASE �������� ���Ǿ����ϴ�. ")
// ��� ���� �������� _DEF_NOWARNING__MACRO_OUTPUT_DEBUG_STRING__MEMORY ���� �Ͻʽÿ�
#endif
#define _MACRO_MEMORY_SCAN_OUTPUT_DEBUG_STRING              _MACRO_OUTPUT_DEBUG_STRING__MEMORY__SUB1 UTIL::g_pUtil->mFN_MemoryScan__OutputDebug
#define _MACRO_MEMORY_SCAN_COMPARE_AB_OUTPUT_DEBUG_STRING   _MACRO_OUTPUT_DEBUG_STRING__MEMORY__SUB1 UTIL::g_pUtil->mFN_MemoryScanCompareAB__OutputDebug
#pragma endregion
#pragma endregion

#pragma region DEBUG Function
        static void sFN_OutputDebugString(LPCSTR fmt, ...)
        {
            CHAR szBuffer[::UTIL::SETTING::gc_LenTempStrBuffer];
            szBuffer[0] = '\0';

            va_list vaArglist;
            va_start(vaArglist, fmt);
            ::UTIL::TUTIL::sFN_Sprintf(szBuffer, fmt, vaArglist);
            va_end(vaArglist);
            
            ::OutputDebugStringA(szBuffer);
        }
        static void sFN_OutputDebugString(LPCWSTR fmt, ...)
        {    
            WCHAR szBuffer[::UTIL::SETTING::gc_LenTempStrBuffer];
            szBuffer[0] = L'\0';

            va_list vaArglist;
            va_start(vaArglist, fmt);
            ::UTIL::TUTIL::sFN_Sprintf(szBuffer, fmt, vaArglist);
            va_end(vaArglist);
            
            ::OutputDebugStringW(szBuffer);
        }
        static void sFN_OutputDebugString_With_Trace(LPCSTR _szFileName, int _Line, LPCSTR fmt, ...)
        {
            CHAR szBuffer[::UTIL::SETTING::gc_LenTempStrBuffer];
            szBuffer[0] = '\0';

            va_list vaArglist;
            va_start(vaArglist, fmt);
            ::UTIL::TUTIL::sFN_Sprintf_With_Trace(_szFileName, _Line, szBuffer, fmt, vaArglist);
            va_end(vaArglist);

            ::OutputDebugStringA(szBuffer);
        }
        static void sFN_OutputDebugString_With_Trace(LPCSTR _szFileName, int _Line, LPCWSTR fmt, ...)
        {
            WCHAR szBuffer[::UTIL::SETTING::gc_LenTempStrBuffer];
            szBuffer[0] = L'\0';

            va_list vaArglist;
            va_start(vaArglist, fmt);
            ::UTIL::TUTIL::sFN_Sprintf_With_Trace(_szFileName, _Line, szBuffer, fmt, vaArglist);
            va_end(vaArglist);

            ::OutputDebugStringW(szBuffer);
        }
        #define _MACRO_OUTPUT_DEBUG_STRING_ALWAYS               ::UTIL::TUTIL::sFN_OutputDebugString
        #define _MACRO_OUTPUT_DEBUG_STRING_TRACE_ALWAYS(...)    ::UTIL::TUTIL::sFN_OutputDebugString_With_Trace(__FILE__, __LINE__, __VA_ARGS__)

#ifdef _DEBUG
        static int sFN_DebugBreakMessage(int _Count, LPCSTR _FileName, int _Line, LPCSTR fmt, ...)
        {
            CHAR szText[UTIL::SETTING::gc_LenTempStrBuffer];

            va_list vaArglist;
            va_start(vaArglist, fmt);
            int n = sFN_Sprintf_With_Trace(_FileName, _Line, szText, fmt, vaArglist);
            va_end(vaArglist);

            // ���� �߰�
            if(n+1 < _MACRO_ARRAY_COUNT(szText))
            {
                szText[n]   = '\n';
                szText[n+1] = '\0';
            }
            sFN_OutputDebugString(szText);

            // TRACE ����
            LPCSTR pText = strstr(szText, "): ");
            if(!pText)
                return IDIGNORE;

            pText += 3;

            CHAR szCaption[UTIL::SETTING::gc_LenTempStrBuffer];
            sprintf_s(szCaption, "DebugBreak (Count: %d)", _Count);

            return MessageBoxA(NULL, pText, szCaption, MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_TASKMODAL | MB_TOPMOST);
        }
        static int sFN_DebugBreakMessage(int _Count, LPCSTR _FileName, int _Line, LPCWSTR fmt, ...)
        {
            WCHAR szText[UTIL::SETTING::gc_LenTempStrBuffer];

            va_list vaArglist;
            va_start(vaArglist, fmt);
            int n = sFN_Sprintf_With_Trace(_FileName, _Line, szText, fmt, vaArglist);
            va_end(vaArglist);

            // ���� �߰�
            if(n+1 < _MACRO_ARRAY_COUNT(szText))
            {
                szText[n]   = L'\n';
                szText[n+1] = L'\0';
            }
            sFN_OutputDebugString(szText);

             // TRACE ����
            LPCWSTR pText = wcsstr(szText, L"): ");
            if(!pText)
                return IDIGNORE;

            pText += 3;

            WCHAR szCaption[UTIL::SETTING::gc_LenTempStrBuffer];
            szCaption[0] = L'\0';
            swprintf_s(szCaption, L"DebugBreak (Count: %d)", _Count);

            return MessageBoxW(NULL, pText, szCaption, MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_TASKMODAL | MB_TOPMOST);
        }
#endif
        DECLSPEC_NOINLINE static int sFN_Messagebox(UINT _Type, LPCSTR cap, LPCSTR fmt, ...)
        {
            CHAR szBuf[UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szBuf);
            szBuf[0] = '\0';

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            auto len = _vscprintf(fmt, vaArglist);
            if(c_MaxLen <= len)
            {
                _DebugBreak("not enough stack size");
                return IDCANCEL;
            }
            vsprintf_s(szBuf, fmt, vaArglist);
            va_end(vaArglist);

            return ::MessageBoxA(0, szBuf, cap, _Type | MB_TASKMODAL | MB_TOPMOST);
        }
        DECLSPEC_NOINLINE static int sFN_Messagebox(UINT _Type, LPCWSTR cap, LPCWSTR fmt, ...)
        {
            WCHAR szBuf[UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szBuf);
            szBuf[0] = L'\0';

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            auto len = _vscwprintf(fmt, vaArglist);
            if(c_MaxLen <= len)
            {
                _DebugBreak("not enough stack size");
                return IDCANCEL;
            }
            vswprintf_s(szBuf, fmt, vaArglist);
            va_end(vaArglist);

            return ::MessageBoxW(0, szBuf, cap, _Type | MB_TASKMODAL | MB_TOPMOST);
        }

        DECLSPEC_NOINLINE static int sFN_Messagebox_With_OutputDebugStr(UINT _Type, LPCSTR cap, LPCSTR fmt, ...)
        {
            CHAR szBuf[UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szBuf);
            szBuf[0] = '\0';

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            if(c_MaxLen <= _vscprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return IDCANCEL;
            }
            auto len = vsprintf_s(szBuf, fmt, vaArglist);
            va_end(vaArglist);
            if(0 < len && len+2 < c_MaxLen && szBuf[len-1] != '\n')
            {
                // �����߰�
                szBuf[len] = '\n';
                szBuf[len+1] = '\0';
            }

            ::OutputDebugStringA(szBuf);
            return ::MessageBoxA(0, szBuf, cap, _Type | MB_TASKMODAL | MB_TOPMOST);
        }
        DECLSPEC_NOINLINE static int sFN_Messagebox_With_OutputDebugStr(UINT _Type, LPCWSTR cap, LPCWSTR fmt, ...)
        {
            WCHAR szBuf[UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szBuf);
            szBuf[0] = L'\0';

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            if(c_MaxLen <= _vscwprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return IDCANCEL;
            }
            auto len = vswprintf_s(szBuf, fmt, vaArglist);
            va_end(vaArglist);
            if(0 < len && len+2 < c_MaxLen && szBuf[len-1] != L'\n')
            {
                // �����߰�
                szBuf[len] = L'\n';
                szBuf[len+1] = L'\0';
            }

            ::OutputDebugStringW(szBuf);
            return ::MessageBoxW(0, szBuf, cap, _Type | MB_TASKMODAL | MB_TOPMOST);
        }
#pragma endregion

    };
}


#pragma region DEBUG Macro
    #define _MACRO_STATIC_ASSERT(Expr) static_assert(Expr, #Expr)
    #define _MACRO_IS_SAME__VARIABLE_TYPE(Variable_1, Variable_2) (std::is_same<decltype(Variable_1), decltype(Variable_2)>::value == true)
    #define _MACRO_IS_SAME__VARIABLE_TYPE_TV(Type, Variable) (std::is_same<Type, decltype(Variable)>::value == true)
    #define _MACRO_IS_SAME__VARIABLE_TYPE_TT(Type_1, Type_2) (std::is_same<Type_1, Type_2>::value == true)

#ifdef _DEBUG
    #define _Assert(Expr)                                                               \
    {                                                                                   \
        if( !(Expr) )                                                                   \
            _DebugBreak(_T(#Expr));                                                     \
    }
    #define _AssertMsg(Expr, ...)                                                       \
    {                                                                                   \
        if( !(Expr) )                                                                   \
            _DebugBreak(__VA_ARGS__);                                                   \
    }

    #define _MACRO_OUTPUT_DEBUG_STRING          _MACRO_OUTPUT_DEBUG_STRING_ALWAYS
    #define _MACRO_OUTPUT_DEBUG_STRING_TRACE    _MACRO_OUTPUT_DEBUG_STRING_TRACE_ALWAYS
#else
    #define _Assert(Expr)           __noop
    #define _AssertMsg(Expr, ...)   __noop

    #define _MACRO_OUTPUT_DEBUG_STRING          __noop
    #define _MACRO_OUTPUT_DEBUG_STRING_TRACE    __noop
#endif


#pragma endregion






namespace UTIL{
    namespace MATH{
#pragma region �� ����(���������) ==  <   <=  >   >=
        /*----------------------------------------------------------------
        /   �� ����(���������)
        /   �� Less, Greater�� ���� �������� �����ؾ��ϴ°�.
        /      A �� B�� ����� ����� Less, Greater ��� ���� �ȴ�.
        /   gFN_Compare__
        ----------------------------------------------------------------*/
        // (A == B)?
        template<typename T>
        BOOL gFN_Compare__Equal(T A, T B, T AllowableError)
        {
            if((A + AllowableError) < B)
                return FALSE;
            else if(A > (B + AllowableError))
                return FALSE;
            return TRUE;
        }
        // (A < B)?
        template<typename T>
        BOOL gFN_Compare__Less__ThanB(T A, T B, T AllowableError)
        {
            if(A < (B + AllowableError))
                return TRUE;
            return FALSE;
        }
        // (A <= B)?
        template<typename T>
        BOOL gFN_Compare__LessEqual__ThanB(T A, T B, T AllowableError)
        {
            if(A <= (B + AllowableError))
                return TRUE;
            return FALSE;
        }
        // (A > B)?
        template<typename T>
        BOOL gFN_Compare__Greater__ThanB(T A, T B, T AllowableError)
        {
            if((A + AllowableError) > B)
                return TRUE;
            return FALSE;
        }
        // (A >= B)?
        template<typename T>
        BOOL gFN_Compare__GreaterEqual__ThanB(T A, T B, T AllowableError)
        {
            if((A + AllowableError) >= B)
                return TRUE;
            return FALSE;
        }
#pragma endregion

    }



    /*----------------------------------------------------------------
    /   ������ �޸� ���� ��� 0x00112233 ����
    ----------------------------------------------------------------*/
#pragma region �޸� ���� ���
    void gFN_OutputDebug__Memory(void* pObj, size_t sizeObj, int nColumn, int sizeUnit);
    void gFN_OutputDebug__Memory_CompareAB(void* pObjA, size_t sizeObjA, void* pObjB, size_t sizeObjB
        , BOOL bShowDifference, int nColumn, int sizeUnit);
    template<typename T>
    void gFN_OutputDebug__Memory_T(T* pObj, int nColumn=4)
    {
        gFN_OutputDebug__Memory(pObj, sizeof(*pObj), nColumn, 4);
    }
    template<typename T1, typename T2>
    void gFN_OutputDebug__Memory_T(T1* pObj1, T2* pObj2, BOOL bShowDifference, int nColumn=4)
    {
        gFN_OutputDebug__Memory_CompareAB(pObj1, sizeof(*pObj1), pObj2, sizeof(*pObj2), bShowDifference, nColumn, 4);
    }

//#define _DEF_NOWARNING__MACRO_OUTPUT_DEBUG_STRING__MEMORY
#if defined(_DEBUG) || defined(_DEF_NOWARNING__MACRO_OUTPUT_DEBUG_STRING__MEMORY)
#define _MACRO_OUTPUT_DEBUG_STRING__MEMORY__SUB1
#else
#define _MACRO_OUTPUT_DEBUG_STRING__MEMORY__SUB1   _DEF_MSG_WARNING("OutputDebug__Memory : RELEASE �������� ���Ǿ����ϴ�. ")
#endif
#define _MACRO_OUTPUT_DEBUG_STRING__MEMORY  _MACRO_OUTPUT_DEBUG_STRING__MEMORY__SUB1 gFN_OutputDebug__Memory_T
#pragma endregion
}


