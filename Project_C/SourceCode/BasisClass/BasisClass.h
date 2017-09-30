#pragma once


/*----------------------------------------------------------------
/       복사 생성자 방지 객체 ( 출처: Effective C++ )
----------------------------------------------------------------*/
class CUnCopyAble{
public:
    CUnCopyAble()   {}
    ~CUnCopyAble()  {}

private:
    CUnCopyAble(const CUnCopyAble&);
    CUnCopyAble& operator = (const CUnCopyAble&);
};

/*----------------------------------------------------------------
/       스마트 포인터
/       [ CSmartPointer ]
/       CSmartPointerRef 객체와 함께 사용합니다.
----------------------------------------------------------------*/
template<typename T>
class CSmartPointerRef;

class CSmartPointerBasis : private CUnCopyAble{
    template<typename>
    friend class CSmartPointerRef;
protected:
    virtual void Add_Ref() = 0;
    virtual void Release_Ref() = 0;
};

template<typename T>
class CSmartPointer : protected CSmartPointerBasis{
    template<typename>
    friend class CSmartPointerRef;
public:
    CSmartPointer();
    explicit CSmartPointer(T* pPointer);
    virtual ~CSmartPointer();

private:
    BOOL        m_bImLive;
    BOOL        m_bImDelete;

    T*          m_pPointer;
    size_t      m_RefCount;

public:
    void operator = (T* pPointer);
    BOOL isEmpty() const;
    BOOL TestPointer(T* pPointer) const;

private:
    virtual void Add_Ref() override;
    virtual void Release_Ref() override;
    __forceinline void Delete();
};

/*----------------------------------------------------------------
/       스마트 포인터 참조 객체
/       [ CSmartPointerRef ]
/       CSmartPointer객체와 함께 사용합니다.
----------------------------------------------------------------*/
template<typename T>
class CSmartPointerRefBasis{
    template<typename>
    friend class CSmartPointerRef;
private:
    CSmartPointerRefBasis()
        : m_pSmartPointer(NULL)
        , m_pPointer(NULL)
    {
    }
    CSmartPointerRefBasis(CSmartPointerBasis* _pSmart, T* pData)
        : m_pSmartPointer(_pSmart)
        , m_pPointer(pData)
    {
    }

private:
    CSmartPointerBasis*     m_pSmartPointer;
    T*                      m_pPointer;
};


template<typename T>
class CSmartPointerRef : public CSmartPointerRefBasis<T>{
public:
    CSmartPointerRef();
    template<typename TUnknown>     CSmartPointerRef(CSmartPointer<TUnknown>* _pData);
    template<typename TUnknown>     CSmartPointerRef(const CSmartPointerRefBasis<TUnknown>& _DataRef);
    ~CSmartPointerRef();

public:
    template<typename TUnknown>
    CSmartPointerRef<T>& operator = (CSmartPointer<TUnknown>* _pData);

    template<typename TUnknown>
    CSmartPointerRef<T>& operator = (const CSmartPointerRefBasis<TUnknown>& _DataRef);

    void Release();                             // 참조 해제
    __forceinline BOOL isEmpty() const;
    __forceinline T* operator * ();             // 포인터 참조
    __forceinline const T* operator * () const; // 포인터 참조
};

