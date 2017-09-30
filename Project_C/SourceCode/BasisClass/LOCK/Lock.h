#pragma once
#include "../BasisClass.h"

/*----------------------------------------------------------------
/   STANDARD 랩핑 버전
/   STANDARD::CLock_CriticalSection
/   STANDARD::CLock_Mutex
/   STANDARD::CLock_Semaphore
/   STANDARD::CLock_Event
/----------------------------------------------------------------
/   class           size(x86)   size(x64)
/   CSpinLock       8           8
/   CSpinLockQuick  8           8
/   CSpinLockRW     8           8
/   CCompositeLock	12          16
/
/   CSpinLockQ      Debug(CSpinLock)    Release(CSpinLockQuick)
/
/   공통: 소멸자에서 잠금 취득(STANDARD 버전은 그대로 종료)
----------------------------------------------------------------*/

namespace UTIL{
    namespace LOCK{

        /*----------------------------------------------------------------
        /       STANDARD
        /   afxmt에 이미 4종이 존재 
        ----------------------------------------------------------------*/
        namespace STANDARD{
            // CRITICAL_SECTION
            class CLock_CriticalSection final{
            public:
                CLock_CriticalSection();
                CLock_CriticalSection(DWORD _SpinCount);
                ~CLock_CriticalSection();

                BOOL Lock();
                void UnLock();

            private:
                CRITICAL_SECTION m_CriticalSection;
            };

            // Mutex
            class CLock_Mutex final{
            public:
                CLock_Mutex();
                ~CLock_Mutex();

                // 이미 예약된 이름이라면 OPEN
                BOOL CreateMutex(BOOL bInitialOwner, LPCSTR lpName);
                BOOL CreateMutex(BOOL bInitialOwner, LPCWSTR lpName = NULL);
                // 생성된 이름만 OPEN / CREATE 하지 않는다
                BOOL OpenMutex(LPCSTR lpName);
                BOOL OpenMutex(LPCWSTR lpName);

                BOOL Lock(DWORD dwMilliseconds);
                BOOL Lock();
                void UnLock();
                BOOL isCreatedHandle() const;

            private:
                HANDLE m_hMutex;
            };

            // Semaphore
            class CLock_Semaphore final{
            public:
                CLock_Semaphore();
                ~CLock_Semaphore();

                // 이미 예약된 이름이라면 OPEN
                BOOL CreateSemaphore(LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);
                BOOL CreateSemaphore(LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName = NULL);
                // 생성된 이름만 OPEN / CREATE 하지 않는다
                BOOL OpenSemaphore(LPCSTR lpName);
                BOOL OpenSemaphore(LPCWSTR lpName);

                BOOL Lock(DWORD dwMilliseconds);
                void UnLock(LONG lReleaseCount);
                BOOL Lock();
                void UnLock();
                BOOL isCreatedHandle() const;

            private:
                HANDLE m_hSemaphore;
            };

            // Event
            class CLock_Event final{
            public:
                CLock_Event();
                ~CLock_Event();

                // 이미 예약된 이름이라면 OPEN
                BOOL CreateEvent(BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
                BOOL CreateEvent(BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName = NULL);
                // 생성된 이름만 OPEN / CREATE 하지 않는다
                BOOL OpenEvent(LPCSTR lpName);
                BOOL OpenEvent(LPCWSTR lpName);

                BOOL Lock();
                BOOL UnLock();
                BOOL Wait(DWORD dwMilliseconds);
                BOOL isCreatedHandle() const;

            private:
                HANDLE  m_hEvent;
            };

        };

        /*----------------------------------------------------------------
        /       Spin Lock
        ----------------------------------------------------------------*/
        #pragma pack(push, 4)
        class CSpinLock final{
            friend class CSpinLockQuick;
            static const unsigned long s_Limit_SpinCount;
            static const DWORD s_Invalid_Thread_ID;
        public:
            CSpinLock();
            ~CSpinLock();

