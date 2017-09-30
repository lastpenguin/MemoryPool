#include "stdafx.h"
#include "./Engine.h"


namespace ENGINE{
    void CEngine::mFN_Write_EngineInformation(::UTIL::ILogWriter* pLog)
    {
        if(!pLog) return;
        #define _LOCAL_MACRO_LOG(bTime, fmt, ...) ::UTIL::TLogWriter_Control::sFN_WriteLog(pLog, bTime, fmt, __VA_ARGS__)

        auto pT = mFN_Query_CompileTime();
        if(pT)
        {
            auto t = *pT;
            _LOCAL_MACRO_LOG(FALSE, _T("Compile Time : %d-%02d-%02d  %02d:%02d:%02d (Engine LIB)")
                , t.Year
                , t.Month
                , t.Day
                , t.Hour
                , t.Minute
                , t.Second);
        }
        if(!mFN_Query_Initialized_Engine())
        {
            // 사용자에게 안내한다
            _MACRO_MESSAGEBOX__ERROR(_T("from Engine"), _T("Failed Initialized Engine...\nFailed Code : 0x%016I64X"), m_stats_Engine_Initialized.m_Failed_Something);


            // 초기화에 실패하였다면 이유를 기록한다
            _LOCAL_MACRO_LOG(TRUE, _T("Failed Initialized Engine... Failed Code : 0x%016I64X")
                , m_stats_Engine_Initialized.m_Failed_Something);
            

            const ::UTIL::TCpu_InstructionSet* pCpuIS = nullptr;
            const SYSTEM_INFO* pSysInfo = nullptr;
            if(::UTIL::g_pSystem_Information)
            {
                pCpuIS = ::UTIL::g_pSystem_Information->mFN_Get_CPU_InstructionSet();
                pSysInfo = ::UTIL::g_pSystem_Information->mFN_Get_SystemInfo();
            }
            //----------------------------------------------------------------
            // 상세 리포트(필요한 정보만 기록)
            //----------------------------------------------------------------
            const auto& code = m_stats_Engine_Initialized.m_FailedCode;
            if(code._ConnectCore){
                _LOCAL_MACRO_LOG(FALSE, _T("\tFailed Connect Core"));
            }
            if(code._Intrinsics && pCpuIS){
                _LOCAL_MACRO_LOG(FALSE, "\t%s\n\t%s\n", pCpuIS->NameVendor(), pCpuIS->NameBrand());
            }
            if(code._OS && ::UTIL::g_pSystem_Information){
                auto osv = *::UTIL::g_pSystem_Information->mFN_Get_OsInfo();
                _LOCAL_MACRO_LOG(FALSE, _T("\tOS : %s\n\tOS Version(%u.%u) ServicePack(%hu.%hu) Build(%u)")
                    , ::UTIL::g_pSystem_Information->mFN_Get_OsName()
                    , osv.dwMajorVersion, osv.dwMinorVersion
                    , osv.wServicePackMajor, osv.wServicePackMinor
                    , osv.dwBuildNumber);
            }
            if(code._SystemMemory){
                _LOCAL_MACRO_LOG(FALSE, _T("\t---- System Memory Information ----"));
                if(pSysInfo)
                    _LOCAL_MACRO_LOG(FALSE, _T("\tPageSize(%u) AllocationGranularity(%u)"), pSysInfo->dwPageSize, pSysInfo->dwAllocationGranularity);

                MEMORYSTATUS MemInfo;
                MemInfo.dwLength = sizeof(MemInfo);
            #pragma warning(push)
            #pragma warning(disable: 28159)
                ::GlobalMemoryStatus(&MemInfo);
            #pragma warning(pop)
                _LOCAL_MACRO_LOG(FALSE, _T("\tMemoryLoad              : %10u %%"), MemInfo.dwMemoryLoad);
                _LOCAL_MACRO_LOG(FALSE, _T("\tPhysical Memory(Total)  : %10Iu KB"), MemInfo.dwTotalPhys / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tPhysical Memory(Free)   : %10Iu KB"), MemInfo.dwAvailPhys / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tPaging File(Total)      : %10Iu KB"), MemInfo.dwTotalPageFile / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tPaging File(Free)       : %10Iu KB"), MemInfo.dwAvailPageFile / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tVirtual Memory(Total)   : %10Iu KB"), MemInfo.dwTotalVirtual / 1024);
                _LOCAL_MACRO_LOG(FALSE, _T("\tVirtual Memory(Free)    : %10Iu KB"), MemInfo.dwAvailVirtual / 1024);
            }
            if(code._InitializeMemoryPool)
                _LOCAL_MACRO_LOG(FALSE, _T("\tFailed preinitialize MemoryPool"));
        }
        #undef _LOCAL_MACRO_LOG
    }
}


