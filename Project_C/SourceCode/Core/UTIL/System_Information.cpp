#include "stdafx.h"
#include "./System_Information.h"

#include <intrin.h>

#pragma warning(disable: 6255)

namespace UTIL{

    namespace{
        struct TOSVersion{
            E_OS_TYPE   m_Type;
            LPCTSTR     m_szName;
            BOOL(*pFN)(OSVERSIONINFOEX& info);
        };

        // https://msdn.microsoft.com/ko-kr/library/windows/desktop/ms724833(v=vs.85).aspx
        // OSVERSIONINFOEX structure 에 따른 OS 판별
        // 탐색순서에 주의(최신 버전을 먼저 탐색)
        // GetVersionEx 함수는 윈도우 2000 이상에서 실행되기 때문에 그 이하는 추가할 필요 없다
        const TOSVersion g_table_OS_Info[] ={
            {E_OS_TYPE::E_OS_TYPE__UNKNOWN, _T("Unknown"), [](OSVERSIONINFOEX& info){
                UNREFERENCED_PARAMETER(info);
                return TRUE;
            }},

            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_NT_UNKNOWN, _T("Unknown : Windows NT"), [](OSVERSIONINFOEX& info){
                return (info.dwPlatformId == VER_PLATFORM_WIN32_NT)? TRUE : FALSE;
            }},

            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_2000, _T("Windows 2000"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                return ((info.dwMajorVersion==5) && (info.dwMinorVersion==0))? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_XP, _T("Windows XP"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                return ((info.dwMajorVersion==5) && (info.dwMinorVersion==1))? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_XP_SP2, _T("Windows XP SP2"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                return ((info.dwMajorVersion==5) && (info.dwMinorVersion==1) && (info.wServicePackMajor==2))? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_XP_SP3, _T("Windows XP SP3"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                return ((info.dwMajorVersion==5) && (info.dwMinorVersion==1) && (info.wServicePackMajor==3))? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_SERVER_2003, _T("Windows Server 2003"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=5 || info.dwMinorVersion!=2) return FALSE;
                return (0 == GetSystemMetrics(SM_SERVERR2))? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_SERVER_2003_R2, _T("Windows Server 2003 R2"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=5 || info.dwMinorVersion!=2) return FALSE;
                return (0 != GetSystemMetrics(SM_SERVERR2))? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_VISTA, _T("Windows Vista"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=6 || info.dwMinorVersion!=0) return FALSE;
                return (info.wProductType == VER_NT_WORKSTATION)? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_SERVER_2008, _T("Windows Server 2008"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=6 || info.dwMinorVersion!=0) return FALSE;
                return (info.wProductType != VER_NT_WORKSTATION)? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_7, _T("Windows 7"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=6 || info.dwMinorVersion!=1) return FALSE;
                return (info.wProductType == VER_NT_WORKSTATION)? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_SERVER_2008_R2, _T("Windows Server 2008 R2"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=6 || info.dwMinorVersion!=1) return FALSE;
                return (info.wProductType != VER_NT_WORKSTATION)? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_8, _T("Windows 8"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=6 || info.dwMinorVersion!=2) return FALSE;
                return (info.wProductType == VER_NT_WORKSTATION)? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_SERVER_2012, _T("Windows Server 2012"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=6 || info.dwMinorVersion!=2) return FALSE;
                return (info.wProductType != VER_NT_WORKSTATION)? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_8_1, _T("Windows 8.1"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=6 || info.dwMinorVersion!=3) return FALSE;
                return (info.wProductType == VER_NT_WORKSTATION)? TRUE : FALSE;
            }},
            {E_OS_TYPE::E_OS_TYPE__MS_WINDOWS_SERVER_2012_R2, _T("Windows Server 2012 R2"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=6 || info.dwMinorVersion!=3) return FALSE;
                return (info.wProductType != VER_NT_WORKSTATION)? TRUE : FALSE;
            }} ,
            {E_OS_TYPE::E_OK_TYPE__MS_WINDOWS_10, _T("Windows 10"), [](OSVERSIONINFOEX& info){
                if(info.dwPlatformId != VER_PLATFORM_WIN32_NT) return FALSE;
                if(info.dwMajorVersion!=10 || info.dwMinorVersion!=0) return FALSE;
                return (info.wProductType == VER_NT_WORKSTATION)? TRUE : FALSE;
            }} ,
        };
    }
    
