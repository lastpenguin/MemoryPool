#pragma once
/*----------------------------------------------------------------
/	[실행중인 시스템 정보]
/
/----------------------------------------------------------------
/   기존 실행중인 프로그램 정보를 분할,
/   패치 경로를 제외하고, OS, 하드웨어 정보를 포함
----------------------------------------------------------------*/

namespace UTIL{

    // 추가되는 항목은 마지막 부분에 추가해야함
    // enum class : VS2010 이상
    typedef enum struct E_OS_TYPE: UINT32{
        E_OS_TYPE__UNKNOWN = 0,

        E_OS_TYPE__MS_WINDOWS_NT_UNKNOWN,
        E_OS_TYPE__MS_WINDOWS_2000,
        E_OS_TYPE__MS_WINDOWS_XP,
        E_OS_TYPE__MS_WINDOWS_XP_SP2,
        E_OS_TYPE__MS_WINDOWS_XP_SP3,
        E_OS_TYPE__MS_WINDOWS_SERVER_2003 ,
        E_OS_TYPE__MS_WINDOWS_SERVER_2003_R2 ,
        E_OS_TYPE__MS_WINDOWS_VISTA ,
        E_OS_TYPE__MS_WINDOWS_SERVER_2008 ,
        E_OS_TYPE__MS_WINDOWS_7 ,
        E_OS_TYPE__MS_WINDOWS_SERVER_2008_R2 ,
        E_OS_TYPE__MS_WINDOWS_8 ,
        E_OS_TYPE__MS_WINDOWS_SERVER_2012 ,
        E_OS_TYPE__MS_WINDOWS_8_1 ,
        E_OS_TYPE__MS_WINDOWS_SERVER_2012_R2 ,
        E_OK_TYPE__MS_WINDOWS_10 ,
    }E_OS_TYPE;

    // https://msdn.microsoft.com/ko-kr/library/hskdteyh.aspx
    // __cpuid, __cpuidex 예제코드
    class TCpu_InstructionSet_DATA{
    public:
        char szName_Vendor[0x20];
        char szName_Brand[0x40];
        bool isIntel_;
        bool isAMD_;
        std::bitset<32> f_1_ECX_;
        std::bitset<32> f_1_EDX_;
        std::bitset<32> f_7_EBX_;
        std::bitset<32> f_7_ECX_;
        std::bitset<32> f_81_ECX_;
        std::bitset<32> f_81_EDX_;
    };
    class TCpu_InstructionSet : protected TCpu_InstructionSet_DATA{
    public:
        inline const char* NameVendor(void) const { return szName_Vendor; }
        inline const char* NameBrand(void) const { return szName_Brand; }

        inline bool SSE3(void) const { return f_1_ECX_[0]; }
        inline bool PCLMULQDQ(void) const { return f_1_ECX_[1]; }
        inline bool MONITOR(void) const { return f_1_ECX_[3]; }
        inline bool SSSE3(void) const { return f_1_ECX_[9]; }
        inline bool FMA(void) const { return f_1_ECX_[12]; }
        inline bool CMPXCHG16B(void) const { return f_1_ECX_[13]; }
        inline bool SSE41(void) const { return f_1_ECX_[19]; }
        inline bool SSE42(void) const { return f_1_ECX_[20]; }
        inline bool MOVBE(void) const { return f_1_ECX_[22]; }
        inline bool POPCNT(void) const { return f_1_ECX_[23]; }
        inline bool AES(void) const { return f_1_ECX_[25]; }
        inline bool XSAVE(void) const { return f_1_ECX_[26]; }
        inline bool OSXSAVE(void) const { return f_1_ECX_[27]; }
        inline bool AVX(void) const { return f_1_ECX_[28]; }
        inline bool F16C(void) const { return f_1_ECX_[29]; }
        inline bool RDRAND(void) const { return f_1_ECX_[30]; }

