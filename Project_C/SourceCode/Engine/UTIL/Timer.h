#pragma once
/*----------------------------------------------------------------
/	[Timer]
/----------------------------------------------------------------
/	고해상도 타이머또는 timeGetTime을를 사용합니다.
/----------------------------------------------------------------
/
/	사용법:
/		#1) 초기화 합니다.
/			mFN_Init( 고해상도 타이머 사용유무)
/			미리 생성된 윈도우를 참조 / 윈도우 생성
/
/       #2) 매 프레임마다 최우선적으로 이함수를 호출합니다.
/           mFN_Frame()
/----------------------------------------------------------------
/
/	작성자: lastpenguin83@gmail.com
/	작성일: 09-01-16(금)
/   수정일:
----------------------------------------------------------------*/

namespace UTIL{

    class CTimer : public CUnCopyAble{
    public:
        CTimer();
        ~CTimer();

    private:
        DWORD64 m_dw64TotalPlayTime_Stack;  // 누적 프로그램 실행시간   (표현범위 회전시 누적)
        DWORD   m_dwStartingTime;           // 프로그램 시작시간        (표현범위 회전시 변경된다)
        float   m_fTotalPlayTimeSec;        // 프로그램 실행시간

        float   m_fFPS;             // Frame Per Seoncd
        float   m_fSPF;             // Second Per Frame

        struct{
            struct{
                LARGE_INTEGER   m_Time_Prev;        // 고해상도 타이머: 이전값
                LARGE_INTEGER   m_Time_Now;         // 고해상도 타이머: 현재값
                LONGLONG        m_Delay_Now;        // 현재 딜레이
                LONGLONG        m_llCycle_Per_Second;
            }mt_Large;

            union{
                struct{
                    DWORD           m_Time_Prev;    // 저해상도 타이머: 이전값
                    DWORD           m_Time_Now;     // 저해상도 타이머: 현재값
                    DWORD           m_Delay_Now;    // 현재 딜레이
                    DWORD           m_dwCycle_Per_Second;
                }mt_Small;

                struct{
                    DWORD           m_Time_Prev;    // 시스템 타이머: 현재값
                    DWORD           m_Time_Now;     // 시스템 타이머: 이전값
                    DWORD           m_Delay_Now;    // 현재 딜레이
                    DWORD           m_dwCycle_Per_Second;
                }mt_System;
            };

            float   m_fCycle_Per_Second;    // 초당 주기
            float   m_fDelay_Now;           // float - 현재 딜레이
        }mt_Time;

        struct {
            float   m_fFPS_Average;     // 평균 FPS
            float   m_Array_fFPS[30];
            float   m_fSum_FPS;
            DWORD   m_Focus_Array_FPS;
        }mt_Statistic; // 통계

        /*----------------------------------------------------------------
        /       옵션
        ----------------------------------------------------------------*/
        BOOL    m_bUsing_Performance_Counter;   // 고해상도 타이머 사용플래그
        BOOL    m_bLimitFPS;                    // 프레임 제한 옵션
        float   m_fLimitFPS;                    // 프레임 제한

        union{
            LONGLONG    m_llDelay_LimitFPS;     // 프레임 제한 딜래이
            DWORD       m_dwDelay_LimitFPS;     // 프레임 제한 딜래이
        };

    public:
        /*----------------------------------------------------------------
        /       매인 프레임워크 전용 인터페이스
        ----------------------------------------------------------------*/
        void mFN_Init(BOOL _bUse_Performance_Counter);
        BOOL mFN_Frame();
                
        // Timer 리셋(로딩등 오래걸리는 작업 완료후 호출합니다)
        void mFN_ResetTimer();

        /*----------------------------------------------------------------
        /       공개 인터페이스
        ----------------------------------------------------------------*/        
        BOOL mFN_Set_LimitFPS(BOOL _bUseLimit, float _fFPS = 60.f);     // Set: 프레임 제한
        _DEF_MAKE_GET_METHOD(Frame_Per_Second, float, m_fFPS);          // Get: 초당 프레임
        _DEF_MAKE_GET_METHOD(Second_Per_Frame, float, m_fSPF);          // Get: 프레임당 시간
        _DEF_MAKE_GET_METHOD(Delay_System, DWORD, mt_Time.mt_System.m_Delay_Now);   // Get: System Delay
        _DEF_MAKE_GET_METHOD(Time_System, DWORD, mt_Time.mt_System.m_Time_Now);     // Get: System Time
        _DEF_MAKE_GET_METHOD(PlayTime, float, m_fTotalPlayTimeSec);                 // Get: Play Time(Sec)
        _DEF_MAKE_GET_METHOD(Avarage_FPS, float, mt_Statistic.m_fFPS_Average);      // Get: 평균 FPS
        _DEF_MAKE_GET_METHOD(UsingPerformance_Counter, BOOL, m_bUsing_Performance_Counter);
        BOOL  mFN_Use_Performance_Counter(BOOL _bUsing);// 고해상도 타이머 사용(On / Off)

        __declspec(property(get = mFN_Get_Frame_Per_Second)) float FPS;
        __declspec(property(get = mFN_Get_Second_Per_Frame)) float SPF;


    private:
        BOOL mFN_QueryPerformanceCounter ();
        BOOL mFN_Test_Performance_Counter();    // 고해상도 타미머 테스트
        __forceinline void mFN_Frame__System_Timer();
        BOOL mFN_Frame__Large_Timer();          // 프레임진행 - 고해상도 타이머
        BOOL mFN_Frame__Small_Timer();          // 프레임진행 - 저해상도 타이머

        void mFN_Cal_Statistic();
    };

};