/*----------------------------------------------------------------
/       싱글톤
/           CSingleton      다중 스레드 안전한 싱글톤
/           캐시라인단위크기의 배수크기를 가진 객체는 자동으로 캐시라인에 정렬합니다
/----------------------------------------------------------------
/
/   예1 :
/       class CTest1{
/           _DEF_FRIEND_SINGLETO        // 필요에 따라 포함하십시오(#1 참조)
/           ...
/       };
/
/       CSingleton<CTest1>::Get_Instance();
/
/   #1 만약 객체의 생성자와 소멸자가 비공개형이라면 다음 매크로를 객체 선언 내부에 포함하십시오.
/       _DEF_FRIEND_SINGLETON
/
/   #2 싱글톤을 적용할 객체들간에 생성 우선순위를 조절하고 싶다면,
/      예를들어 ObjA가 ObjB보다 먼저 생성되어야 한다면
/      ObjB의 생성자에서 CSingleton<ObjA>::Create_Instance() 를 호출합니다.
/
/   #3 만약, 전체에서 오직1개의 객체가 아닌,
/      어떤 식별자를 기준으로 식별자단위로 싱글톤 객체를 사용하고 싶다면,
/      TInstanceFromGroup를 사용하십시오.
/
----------------------------------------------------------------*/
#define     _DEF_FRIEND_SINGLETON       template<typename, typename> friend class CSingleton; template<typename, typename> friend class CSingleton_ManagedDeleteTiming;

template<typename T, typename TInstanceFromGroup = T>
class CSingleton : protected CUnCopyAble{
public:
    static T* Get_Instance();
    static void Create_Instance();

private:
    static T* __Internal__Create_Instance__Default();
    static T* __Internal__Create_Instance__AlignedCacheline();
    static T* s_pInstance;
    static volatile LONG s_isCreated;
};

// CSingleton_ManagedDeleteTiming 는 기본형인 CSingleton 과 달리 삭제타이밍을 조절합니다
//      이것은 삭제 순서를 조절하는데 필요합니다
//      아래의 예와 같은 경우에 필요합니다
//          CA::CSingleton
//          CB::CSingleton
//
//          CA::CreateInstance
//          CA()
//              CB::CreateInstance  // CA 에서 사용으로 인하여
//              CB()
//              ~CB()
//          ~CA()   // CA 생성자에서 만약 CB를 사용하려한다면 오류
//
//          다음과 같이 해결
//          CA() 초기에, CB::Reference_Attach()
//          ~CA() 마지막에 CB::Reference_Detach()
// 이것은 동적할당 됩니다
// T 의 크기가 캐시라인크기의 배수라면 캐시라인에 정렬되도록 할당됩니다
template<typename T, typename TInstanceFromGroup = T>
class CSingleton_ManagedDeleteTiming : protected CUnCopyAble{
public:
    static T* Get_Instance();
    static void Create_Instance();
    static void Reference_Attach();
    static void Reference_Detach();

private:
    static void Destroy();

private:
    static T* s_pInstance;
    static volatile LONG s_isCreated;
    static volatile LONG s_isClosed;
    static volatile LONG s_cntAttach;  // 최대 카운팅 주의

    template<typename TSingleton>
    struct TSensor__ProgramEnd{
        ~TSensor__ProgramEnd()
        {
            auto valPrevious = ::InterlockedExchange(&TSingleton::s_isClosed, 1);
            if(0 == valPrevious)
            {
                if(0 == TSingleton::s_cntAttach)
                    TSingleton::Destroy();
            }
            else
            {
                *((UINT32*)NULL) = 0x003BBA4A; // 의도적 오류
            }
        }
    };
};
/*----------------------------------------------------------------
/       비 재귀 객체( Tree형태의 자료의 재귀호출을 대체 한다 )
----------------------------------------------------------------*/
template<typename TNode, typename TParam>
class CNonRecursive : private CUnCopyAble{
    struct _tag_NodeUnit{
        TNode*  m_pNode;
        TParam  m_Param;
    };

public:
    CNonRecursive();
    CNonRecursive(size_t _FirstSize);
    ~CNonRecursive();

private:
    const size_t    m_FirstSize;    // 최초 확장사이즈

    _tag_NodeUnit*  m_pStack;       // 버퍼
    size_t          m_Size_Buffer;  // 버퍼크기
    size_t          m_Size_Using;   // 사용중인 크기

public:
    __inline void Reserve(size_t _Count);
    __inline void Push(TNode* _Node, TParam _Param);
    __inline TNode* Pop(TParam* _Param);
};

#include "./BasisClass.hpp"