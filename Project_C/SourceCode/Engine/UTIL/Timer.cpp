#include "stdafx.h"
#include "./Timer.h"

#include <mmsystem.h>

namespace UTIL
{
    namespace{
        template<typename T>
        __forceinline T gFN_Cal_ElapsedTime(T prev, T now, T max)
        {
            if(prev <= now)
                return now - prev;

            return (max - prev) + now + 1; // 시간 0을 포함하기 위해 1 추가
        }
    }
    CTimer::CTimer()
        : m_dw64TotalPlayTime_Stack(0)
        , m_dwStartingTime(0)
        , m_fTotalPlayTimeSec(0.f)
        , m_fFPS(0.f)
        , m_fSPF(0.f)
        , m_bUsing_Performance_Counter(FALSE)
        , m_bLimitFPS(FALSE)
        , m_fLimitFPS(0.f)
        , m_llDelay_LimitFPS(0)                 // is m_Delay_LimitFPS
    {
        mFN_Init(TRUE);
        mFN_ResetTimer();
    }

    CTimer::~CTimer()
    {
    }

    /*----------------------------------------------------------------
    /       매인 프레임워크 전용 인터페이스
    ----------------------------------------------------------------*/
    void CTimer::mFN_Init(BOOL _bUse_Performance_Counter)
    {
        ::timeBeginPeriod(1);
        mt_Time.mt_System.m_Time_Now  = ::timeGetTime();
        ::timeEndPeriod(1);
        mt_Time.mt_System.m_Time_Prev = mt_Time.mt_System.m_Time_Now;
        mt_Time.mt_System.m_Delay_Now = 0;
        mt_Time.mt_System.m_dwCycle_Per_Second = 1000;

        // 시작시간 기록
        m_dwStartingTime = mt_Time.mt_System.m_Time_Now;

        if(_bUse_Performance_Counter && mFN_Test_Performance_Counter())
        {
            /* 고해상도 타이머 사용 */
            m_bUsing_Performance_Counter = TRUE;

            mFN_QueryPerformanceCounter();
            mt_Time.mt_Large.m_Time_Prev = mt_Time.mt_Large.m_Time_Now;
            mt_Time.mt_Large.m_Delay_Now = 0;

            // Cycle_Per_Second 는 mFN_Test_Performance_Counter() 메소드에서 초기화
        }
        else
        {
            /* timeGetTime 사용 */
            m_bUsing_Performance_Counter = FALSE;

            // mt_System mt_Small 공용체
            mt_Time.m_fCycle_Per_Second = static_cast<float>(mt_Time.mt_System.m_dwCycle_Per_Second);
        }

        // 기타 정보
        m_fFPS = 0.f;
        m_fSPF = 0.f;

        // 통계
        mt_Statistic.m_fFPS_Average     = 0.f;
        mt_Statistic.m_fSum_FPS         = 0.f;
        mt_Statistic.m_Focus_Array_FPS  = MAXDWORD; // magic code
    }

    BOOL CTimer::mFN_Frame()
    {
        BOOL _bSuccess;
        mFN_Frame__System_Timer();
        if(m_bUsing_Performance_Counter)
            _bSuccess = mFN_Frame__Large_Timer();
        else
            _bSuccess = mFN_Frame__Small_Timer();

        if(_bSuccess)
        {
            // 업데이트
            m_fFPS = mt_Time.m_fCycle_Per_Second / mt_Time.m_fDelay_Now;
            m_fSPF = mt_Time.m_fDelay_Now / mt_Time.m_fCycle_Per_Second;

            // 통계 계산
            mFN_Cal_Statistic();
        }
        else
        {
            // 타이머 프레임 제한이 걸려있는 경우에 한하여,
            // 제한시간이 지나지 않은 경우 기본적으로 아무일도 하지 않아야 하지만,
            // 이때 SPF, Delay_System등이 무효 하므로 잠시 0으로 셋한다.
            //  단, FPS는 통계를 위해 사용하기때문에 그대로 둔다.
            mt_Time.mt_System.m_Delay_Now = 0;
            m_fSPF                        = 0.f;
        }

        return _bSuccess;        
    }

