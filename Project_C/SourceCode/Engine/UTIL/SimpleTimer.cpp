#include "stdafx.h"
#include "./SimpleTimer.h"

#include <mmsystem.h>

namespace UTIL{


    CSimpleTimer::CSimpleTimer()
        : m_bUsing_Performance_Counter(TRUE)
        , m_dwTimeNow(0)
        , m_dwCycle_Per_Second(0)
        , m_fCycle_Per_Second(0.f)
    {
        if(!mFN_Test_Performance_Counter())
        {
            m_dwCycle_Per_Second = 1000;
        }

        mFN_Frame();
    }

    // 고해상도 타미머 테스트
    BOOL CSimpleTimer::mFN_Test_Performance_Counter()
    {
        LARGE_INTEGER _Time_Frequency;
        if( !QueryPerformanceFrequency(&_Time_Frequency) )
            return FALSE;

        if(_Time_Frequency.LowPart == 0)
            return FALSE;

        LARGE_INTEGER _Time_Count;
        if(!QueryPerformanceCounter(&_Time_Count))
            return FALSE;

        m_dwCycle_Per_Second    = _Time_Frequency.LowPart;
        m_fCycle_Per_Second     = static_cast<float>(_Time_Frequency.LowPart);
        return TRUE;
    }

    // 현재 시간을 업데이트
    void CSimpleTimer::mFN_Frame()
    {
        if(m_bUsing_Performance_Counter)
        {
            LARGE_INTEGER _Time;
            DWORD_PTR oldmask = ::SetThreadAffinityMask(::GetCurrentThread(), 1);
            QueryPerformanceCounter(&_Time);
            ::SetThreadAffinityMask(::GetCurrentThread(), oldmask);
            m_dwTimeNow = _Time.LowPart;
        }
        else
        {
            ::timeBeginPeriod(1);
            m_dwTimeNow = ::timeGetTime();
            ::timeEndPeriod(1);
        }
    }

    // 초당 사이클을 리턴
    DWORD CSimpleTimer::mFN_Get_Cycle() const
    {
        return m_dwCycle_Per_Second;
    }

    // 현재 시간을 리턴
    DWORD CSimpleTimer::mFN_Get_Time() const
    {
        return m_dwTimeNow;
    }

    // 이전 시간을 인자로 받아 지난 시간을 리턴(Sec)
    float CSimpleTimer::mFN_Get_DelaySec(DWORD _dwPrevTime) const
    {
        if(m_dwTimeNow < _dwPrevTime)
        {
            // 타이머가 표현범위를 오버플로우 한것으로 간주한다.
            DWORD dwOver = MAXDWORD - _dwPrevTime;
            float fSec      = static_cast<float>( (dwOver / m_dwCycle_Per_Second) );
            float fMilliSec = static_cast<float>( (dwOver % m_dwCycle_Per_Second) ) + 1.f;
            fMilliSec /= m_dwCycle_Per_Second;

            return ( fSec + fMilliSec + (static_cast<float>(m_dwTimeNow) / m_fCycle_Per_Second) );
        }
        else
        {
            DWORD dwDealy = m_dwTimeNow - _dwPrevTime;
            if(dwDealy == 0)
                return 0.f;

            return ( static_cast<float>(dwDealy) / m_fCycle_Per_Second );
        }
    }

    // 고해상도 타이머 사용(On / Off)
    BOOL CSimpleTimer::mFN_Use_Performance_Counter(BOOL _bUsing)
    {
        if(m_bUsing_Performance_Counter == _bUsing)
            return TRUE;

        if(_bUsing)
        {
            if( !mFN_Test_Performance_Counter() )
                return FALSE;

            m_bUsing_Performance_Counter = TRUE;
        }
        else
        {
            m_dwCycle_Per_Second = 1000;
            m_fCycle_Per_Second  = 1000.f;

            m_bUsing_Performance_Counter = FALSE;
        }

        return TRUE;
    }


};