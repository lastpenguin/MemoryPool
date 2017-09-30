#include "./System_Information_Interface.h"

namespace UTIL{
    class TCpu_InstructionSet_Test : public TCpu_InstructionSet{
    public:
        void Test();
    };

    class CSystem_Information : public ISystem_Information{
    public:
        CSystem_Information();
        virtual ~CSystem_Information();

    private:
        SYSTEM_INFO     m_SystemInfo;
        E_OS_TYPE       m_OsType;
        LPCTSTR         m_szOsName;
        OSVERSIONINFOEX m_OsVersion;

        TCpu_InstructionSet_Test m_Cpu_InstructionSet;

        DWORD           m_NumProcessor_Logical;
        DWORD           m_NumProcessor_PhysicalCore;

    private:
        void mFN_CheckOs();
        void mFN_CheckCpu1();
        void mFN_CheckCpu2();

    public:
        /*----------------------------------------------------------------
        /       사용자 인터페이스
        ----------------------------------------------------------------*/
        virtual const TCpu_InstructionSet*  mFN_Get_CPU_InstructionSet() const override;
        virtual const SYSTEM_INFO*          mFN_Get_SystemInfo() const override;

        virtual DWORD mFN_Get_NumProcessor_Logical() const override;
        virtual DWORD mFN_Get_NumProcessor_PhysicalCore() const override;
        virtual DWORD mFN_Get_ProcessorType() const override;

        virtual E_OS_TYPE   mFN_Get_OsType() const override;
        virtual LPCTSTR     mFN_Get_OsName() const override;
        virtual BOOL        mFN_Query_Os64Bit() const override;

        virtual BOOL mFN_Query_OSVersion_Test_Above(DWORD dwMajor, DWORD dwMinor, WORD wServicePackMajor, WORD wServicePackMinor) const override;
        virtual const OSVERSIONINFOEX* mFN_Get_OsInfo() const override;
        virtual DWORD mFN_Get_OsVersion_Major() const override;
        virtual DWORD mFN_Get_OsVersion_Minor() const override;
        virtual DWORD mFN_Get_OsBuildNumber() const override;
	};


};