#include "stdafx.h"
#include "./Lock.h"

namespace UTIL{
    namespace LOCK{
        /*----------------------------------------------------------------
        /       사용자 Lock 설정
        ----------------------------------------------------------------*/
        // Spin Limit = 4095
        const unsigned long CSpinLock::s_Limit_SpinCount = 0x00000FFF;
        const unsigned long CCompositeLock::s_Limit_SpinCount = 0x00000FFF;

        const DWORD CSpinLock::s_Invalid_Thread_ID = 0; // 스레드 생성시 실패하면 0을 리턴하는 것을 근거로
        /*----------------------------------------------------------------
        /       STANDARD
        ----------------------------------------------------------------*/
        namespace STANDARD{
            // CRITICAL_SECTION
            CLock_CriticalSection::CLock_CriticalSection()
            {
                ::InitializeCriticalSection(&m_CriticalSection);
            }
            CLock_CriticalSection::CLock_CriticalSection(DWORD _SpinCount)
            {
                DWORD n = _SpinCount;
                for(;;)
                {
                    if(::InitializeCriticalSectionAndSpinCount(&m_CriticalSection, _SpinCount))
                        return;
                    if(!n)
                        break;
                    n /= 2;
                }
                ::InitializeCriticalSection(&m_CriticalSection);
            }
            CLock_CriticalSection::~CLock_CriticalSection()
            {
                ::DeleteCriticalSection(&m_CriticalSection);
            }
            BOOL CLock_CriticalSection::Lock()
            {
                ::EnterCriticalSection(&m_CriticalSection);
                return TRUE;
            }
            void CLock_CriticalSection::UnLock()
            {
                ::LeaveCriticalSection(&m_CriticalSection);
            }

