#include "stdafx.h"
#include "CppUnitTest.h"

#include "../../Core/Core.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace{
    template<size_t _size>
    struct TMemory_Aligned{
        TMemory_Aligned()
        {
            pS = (byte*)::VirtualAlloc(0, _size + (64*1024), MEM_RESERVE, PAGE_NOACCESS);
            Assert::IsTrue(nullptr != pS);
            pBuffer = (byte*)::VirtualAlloc(pS+(4*1024), size, MEM_COMMIT, PAGE_READWRITE);
            Assert::IsTrue(nullptr != pBuffer);
            _MACRO_OUTPUT_DEBUG_STRING_TRACE_ALWAYS(_T("Valid Address: 0x%p ~ 0x%p\n"), pBuffer, pBuffer+size);
        }
        ~TMemory_Aligned()
        {
            if(pS)
                ::VirtualFree(pS, 0, MEM_RELEASE);
        }
        byte* pS = nullptr;
        byte* pBuffer = nullptr;
        const size_t size = _size;
    };
    static TMemory_Aligned<64*1024> g_buffer;

    void gFN_OffMessageCoreDLL_IsLimitedVersion()
    {
        ::CORE::g_instance_CORE.mFN_Set_OnOff_IsLimitedVersion_Messagebox(FALSE);
    }
}
namespace UnitTest_Core_MemoryCompare_BYTE
{		
    TEST_CLASS(메모리비교_BYTE)
    {
    protected:
        decltype(::CORE::g_instance_UTIL)& util = ::CORE::g_instance_UTIL;

        const size_t SizeBuffer = g_buffer.size;
        byte* pBuffer = g_buffer.pBuffer;
        const byte code = 0x32;
    public:
        BOOL SafeCompare(const void* pAddress, size_t size)
        {
            const byte* pS = (const byte*)pAddress;
            const byte* pE = pS + size;
            for(const byte* p = pS; p != pE; p++)
            {
                if(*p != code)
                    return FALSE;
            }
            return TRUE;
        }

        void Test1()
        {
            UINT64 s, e;
            UINT64 tick[64];
            BOOL done[64];

            // 대형
            size_t size = SizeBuffer - 64;
            _MACRO_OUTPUT_DEBUG_STRING_TRACE_ALWAYS(_T("---정렬단위 64~1 : Size %Iu---\n"), size);
            for(size_t i=0; i<64; i++)
            {
                // 주소 정렬 64 ~ 1
                auto p = pBuffer+i; 

                s = __rdtsc();
                done[i] = util.mFN_MemoryCompare(p, size, code);
                e = __rdtsc();
                tick[i] = e - s;

                if(done[i] == SafeCompare(p, size))
                    done[i] = TRUE;
                else
                    done[i] = FALSE;
            }
            for(size_t i=0; i<64; i++)
            {
                _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(_T("Aligned[%2Iu] %s %llu\n")
                    , 64 - i
                    , (done[i])? _T("SUCCEED") : _T("FAILED")
                    , tick[i]);
                Assert::IsTrue(TRUE == done[i]);
            }

            // 중형(유효 주소의 뒷부분으로 테스트)
            size = 256;
            _MACRO_OUTPUT_DEBUG_STRING_TRACE_ALWAYS(_T("---정렬단위 64~1 : Size %Iu---\n"), size);
            for(size_t i=0; i<64; i++)
            {
                // 주소 정렬 1~64
                auto p = pBuffer + SizeBuffer - size - i;

                s = __rdtsc();
                done[i] = util.mFN_MemoryCompare(p, size, code);
                e = __rdtsc();
                tick[i] = e - s;

                if(done[i] == SafeCompare(p, size))
                    done[i] = TRUE;
                else
                    done[i] = FALSE;
            }
            for(size_t i=0; i<64; i++)
            {
                _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(_T("Aligned[%2Iu] %s %llu\n")
                    , 64 - i
                    , (done[i])? _T("SUCCEED") : _T("FAILED")
                    , tick[i]);
                Assert::IsTrue(TRUE == done[i]);
            }

            // 소형(유효 주소의 뒷부분으로 테스트)
            // 64~1
            _MACRO_OUTPUT_DEBUG_STRING_TRACE_ALWAYS(_T("---소형크기 1~64\n"));
            for(size_t i=0; i<64; i++)
            {
                size = i+1;
                auto p = pBuffer + SizeBuffer - size;

                s = __rdtsc();
                done[i] = util.mFN_MemoryCompare(p, size, code);
                e = __rdtsc();
                tick[i] = e - s;

                if(done[i] == SafeCompare(p, size))
                    done[i] = TRUE;
                else
                    done[i] = FALSE;
            }
            for(size_t i=0; i<64; i++)
            {
                _MACRO_OUTPUT_DEBUG_STRING_ALWAYS(_T("Size[%2Iu] %s %llu\n")
                    , i+1
                    , (done[i])? _T("SUCCEED") : _T("FAILED")
                    , tick[i]);
                Assert::IsTrue(TRUE == done[i]);
            }
        }

        TEST_METHOD(일치테스트)
        {
            gFN_OffMessageCoreDLL_IsLimitedVersion();
            ::memset(pBuffer, code, SizeBuffer);
            Test1();
        }
        TEST_METHOD(불일치테스트_RANDOM)
        {
            gFN_OffMessageCoreDLL_IsLimitedVersion();
            ::memset(pBuffer, code, SizeBuffer);
            for(int i=0; i<1000; i++)
            {
                size_t r = ::rand();
                r %= SizeBuffer;
                pBuffer[r] = code^0x11;
            }
            Test1();
        }
        TEST_METHOD(불일치테스트_S)
        {
            gFN_OffMessageCoreDLL_IsLimitedVersion();
            ::memset(pBuffer, code, SizeBuffer);
            pBuffer[0] = code^0x11;
            Test1();
        }
        TEST_METHOD(불일치테스트_E)
        {
            gFN_OffMessageCoreDLL_IsLimitedVersion();
            ::memset(pBuffer, code, SizeBuffer);
            pBuffer[SizeBuffer-1] = code^0x11;
            Test1();
        }

    };
}