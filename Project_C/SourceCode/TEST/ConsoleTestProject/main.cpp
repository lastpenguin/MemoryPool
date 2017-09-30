#include "main.h"


#include "./test_memorypool.h"
//#include "./test_memorypool_std_allocator.h"
//#include "./test_MiniDumpWrite.h"


int main(int argc, char **argv)
{
    ::SetThreadAffinityMask(::GetCurrentThread(), 1);
    test();
    return 0;
}