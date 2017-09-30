class CDLL_Opener : CUnCopyAble{
public:
    CDLL_Opener();
    ~CDLL_Opener();

    BOOL mFN_Load_Library(LPCSTR szFileName);
    BOOL mFN_Load_Library(LPWSTR szFileName);
    BOOL mFN_Free_Library();
    FARPROC mFN_Get_ProcAddress(LPCSTR szProcName);

private:
    HMODULE m_hDLL;
};

CDLL_Opener::CDLL_Opener()
    : m_hDLL(NULL)
{
}
CDLL_Opener::~CDLL_Opener()
{
    mFN_Free_Library();
}
BOOL CDLL_Opener::mFN_Load_Library(LPCSTR szFileName)
{
    _Assert(!m_hDLL);
    if(m_hDLL) return FALSE;
    m_hDLL =::LoadLibraryA(szFileName);
    return (m_hDLL)? TRUE : FALSE;
}
BOOL CDLL_Opener::mFN_Load_Library(LPWSTR szFileName)
{
    _Assert(!m_hDLL);
    if(m_hDLL) return FALSE;
    m_hDLL =::LoadLibraryW(szFileName);
    return (m_hDLL)? TRUE : FALSE;
}
BOOL CDLL_Opener::mFN_Free_Library()
{
    if(!m_hDLL) return FALSE;
    return ::FreeLibrary(m_hDLL);
}
FARPROC CDLL_Opener::mFN_Get_ProcAddress(LPCSTR szProcName)
{
    if(!m_hDLL) return NULL;
    return ::GetProcAddress(m_hDLL, szProcName);
}