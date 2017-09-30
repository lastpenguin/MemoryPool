#include "../../Output/Engine_include.h"
#include "../../Output/Engine_import.h"

#pragma warning(disable: 4101)
#pragma warning(disable: 4717)
::ENGINE::CEngine engine;
#define MiniDump CMiniDump

void test0()
{
    const BOOL bExitProgram = 1;
    MiniDump::ForceDump(bExitProgram);
}
void test1()
{
    int* p = nullptr;
    *p = 0;
}
void test2()
{
    void* p = _alloca(1024);
    test2();
}
void test3()
{
    for(;;)
    {
        const size_t nSize = 1024 * 1024 * 64; // 64MB
        void* p = new char[nSize];
        printf("0x%p = %s\n", p,
            (p)? "SUCCEED" : "FAILED");
    }
}
void test()
{
    void(*pArray[])(void) = 
    {
        test0, test1, test2, test3,
    };

    printf("Seletect Test\n"
        "[0] ForceDumpWrite\n"
        "[1] Invalid Pointer Access\n"
        "[2] Stack Overflow\n"
        "[3] Fail new\n"
    );
    int iTest;
    scanf_s("%d", &iTest);

    const int MaxValidTestCase = _MACRO_ARRAY_COUNT(pArray);
    if(iTest < 0 || MaxValidTestCase <= iTest)
        return;

    pArray[iTest]();

    ::system("pause");
}