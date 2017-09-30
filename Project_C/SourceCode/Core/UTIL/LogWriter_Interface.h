#pragma once
namespace UTIL{
    __interface ILogWriter{
        void mFN_WriteLog(BOOL _bWriteTime, LPCSTR _szText);
        void mFN_WriteLog(BOOL _bWriteTime, LPCWSTR _szText);

        LPCTSTR mFN_Get_LogPath() const;
    };

    struct TLogWriter_Control{
        DECLSPEC_NOINLINE static void sFN_WriteLog(ILogWriter* pLog, BOOL bWriteTime, LPCSTR fmt, ...)
        {
            if(!pLog)
                return;

            CHAR szText[::UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szText) - 2;

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            if(c_MaxLen < _vscprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return;
            }
            int cntLen = vsprintf_s(szText, _MACRO_ARRAY_COUNT(szText), fmt, vaArglist);
            va_end(vaArglist);

            // 개행 문자 추가
            if(0 < cntLen && szText[cntLen-1] != '\n')
                sprintf_s(szText+cntLen, _MACRO_ARRAY_COUNT(szText)-cntLen, "\r\n");

            pLog->mFN_WriteLog(bWriteTime, szText);
        }
        DECLSPEC_NOINLINE static void sFN_WriteLog(ILogWriter* pLog, BOOL bWriteTime, LPCWSTR fmt, ...)
        {
            if(!pLog)
                return;

            WCHAR szText[::UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szText) - 2;

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            if(c_MaxLen < _vscwprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return;
            }
            int cntLen = vswprintf_s(szText, _MACRO_ARRAY_COUNT(szText), fmt, vaArglist);
            va_end(vaArglist);

            // 개행 문자 추가
            if(0 < cntLen && szText[cntLen-1] != L'\n')
                swprintf_s(szText+cntLen, _MACRO_ARRAY_COUNT(szText)-cntLen, L"\r\n");

            pLog->mFN_WriteLog(bWriteTime, szText);
        }
        DECLSPEC_NOINLINE static void sFN_WriteLog_Outputdebug(ILogWriter* pLog, BOOL bWriteTime, BOOL bAlwaysOutputDebug, LPCSTR fmt, ...)
        {
            CHAR szText[::UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szText) - 2;

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            if(c_MaxLen < _vscprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return;
            }
            int cntLen = vsprintf_s(szText, _MACRO_ARRAY_COUNT(szText), fmt, vaArglist);
            va_end(vaArglist);

            // 개행 문자 추가
            if(0 < cntLen && szText[cntLen-1] != '\n')
                sprintf_s(szText+cntLen, _MACRO_ARRAY_COUNT(szText)-cntLen, "\r\n");
            
            if(bAlwaysOutputDebug)
                _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(szText);
            else
                _MACRO_OUTPUT_DEBUG_STRING(szText);

            if(pLog)
                pLog->mFN_WriteLog(bWriteTime, szText);
        }
        DECLSPEC_NOINLINE static void sFN_WriteLog_Outputdebug(ILogWriter* pLog, BOOL bWriteTime, BOOL bAlwaysOutputDebug, LPCWSTR fmt, ...)
        {

            WCHAR szText[::UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szText) - 2;

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            if(c_MaxLen < _vscwprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return;
            }
            int cntLen = vswprintf_s(szText, _MACRO_ARRAY_COUNT(szText), fmt, vaArglist);
            va_end(vaArglist);

            // 개행 문자 추가
            if(0 < cntLen && szText[cntLen-1] != L'\n')
                swprintf_s(szText+cntLen, _MACRO_ARRAY_COUNT(szText)-cntLen, L"\r\n");

            if(bAlwaysOutputDebug)
                _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(szText);
            else
                _MACRO_OUTPUT_DEBUG_STRING(szText);

            if(pLog)
                pLog->mFN_WriteLog(bWriteTime, szText);
        }
        DECLSPEC_NOINLINE static void sFN_WriteLog_Trace(ILogWriter* pLog, BOOL bWriteTime, LPCSTR _File, int _Line, LPCSTR fmt, ...)
        {
            CHAR szText[::UTIL::SETTING::gc_LenTempStrBuffer];
            szText[0] = '\0';
            
            va_list vaArglist;
            va_start(vaArglist, fmt);
            int cntLen = ::UTIL::TUTIL::sFN_Sprintf_With_Trace(_File, _Line, szText, fmt, vaArglist);
            va_end(vaArglist);

            // 개행 문자 추가
            if(0 < cntLen && szText[cntLen-1] != '\n')
                sprintf_s(szText+cntLen, _MACRO_ARRAY_COUNT(szText)-cntLen, "\r\n");
            _MACRO_OUTPUT_DEBUG_STRING(szText);

            // TRACE 제외
            LPCSTR pText = strstr(szText, "): ");
            if(pText && pLog)
                pLog->mFN_WriteLog(bWriteTime, pText+3);
        }
        DECLSPEC_NOINLINE static void sFN_WriteLog_Trace(ILogWriter* pLog, BOOL bWriteTime, LPCSTR _File, int _Line, LPCWSTR fmt, ...)
        {
            WCHAR szText[::UTIL::SETTING::gc_LenTempStrBuffer];
            szText[0] = L'\0';

            va_list vaArglist;
            va_start(vaArglist, fmt);
            int cntLen = ::UTIL::TUTIL::sFN_Sprintf_With_Trace(_File, _Line, szText, fmt, vaArglist);
            va_end(vaArglist);

            // 개행 문자 추가
            if(0 < cntLen && szText[cntLen-1] != L'\n')
                swprintf_s(szText+cntLen, _MACRO_ARRAY_COUNT(szText)-cntLen, L"\r\n");
            _MACRO_OUTPUT_DEBUG_STRING(szText);

            // TRACE 제외
            LPCWSTR pText = wcsstr(szText, L"): ");
            if(pText && pLog)
                pLog->mFN_WriteLog(bWriteTime, pText+3);
        }
        DECLSPEC_NOINLINE static void sFN_WriteLog_Mesagebox(ILogWriter* pLog, BOOL bWriteTime, UINT icon, LPCSTR fmt, ...)
        {
            _Assert(icon == MB_ICONINFORMATION || icon == MB_ICONWARNING || icon == MB_ICONERROR);

            switch(icon)
            {
                case MB_ICONWARNING:
                case MB_ICONERROR:
                case MB_ICONINFORMATION:
                    break;
                default:
                    icon = MB_ICONINFORMATION;
            }

            CHAR szText[::UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szText) - 2;

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            if(c_MaxLen < _vscprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return;
            }
            int cntLen = vsprintf_s(szText, _MACRO_ARRAY_COUNT(szText), fmt, vaArglist);
            va_end(vaArglist);

            // 개행 문자 추가
            if(0 < cntLen && szText[cntLen-1] != '\n')
                sprintf_s(szText+cntLen, _MACRO_ARRAY_COUNT(szText)-cntLen, "\r\n");

            if(pLog)
                pLog->mFN_WriteLog(bWriteTime, szText);
            ::MessageBoxA(0, szText, nullptr, icon | MB_TASKMODAL | MB_TOPMOST);
        }
        DECLSPEC_NOINLINE static void sFN_WriteLog_Mesagebox(ILogWriter* pLog, BOOL bWriteTime, UINT icon, LPCWSTR fmt, ...)
        {
            _Assert(icon == MB_ICONINFORMATION || icon == MB_ICONWARNING || icon == MB_ICONERROR);

            switch(icon)
            {
            case MB_ICONWARNING:
            case MB_ICONERROR:
            case MB_ICONINFORMATION:
                break;
            default:
                icon = MB_ICONINFORMATION;
            }

            WCHAR szText[::UTIL::SETTING::gc_LenTempStrBuffer];
            const int c_MaxLen = _MACRO_ARRAY_COUNT(szText) - 2;

            va_list	vaArglist;
            va_start(vaArglist, fmt);
            if(c_MaxLen < _vscwprintf(fmt, vaArglist))
            {
                _DebugBreak("not enough stack size");
                return;
            }
            int cntLen = vswprintf_s(szText, _MACRO_ARRAY_COUNT(szText), fmt, vaArglist);
            va_end(vaArglist);

            // 개행 문자 추가
            if(0 < cntLen && szText[cntLen-1] != L'\n')
                swprintf_s(szText+cntLen, _MACRO_ARRAY_COUNT(szText)-cntLen, L"\r\n");

            if(pLog)
                pLog->mFN_WriteLog(bWriteTime, szText);
            ::MessageBoxW(0, szText, nullptr, icon | MB_TASKMODAL | MB_TOPMOST);
        }
    };
}