    namespace{
        #pragma pack(push, 4)
        struct _TYPE_INT32_4{
            int a[4];

            int operator [](int index){return a[index];}
            int* data(){ return a; }
        };
        #pragma pack(pop)
    }
    template<typename TFunction>
    struct TFainally{
        TFainally(TFunction _fun)
            : m_FN(_fun)
        {
        }
        ~TFainally()
        {
            m_FN();
        }
        TFunction m_FN;
    };
    void TCpu_InstructionSet_Test::Test()
    {
    #pragma warning(push)
    #pragma warning(disable: 6386)
        const int _LimitSizeStack = 64*1024; // 64KB
        //----------------------------------------------------------------
        memset(this, 0, sizeof(*this));

        int nIds_;
        int nExIds_;

        _TYPE_INT32_4* data_ = nullptr;
        _TYPE_INT32_4* extdata_ = nullptr;

        int _nData_;
        int _nExtdata_;

        _MACRO_STATIC_ASSERT(sizeof(_TYPE_INT32_4) == sizeof(INT32)*4);
        union{
            _TYPE_INT32_4 cput;
            int cpui[4];
        };

        // Calling __cpuid with 0x0 as the function_id argument
        // gets the number of the highest valid function ID.
        __cpuid(cpui, 0);
        nIds_ = cpui[0];

        _nData_ = 1 + nIds_;
        _MACRO_SAFE_LOCAL_ALLOCA(data_, sizeof(_TYPE_INT32_4) * _nData_, _LimitSizeStack);
        if(!data_)
        {
            _Error_OutOfMemory();
            return;
        }
        for(int i = 0; i <= nIds_; ++i)
        {
            __cpuidex(cpui, i, 0);
            data_[i] = cput;
        }

        // Capture vendor string
        *reinterpret_cast<int*>(szName_Vendor) = data_[0][1];
        *reinterpret_cast<int*>(szName_Vendor + 4) = data_[0][3];
        *reinterpret_cast<int*>(szName_Vendor + 8) = data_[0][2];

        if(0 == std::strcmp(szName_Vendor, "GenuineIntel"))
        {
            isIntel_ = true;
        }
        else if(0 == std::strcmp(szName_Vendor, "AuthenticAMD"))
        {
            isAMD_ = true;
        }

        // load bitset with flags for function 0x00000001
        if(nIds_ >= 1)
        {
            f_1_ECX_ = data_[1][2];
            f_1_EDX_ = data_[1][3];
        }

        // load bitset with flags for function 0x00000007
        if(nIds_ >= 7)
        {
            f_7_EBX_ = data_[7][1];
            f_7_ECX_ = data_[7][2];
        }

        // Calling __cpuid with 0x80000000 as the function_id argument
        // gets the number of the highest valid extended ID.
        __cpuid(cpui, 0x80000000);
        nExIds_ = cpui[0];

        _nExtdata_ = 1 + (nExIds_ - (int)0x80000000);
        _MACRO_SAFE_LOCAL_ALLOCA(extdata_, sizeof(_TYPE_INT32_4) * _nExtdata_, _LimitSizeStack);
        if(!extdata_)
        {
            _Error_OutOfMemory();
            return;
        }
        for(int i = 0x80000000, iSel = 0; i <= nExIds_; ++i)
        {
            __cpuidex(cpui, i, 0);
            extdata_[iSel++] = cput;
        }

        // load bitset with flags for function 0x80000001
        if(nExIds_ >= 0x80000001)
        {
            f_81_ECX_ = extdata_[1][2];
            f_81_EDX_ = extdata_[1][3];
        }

        // Interpret CPU brand string if reported
        if(nExIds_ >= 0x80000004)
        {
            memcpy(szName_Brand, extdata_[2].data(), sizeof(cpui));
            memcpy(szName_Brand + 16, extdata_[3].data(), sizeof(cpui));
            memcpy(szName_Brand + 32, extdata_[4].data(), sizeof(cpui));
        }
    #pragma warning(pop)
    }

