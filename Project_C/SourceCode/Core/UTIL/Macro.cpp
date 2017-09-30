#include "stdafx.h"
#include "./Macro.h"

#include "./MemoryPool_Interface.h"


namespace UTIL{

#pragma region 문자열 처리
    size_t TUTIL::mFN_strlen(LPCSTR _str)
    {
        return ::strlen(_str);
    }
    size_t TUTIL::mFN_strlen(LPCWSTR _str)
    {
        return ::wcslen(_str);
    }

    /*----------------------------------------------------------------
    /       문자열을 카피합니다.
    /       Dest의 버퍼를 할당후, 문자열을 복사합니다.
    ----------------------------------------------------------------*/
    void TUTIL::mFN_Make_Clone__String(LPSTR&  _pDest, LPCSTR  _pSorce)
    {
        if(!_pSorce)
            return;
        size_t _Len = strlen(_pSorce) + 1;
        _SAFE_NEW_ARRAY(_pDest, CHAR, _Len);
        memcpy(_pDest, _pSorce, sizeof(CHAR) * _Len);
    }
    void TUTIL::mFN_Make_Clone__String(LPSTR&  _pDest, LPCWSTR _pSorce)
    {
        if(!_pSorce)
            return;
        int _Len = WideCharToMultiByte(CP_ACP, 0
            , _pSorce, -1
            , NULL, 0
            , NULL, FALSE);
        _SAFE_NEW_ARRAY(_pDest, CHAR, _Len);
        WideCharToMultiByte(CP_ACP, 0
            , _pSorce, -1
            , _pDest, _Len
            , NULL, FALSE);
    }
    void TUTIL::mFN_Make_Clone__String(LPWSTR& _pDest, LPCSTR  _pSorce)
    {
        if(!_pSorce)
            return;
        int _Len = MultiByteToWideChar(CP_ACP, 0
            , _pSorce, -1
            , NULL, 0);
        _SAFE_NEW_ARRAY(_pDest, WCHAR, _Len);
        MultiByteToWideChar(CP_ACP, 0
            , _pSorce, -1
            , _pDest, _Len);
    }
    void TUTIL::mFN_Make_Clone__String(LPWSTR& _pDest, LPCWSTR _pSorce)
    {
        if(!_pSorce)
            return;
        size_t _Len = wcslen(_pSorce) + 1;
        _SAFE_NEW_ARRAY(_pDest, WCHAR, _Len);
        memcpy(_pDest, _pSorce, sizeof(WCHAR) * _Len);
    }
    /*----------------------------------------------------------------
    /       문자열을 카피합니다.(메모리풀 버전)
    ----------------------------------------------------------------*/
    void TUTIL::mFN_Make_Clone__String__From_MemoryPool(LPSTR&  _pDest, LPCSTR  _pSorce)
    {
        if(_pDest || !_pSorce)
        {
            _Error(_T("Invalid Parameter"));
            return;
        }

        size_t _Len = strlen(_pSorce) + 1;
#if _DEF_USING_DEBUG_MEMORY_LEAK
        _pDest = (LPSTR)g_pMem->mFN_Get_Memory(sizeof(CHAR)*_Len, __FILE__, __LINE__);
#else
        _pDest = (LPSTR)g_pMem->mFN_Get_Memory(sizeof(CHAR)*_Len);
#endif

        memcpy(_pDest, _pSorce, sizeof(CHAR) * _Len);
    }
    void TUTIL::mFN_Make_Clone__String__From_MemoryPool(LPSTR&  _pDest, LPCWSTR _pSorce)
    {
        if(_pDest || !_pSorce)
        {
            _Error(_T("Invalid Parameter"));
            return;
        }

        int _Len = WideCharToMultiByte(CP_ACP, 0
            , _pSorce, -1
            , NULL, 0
            , NULL, FALSE);
#if _DEF_USING_DEBUG_MEMORY_LEAK
        _pDest = (LPSTR)g_pMem->mFN_Get_Memory(sizeof(CHAR)*_Len, __FILE__, __LINE__);
#else
        _pDest = (LPSTR)g_pMem->mFN_Get_Memory(sizeof(CHAR)*_Len);
#endif

        WideCharToMultiByte(CP_ACP, 0
            , _pSorce, -1
            , _pDest, _Len
            , NULL, FALSE);
    }
    void TUTIL::mFN_Make_Clone__String__From_MemoryPool(LPWSTR& _pDest, LPCSTR  _pSorce)
    {
        if(_pDest || !_pSorce)
        {
            _Error(_T("Invalid Parameter"));
            return;
        }

        int _Len = MultiByteToWideChar(CP_ACP, 0
            , _pSorce, -1
            , NULL, 0);
#if _DEF_USING_DEBUG_MEMORY_LEAK
        _pDest = (LPWSTR)g_pMem->mFN_Get_Memory(sizeof(WCHAR)*_Len, __FILE__, __LINE__);
#else
        _pDest = (LPWSTR)g_pMem->mFN_Get_Memory(sizeof(WCHAR)*_Len);
#endif

        MultiByteToWideChar(CP_ACP, 0
            , _pSorce, -1
            , _pDest, _Len);
    }
    void TUTIL::mFN_Make_Clone__String__From_MemoryPool(LPWSTR& _pDest, LPCWSTR _pSorce)
    {
        if(_pDest || !_pSorce)
        {
            _Error(_T("Invalid Parameter"));
            return;
        }

        size_t _Len = wcslen(_pSorce) + 1;
#if _DEF_USING_DEBUG_MEMORY_LEAK
        _pDest = (LPWSTR)g_pMem->mFN_Get_Memory(sizeof(WCHAR)*_Len, __FILE__, __LINE__);
#else
        _pDest = (LPWSTR)g_pMem->mFN_Get_Memory(sizeof(WCHAR)*_Len);
#endif

        memcpy(_pDest, _pSorce, sizeof(WCHAR) * _Len);
    }
#pragma endregion

#pragma region Windows / Thread
    /*----------------------------------------------------------------
    /   루트 윈도우핸들을 구하는 함수
    /       차일드 속성을 가지지 않는 가장 가까운 상위의 부모윈도우를 찾습니다.
    /       _bNonChild  - TRUE  : 차일드속성이 아닌 가까운 상위 윈도우
    /                   - FALSE : 최상위의 윈도우
    ----------------------------------------------------------------*/
    HWND TUTIL::mFN_GetRootWindow(HWND _hWnd, BOOL _bNonChild)
    {
        assert(_hWnd);

        LONG _Style;
        if(_bNonChild)
        {
            _Style = GetWindowLong(_hWnd, GWL_STYLE);
            if(!(_Style & WS_CHILD))
                return _hWnd;
        }

        HWND _hWndNow    = _hWnd;
        HWND _hWndParent = GetParent(_hWndNow);
        while(_hWndParent)
        {
            if(_bNonChild)
            {

                _Style = GetWindowLong(_hWndParent, GWL_STYLE);
                if(!(_Style & WS_CHILD))
                {
                    _hWndNow = _hWndParent;
                    break;
                }
            }

            _hWndNow    = _hWndParent;
            _hWndParent = GetParent(_hWndNow);
        }

        return _hWndNow;
    }
    /*----------------------------------------------------------------
    /   윈도우의 클라이언트 영역 크기를 변경합니다.
    ----------------------------------------------------------------*/
    //BOOL TUTIL::mFN_Set_ClientSize(HWND _hWnd, LONG _Width, LONG _Height)
    //{
    //    if(!_hWnd)
    //        return FALSE;

