#pragma once

namespace UTIL{


    class CSimpleTimer{
    public:
        CSimpleTimer();

    private:
        // 고해상도 타미머 테스트
        BOOL mFN_Test_Performance_Counter();

    private:
        BOOL    m_bUsing_Performance_Counter;

        DWORD   m_dwTimeNow;
        DWORD   m_dwCycle_Per_Second;
        float   m_fCycle_Per_Second;

    public:
        void  mFN_Frame();                                  // 현재 시간을 업데이트
        DWORD mFN_Get_Cycle() const;                        // 초당 사이클을 리턴
        DWORD mFN_Get_Time() const;                         // 현재 시간을 리턴
        float mFN_Get_DelaySec(DWORD _dwPrevTime) const;    // 이전 시간을 인자로 받아 지난 시간을 리턴(Sec)
        
        _DEF_MAKE_GET_METHOD(UsingPerformance_Counter, BOOL, m_bUsing_Performance_Counter);
        BOOL  mFN_Use_Performance_Counter(BOOL _bUsing);    // 고해상도 타이머 사용(On / Off)
    };


};