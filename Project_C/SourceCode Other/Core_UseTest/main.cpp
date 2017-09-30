#include "../../Library Public Version/Core Library BETA 2.0/User/Core_import.h"

int main()
{
#if __X86
    ::CORE::Connect_CORE(_T("Core_x86.dll"), nullptr, nullptr);
#elif __X64
    ::CORE::Connect_CORE(_T("Core_x64.dll"), nullptr, nullptr);
#endif

    int *p = nullptr; 
    _MACRO_NEW__FROM_MEMORYPOOL(p, int(10));
    _MACRO_DELETE__FROM_MEMORYPOOL(p);

    system("pause");
}