    CSystem_Information::CSystem_Information()
        : m_OsType(g_table_OS_Info[0].m_Type), m_szOsName(g_table_OS_Info[0].m_szName)
        , m_NumProcessor_Logical(0), m_NumProcessor_PhysicalCore(0)
    {
        // GetSystemInfo 는 Windows 2000이상에서 실행된다
        GetSystemInfo(&m_SystemInfo);

        mFN_CheckOs();
        mFN_CheckCpu1();
        mFN_CheckCpu2();
    }

    CSystem_Information::~CSystem_Information()
    {
    }

    // Os 정보를 조사.
    void CSystem_Information::mFN_CheckOs()
    {
        // MSDN의 샘플코드를 사용한다.
        // 출처: ms-help://MS.VSCC.v90/MS.MSDNQTR.v90.ko/sysinfo/base/getting_the_system_version.htm
        
        OSVERSIONINFOEX& osvi = m_OsVersion; // 참조
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    #pragma warning(disable: 4996)
    #pragma warning(disable: 28159)
        if( !GetVersionEx( reinterpret_cast<LPOSVERSIONINFO>(&osvi) ) )
        {
            osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            
            if( !GetVersionEx( reinterpret_cast<LPOSVERSIONINFO>(&osvi) ) )
                return;
        }
    #pragma warning(default: 4996)
    #pragma warning(default: 28159)
        for(int i=_MACRO_ARRAY_COUNT(g_table_OS_Info)-1; 0<i; i--)
        {
            if(g_table_OS_Info[i].pFN(osvi))
            {
                m_OsType    = g_table_OS_Info[i].m_Type;
                m_szOsName  = g_table_OS_Info[i].m_szName;
                break;
            }
        }
    }

    // CPU 정보를 조사.
    void CSystem_Information::mFN_CheckCpu1()
    {
        m_Cpu_InstructionSet.Test();
    }

    void CSystem_Information::mFN_CheckCpu2()
    {
        // MSDN의 샘플코드를 사용한다.
        // 출처:https://msdn.microsoft.com/ko-kr/library/windows/desktop/ms683194(v=vs.85).aspx
        BOOL bSucceed = FALSE;
        auto __pFN_Final = [&, this](){
            if(!bSucceed)
            {
                // OS가 WindowsXP SP3 미만이라면 GetLogicalProcessorInformation 함수를 찾지 못해
                // CPU를 아직 체크하지 못한 상황이다
                m_NumProcessor_Logical      = m_SystemInfo.dwNumberOfProcessors;
                m_NumProcessor_PhysicalCore = m_SystemInfo.dwNumberOfProcessors;
            }
        };
        _MACRO_CREATE__FUNCTION_OBJECT_FINALLYQ(__pFN_Final);

        typedef BOOL(WINAPI *LPFN_GLPI)(
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION,
            PDWORD);
        LPFN_GLPI pFN = (LPFN_GLPI)GetProcAddress(::GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformation");
        if(!pFN)
            return;

        DWORD numaNodeCount         = 0;
        DWORD processorPackageCount = 0;

        DWORD logicalProcessorCount = 0;
        DWORD processorCoreCount    = 0;

        DWORD processorL1CacheCount = 0;
        DWORD processorL2CacheCount = 0;
        DWORD processorL3CacheCount = 0;

        SYSTEM_LOGICAL_PROCESSOR_INFORMATION* pInfo = nullptr;
        DWORD dwSizeBuffer = 0;
        pFN(pInfo, &dwSizeBuffer);

        _MACRO_SAFE_LOCAL_ALLOCA(pInfo, dwSizeBuffer, 64*1024);
        if(!pInfo)
        {
            _Error_OutOfMemory();
            return;
        }

        if(!pFN(pInfo, &dwSizeBuffer))
            return;

        auto pCul = pInfo;
        for(DWORD i=0; i<dwSizeBuffer; i+=sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION))
        {
            switch(pCul->Relationship)
            {
            case RelationCache:
                switch(pCul->Cache.Level)
                {
                case 1:
                    processorL1CacheCount++;
                    break;
                case 2:
                    processorL2CacheCount++;
                    break;
                case 3:
                    processorL3CacheCount++;
                    break;
                }
                break;

            case RelationProcessorCore:
                processorCoreCount++;
                logicalProcessorCount += (DWORD)_MACRO_BIT_COUNT(pCul->ProcessorMask);
                break;

            case RelationProcessorPackage:
                // Logical processors share a physical package.
                processorPackageCount++;
                break;

            case RelationNumaNode:
                // Non-NUMA systems report a single record of this type.
                numaNodeCount++;
                break;
            }
            pCul++;
        }

        m_NumProcessor_Logical      = logicalProcessorCount;
        m_NumProcessor_PhysicalCore = processorCoreCount;
        UNREFERENCED_PARAMETER(numaNodeCount);
        UNREFERENCED_PARAMETER(processorPackageCount);   // CPU 수
        
        bSucceed = TRUE;
    }