    //    RECT rectC, rectW;
    //    if(!GetClientRect(_hWnd, &rectC))
    //        return FALSE;

    //    const LONG LocalW = rectC.right;
    //    const LONG LocalH = rectC.bottom;
    //    if(LocalW == _Width && LocalH == _Height)
    //        return TRUE;



    //    if(!GetWindowRect(_hWnd, &rectW))
    //        return FALSE;

    //    const LONG WinW = rectW.right - rectW.left;
    //    const LONG WinH = rectW.bottom - rectW.top;

    //    const LONG BorderW = WinW - LocalW;
    //    const LONG BorderH = WinH - LocalH;
    //    const LONG NewW = _Width + BorderW;
    //    const LONG NewH = _Height + BorderH;

    //    if(!::SetWindowPos(_hWnd, NULL, 0, 0
    //        , NewW
    //        , NewH
    //        , SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSENDCHANGING))
    //        return FALSE;


    //    GetClientRect(_hWnd, &rectC);
    //    if(rectC.right != _Width || rectC.bottom != _Height)
    //        return FALSE;
    //    return TRUE;
    //}
    BOOL TUTIL::mFN_Set_ClientSize(HWND _hWnd, LONG _Width, LONG _Height)
    {
        if(!_hWnd)
            return FALSE;

        RECT rect;
        if(!GetClientRect(_hWnd, &rect))
            return FALSE;
        if(rect.right == _Width && rect.top == _Height)
            return TRUE;

        rect.left = rect.top = 0;
        rect.bottom = _Width;
        rect.right  = _Height;

        DWORD style = ::GetWindowLong(_hWnd, GWL_STYLE);
        DWORD styleEx = ::GetWindowLong(_hWnd, GWL_EXSTYLE);
        BOOL bMenu = GetMenu(_hWnd)? TRUE : FALSE;

        ::AdjustWindowRectEx(&rect, style, bMenu, styleEx);
        ::SetWindowPos(_hWnd, 0, 0, 0, rect.right-rect.left, rect.bottom-rect.top, SWP_NOMOVE);

        #ifdef _DEBUG
        ::GetClientRect(_hWnd, &rect);
        _Assert(rect.right-rect.top == _Width && rect.bottom-rect.top == _Height);
        #endif

        return TRUE;
    }