// 다음 인스턴스 포인터를 프로젝트의 해당되는 변수명으로 정의하십시오
//      _LOG_SYSTEM_INSTANCE_POINTER
//      _LOG_DEBUG__INSTANCE_POINTER

#define _LOG_SYSTEM(...)                ::UTIL::TLogWriter_Control::sFN_WriteLog(_LOG_SYSTEM_INSTANCE_POINTER, FALSE, __VA_ARGS__)
#define _LOG_DEBUG(...)                 ::UTIL::TLogWriter_Control::sFN_WriteLog(_LOG_DEBUG__INSTANCE_POINTER, FALSE, __VA_ARGS__)
#define _LOG_SYSTEM__WITH__TIME(...)    ::UTIL::TLogWriter_Control::sFN_WriteLog(_LOG_SYSTEM_INSTANCE_POINTER, TRUE, __VA_ARGS__)
#define _LOG_DEBUG__WITH__TIME(...)     ::UTIL::TLogWriter_Control::sFN_WriteLog(_LOG_DEBUG__INSTANCE_POINTER, TRUE, __VA_ARGS__)
#define _LOG_SYSTEM__WITH__OUTPUTDEBUGSTR(bWriteTime, ...) ::UTIL::TLogWriter_Control::sFN_WriteLog_Outputdebug(_LOG_SYSTEM_INSTANCE_POINTER, bWriteTime, FALSE, __VA_ARGS__)
#define _LOG_DEBUG__WITH__OUTPUTDEBUGSTR(bWriteTime, ...)  ::UTIL::TLogWriter_Control::sFN_WriteLog_Outputdebug(_LOG_DEBUG__INSTANCE_POINTER, bWriteTime, FALSE, __VA_ARGS__)
#define _LOG_SYSTEM__WITH__OUTPUTDEBUGSTR_ALWAYS(bWriteTime, ...) ::UTIL::TLogWriter_Control::sFN_WriteLog_Outputdebug(_LOG_SYSTEM_INSTANCE_POINTER, bWriteTime, TRUE, __VA_ARGS__)
#define _LOG_DEBUG__WITH__OUTPUTDEBUGSTR_ALWAYS(bWriteTime, ...)  ::UTIL::TLogWriter_Control::sFN_WriteLog_Outputdebug(_LOG_DEBUG__INSTANCE_POINTER, bWriteTime, TRUE, __VA_ARGS__)
#define _LOG_SYSTEM__WITH__MESSAGEBOX(bWriteTime, MB_ICON, ...) ::UTIL::TLogWriter_Control::sFN_WriteLog_Mesagebox(_LOG_SYSTEM_INSTANCE_POINTER, bWriteTime, MB_ICON, __VA_ARGS__)
#define _LOG_DEBUG__WITH__MESSAGEBOX(bWriteTime, MB_ICON, ...) ::UTIL::TLogWriter_Control::sFN_WriteLog_Mesagebox(_LOG_DEBUG__INSTANCE_POINTER, bWriteTime, MB_ICON, __VA_ARGS__)


#ifdef _DEBUG
#define _LOG_SYSTEM__WITH__TRACE(bWriteTime, ...)   ::UTIL::TLogWriter_Control::sFN_WriteLog_Trace(_LOG_SYSTEM_INSTANCE_POINTER, bWriteTime, __FILE__, __LINE__, __VA_ARGS__)
#define _LOG_DEBUG__WITH__TRACE(bWriteTime, ...)    ::UTIL::TLogWriter_Control::sFN_WriteLog_Trace(_LOG_DEBUG__INSTANCE_POINTER, bWriteTime, __FILE__, __LINE__, __VA_ARGS__)
#else
#define _LOG_SYSTEM__WITH__TRACE(bWriteTime, ...)   ::UTIL::TLogWriter_Control::sFN_WriteLog(_LOG_SYSTEM_INSTANCE_POINTER, bWriteTime, __VA_ARGS__)
#define _LOG_DEBUG__WITH__TRACE(bWriteTime, ...)    ::UTIL::TLogWriter_Control::sFN_WriteLog(_LOG_DEBUG__INSTANCE_POINTER, bWriteTime, __VA_ARGS__)
#endif