            // Mutex
            CLock_Mutex::CLock_Mutex()
                :m_hMutex(NULL)
            {
            }
            CLock_Mutex::~CLock_Mutex()
            {
                _SAFE_CLOSE_HANDLE(m_hMutex);
            }
            BOOL CLock_Mutex::CreateMutex(BOOL bInitialOwner, LPCSTR lpName)
            {
                if(m_hMutex) return FALSE;
                m_hMutex = ::CreateMutexA(NULL, bInitialOwner, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Mutex::CreateMutex(BOOL bInitialOwner, LPCWSTR lpName)
            {
                if(m_hMutex) return FALSE;
                m_hMutex = ::CreateMutexW(NULL, bInitialOwner, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Mutex::OpenMutex(LPCSTR lpName)
            {
                if(m_hMutex) return FALSE;
                m_hMutex = ::OpenMutexA(NULL, FALSE, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Mutex::OpenMutex(LPCWSTR lpName)
            {
                if(m_hMutex) return FALSE;
                m_hMutex = ::OpenMutexW(NULL, FALSE, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Mutex::Lock(DWORD dwMilliseconds)
            {
                switch(::WaitForSingleObject(m_hMutex, dwMilliseconds))
                {
                case WAIT_ABANDONED:
                case WAIT_OBJECT_0:
                    return TRUE;
                }
                return FALSE;
            }
            BOOL CLock_Mutex::Lock()
            {
                return Lock(INFINITE);
            }
            void CLock_Mutex::UnLock()
            {
                ::ReleaseMutex(m_hMutex);
            }
            BOOL CLock_Mutex::isCreatedHandle() const
            {
                return (m_hMutex)? TRUE : FALSE;
            }

            // Semaphore
            CLock_Semaphore::CLock_Semaphore()
                : m_hSemaphore(NULL)
            {
            }
            CLock_Semaphore::~CLock_Semaphore()
            {
                _SAFE_CLOSE_HANDLE(m_hSemaphore);
            }
            BOOL CLock_Semaphore::CreateSemaphore(LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName)
            {
                if(m_hSemaphore) return FALSE;
                m_hSemaphore = ::CreateSemaphoreA(NULL, lInitialCount, lMaximumCount, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Semaphore::CreateSemaphore(LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName)
            {
                if(m_hSemaphore) return FALSE;
                m_hSemaphore = ::CreateSemaphoreW(NULL, lInitialCount, lMaximumCount, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Semaphore::OpenSemaphore(LPCSTR lpName)
            {
                if(m_hSemaphore) return FALSE;
                m_hSemaphore = ::OpenSemaphoreA(NULL, FALSE, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Semaphore::OpenSemaphore(LPCWSTR lpName)
            {
                if(m_hSemaphore) return FALSE;
                m_hSemaphore = ::OpenSemaphoreW(NULL, FALSE, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Semaphore::Lock(DWORD dwMilliseconds)
            {
                switch(::WaitForSingleObject(m_hSemaphore, dwMilliseconds))
                {
                case WAIT_ABANDONED:
                case WAIT_OBJECT_0:
                    return TRUE;
                }
                return FALSE;
            }
            void CLock_Semaphore::UnLock(LONG lReleaseCount)
            {
                ::ReleaseSemaphore(m_hSemaphore, lReleaseCount, NULL);
            }
            BOOL CLock_Semaphore::Lock()
            {
                return Lock(INFINITE);
            }
            void CLock_Semaphore::UnLock()
            {
                UnLock(1);
            }
            BOOL CLock_Semaphore::isCreatedHandle() const
            {
                return (m_hSemaphore)? TRUE : FALSE;
            }

            // Event
            CLock_Event::CLock_Event()
                : m_hEvent(NULL)
            {
            }
            CLock_Event::~CLock_Event()
            {
                _SAFE_CLOSE_HANDLE(m_hEvent);
            }
            BOOL CLock_Event::CreateEvent(BOOL bManualReset, BOOL bInitialState, LPCSTR lpName)
            {
                if(m_hEvent) return FALSE;
                m_hEvent = ::CreateEventA(NULL, bManualReset, bInitialState, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Event::CreateEvent(BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName)
            {
                if(m_hEvent) return FALSE;
                m_hEvent = ::CreateEventW(NULL, bManualReset, bInitialState, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Event::OpenEvent(LPCSTR lpName)
            {
                if(m_hEvent) return FALSE;
                m_hEvent = ::OpenEventA(NULL, FALSE, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Event::OpenEvent(LPCWSTR lpName)
            {
                if(m_hEvent) return FALSE;
                m_hEvent = ::OpenEventW(NULL, FALSE, lpName);
                return isCreatedHandle();
            }
            BOOL CLock_Event::Lock()
            {
                return ResetEvent(m_hEvent);
            }
            BOOL CLock_Event::UnLock()
            {
                return SetEvent(m_hEvent);
            }
            BOOL CLock_Event::Wait(DWORD dwMilliseconds)
            {
                switch(::WaitForSingleObject(m_hEvent, dwMilliseconds))
                {
                case WAIT_ABANDONED:
                case WAIT_OBJECT_0:
                    return TRUE;
                }
                return FALSE;
            }
            BOOL CLock_Event::isCreatedHandle() const
            {
                return (m_hEvent)? TRUE : FALSE;
            }
        };

        /*----------------------------------------------------------------
        /       Spin Lock
        ----------------------------------------------------------------*/
        _MACRO_STATIC_ASSERT(sizeof(CSpinLock) == 8);

        CSpinLock::CSpinLock()
            : m_intoFlag(s_Invalid_Thread_ID)
        {
        }
        CSpinLock::~CSpinLock()
        {
        }
        void CSpinLock::__Debug_LockBefore(DWORD tid)
        {
        #ifdef _DEBUG
            const DWORD flag = m_intoFlag;
            _AssertMsg(m_intoFlag != tid, _T("is Locked\nfrom this thread(ID : %u)"), tid);
        #else
            UNREFERENCED_PARAMETER(tid);
        #endif
        }
        void CSpinLock::Lock()
        {
            const auto tid = ::GetCurrentThreadId();
            __Debug_LockBefore(tid);

            for(unsigned long cntSpin=0; cntSpin<s_Limit_SpinCount; ++cntSpin)
            {
                if(s_Invalid_Thread_ID == ::InterlockedCompareExchange(&m_intoFlag, tid, s_Invalid_Thread_ID))
                    return;
            }
            for(;;)
            {
                if(s_Invalid_Thread_ID == ::InterlockedCompareExchange(&m_intoFlag, tid, s_Invalid_Thread_ID))
                    return;

                ::UTIL::TUTIL::sFN_SwithToThread();
            }
        }
        void CSpinLock::Lock__Busy()
        {
            const auto tid = ::GetCurrentThreadId();
            __Debug_LockBefore(tid);

            while(s_Invalid_Thread_ID != ::InterlockedCompareExchange(&m_intoFlag, tid, s_Invalid_Thread_ID));
        }
        void CSpinLock::Lock__Lazy()
        {
            const auto tid = ::GetCurrentThreadId();
            __Debug_LockBefore(tid);

            while(s_Invalid_Thread_ID != ::InterlockedCompareExchange(&m_intoFlag, tid, s_Invalid_Thread_ID))
                ::UTIL::TUTIL::sFN_SwithToThread();
        }
        BOOL CSpinLock::Lock__NoInfinite(DWORD _LimitSpin)
        {
            const auto tid = ::GetCurrentThreadId();
            __Debug_LockBefore(tid);

            for(DWORD cntSpin = 0; cntSpin < _LimitSpin; cntSpin++)
            {
                if(s_Invalid_Thread_ID == ::InterlockedCompareExchange(&m_intoFlag, tid, s_Invalid_Thread_ID))
                    return TRUE;
            }
            return FALSE;
        }
        BOOL CSpinLock::Lock__NoInfinite__Lazy(DWORD _LimitSpin)
        {
            const auto tid = ::GetCurrentThreadId();
            __Debug_LockBefore(tid);

            for(DWORD cntSpin = 0; cntSpin < _LimitSpin; cntSpin++)
            {
                if(s_Invalid_Thread_ID == ::InterlockedCompareExchange(&m_intoFlag, tid, s_Invalid_Thread_ID))
                    return TRUE;
                ::UTIL::TUTIL::sFN_SwithToThread();
            }
            return FALSE;
        }
        void CSpinLock::UnLock()
        {
            _Assert(m_intoFlag != s_Invalid_Thread_ID);
            m_intoFlag = s_Invalid_Thread_ID;
            //std::_Atomic_thread_fence(std::memory_order_release);
        }
        void CSpinLock::UnLock_Safe()
        {
            const auto tid = ::GetCurrentThreadId();
            if(m_intoFlag != tid)
            {
            #ifdef _DEBUG
                _DebugBreak("Locked_from_ThreadID != This Thread ID");
            #endif
                return;
            }
            m_intoFlag = s_Invalid_Thread_ID;
        }

        BOOL CSpinLock::Query_isLocked() const
        {
            return (m_intoFlag != s_Invalid_Thread_ID);
        }
        BOOL CSpinLock::Query_isLocked_from_CurrentThread() const
        {
            const auto tid = ::GetCurrentThreadId();
            return (m_intoFlag == tid);
        }
        /*----------------------------------------------------------------
        /       Spin Lock(빠른버전 : 그러나 위험한)
        ----------------------------------------------------------------*/
        _MACRO_STATIC_ASSERT(sizeof(CSpinLock) == sizeof(CSpinLockQuick));
        _MACRO_STATIC_ASSERT(sizeof(CSpinLockQuick) == 8);
        CSpinLockQuick::CSpinLockQuick()
            : m_intoFlag(0)
        {
        }
        CSpinLockQuick::~CSpinLockQuick()
        {
        }
        void CSpinLockQuick::Lock()
        {
            if(0 == ::InterlockedExchange(&m_intoFlag, 1))
                return;

            for(unsigned long cntSpin=1; cntSpin < CSpinLock::s_Limit_SpinCount; ++cntSpin)
            {
                if(m_intoFlag)
                    continue;

                if(0 == ::InterlockedExchange(&m_intoFlag, 1))
                    return;
            }
            for(;;)
            {
                if(m_intoFlag)
                    continue;

                if(0 == ::InterlockedExchange(&m_intoFlag, 1))
                    return;

                ::UTIL::TUTIL::sFN_SwithToThread();
            }
        }
        void CSpinLockQuick::Lock__Busy()
        {
            if(0 == ::InterlockedExchange(&m_intoFlag, 1))
                return;

            for(;;)
            {
                if(m_intoFlag)
                    continue;
                if(0 == ::InterlockedExchange(&m_intoFlag, 1))
                    return;
            }
        }
        void CSpinLockQuick::Lock__Lazy()
        {
            for(;;)
            {
                if(m_intoFlag)
                    ;
                else if(0 == ::InterlockedExchange(&m_intoFlag, 1))
                    return;

                ::UTIL::TUTIL::sFN_SwithToThread();
            }
        }
        BOOL CSpinLockQuick::Lock__NoInfinite(DWORD _LimitSpin)
        {
            if(0 == ::InterlockedExchange(&m_intoFlag, 1))
                return TRUE;

            for(DWORD cntSpin = 1; cntSpin < _LimitSpin; cntSpin++)
            {
                if(m_intoFlag)
                    continue;

                if(0 == ::InterlockedExchange(&m_intoFlag, 1))
                    return TRUE;
            }
            return FALSE;
        }
        BOOL CSpinLockQuick::Lock__NoInfinite__Lazy(DWORD _LimitSpin)
        {
            for(DWORD cntSpin = 0; cntSpin < _LimitSpin; cntSpin++)
            {
                if(m_intoFlag)
                    ;
                else if(0 == ::InterlockedExchange(&m_intoFlag, 1))
                    return TRUE;

                ::UTIL::TUTIL::sFN_SwithToThread();
            }
            return FALSE;
        }
        void CSpinLockQuick::UnLock()
        {
            m_intoFlag = 0;
        }
        BOOL CSpinLockQuick::Query_isLocked() const
        {
            return (m_intoFlag == 1);
        }

        /*----------------------------------------------------------------
        /       Spin Lock
        /           읽기 무잠금 버전
        /           쓰기 잠금에 우선권
        /           키 사용량
        /               읽기: 1
        /               쓰기: Query_WriteLock_NeedKeys 에 질의
        ----------------------------------------------------------------*/
        _MACRO_STATIC_ASSERT(sizeof(CSpinLockRW) == 8);
        namespace{
            const LONG gc_CSpinLockRW__Max_PhysicalThread = 64;
            const LONG gc_CSpinLockRW__WriteLockCode = 0x00010000;
            
            _MACRO_STATIC_ASSERT(gc_CSpinLockRW__Max_PhysicalThread < gc_CSpinLockRW__WriteLockCode);
            // 최대 물리적 스레드수가 모두 쓰기 잠금과 읽기 잠금을 모두 사용한 경우
            _MACRO_STATIC_ASSERT(gc_CSpinLockRW__WriteLockCode < (gc_CSpinLockRW__WriteLockCode * gc_CSpinLockRW__Max_PhysicalThread + gc_CSpinLockRW__Max_PhysicalThread));
        }
        CSpinLockRW::CSpinLockRW()
        : m_MaxKeys(gc_CSpinLockRW__Max_PhysicalThread)
        , m_cntInto(0)
        {
        }
        CSpinLockRW::CSpinLockRW(LONG _max_read_key)
        : m_MaxKeys(_max_read_key)
        , m_cntInto(0)
        {
        #ifdef _DEBUG
            _Assert(_max_read_key <= gc_CSpinLockRW__Max_PhysicalThread);
        #endif

            if(m_MaxKeys > gc_CSpinLockRW__Max_PhysicalThread)
                m_MaxKeys = gc_CSpinLockRW__Max_PhysicalThread;
        }

        CSpinLockRW::~CSpinLockRW()
        {
        }

        void CSpinLockRW::Begin_Read()
        {
            for(;;)
            {
                // 읽기의 경우 사전 테스트를 하는 경우 성능저하가 있다
                //      다수의 스레드가 동시진입이 허용되기 때문에
                //      잠금으로 인한 대기가 짧기 때문

                const LONG lResult = ::InterlockedExchangeAdd(&m_cntInto, 1);
                if(lResult < m_MaxKeys)
                    return;

                // 다른 스레드가 끼어들었다면 양보한다
                ::InterlockedDecrement(&m_cntInto);
                if(lResult >= gc_CSpinLockRW__WriteLockCode)
                {
                    // 쓰기 락이 끼어들었다면 점유율 양보
                    while(Query_isLockedWrite())
                        ::UTIL::TUTIL::sFN_SwithToThread();
                }
            }
        }
        void CSpinLockRW::End_Read()
        {
            ::InterlockedDecrement(&m_cntInto);
        }
        BOOL CSpinLockRW::Begin_Write()
        {
            if(m_cntInto >= gc_CSpinLockRW__WriteLockCode) // 다른 스레드가 쓰기 잠금중
                return FALSE;

            const LONG lNow = ::InterlockedAdd(&m_cntInto, gc_CSpinLockRW__WriteLockCode);
            if(lNow >= gc_CSpinLockRW__WriteLockCode*2)
            {
                // 다른 스레드가 쓰기 잠금을 끼어들었다면, 양보한다
                ::InterlockedAdd(&m_cntInto, -gc_CSpinLockRW__WriteLockCode);
                return FALSE;
            }

            // 모든 읽기잠금이 끝날때까지 대기
            while(m_cntInto != gc_CSpinLockRW__WriteLockCode);
            return TRUE;
        }
        void CSpinLockRW::End_Write()
        {
            if(0 > ::InterlockedAdd(&m_cntInto, -gc_CSpinLockRW__WriteLockCode))
            {
                _Error(_T("Lock UnLock 짝을 맞춰 호출해야 합니다"));
            }
        }
        void CSpinLockRW::Begin_Write__INFINITE()
        {
            while(!Begin_Write())
                ::UTIL::TUTIL::sFN_SwithToThread();
        }
        void CSpinLockRW::Begin_Write__INFINITE__Busy()
        {
            while(!Begin_Write());
        }
        BOOL CSpinLockRW::Query_isLocked() const
        {
            return (m_cntInto > 0);
        }
        BOOL CSpinLockRW::Query_isLockedWrite() const
        {
            return (m_cntInto >= gc_CSpinLockRW__WriteLockCode)? TRUE :FALSE;
        }

        /*----------------------------------------------------------------
        /       Composite Lock
        ----------------------------------------------------------------*/
        CCompositeLock::CCompositeLock()
            : m_CntInto(0)
            , m_CreatedEvent(0)
            , m_hEvent(NULL)
#if defined(_DEBUG)
            , m_cnt_IntoCurrent__DEBUG(0)
            , m_cnt_IntoTotal__DEBUG(0)
            , m_cnt_TotalWaitForEvent__DEBUG(0)
#endif
        {
        }

        CCompositeLock::~CCompositeLock()
        {
            //Lock(); // 사용이 모두 끝날때까지 대기
            _SAFE_CLOSE_HANDLE(m_hEvent);
        }

        BOOL CCompositeLock::Lock()
        {
            // 스핀카운트 테스트(이벤트 대기중인 스레드가 없는 경우에만)
            if(m_CntInto <= 1)
            {
                for(unsigned long cntSpin=0; cntSpin<s_Limit_SpinCount; ++cntSpin)
                {
                    // 0아니라면 continue
                    if(m_CntInto)
                        continue;

                    if(0 == ::InterlockedCompareExchange(&m_CntInto, 1, 0))
                    {
                        // ▼ 이 영역은 동시에 1개 스레드만이 진입한다.
#if defined(_DEBUG)
                        mFN_Test_Begin();
#endif
                        return TRUE;
                    }
                }
            }

            // 테스트 회수를 넘었다면, 이벤트를 기다리기로 한다.

            // 동시 진입수++(※ Increment, Decrement는 결과값을 리턴함을 주의)
            if( 1 == ::InterlockedIncrement(&m_CntInto) )
            {
                // 이전에 진입한 스레드가 빠져나왔다. 이벤트를 사용할 필요가 없다.
#if defined(_DEBUG)
                mFN_Test_Begin();
#endif
                return TRUE;
            }

            mFN_WaitForEvent();
            return TRUE;
        }

    #pragma warning(push)
    #pragma warning(disable: 6387)
        void CCompositeLock::UnLock()
        {
            // 이함수는 동시에 1개 스레드만이 진입한다.
        #if defined(_DEBUG)
            mFN_Test_End();
        #endif

            LONG _ResultVal = InterlockedDecrement(&m_CntInto);
            if(0 < _ResultVal)
            {
                // ▼ 이벤트를 기다리는 스레드가 있다.

                // 이벤트가 생성될때까지 대기
                while(!m_hEvent)
                    ::UTIL::TUTIL::sFN_SwithToThread();

                ::SetEvent(m_hEvent);
            }
        #if defined(_DEBUG)
            else if(_ResultVal < 0)
            {
                __debugbreak();
            }
        #endif
        }

        void CCompositeLock::mFN_WaitForEvent()
        {
            if(!m_hEvent)
                mFN_Create_EventHandle();

            // 이벤트를 기다린다.
            WaitForSingleObject(m_hEvent, INFINITE);

            // ▼ 이 영역은 동시에 1개 스레드만이 진입한다.
        #if defined(_DEBUG)
            ++m_cnt_TotalWaitForEvent__DEBUG;
            mFN_Test_Begin();
        #endif
        }
    #pragma warning(pop)

        void CCompositeLock::mFN_Create_EventHandle()
        {
            // 이벤트가 만들어지지 않았다면...
            if(0 == ::InterlockedExchange(&m_CreatedEvent, 1))
            {
                //  생성한다.
                do{
                    m_hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
                }while(!m_hEvent);
            }
            else
            {
                // 다른스레드가 이벤트를 만드는 중이다. 대기한다.
                while(!m_hEvent)
                    ::UTIL::TUTIL::sFN_SwithToThread();
            }
        }

#if defined(_DEBUG)
        void CCompositeLock::mFN_Test_Begin()
        {
            LONG valNow = ::InterlockedIncrement(&m_cnt_IntoCurrent__DEBUG);
            if(valNow != 1)
                ::__debugbreak();   // 임계구역에 2개 이상의 스레드가 진입했습니다.
        }

        void CCompositeLock::mFN_Test_End()
        {
            LONG valNow = ::InterlockedDecrement(&m_cnt_IntoCurrent__DEBUG);
            if(valNow != 0)
                ::__debugbreak();   // 임계구역에 2개 이상의 스레드가 진입했습니다.

            ++m_cnt_IntoTotal__DEBUG;
        }

        double CCompositeLock::mFN_Test_CntWaitForEvent_Per_Lock() const
        {
            return ( (double)m_cnt_TotalWaitForEvent__DEBUG / (double)m_cnt_IntoTotal__DEBUG );
            //LONGLONG _IntegerPart   =
            //LONGLONG _FractionPart  =
        }
#endif


    };
};