    // Timer 리셋(로딩등 오래걸리는 작업 완료후 호출합니다)
    void CTimer::mFN_ResetTimer()
    {
        if(m_bUsing_Performance_Counter)
        {
            mFN_QueryPerformanceCounter();
            mt_Time.mt_Large.m_Time_Prev = mt_Time.mt_Large.m_Time_Now;
            mt_Time.mt_Large.m_Delay_Now = 0;
        }
                
        ::timeBeginPeriod(1);
        mt_Time.mt_System.m_Time_Now  = ::timeGetTime();
        ::timeEndPeriod(1);
        mt_Time.mt_System.m_Time_Prev = mt_Time.mt_Small.m_Time_Now;
        mt_Time.mt_System.m_Delay_Now = 0;

        mt_Time.m_fDelay_Now = 0.f;

        //m_fFPS = 0.f;     // FPS는 통계를 위해 그대로 둔다.
        m_fSPF = 0.f;
    }

    /*----------------------------------------------------------------
    /       공개 인터페이스
    ----------------------------------------------------------------*/

    // Set: 프레임 제한
    BOOL CTimer::mFN_Set_LimitFPS(BOOL _bUseLimit, float _fFPS)
    {
        m_bLimitFPS = _bUseLimit;
        if(FALSE == _bUseLimit)
            return TRUE;

        //예외처리
        if(0.f >= _fFPS || _fFPS > mt_Time.m_fCycle_Per_Second)
        {
            _DebugBreak( _T("FPS 제한 수치가 잘못되었습니다.\n") );
            m_bLimitFPS = FALSE;
            return FALSE;
        }

        m_fLimitFPS = _fFPS;

        // 0.5 를 더해 반올림 하지 않으면 저해상도 타이머(초당 1000틱)의 경우
        // 60 프레임제한의 경우 62 프레임이 나오게 된다
        // 사용자의 의도는 어떤 수치를 넘지 않도록 제한하기 때문에 이는 올바른 결과가 아니다
        if(m_bUsing_Performance_Counter)
            m_llDelay_LimitFPS  = static_cast<LONGLONG>( mt_Time.m_fCycle_Per_Second / _fFPS + 0.5f);
        else
            m_dwDelay_LimitFPS  = static_cast<DWORD>( mt_Time.m_fCycle_Per_Second / _fFPS + 0.5f);

        return TRUE;
    }

    BOOL CTimer::mFN_QueryPerformanceCounter()
    {
        DWORD_PTR oldmask = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
        BOOL bSuccessed = ::QueryPerformanceCounter(&mt_Time.mt_Large.m_Time_Now);
        ::SetThreadAffinityMask(::GetCurrentThread(), oldmask);
        return bSuccessed;
    }

    // 고해상도 타이머 사용(On / Off)
    BOOL CTimer::mFN_Use_Performance_Counter(BOOL _bUsing)
    {
        if(m_bUsing_Performance_Counter == _bUsing)
            return TRUE;

        if(_bUsing)
        {
            if(!mFN_Test_Performance_Counter())
                return FALSE;

            if(m_bLimitFPS)
                m_llDelay_LimitFPS = static_cast<LONGLONG>(mt_Time.m_fCycle_Per_Second / m_fLimitFPS);
        }
        else
        {
            mt_Time.mt_Small.m_dwCycle_Per_Second   = 1000;
            mt_Time.m_fCycle_Per_Second             = 1000.f;

            if(m_bLimitFPS)
                m_dwDelay_LimitFPS = static_cast<DWORD>(mt_Time.m_fCycle_Per_Second / m_fLimitFPS);
        }
        m_bUsing_Performance_Counter = _bUsing;
        
        mFN_ResetTimer();
        return TRUE;
    }