            void Lock();                                // Default
            void Lock__Busy();                          // 바쁜 대기(스레드간 동시 진입 가능성이 매우 낮은 곳에만 사용하십시오)
            void Lock__Lazy();                          // 우선순위가 낮은 잠금
            BOOL Lock__NoInfinite(DWORD _LimitSpin);       // 제한된 대기(스핀 카운트 제한)
            BOOL Lock__NoInfinite__Lazy(DWORD _LimitSpin); // 제한된 대기(스핀 카운트 제한) + 우선순위가 낮은 잠금
            void UnLock();
            void UnLock_Safe(); // 잠근을 소유한 스레드만 유효

            BOOL Query_isLocked() const;
            BOOL Query_isLocked_from_CurrentThread() const;
        private:
            __forceinline void __Debug_LockBefore(DWORD tid);

        private:
            volatile DWORD m_intoFlag;
            DWORD : 32;
        };
        #pragma pack(pop)
        /*----------------------------------------------------------------
        /       Spin Lock(빠른버전 : 그러나 위험한)
        ----------------------------------------------------------------*/
        #pragma pack(push, 4)
        class CSpinLockQuick final{
        public:
            CSpinLockQuick();
            ~CSpinLockQuick();

            void Lock();                                // Default
            void Lock__Busy();                          // 바쁜 대기(스레드간 동시 진입 가능성이 매우 낮은 곳에만 사용하십시오)
            void Lock__Lazy();                          // 우선순위가 낮은 잠금(그리고 낮은 CPU 점유)
            BOOL Lock__NoInfinite(DWORD _LimitSpin);       // 제한된 대기(스핀 카운트 제한)
            BOOL Lock__NoInfinite__Lazy(DWORD _LimitSpin); // 제한된 대기(스핀 카운트 제한) + 우선순위가 낮은 잠금
            void UnLock();

            BOOL Query_isLocked() const;

        private:
            volatile LONG m_intoFlag;
            DWORD : 32;
        };
        #pragma pack(pop)

    #ifdef _DEBUG
        typedef CSpinLock CSpinLockQ;
    #else
        typedef CSpinLockQuick CSpinLockQ;
    #endif

        /*----------------------------------------------------------------
        /       Spin Lock Read / Write
        /           읽기 무잠금 버전
        /           쓰기 잠금에 우선권
        ----------------------------------------------------------------*/
        #pragma pack(push, 4)
        class CSpinLockRW final{
        public:
            CSpinLockRW();
            CSpinLockRW(LONG _max_read_key);
            ~CSpinLockRW();

            void Begin_Read();
            void End_Read();

            // TRUE  : 잠금 성공
            // FALSE : 실패(다른 스레드가 먼저 쓰기 잠금중)
            //         이에 따른 처리는 두가지 방법이 있다
            //         읽기로 다시 돌아가 원하는 결과가 이미 설정되었는지 다시 확인하거나,
            //         또는 성공할때까지 반복 while(!Begin_Write());
            BOOL Begin_Write();
            void End_Write();

            // 쓰기 잠금 무제한 기다림
            void Begin_Write__INFINITE();
            void Begin_Write__INFINITE__Busy();

            BOOL Query_isLocked() const;
            BOOL Query_isLockedWrite() const;

        private:
            LONG m_MaxKeys;
            volatile LONG m_cntInto;
        };
        #pragma pack(pop)

        /*----------------------------------------------------------------
        /       Composite Lock
        ----------------------------------------------------------------*/
        class CCompositeLock final{
        public:
            CCompositeLock();
            ~CCompositeLock();

        private:
            volatile LONG   m_CntInto;
            volatile LONG   m_CreatedEvent;
            HANDLE          m_hEvent;


            static const unsigned long s_Limit_SpinCount;

#if defined(_DEBUG)
            volatile LONG   m_cnt_IntoCurrent__DEBUG;       // 동시 진입수
            LONGLONG        m_cnt_IntoTotal__DEBUG;         // 누적 진입수
            LONGLONG        m_cnt_TotalWaitForEvent__DEBUG; // 이벤트 대기수
#endif

        public:
            BOOL Lock();
            void UnLock();

        private:
            void mFN_WaitForEvent();
            void mFN_Create_EventHandle();
#if defined(_DEBUG)
            void mFN_Test_Begin();  // 디버깅 함수
            void mFN_Test_End();    // 디버깅 함수

        public:
            // 락1회당 평균 이벤트 대기횟수
            double mFN_Test_CntWaitForEvent_Per_Lock() const;
#endif
        };



