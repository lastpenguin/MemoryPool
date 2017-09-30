#include "stdafx.h"
#include "./LogWriter.h"

#include <atlpath.h>

#pragma warning(disable: 6255)
#ifdef MAX_PATH
#undef MAX_PATH
#define MAX_PATH _DEF_COMPILE_MSG__WARNING("CLogWriter::sc_MaxPath 를 사용하십시오")
#endif
namespace UTIL
{
    namespace{
        static const BOOL   gc_bUse_Alloca = FALSE;
        static const size_t gc_LenTempStrBuffer__LogWrite = 1024 * 4;
    }
    //----------------------------------------------------------------
    //  size_t nLimit_WriteCount
    //      기록수 제한(WriteLog 호출수)
    //      만약 프로그램의 중대한 오류로 지나치게 많은 로그가 작성된다면,
    //      대부분 같은 내용이 계속해서 중복되는 경우가 많으며
    //      저장장치에 무리가 가게 된다
    //      이 값은 신중하게 결정해야 한다
    //      (0 : 제한 없음)
    //  LPCTSTR _LogFileName
    //      로그파일명
    //      ※ 같은 경로에 같은 파일명을 사용해서는 안된다
    //  LPCTSTR _LogFolder
    //      이것은 _Rootpath 의 하위 경로이다
    //  LPCTSTR _Rootpath
    //      지정되지 않거나 무효한 경로의 경우 실행파일의 경로를 사용
    //      ※ 이것은 로그폴더가 아닌 그 부모폴더이다
    //  BOOL _bEncoding_is_UniCode
    //      로그파일 인코딩(Ascii / UniCode)
    //----------------------------------------------------------------
    CLogWriter::CLogWriter(size_t nLimit_WriteCount, LPCTSTR _LogFileName, LPCTSTR _LogFolder, LPCTSTR _Rootpath, BOOL _bEncoding_is_UniCode)
        : m_bEncoding_is_UniCode(_bEncoding_is_UniCode)
        , m_bStopWriteLog(FALSE)
        , m_nLimit_WriteLog(nLimit_WriteCount)
        , m_Counting_WriteLog(0)
    {
        _DEF_CACHE_ALIGNED_TEST__THIS();
        if(!_LogFileName)
            _LogFileName = _T("log.txt");

        m_strPath[0] = 0;

        // 파일 경로 조립
        if(_Rootpath && ATLPath::IsDirectory(_Rootpath))
            _tcscpy_s(m_strPath, _Rootpath);
        else
            ::GetModuleFileName(NULL, m_strPath, sc_MaxPath);
        
        ATLPath::RemoveFileSpec(m_strPath);

        if(_LogFolder)
            ATLPath::Append(m_strPath, _LogFolder);

    #pragma warning(push)
    #pragma warning(disable: 6031)
        // 폴더를 만든다
        _tmkdir(m_strPath);
    #pragma warning(pop)

        // + 파일명
        if(!_LogFileName)
            _Error(_T("CLogWriter::CLogWriter 지정되지 않은 파일명"));
        ATLPath::Append(m_strPath, _LogFileName);

        // 파일을 만든다.
        FILE* pFile = NULL;
        if(m_bEncoding_is_UniCode)
            _tfopen_s(&pFile, m_strPath, _T("wt, ccs=UNICODE"));
        else
            _tfopen_s(&pFile, m_strPath, _T("wt"));
        if(pFile)
            fclose(pFile);
        else
            _Error(_T("Failed Create File : %s"), m_strPath);

        // 시작시간 기록
        mFN_WriteLog__Time_StartEnd(TRUE);
    }

    CLogWriter::~CLogWriter()
    { 
        // 종료시간 기록
        mFN_WriteLog__Time_StartEnd(FALSE);
    }

