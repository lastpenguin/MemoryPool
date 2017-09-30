#pragma once
/*----------------------------------------------------------------
/	[로그 기록기]
/
/----------------------------------------------------------------
/   로그를 관리하는 객체 입니다.
/   객체당 1개의 파일을 담당합니다.
/
/----------------------------------------------------------------
/
/	버  전: 0.1
/	작성자: lastpenguin83@gmail.com
/	작성일: 08-04-20(일)
/	수정일: 00-00-00(00)
----------------------------------------------------------------*/
#include "../../Core/UTIL/LogWriter_Interface.h"
#include "../../BasisClass/LOCK/Lock.h"


namespace UTIL{

    #pragma warning(push)
    #pragma warning(disable: 4324)
    class _DEF_CACHE_ALIGN CLogWriter : public ILogWriter, private CUnCopyAble{
    #if defined(_UNICODE)
        static const BOOL sc_bUniCodeCompile = TRUE;
    #else
        static const BOOL sc_bUniCodeCompile = FALSE;
    #endif
        // 8바이트 배수에 맞춤
        static const DWORD sc_MaxPath = (MAX_PATH % 8u)? ((MAX_PATH / 8u)*8u + 8u) : MAX_PATH;

    public:
        //----------------------------------------------------------------
        CLogWriter(size_t nLimit_WriteCount
            , LPCTSTR _LogFileName = _T("log.txt")
            , LPCTSTR _LogFolder = _T("LOG")
            , LPCTSTR _Rootpath = nullptr
            , BOOL _bEncoding_is_UniCode = sc_bUniCodeCompile);
        virtual ~CLogWriter();

    public:
        /*----------------------------------------------------------------
        /   사용자 인터페이스
        ----------------------------------------------------------------*/
        virtual void mFN_WriteLog(BOOL _bWriteTime, LPCSTR _szText) override; // MULTIBYTE -> MULTIBYTE / UNICODE
        virtual void mFN_WriteLog(BOOL _bWriteTime, LPCWSTR _szText) override;// UNICODE   -> MULTIBYTE / UNICODE

        virtual LPCTSTR mFN_Get_LogPath() const override;

    protected:
        BOOL mFN_WriteLog_Internal(BOOL _bWriteTime, LPCSTR _szText);
        BOOL mFN_WriteLog_Internal(BOOL _bWriteTime, LPCWSTR _szText);

        BOOL  mFN_WriteLog__MULTIBYTE(BOOL _bWriteTime, LPCSTR _szText);
        BOOL  mFN_WriteLog__UNICODE(BOOL _bWriteTime, LPCWSTR _szText);
        FILE* mFN_OpenFile();

        void mFN_WriteLog__Time_StartEnd(BOOL isStart);
        void mFN_Stop_WriteLog();


    private:
        TCHAR m_strPath[sc_MaxPath];

        const BOOL m_bEncoding_is_UniCode;
        BOOL m_bStopWriteLog;
        const size_t m_nLimit_WriteLog;
        size_t m_Counting_WriteLog;

        //_DEF_CACHE_ALIGN
        LOCK::CCompositeLock    m_Lock;
        
        
    };
    #pragma warning(pop)

};