    /*----------------------------------------------------------------
    /   WinProc을 변경합니다. (Return: 성공 / 실패)
    /       _pFN_After      : 새로이 설정될 WINPROC
    /       _pFN_Before     : 이전에 사용되던 WINPROC 조사
    ----------------------------------------------------------------*/
    BOOL TUTIL::mFN_Change_WinProc(HWND _hWnd, WNDPROC _pFN_After, WNDPROC* _pFN_Before)
    {
        assert(_hWnd && _pFN_After);

        if(!_hWnd || !_pFN_After)
            return FALSE;


        LONG_PTR __long = ::SetWindowLongPtr(_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(_pFN_After));
        if(0 == __long)
            return FALSE;

        *_pFN_Before = reinterpret_cast<WNDPROC>(__long);
        return TRUE;
    }
    /*----------------------------------------------------------------
    /       스레드 이름변경
    /   출처: http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vsdebug/html/vxtsksettingthreadname.asp
    ----------------------------------------------------------------*/
    void TUTIL::mFN_SetThreadName(DWORD _ThreadID, LPCSTR _ThreadName)
    {
#pragma pack(push, 4)
        typedef struct {
            DWORD   dwType;
            LPCSTR  szName;
            DWORD   dwThreadID;
            DWORD   dwFlags;
        } THREADNAME_INFO;
#pragma pack(pop)

        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = _ThreadName;
        info.dwThreadID = _ThreadID;
        info.dwFlags = 0;

        __try
        {
            ::RaiseException(0x406d1388, 0,
                sizeof(THREADNAME_INFO) / sizeof(DWORD),
                reinterpret_cast<ULONG_PTR*>(&info));
        }
        //__except(EXCEPTION_CONTINUE_EXECUTION)
        __except(GetExceptionCode()== EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
        }
    }
#pragma endregion

#pragma region 기타
    // 프로젝트 컴파일 시간을 얻는다(빌드날짜 시간)
    TSystemTime TUTIL::mFN_GetCurrentProjectCompileTime(LPCSTR in__DATE__, LPCSTR in_TIME__)
    {
#pragma pack(push, 4)
        const char c_Months[] ={"Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec End"};
        const DWORD32* pM = reinterpret_cast<const DWORD32*>(&c_Months[0]);
        struct _DateField{
            DWORD32  m;      // 3 + 1
            char     d[2+1];
            char     y[4+1];
        };
        struct _TimeField{
            char    h[2+1];
            char    m[2+1];
            char    s[2+1];
        };
#pragma pack(pop)
        const _DateField* pD = reinterpret_cast<const _DateField*>(in__DATE__);
        const _TimeField* pT = reinterpret_cast<const _TimeField*>(in_TIME__);

        WORD m=0;
        while(m<12)
        {
            if(pD->m == pM[m++])
                break;
        }

        const TSystemTime R ={atoi(pD->y), m, atoi(pD->d), atoi(pT->h), atoi(pT->m), atoi(pT->s)};
        return R;
    }
#pragma warning(push)
#pragma warning(disable: 4996)
    namespace{
        const UINT32 gc_precomputed4bit[] = 
        {
            0,
            1, 1, 2, 1, 2,
            2, 3, 1, 2, 2,
            3, 2, 3, 3, 4,
        };
    }
    // on bit counting
    // 다음의 POPCNT 내장함수가 존재한다(하지만 이것은 인텔의 경우 네할렘 이상급에서 사용된다)
    //      __popcnt __popcnt16 __popcnt64 _mm_popcnt_u32 _mm_popcnt_u64
    UINT32 TUTIL::mFN_BitCount(const UINT8* pData, size_t nByte) const
    {
        UINT32 cnt = 0;
        for(size_t i=0; i<nByte; i++)
        {
            const byte& val = *pData;
            cnt += gc_precomputed4bit[val & 0x0f];
            cnt += gc_precomputed4bit[(val>>4) & 0x0f];
            pData++;
        }
        return cnt;
    }
    UINT32 TUTIL::mFN_BitCount(UINT8 val) const
    {
        return gc_precomputed4bit[val & 0x0f]
            + gc_precomputed4bit[(val>>4) & 0x0f];
    }
#define _LOCAL_MACRO_BITCOUNT16(v, i) gc_precomputed4bit[v >> (16*i+0) & 0x0f] + gc_precomputed4bit[v >> (16*i+4) & 0x0f]\
    + gc_precomputed4bit[v >> (16*i+8) & 0x0f] + gc_precomputed4bit[v >> (16*i+12) & 0x0f]