namespace ENGINE{
    BOOL CEngine::mFN_Test_System_Intrinsics()
    {
        // 코드에서 사용한 내장함수는 모두 체크할 것
        auto pIntrinsics = UTIL::g_pSystem_Information->mFN_Get_CPU_InstructionSet();
        if(!pIntrinsics->SSE2())
            return FALSE;

        return TRUE;
    }
    BOOL CEngine::mFN_Test_System_OS()
    {
        // x64 실행파일을 x32 OS에서 실행하는 경우 확인
        // 애초에 실행은 되는가?
        #if __X64
        if(!UTIL::g_pSystem_Information->mFN_Query_Os64Bit())
            return FALSE;
        #endif

        // OS 버전 체크
        {
            // 이 것은 UTIL::E_OS_TYPE내부에 정의된 것과 비교하는 것은 지양해야한다
            // 새로운 상위버전 OS가 나왔을때, 코드가 업데이트 되지않았다면 조건을 만족하지 못하기 때문에
            // 가능하다면 OSVERSIONINFO 의 버전과 직접 비교한다
            // 실행 조건 XP SP3 이상
            // XP SP3 ver(5, 1, 3, 0)
            if(!UTIL::g_pSystem_Information->mFN_Query_OSVersion_Test_Above(5, 1, 3, 0))
                return FALSE;
        }
        return TRUE;
    }
    BOOL CEngine::mFN_Test_System_Memory()
    {
        auto pSysInfo = UTIL::g_pSystem_Information->mFN_Get_SystemInfo();
        // 주소 단위 64KB 배수 확인
        {
            const size_t iAddress_per_Reserve = 64 *1024;
            if(1 > pSysInfo->dwAllocationGranularity / iAddress_per_Reserve)
                return FALSE;
            else if(0 != pSysInfo->dwAllocationGranularity % iAddress_per_Reserve)
                return FALSE;
        }
        // 할당 단위 4KB 배수 확인
        {
            const size_t iMinPageSize = 4 *1024;
            if(1 > pSysInfo->dwPageSize / iMinPageSize)
                return FALSE;
            else if(0 != pSysInfo->dwPageSize % iMinPageSize)
                return FALSE;
        }
        // 필요한 최소 메모리 체크
        {
            MEMORYSTATUS MemInfoSys;
            MemInfoSys.dwLength = sizeof(MemInfoSys);
        #pragma warning(push)
        #pragma warning(disable: 28159)
            ::GlobalMemoryStatus(&MemInfoSys);
        #pragma warning(pop)

            // 램 512MB 제한(어떤 환경에서 실행될지 모른다 수정되는 코드를 적게 하기 위해...)
            size_t limit = 512 * 1024 * 1024;
            // 실제 메모리는 약간 적게 잡히기 때문에 약간 여유분을 적용
            limit = limit * 95 / 100;
            if(MemInfoSys.dwTotalPhys < limit)
                return FALSE;
        }

        return TRUE;
    }
    BOOL CEngine::mFN_Initialize_MemoryPool()
    {
        // to do
        // XML 등의 설정파일을 파싱하여 설정
        //  현실적으로 가능한 것
        //  최근 n개의 실행 통계를 보관하여
        //  최적의 메모리풀 초기화를 계산해서 사용

        return TRUE;
    }
    

}