        inline bool MSR(void) const { return f_1_EDX_[5]; }
        inline bool CX8(void) const { return f_1_EDX_[8]; }
        inline bool SEP(void) const { return f_1_EDX_[11]; }
        inline bool CMOV(void) const { return f_1_EDX_[15]; }
        inline bool CLFSH(void) const { return f_1_EDX_[19]; }
        inline bool MMX(void) const { return f_1_EDX_[23]; }
        inline bool FXSR(void) const { return f_1_EDX_[24]; }
        inline bool SSE(void) const { return f_1_EDX_[25]; }
        inline bool SSE2(void) const { return f_1_EDX_[26]; }

        inline bool FSGSBASE(void) const { return f_7_EBX_[0]; }
        inline bool BMI1(void) const { return f_7_EBX_[3]; }
        inline bool HLE(void) const { return isIntel_ && f_7_EBX_[4]; }
        inline bool AVX2(void) const { return f_7_EBX_[5]; }
        inline bool BMI2(void) const { return f_7_EBX_[8]; }
        inline bool ERMS(void) const { return f_7_EBX_[9]; }
        inline bool INVPCID(void) const { return f_7_EBX_[10]; }
        inline bool RTM(void) const { return isIntel_ && f_7_EBX_[11]; }
        inline bool AVX512F(void) const { return f_7_EBX_[16]; }
        inline bool RDSEED(void) const { return f_7_EBX_[18]; }
        inline bool ADX(void) const { return f_7_EBX_[19]; }
        inline bool AVX512PF(void) const { return f_7_EBX_[26]; }
        inline bool AVX512ER(void) const { return f_7_EBX_[27]; }
        inline bool AVX512CD(void) const { return f_7_EBX_[28]; }
        inline bool SHA(void) const { return f_7_EBX_[29]; }

        inline bool PREFETCHWT1(void) const { return f_7_ECX_[0]; }

        inline bool LAHF(void) const { return f_81_ECX_[0]; }
        inline bool LZCNT(void) const { return isIntel_ && f_81_ECX_[5]; }
        inline bool ABM(void) const { return isAMD_ && f_81_ECX_[5]; }
        inline bool SSE4a(void) const { return isAMD_ && f_81_ECX_[6]; }
        inline bool XOP(void) const { return isAMD_ && f_81_ECX_[11]; }
        inline bool TBM(void) const { return isAMD_ && f_81_ECX_[21]; }

        inline bool SYSCALL(void) const { return isIntel_ && f_81_EDX_[11]; }
        inline bool MMXEXT(void) const { return isAMD_ && f_81_EDX_[22]; }
        inline bool RDTSCP(void) const { return isIntel_ && f_81_EDX_[27]; }
        inline bool _3DNOWEXT(void) const { return isAMD_ && f_81_EDX_[30]; }
        inline bool _3DNOW(void) const { return isAMD_ && f_81_EDX_[31]; }
    };

	__interface ISystem_Information{
        const TCpu_InstructionSet* mFN_Get_CPU_InstructionSet() const;
        const SYSTEM_INFO*         mFN_Get_SystemInfo() const;

        DWORD mFN_Get_NumProcessor_Logical() const;
        DWORD mFN_Get_NumProcessor_PhysicalCore() const;
        DWORD mFN_Get_ProcessorType() const;

        E_OS_TYPE   mFN_Get_OsType() const;
        LPCTSTR     mFN_Get_OsName() const;
        BOOL        mFN_Query_Os64Bit() const;

        BOOL mFN_Query_OSVersion_Test_Above(DWORD dwMajor, DWORD dwMinor, WORD wServicePackMajor, WORD wServicePackMinor) const;
        const OSVERSIONINFOEX* mFN_Get_OsInfo() const;
        DWORD mFN_Get_OsVersion_Major() const;
        DWORD mFN_Get_OsVersion_Minor() const;
        DWORD mFN_Get_OsBuildNumber() const;
	};


};