    UINT32 TUTIL::mFN_BitCount(UINT16 val) const
    {
        return _LOCAL_MACRO_BITCOUNT16(val, 0);
    }
    UINT32 TUTIL::mFN_BitCount(UINT32 val) const
    {
        return _LOCAL_MACRO_BITCOUNT16(val, 0) + _LOCAL_MACRO_BITCOUNT16(val, 1);
    }
    UINT32 TUTIL::mFN_BitCount(UINT64 val) const
    {
        return _LOCAL_MACRO_BITCOUNT16(val, 0) + _LOCAL_MACRO_BITCOUNT16(val, 1)
            + _LOCAL_MACRO_BITCOUNT16(val, 2) + _LOCAL_MACRO_BITCOUNT16(val, 3);
    }
#undef _LOCAL_MACRO_BITCOUNT16
    /*----------------------------------------------------------------
    /   지정된 메모리 비교
    ----------------------------------------------------------------*/
    namespace{
    #pragma warning(push)
    #pragma warning(disable: 6011)
    #pragma warning(disable: 28182)
    #pragma pack(push, 1)
        BOOL gFN_MemoryCompare_Under_SIZET(const BYTE* p, size_t n, BYTE code)
        {
            _CompileHint(p && n<sizeof(size_t));

            union{
                size_t data = 0;
                BYTE code[sizeof(size_t)];
            }val_sizet, code_sizet;

            for(size_t i=0; i<n; i++)
            {
                val_sizet.code[i] = p[i];
                code_sizet.code[i] = code;
            }

            return val_sizet.data == code_sizet.data;
        }
        BOOL gFN_MemoryCompare_nSIZET(const size_t* pSource, size_t SizeSource, size_t code)
        {
            _Assert(pSource && SizeSource);
        #ifdef _DEBUG
            const size_t c_mask_one_under_SizeT = sizeof(size_t)-1;
            _Assert(!(SizeSource & c_mask_one_under_SizeT));
        #endif

        #if __X86
            const size_t c_code = code | (code<<8) |  (code<<16) | (code<<24);
        #elif __X64
            const size_t c_code = code | (code<<8) |  (code<<16) | (code<<24) | (code<<32) | (code<<40) | (code<<48) | (code<<56);
        #endif

            const size_t* pC = pSource;
            const size_t* pEND = reinterpret_cast<const size_t*>((const BYTE*)pSource + SizeSource);
            do{
                if(*pC != c_code)
                    return FALSE;
            } while(++pC != pEND);
            return TRUE;
        }
        BOOL gFN_MemoryCompare_Tiny(const void* pSource, size_t SizeSource, unsigned char code)
        {
            _Assert(pSource && SizeSource);
            const BYTE* pC = (const BYTE*)pSource;
            const BYTE* pE = pC + SizeSource;

            const size_t c_mask_zero_under_SizeT = ~(sizeof(size_t)-1);
            const size_t c_Size_from_ValidUnits = SizeSource & c_mask_zero_under_SizeT;
            if(c_Size_from_ValidUnits)
            {
                if(!gFN_MemoryCompare_nSIZET((const size_t*)pC, c_Size_from_ValidUnits, code))
                    return FALSE;
                const BYTE* pE_SIZET = pC + c_Size_from_ValidUnits;
                if(pE_SIZET == pE)
                    return TRUE;
                pC = pE_SIZET;
                SizeSource = pE - pE_SIZET;
            }

            return gFN_MemoryCompare_Under_SIZET(pC, SizeSource, code);
        }
        // 16바이트에 정렬된 주소, 데이터는 16바이트 배수
        BOOL gFN_MemoryCompare_Aligned_128BIT(const void* pSource, size_t SizeSource, size_t code)
        {
            _Assert(pSource && SizeSource);
            const size_t c_mask_one_under_16BYTE = 16-1;
            const size_t c_mask_zero_under_16BYTE = ~((size_t)16-1);
            _Assert(!(SizeSource & c_mask_one_under_16BYTE));

        #if __X86
            const size_t c_code = code | (code<<8) |  (code<<16) | (code<<24);
        #elif __X64
            const size_t c_code = code | (code<<8) |  (code<<16) | (code<<24) | (code<<32) | (code<<40) | (code<<48) | (code<<56);
        #endif

            const size_t* pC = (const size_t*)pSource;
            const size_t* pEND = reinterpret_cast<const size_t*>((const BYTE*)pSource + SizeSource);
            _Assert(!((size_t)pC & c_mask_one_under_16BYTE));
            _Assert(!((size_t)pEND & c_mask_one_under_16BYTE));
            do{
            #if __X86
                const size_t unlike0 = pC[0] ^ c_code;
                const size_t unlike1 = pC[1] ^ c_code;
                const size_t unlike2 = pC[2] ^ c_code;
                const size_t unlike3 = pC[3] ^ c_code;
                if(unlike0 | unlike1 | unlike2 | unlike3)
                    return FALSE;
                pC += 4;
            #elif __X64
                const size_t unlike0 = pC[0] ^ c_code;
                const size_t unlike1 = pC[1] ^ c_code;
                if(unlike0 | unlike1)
                    return FALSE;
                pC += 2;
            #endif
            } while(pC != pEND);
            return TRUE;
        }
        // 16바이트에 정렬된 주소, 데이터는 16바이트 배수
        BOOL gFN_MemoryCompare_Aligned_128BIT_SIMD_SSE2(const void* pSource, size_t SizeSource, unsigned char code)
        {
            // SSE2 명령어 한정(2001년 이후 CPU)
            _Assert(pSource && SizeSource);
            const size_t c_SizeChunk = sizeof(__m128i);
            const size_t c_nChunk = SizeSource / c_SizeChunk;
            _Assert(SizeSource % c_SizeChunk == 0);

            const __m128i c_code = _mm_set1_epi8((char)code);
            const __m128i* p = (const __m128i*)pSource;

            // X 단위로 처리
            const size_t c_Chunk_X = 4;
            const size_t c_SizeChunk_X = c_SizeChunk * c_Chunk_X;
            const size_t c_nChunk_X = SizeSource / c_SizeChunk_X;
            if(0 < c_nChunk_X)
            {
                const __m128i* pEND_N_X = p + (c_Chunk_X*c_nChunk_X);
                do{
                    const __m128i r0 = _mm_xor_si128(p[0], c_code);
                    const __m128i r1 = _mm_xor_si128(p[1], c_code);
                    const __m128i r2 = _mm_xor_si128(p[2], c_code);
                    const __m128i r3 = _mm_xor_si128(p[3], c_code);
                    const __m128i r01 = _mm_or_si128(r0, r1);
                    const __m128i r23 = _mm_or_si128(r2, r3);
                    const __m128i r0123 = _mm_or_si128(r01, r23);
                    if(r0123.m128i_u64[0] | r0123.m128i_u64[1])
                        return FALSE;
                    p += c_Chunk_X;
                }while(p != pEND_N_X);
            }

            // 남은 부분 1개 단위 처리
            const __m128i* pEND_All = reinterpret_cast<const __m128i*>((byte*)pSource + SizeSource);
            while(p != pEND_All)
            {
                const __m128i r = _mm_xor_si128(*p, c_code);
                if(r.m128i_u64[0] | r.m128i_u64[1])
                    return FALSE;
                p++;
            }
            return TRUE;
        }
    #pragma pack(pop)
    #pragma warning(pop)