    /*----------------------------------------------------------------
    /   사용자 인터페이스
    ----------------------------------------------------------------*/
    // m_Counting_WriteLog 오버플로우 가능성
    // 파일을 기록하는 경우에 한하여
    //      한번의 명령으로 1바이트만 기록(\0)한다고 가정,
    //      x86 max size_t 는 아스키 기준 최소 4GB -> 문제가 되지 않는다
    DECLSPEC_NOINLINE void CLogWriter::mFN_WriteLog(BOOL _bWriteTime, LPCSTR _szText)
    {
        if(!_szText || _szText[0] == NULL)
            return;

        if(!m_nLimit_WriteLog)
            mFN_WriteLog_Internal(_bWriteTime, _szText);

        //std::_Atomic_thread_fence(std::memory_order::memory_order_consume);// noinline 이기 때문에 불필요
        if(m_bStopWriteLog)
            return;

        const auto prev = ::InterlockedExchangeAdd(&m_Counting_WriteLog, 1);
        if(prev < m_nLimit_WriteLog)
            mFN_WriteLog_Internal(_bWriteTime, _szText);
        else
            mFN_Stop_WriteLog(); // now == 0 : 오버플로우
    }
    DECLSPEC_NOINLINE void CLogWriter::mFN_WriteLog(BOOL _bWriteTime, LPCWSTR _szText)
    {
        if(!_szText || _szText[0] == NULL)
            return;

        if(!m_nLimit_WriteLog)
            mFN_WriteLog_Internal(_bWriteTime, _szText);

        //std::_Atomic_thread_fence(std::memory_order::memory_order_consume);// noinline 이기 때문에 불필요
        if(m_bStopWriteLog)
            return;

        const auto prev = ::InterlockedExchangeAdd(&m_Counting_WriteLog, 1);
        if(prev < m_nLimit_WriteLog)
            mFN_WriteLog_Internal(_bWriteTime, _szText);
        else
            mFN_Stop_WriteLog(); // now == 0 : 오버플로우
    }

    LPCTSTR CLogWriter::mFN_Get_LogPath() const
    {
        return m_strPath;
    }
    //----------------------------------------------------------------
    //로그를 기록합니다. MULTIBYTE -> MULTIBYTE / UNICODE
    BOOL CLogWriter::mFN_WriteLog_Internal(BOOL _bWriteTime, LPCSTR _szText)
    {
        if(!_szText || _szText[0] == NULL)
            return FALSE;

        if(!m_bEncoding_is_UniCode)
        {
            return mFN_WriteLog__MULTIBYTE(_bWriteTime, _szText);
        }
        else
        {
            if(gc_bUse_Alloca)
            {
                int LenW = MultiByteToWideChar(CP_ACP, 0
                    , _szText, -1
                    , NULL, 0);

                WCHAR* __szTempBuff_W = (WCHAR*)_alloca(sizeof(WCHAR) * LenW);
                MultiByteToWideChar(CP_ACP, 0
                    , _szText, -1
                    , __szTempBuff_W, LenW);

                return mFN_WriteLog__UNICODE(_bWriteTime, __szTempBuff_W);
            }
            else
            {
                WCHAR __szTempBuff_W[gc_LenTempStrBuffer__LogWrite];
                MultiByteToWideChar(CP_ACP, 0
                    , _szText, -1
                    , __szTempBuff_W, _MACRO_ARRAY_COUNT(__szTempBuff_W));

                return mFN_WriteLog__UNICODE(_bWriteTime, __szTempBuff_W);
            }
        }
    }
    //로그를 기록합니다. UNICODE -> MULTIBYTE / UNICODE
    BOOL CLogWriter::mFN_WriteLog_Internal(BOOL _bWriteTime, LPCWSTR _szText)
    {
        if(!_szText || _szText[0] == NULL)
            return FALSE;

        if(m_bEncoding_is_UniCode)
        {
            return mFN_WriteLog__UNICODE(_bWriteTime, _szText);
        }
        else
        {
            if(gc_bUse_Alloca)
            {
                int LenA = WideCharToMultiByte(CP_ACP, 0
                    , _szText, -1
                    , NULL, 0
                    , NULL, FALSE);
                char* __szTempBuff_A = (char*)_alloca(sizeof(char) * LenA);
                WideCharToMultiByte(CP_ACP, 0
                    , _szText, -1
                    , __szTempBuff_A, LenA
                    , NULL, FALSE);

                return mFN_WriteLog__MULTIBYTE(_bWriteTime, __szTempBuff_A);
            }
            else
            {
                char __szTempBuff_A[gc_LenTempStrBuffer__LogWrite];
                WideCharToMultiByte(CP_ACP, 0
                    , _szText, -1
                    , __szTempBuff_A, _MACRO_ARRAY_COUNT(__szTempBuff_A)
                    , NULL, FALSE);

                return mFN_WriteLog__MULTIBYTE(_bWriteTime, __szTempBuff_A);
            }
        }
    }
    //----------------------------------------------------------------
    BOOL CLogWriter::mFN_WriteLog__MULTIBYTE(BOOL _bWriteTime, LPCSTR _szText)
    {
        if(m_bEncoding_is_UniCode)
            return FALSE;

        CHAR szTemp[gc_LenTempStrBuffer__LogWrite];
        LPCSTR pSTR_output;
        if(_bWriteTime)
        {
            SYSTEMTIME st;
            GetLocalTime(&st);
            sprintf_s(szTemp, "[%d-%02d-%02d %02d:%02d:%02d] %s"
                , st.wYear, st.wMonth, st.wDay
                , st.wHour, st.wMinute, st.wSecond
                , _szText);
            pSTR_output = szTemp;
        }
        else
        {
            pSTR_output = _szText;
        }

        m_Lock.Lock();
        FILE* pFile = mFN_OpenFile();
        BOOL bDone  = (pFile)? TRUE : FALSE;
        if(pFile)
        {
            fputs(pSTR_output, pFile);
            if(fclose(pFile))
                bDone = FALSE;
        }
        m_Lock.UnLock();

        return bDone;
    }