    /*----------------------------------------------------------------
    /       사용자 인터페이스
    ----------------------------------------------------------------*/

    const TCpu_InstructionSet* CSystem_Information::mFN_Get_CPU_InstructionSet() const
    {
        return &m_Cpu_InstructionSet;
    }
    const SYSTEM_INFO* CSystem_Information::mFN_Get_SystemInfo() const
    {
        return &m_SystemInfo;
    }
    DWORD CSystem_Information::mFN_Get_NumProcessor_Logical() const
    {
        // return m_SystemInfo.dwNumberOfProcessors;
        return m_NumProcessor_Logical;
    }
    DWORD CSystem_Information::mFN_Get_NumProcessor_PhysicalCore() const
    {
        return m_NumProcessor_PhysicalCore;
    }
    DWORD CSystem_Information::mFN_Get_ProcessorType() const
    {
        return m_SystemInfo.dwProcessorType;
    }


    E_OS_TYPE CSystem_Information::mFN_Get_OsType() const
    {
        return m_OsType;
    }
    LPCTSTR CSystem_Information::mFN_Get_OsName() const
    {
        return m_szOsName;
    }
    BOOL CSystem_Information::mFN_Query_Os64Bit() const
    {
        switch(m_SystemInfo.wProcessorArchitecture)
        {
        case PROCESSOR_ARCHITECTURE_IA64:
        case PROCESSOR_ARCHITECTURE_AMD64:
        case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
            return TRUE;
        default:
            return FALSE;
        }
    }

    BOOL CSystem_Information::mFN_Query_OSVersion_Test_Above(DWORD dwMajor, DWORD dwMinor, WORD wServicePackMajor, WORD wServicePackMinor) const
    {
        if(m_OsVersion.dwMajorVersion > dwMajor)
            return TRUE;
        else if(m_OsVersion.dwMajorVersion < dwMajor)
            return FALSE;

        if(m_OsVersion.dwMinorVersion > dwMinor)
            return TRUE;
        else if(m_OsVersion.dwMinorVersion < dwMinor)
            return FALSE;

        if(m_OsVersion.wServicePackMajor > wServicePackMajor)
            return TRUE;
        else if(m_OsVersion.wServicePackMajor < wServicePackMajor)
            return FALSE;

        if(m_OsVersion.wServicePackMinor > wServicePackMinor)
            return TRUE;
        else if(m_OsVersion.wServicePackMinor < wServicePackMinor)
            return FALSE;

        return TRUE;
    }
    const OSVERSIONINFOEX * CSystem_Information::mFN_Get_OsInfo() const
    {
        return &m_OsVersion;
    }
    DWORD CSystem_Information::mFN_Get_OsVersion_Major() const
    {
        return m_OsVersion.dwMajorVersion;
    }
    DWORD CSystem_Information::mFN_Get_OsVersion_Minor() const
    {
        return m_OsVersion.dwMinorVersion;
    }
    DWORD CSystem_Information::mFN_Get_OsBuildNumber() const
    {
        return LOWORD(m_OsVersion.dwBuildNumber);
    }


};