        BOOL gFN_MemoryCompare_Default(const void* pSource, size_t SizeSource, unsigned char code)
        {
            _Assert(pSource && SizeSource);
            const size_t c_mask_one_under_16BYTE = 16-1;
            const size_t c_mask_zero_under_16BYTE = ~((size_t)16-1);

            const BYTE* pF = (const BYTE*)pSource;
            const BYTE* pE = pF + SizeSource;

            const BYTE* p16F = reinterpret_cast<const BYTE*>((size_t)pF & c_mask_zero_under_16BYTE);
            const BYTE* p16E = reinterpret_cast<const BYTE*>((size_t)pE & c_mask_zero_under_16BYTE);
            if(p16F < pF)
                p16F += 16;

            // 메모리 구조
            // [pF~p16F] [p16F~p16E] [p16E~pE]
            const BOOL bExist_Front = (p16F < p16E);
            const BOOL bExist_Middle = (pF < p16F);
            const BOOL bExist_Back = (p16E < pE);
            if(!bExist_Middle)
            {
                if(!gFN_MemoryCompare_Tiny(pF, SizeSource, code))
                    return FALSE;
                return TRUE;
            }
            //----------------------------------------------------------------
            if(bExist_Front)
            {
                if(!gFN_MemoryCompare_Tiny(pF, p16F - pF, code))
                    return FALSE;
            }
            if(bExist_Middle)
            {
                if(!gFN_MemoryCompare_Aligned_128BIT(p16F, p16E-p16F, code))
                    return FALSE;
            }
            if(bExist_Back)
            {
                if(!gFN_MemoryCompare_Tiny(p16E, pE - p16E, code))
                    return FALSE;
            }
            return TRUE;
        }
        BOOL gFN_MemoryCompare_SIMD_SSE2(const void* pSource, size_t SizeSource, unsigned char code)
        {
            _Assert(pSource && SizeSource);
            const size_t c_mask_one_under_16BYTE = 16-1;
            const size_t c_mask_zero_under_16BYTE = ~((size_t)16-1);

            const BYTE* pF = (const BYTE*)pSource;
            const BYTE* pE = pF + SizeSource;

            const BYTE* p16F = reinterpret_cast<const BYTE*>((size_t)pF & c_mask_zero_under_16BYTE);
            const BYTE* p16E = reinterpret_cast<const BYTE*>((size_t)pE & c_mask_zero_under_16BYTE);
            if(p16F < pF)
                p16F += 16;

            // 메모리 구조
            // [pF~p16F] [p16F~p16E] [p16E~pE]
            const BOOL bExist_Front = (p16F < p16E);
            const BOOL bExist_Middle = (pF < p16F);
            const BOOL bExist_Back = (p16E < pE);
            if(!bExist_Middle)
            {
                if(!gFN_MemoryCompare_Tiny(pF, SizeSource, code))
                    return FALSE;
                return TRUE;
            }
            //----------------------------------------------------------------
            if(bExist_Front)
            {
                if(!gFN_MemoryCompare_Tiny(pF, p16F - pF, code))
                    return FALSE;
            }
            if(bExist_Middle)
            {
                if(!gFN_MemoryCompare_Aligned_128BIT_SIMD_SSE2(p16F, p16E-p16F, code))
                    return FALSE;
            }
            if(bExist_Back)
            {
                if(!gFN_MemoryCompare_Tiny(p16E, pE - p16E, code))
                    return FALSE;
            }
            return TRUE;
        }
    }
    BOOL TUTIL::mFN_MemoryCompare(const void* pSource, size_t SizeSource, unsigned char code) const
    {
        if(!pSource || !SizeSource)
            return FALSE;
        if(SizeSource <= 16)
            return gFN_MemoryCompare_Tiny(pSource, SizeSource, code);
        else
            return gFN_MemoryCompare_Default(pSource, SizeSource, code);
    }
    BOOL TUTIL::mFN_MemoryCompareSSE2(const void* pSource, size_t SizeSource, unsigned char code) const
    {
        if(!pSource || !SizeSource)
            return FALSE;
        if(SizeSource <= 16)
            return gFN_MemoryCompare_Tiny(pSource, SizeSource, code);
        else
            return gFN_MemoryCompare_SIMD_SSE2(pSource, SizeSource, code);
    }