    BOOL CLogWriter::mFN_WriteLog__UNICODE(BOOL _bWriteTime, LPCWSTR _szText)
    {
        if(!m_bEncoding_is_UniCode)
            return FALSE;

        WCHAR szTemp[gc_LenTempStrBuffer__LogWrite];
        szTemp[0] = L'\0';

        LPCWSTR pSTR_output;
        if(_bWriteTime)
        {
            SYSTEMTIME st;
            GetLocalTime(&st);
            swprintf_s(szTemp, L"[%d-%02d-%02d %02d:%02d:%02d] %s"
                , st.wYear, st.wMonth, st.wDay
                , st.wHour, st.wMinute, st.wSecond
                , _szText);
            pSTR_output = szTemp;
        }
        else
        {
            pSTR_output = _szText;
        }

        m_Lock.Lock();
        FILE* pFile = mFN_OpenFile();
        BOOL bDone  = (pFile)? TRUE : FALSE;
        if(pFile)
        {
            fputws(pSTR_output, pFile);
            if(fclose(pFile))
                bDone = FALSE;
        }
        m_Lock.UnLock();

        return bDone;
    }

    FILE* CLogWriter::mFN_OpenFile()
    {
        FILE* pFile;
        if(m_bEncoding_is_UniCode)
            _tfopen_s(&pFile, m_strPath, _T("at, ccs=UNICODE"));
        else
            _tfopen_s(&pFile, m_strPath, _T("at"));

        return pFile;
    }

    void CLogWriter::mFN_WriteLog__Time_StartEnd(BOOL isStart)
    {
        if(isStart)
        {
            if(!m_nLimit_WriteLog)
            {
                mFN_WriteLog_Internal(TRUE, _T("Begin Log - Unlimited\n"));
            }
            else
            {
                TCHAR szTemp[256];
                _stprintf_s(szTemp, _T("Begin Log - Limited(%Iu)\n"), m_nLimit_WriteLog);
                mFN_WriteLog_Internal(TRUE, szTemp);
            }
        }
        else
        {
            if(!m_nLimit_WriteLog)
            {
                mFN_WriteLog_Internal(TRUE, _T("End Log\n"));
            }
            else
            {
                TCHAR szTemp[256];
                _stprintf_s(szTemp, _T("End Log - WriteCount(%Iu)\n"), m_Counting_WriteLog);
                mFN_WriteLog_Internal(TRUE, szTemp);
            }
        }
    }
    DECLSPEC_NOINLINE void CLogWriter::mFN_Stop_WriteLog()
    {
        if(FALSE == InterlockedExchange((LONG*)&m_bStopWriteLog, (LONG)TRUE))
            mFN_WriteLog_Internal(TRUE, _T("Stop Log\n"));

        if(m_Counting_WriteLog != m_nLimit_WriteLog)
            ::InterlockedExchange(&m_Counting_WriteLog, m_nLimit_WriteLog);
    }


};