    // 고해상도 타미머 테스트
    BOOL CTimer::mFN_Test_Performance_Counter()
    {
        LARGE_INTEGER _Time_Frequency;
        if( !QueryPerformanceFrequency(&_Time_Frequency) )
            return FALSE;

        if(_Time_Frequency.QuadPart == 0)
            return FALSE;

        if(!mFN_QueryPerformanceCounter())
            return FALSE;
        
        mt_Time.mt_Large.m_llCycle_Per_Second   = _Time_Frequency.QuadPart;
        mt_Time.m_fCycle_Per_Second             = static_cast<float>(_Time_Frequency.QuadPart);
        return TRUE;
    }

    __forceinline void CTimer::mFN_Frame__System_Timer()
    {
        ::timeBeginPeriod(1);
        mt_Time.mt_System.m_Time_Now  = ::timeGetTime();
        ::timeEndPeriod(1);
        mt_Time.mt_System.m_Delay_Now = gFN_Cal_ElapsedTime<DWORD>(mt_Time.mt_System.m_Time_Prev, mt_Time.mt_System.m_Time_Now, MAXDWORD);


        // 동작 시간 업데이트
        if(mt_Time.mt_System.m_Time_Prev > mt_Time.mt_System.m_Time_Now)
        {
            m_dw64TotalPlayTime_Stack += (MAXDWORD - m_dwStartingTime);
            m_dw64TotalPlayTime_Stack += 1;

            m_dwStartingTime = 0;
        }
        const DWORD64 dw64Total = m_dw64TotalPlayTime_Stack + (mt_Time.mt_System.m_Time_Now - m_dwStartingTime);
        m_fTotalPlayTimeSec = static_cast<float>(dw64Total) / mt_Time.mt_System.m_dwCycle_Per_Second;
    }

    // 프레임진행 - 고해상도 타이머
    BOOL CTimer::mFN_Frame__Large_Timer()
    {
        mFN_QueryPerformanceCounter();
        mt_Time.mt_Large.m_Delay_Now = gFN_Cal_ElapsedTime<LONGLONG>(mt_Time.mt_Large.m_Time_Prev.QuadPart, mt_Time.mt_Large.m_Time_Now.QuadPart, MAXLONGLONG);

        if(m_bLimitFPS && mt_Time.mt_Large.m_Delay_Now < m_llDelay_LimitFPS)
            return FALSE;;

        mt_Time.mt_Large.m_Time_Prev = mt_Time.mt_Large.m_Time_Now;
        mt_Time.m_fDelay_Now = static_cast<float>(mt_Time.mt_Large.m_Delay_Now);
        return TRUE;
    }

    // 프레임진행 - 저해상도 타이머
    BOOL CTimer::mFN_Frame__Small_Timer()
    {
        // mt_Time.mt_System mt_Time.mt_Small 은 공용체
        if(m_bLimitFPS && mt_Time.mt_Small.m_Delay_Now < m_dwDelay_LimitFPS)
            return FALSE;

        mt_Time.mt_Small.m_Time_Prev = mt_Time.mt_Small.m_Time_Now;
        mt_Time.m_fDelay_Now = static_cast<float>(mt_Time.mt_Small.m_Delay_Now);
        return TRUE;
    }

    void CTimer::mFN_Cal_Statistic()
    {
        const DWORD cntArray = sizeof(mt_Statistic.m_Array_fFPS) / sizeof(float);
        DWORD& Focus = mt_Statistic.m_Focus_Array_FPS;
        if(mt_Statistic.m_Focus_Array_FPS != MAXDWORD)
        {
            mt_Statistic.m_fSum_FPS -= mt_Statistic.m_Array_fFPS[Focus];
            mt_Statistic.m_fSum_FPS += mFN_Get_Frame_Per_Second();
            mt_Statistic.m_Array_fFPS[Focus] = mFN_Get_Frame_Per_Second();
            if(++Focus >= cntArray)
                Focus = 0;
        }
        else
        {
            Focus = 0;
            for(DWORD i=0; i<cntArray; i++)
                mt_Statistic.m_Array_fFPS[i] = mFN_Get_Frame_Per_Second();

            mt_Statistic.m_fSum_FPS = mFN_Get_Frame_Per_Second() * cntArray;
        }
        mt_Statistic.m_fFPS_Average = mt_Statistic.m_fSum_FPS / static_cast<float>(cntArray);
    }

};