    /*----------------------------------------------------------------
    /   지정된 메모리 상태 문자열출력
    /   pOutputSTR 출력 버퍼가 지정 되지 않는 경우 필요한 버퍼 크기 리턴
    ----------------------------------------------------------------*/
    size_t TUTIL::mFN_MemoryScan(LPSTR pOutputSTR, size_t nOutputLen
        , void* pObj, size_t sizeObj, int nColumn, int sizeUnit)
    {
        _Assert(sizeObj > 0 && nColumn > 0 && sizeUnit > 0);

        // 필요글자: 제목 + 바이트당2, 유닛단위당1(빈칸 또는 줄내림)
        const size_t LenSTR = (sizeObj * 2) + ((sizeObj+sizeUnit-1) / sizeUnit) + 1;
        if(!pOutputSTR || 0 == nOutputLen)
            return LenSTR;
        if(nOutputLen < LenSTR)
        {
            pOutputSTR[0] = '\0';
            return 0;
        }

        LPSTR pSTR = pOutputSTR;
        const byte *p = (const byte*)pObj;
        size_t i=0;
        while(i<sizeObj)
        {
            int j=0;
            for(;;)
            {
                for(int k=0; k<sizeUnit; k++)
                {
                    pSTR += sprintf(pSTR, "%02X", *p++);
                    if(++i==sizeObj)
                        break;
                }
                if(++j == nColumn || i == sizeObj)
                    break;
                *pSTR++ = ' ';
            }
            *pSTR++ = '\n';
        }
        *pSTR = '\0';

        return LenSTR;
    }
    size_t TUTIL::mFN_MemoryScan(LPWSTR pOutputSTR, size_t nOutputLen
        , void* pObj, size_t sizeObj, int nColumn, int sizeUnit)
    {
        _Assert(sizeObj > 0 && nColumn > 0 && sizeUnit > 0);

        // 필요글자: 제목 + 바이트당2, 유닛단위당1(빈칸 또는 줄내림)
        const size_t LenSTR = (sizeObj * 2) + ((sizeObj+sizeUnit-1) / sizeUnit) + 1;
        if(!pOutputSTR || 0 == nOutputLen)
            return LenSTR;
        if(nOutputLen < LenSTR)
        {
            pOutputSTR[0] = L'\0';
            return 0;
        }

        LPWSTR pSTR = pOutputSTR;
        const byte *p = (const byte*)pObj;
        size_t i=0;
        while(i<sizeObj)
        {
            int j=0;
            for(;;)
            {
                for(int k=0; k<sizeUnit; k++)
                {
                    pSTR += swprintf(pSTR, L"%02X", *p++);
                    if(++i==sizeObj)
                        break;
                }
                if(++j == nColumn || i == sizeObj)
                    break;
                *pSTR++ = L' ';
            }
            *pSTR++ = L'\n';
        }
        *pSTR = L'\0';

        return LenSTR;
    }
    size_t TUTIL::mFN_MemoryScanCompareAB(LPSTR pOutputSTR, size_t nOutputLen
        , void* pObjA, size_t sizeObjA, void* pObjB, size_t sizeObjB
        , BOOL bShowDifference, int nColumn, int sizeUnit)
    {
        size_t sizeObj = min(sizeObjA, sizeObjB);
        _Assert(sizeObj > 0 && nColumn > 0 && sizeUnit > 0);

        // 필요글자: 제목 + 바이트당2, 유닛단위당1(빈칸 또는 줄내림)
        const size_t LenSTR = (sizeObj * 2) + ((sizeObj+sizeUnit-1) / sizeUnit) + 1;
        if(!pOutputSTR || 0 == nOutputLen)
            return LenSTR;
        if(nOutputLen < LenSTR)
        {
            pOutputSTR[0] = '\0';
            return 0;
        }

        LPSTR pSTR = pOutputSTR;
        const byte *pA = (const byte *)pObjA;
        const byte *pB = (const byte *)pObjB;
        size_t i=0;
        while(i<sizeObj)
        {
            int j=0;
            for(;;)
            {
                for(int k=0; k<sizeUnit; k++)
                {
                    if(bShowDifference ^ (*pA == *pB)){
                        pSTR += sprintf(pSTR, "%02X", *pA);
                    }
                    else{
                        *pSTR =  *(pSTR+1) = '_';
                        pSTR += 2;
                    }
                    pA++; pB++;
                    if(++i==sizeObj)
                        break;
                }
                if(++j == nColumn || i == sizeObj)
                    break;
                *pSTR++ = ' ';
            }
            *pSTR++ = '\n';
        }
        *pSTR = '\0';

        return LenSTR;
    }
    size_t TUTIL::mFN_MemoryScanCompareAB(LPWSTR pOutputSTR, size_t nOutputLen
        , void* pObjA, size_t sizeObjA, void* pObjB, size_t sizeObjB
        , BOOL bShowDifference, int nColumn, int sizeUnit)
    {
        size_t sizeObj = min(sizeObjA, sizeObjB);
        _Assert(sizeObj > 0 && nColumn > 0 && sizeUnit > 0);

        // 필요글자: 제목 + 바이트당2, 유닛단위당1(빈칸 또는 줄내림)
        const size_t LenSTR = (sizeObj * 2) + ((sizeObj+sizeUnit-1) / sizeUnit) + 1;
        if(!pOutputSTR || 0 == nOutputLen)
            return LenSTR;
        if(nOutputLen < LenSTR)
        {
            pOutputSTR[0] = L'\0';
            return 0;
        }

        LPWSTR pSTR = pOutputSTR;
        const byte *pA = (const byte *)pObjA;
        const byte *pB = (const byte *)pObjB;
        size_t i=0;
        while(i<sizeObj)
        {
            int j=0;
            for(;;)
            {
                for(int k=0; k<sizeUnit; k++)
                {
                    if(bShowDifference ^ (*pA == *pB)){
                        pSTR += swprintf(pSTR, L"%02X", *pA);
                    }
                    else{
                        *pSTR =  *(pSTR+1) = L'_';
                        pSTR += 2;
                    }
                    pA++; pB++;
                    if(++i==sizeObj)
                        break;
                }
                if(++j == nColumn || i == sizeObj)
                    break;
                *pSTR++ = L' ';
            }
            *pSTR++ = L'\n';
        }
        *pSTR = L'\0';

        return LenSTR;
    }
#pragma warning(pop)
#pragma endregion


}