        /*----------------------------------------------------------------
        /       자동락
        ----------------------------------------------------------------*/
        template<typename TLockPTR>
        class CAutoLock : private CUnCopyAble{
        public:
            // 수동옵션이 아닌경우 생성자에서 잠금
            explicit CAutoLock(TLockPTR pLock, BOOL bManualLocked = FALSE)
                : m_pLockObj(pLock)
                , m_bIsLocked(bManualLocked)
            {
                if(!bManualLocked)
                    Lock();
            }
            ~CAutoLock()
            {
                UnLock();
            }

        public:
            void Lock()
            {
                if(m_bIsLocked)
                    return;
                m_bIsLocked = TRUE;
                m_pLockObj->Lock();
            }
            void UnLock()
            {
                if(!m_bIsLocked)
                    return;
                m_pLockObj->UnLock();
                m_bIsLocked = FALSE;
            }

        private:
            TLockPTR m_pLockObj;
            BOOL     m_bIsLocked;
        };
        template<typename TLockPTR>
        class CAutoLockR: private CUnCopyAble{
        public:
            // 수동옵션이 아닌경우 생성자에서 잠금
            CAutoLockR(TLockPTR pLock, BOOL bManualLocked = FALSE)
                : m_pLockObj(pLock)
                , m_bIsLocked(bManualLocked)
            {
                if(!bManualLocked)
                    Begin_Read();
            }
            ~CAutoLockR()
            {
                End_Read();
            }

        public:
            void Begin_Read()
            {
                if(m_bIsLocked)
                    return;
                m_bIsLocked = TRUE;
                m_pLockObj->Begin_Read();
            }
            void End_Read()
            {
                if(!m_bIsLocked)
                    return;
                m_pLockObj->End_Read();
                m_bIsLocked = FALSE;
            }

        private:
            TLockPTR m_pLockObj;
            BOOL     m_bIsLocked;
        };
        template<typename TLockPTR>
        class CAutoLockW: private CUnCopyAble{
        public:
            // 수동옵션이 아닌경우 생성자에서 잠금
            CAutoLockW(TLockPTR pLock, BOOL bManualLocked = FALSE)
                : m_pLockObj(pLock)
                , m_bIsLocked(bManualLocked)
            {
                if(!bManualLocked)
                    Begin_Write__INFINITE();
            }
            ~CAutoLockW()
            {
                End_Write();
            }

        public:
            void Begin_Write__INFINITE()
            {
                if(m_bIsLocked)
                    return;
                m_pLockObj->Begin_Write__INFINITE();
                m_bIsLocked = TRUE;
            }
            BOOL Begin_Write()
            {
                if(m_bIsLocked)
                    return TRUE;
                m_bIsLocked = m_pLockObj->Begin_Write();
                return m_bIsLocked;
            }
            void End_Write()
            {
                if(!m_bIsLocked)
                    return;
                m_pLockObj->End_Write();
                m_bIsLocked = FALSE;
            }

        private:
            TLockPTR m_pLockObj;
            BOOL     m_bIsLocked;
        };

    }// ~ namespace LOCK
}// ~ namespace UTIL
#ifndef _DEF_VARIABLE_UNIQUE_NAME
#error undefined macro _DEF_VARIABLE_UNIQUE_NAME
#endif
#define _SAFE_LOCK_SPINLOCK(pLockObj, bManualLocked) ::UTIL::LOCK::CAutoLock<decltype((pLockObj))> _DEF_VARIABLE_UNIQUE_NAME(__Local_AutoLock)((pLockObj), bManualLocked)
#define _SAFE_LOCK_SPINLOCKRW_R(pLockObj, bManualLocked) ::UTIL::LOCK::CAutoLockR<decltype((pLockObj))> _DEF_VARIABLE_UNIQUE_NAME(__Local_AutoLock)((pLockObj), bManualLocked)
#define _SAFE_LOCK_SPINLOCKRW_W(pLockObj, bManualLocked) ::UTIL::LOCK::CAutoLockW<decltype((pLockObj))> _DEF_VARIABLE_UNIQUE_NAME(__Local_AutoLock)((pLockObj), bManualLocked)