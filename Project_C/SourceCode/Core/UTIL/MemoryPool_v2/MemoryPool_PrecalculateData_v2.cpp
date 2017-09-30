#include "stdafx.h"
#if _DEF_MEMORYPOOL_MAJORVERSION == 2
#include "./MemoryPoolCore_v2.h"
#include "./MemoryPool_v2.h"
#include "../../Core.h"

namespace UTIL{
namespace MEM{
    namespace GLOBAL{
        namespace SETTING{
            extern const size_t gc_Array_Limit[11];
            extern const size_t gc_Array_MinUnit[11];
        #pragma region 메모리풀별 유닛 크기
        #define _LOCAL_MACRO_UNITSIZE8(s, up) s+up*0, s+up*1, s+up*2, s+up*3, s+up*4, s+up*5, s+up*6, s+up*7,
        #define _LOCAL_MACRO_UNITSIZE16(s, up) _LOCAL_MACRO_UNITSIZE8(s, up) _LOCAL_MACRO_UNITSIZE8(s+up*8, up)
        #define _LOCAL_MACRO_UNITSIZE32(s, up) _LOCAL_MACRO_UNITSIZE16(s, up) _LOCAL_MACRO_UNITSIZE16(s+up*16, up)
        #define _LOCAL_MACRO_UNITSIZE48(s, up) _LOCAL_MACRO_UNITSIZE32(s, up) _LOCAL_MACRO_UNITSIZE16(s+up*32, up)
        #define _LOCAL_MACRO_UNITSIZE56(s, up) _LOCAL_MACRO_UNITSIZE48(s, up) _LOCAL_MACRO_UNITSIZE8(s+up*48, up)
        #define _LOCAL_MACRO_UNITSIZE64(s, up) _LOCAL_MACRO_UNITSIZE32(s, up) _LOCAL_MACRO_UNITSIZE32(s+up*32, up)
        #define _LOCAL_MACRO_S(i) (UINT32)(SETTING::gc_Array_Limit[i-1]+SETTING::gc_Array_MinUnit[i])
        #define _LOCAL_MACRO_UP(i) (UINT32)(SETTING::gc_Array_MinUnit[i])
            const UINT32 gc_Array_MemoryPoolUnitSize[gc_Num_MemoryPool] ={
                _LOCAL_MACRO_UNITSIZE32(8, _LOCAL_MACRO_UP(0))
                _LOCAL_MACRO_UNITSIZE16(_LOCAL_MACRO_S(1), _LOCAL_MACRO_UP(1))
                _LOCAL_MACRO_UNITSIZE16(_LOCAL_MACRO_S(2), _LOCAL_MACRO_UP(2))
                _LOCAL_MACRO_UNITSIZE16(_LOCAL_MACRO_S(3), _LOCAL_MACRO_UP(3))
                _LOCAL_MACRO_UNITSIZE16(_LOCAL_MACRO_S(4), _LOCAL_MACRO_UP(4))
                _LOCAL_MACRO_UNITSIZE16(_LOCAL_MACRO_S(5), _LOCAL_MACRO_UP(5))
                _LOCAL_MACRO_UNITSIZE16(_LOCAL_MACRO_S(6), _LOCAL_MACRO_UP(6))
                _LOCAL_MACRO_UNITSIZE48(_LOCAL_MACRO_S(7), _LOCAL_MACRO_UP(7))
                _LOCAL_MACRO_UNITSIZE56(_LOCAL_MACRO_S(8), _LOCAL_MACRO_UP(8))
                _LOCAL_MACRO_UNITSIZE8(_LOCAL_MACRO_S(9), _LOCAL_MACRO_UP(9))
                _LOCAL_MACRO_UNITSIZE16(_LOCAL_MACRO_S(10), _LOCAL_MACRO_UP(10)) (_LOCAL_MACRO_S(10) + _LOCAL_MACRO_UP(10)*16), (_LOCAL_MACRO_S(10) + _LOCAL_MACRO_UP(10)*17),
            };
        #undef _LOCAL_MACRO_UNITSIZE8
        #undef _LOCAL_MACRO_UNITSIZE16
        #undef _LOCAL_MACRO_UNITSIZE32
        #undef _LOCAL_MACRO_UNITSIZE48
        #undef _LOCAL_MACRO_UNITSIZE56
        #undef _LOCAL_MACRO_UNITSIZE64
        #undef _LOCAL_MACRO_S
        #undef _LOCAL_MACRO_UP
        #pragma endregion

        #pragma region 각 유닛크기(메모리풀)별 메모리 확보 단위
            // 6KB(6144B) 이하는 S방식 256KB 단위 처리
            // val == 0 : S방식
            // val > 0 : N방식
            // 단위는 KB
            const UINT32 gc_Precalculate_MemoryPoolExpansionSize[][2]={
                // ~6KB 104 0~103
                {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
                {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
                {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
                {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
                {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
                {0, 0}, {0, 0}, {0, 0}, {0, 0},
                //--------------------------------
                // ~8KB 8 104~111
                {512, 512}, {512, 1024}, // 6400 6656
                {512, 1024}, {256, 512}, // 6912 7168            
                {512, 1024}, {512, 1024},// 7424 7680
                {512, 1024}, {512, 512}, // 7936 8192
                //--------------------------------
                // ~16KB 16 112~127
                {512, 1024}, {0, 0},        // 8704 9216
                {512, 1024}, {512, 1024},   // 9728 10240
                {0, 0}, {0, 0},             // 10752 11264
                {0, 0}, {0, 0},             // 11776 12288
                {512, 1024}, {512, 512},    // 12800 13312
                {1024, 1024}, {0, 0},       // 13824 14336
                {512, 512}, {1024, 2048},   // 14848 15360
                {1024, 2048}, {1024, 2048}, // 15872 16384
                //--------------------------------
                // ~64KB 48 128~175
                {1024, 1024}, {1024, 2048}, // 17408 18432
                {1024, 1024}, {1024, 1024}, // 19456 20480
                {512, 512}, {512, 512},     // 21504 22528
                {512, 512}, {512, 512},     // 23552 24576
                {512, 2048}, {1024, 1024},  // 25600 26624
                {1024, 2048}, {256, 512},   // 27648 28672            
                {1024, 1024}, {512, 512},   // 29696 30720
                {512, 2048}, {1024, 2048},  // 31744 32768
                {1024, 2048}, {512, 512},   // 33792 34816
                {1024, 1024}, {512, 1024},  // 35840 36864
                {1024, 1024}, {1024, 2048}, // 37888 38912
                {512, 1024}, {1024, 2048},  // 39936 40960
                {512, 2048}, {512, 1024},   // 41984 43008
                {1024, 2048}, {1024, 1024}, // 44032 45056
                {1024, 2048}, {1024, 1024}, // 46080 47104
                {1024, 2048}, {1024, 1024}, // 48128 49152
                {512, 2048}, {512, 1024},   // 50176 51200
                {1024, 1024}, {1024, 2048}, // 52224 53248
                {1024, 1024}, {2048, 4096}, // 54272 55296
                {2048, 4096}, {1024, 1024}, // 56320 57344
                {2048, 4096}, {1024, 2048}, // 58368 59392
                {1024, 4096}, {1024, 1024}, // 60416 61440
                {2048, 4096}, {1024, 4096}, // 62464 63488
                {1024, 1024}, {4096, 4096}, // 64512 65536
                //--------------------------------
                // ~512KB 56 176~231
                // 176 ~ 179
                {2048, 4096}, {2048, 4096}, {2048, 2048}, {2048, 2048},
                // 180 ~ 183
                {2048, 4096}, {2048, 2048}, {2048, 2048}, {4096, 4096},
                // 184 ~ 187
                {2048, 2048}, {2048, 2048}, {2048, 2048}, {4096, 4096},
                // 188 ~ 191
                {2048, 2048}, {4096, 4096}, {2048, 4096}, {4096, 4096},
                // 192 ~ 195
                {4096, 4096}, {4096, 4096}, {4096, 4096}, {2048, 4096},
                // 196 ~ 199
                {4096, 4096}, {4096, 4096}, {4096, 4096}, {4096, 4096},
                // 200 ~ 203
                {4096, 4096}, {4096, 4096}, {4096, 4096}, {4096, 4096},
                // 204 ~ 207
                {4096, 4096}, {4096, 4096}, {4096, 4096}, {4096, 4096},
                // 208 ~ 211
                {4096, 4096}, {4096, 4096}, {4096, 4096}, {4096, 4096},
                // 213 ~ 215
                {4096, 4096}, {4096, 4096}, {4096, 4096}, {4096, 4096},
                // 216 ~ 219
                {4096, 4096}, {4096, 4096}, {4096, 4096}, {4096, 8192},
                // 220 ~ 223
                {4096, 8192}, {4096, 4096}, {4096, 4096}, {4096, 4096},
                // 224 ~ 227
                {4096, 8192}, {4096, 8192}, {4096, 8192}, {4096, 8192},
                // 228 ~ 231
                {4096, 4096}, {4096, 4096}, {4096, 4096}, {4096, 8192},
                //--------------------------------
                // ~1MB 8 232~239
                // 232 ~ 235
                {4096, 4096}, {4096, 4096}, {4096, 8192}, {4096, 4096},
                // 236 ~ 239
                {4096, 8192}, {4096, 8192}, {4096, 4096}, //239번(1MB)는 자동 계산
                //--------------------------------
                // ~10MB 18 240~257
                // 자동 계산
            };// 자동계산은 관리단위 크기로 관리할수 없거나, 메모리 사용효율이 나쁜단위에만 사용해야 한다
        }

        namespace{
            DECLSPEC_NOINLINE UINT32 gFN_Calculate_MemoryPoolExpansionSize(UINT32 _IndexThisPool, UINT32 nNeedUnits)
            {
            #pragma warning(push)
            #pragma warning(disable: 6385)
            #define temp ::UTIL::MEM::GLOBAL::SETTING::gc_Array_MemoryPoolUnitSize
                _AssertReleaseMsg(_IndexThisPool < _MACRO_ARRAY_COUNT(temp), _T("Invalid MemoryPool Index"));
                const size_t nUnits = nNeedUnits;
                const size_t UnitSize = temp[_IndexThisPool] + sizeof(::UTIL::MEM::TDATA_BLOCK_HEADER);
                size_t size = (nUnits * UnitSize) + ::UTIL::MEM::GLOBAL::SETTING::gc_SizeMemoryPoolAllocation_DefaultCost;

                auto sysinfo = ::CORE::g_instance_CORE.mFN_Get_System_Information()->mFN_Get_SystemInfo();
                // 페이지단위 기준(보통 4KB)
                const size_t minimalSize = sysinfo->dwPageSize; // 이것은 1024의 배수라고 확신한다
                const size_t u = size / minimalSize;
                const size_t d = size % minimalSize;
                size_t result = u * minimalSize;
                if(d)
                    result += minimalSize;

                return (UINT32)(result / 1024);
            #undef temp
            #pragma warning(pop)
            }
        }

        UINT32 gFN_Precalculate_MemoryPoolExpansionSize__GetSmallSize(UINT32 _IndexThisPool)
        {
            if(_IndexThisPool < _MACRO_ARRAY_COUNT(SETTING::gc_Precalculate_MemoryPoolExpansionSize))
                return SETTING::gc_Precalculate_MemoryPoolExpansionSize[_IndexThisPool][0];
            else
                return gFN_Calculate_MemoryPoolExpansionSize(_IndexThisPool, 4);
        }
        UINT32 gFN_Precalculate_MemoryPoolExpansionSize__GetBigSize(UINT32 _IndexThisPool)
        {
            if(_IndexThisPool < _MACRO_ARRAY_COUNT(SETTING::gc_Precalculate_MemoryPoolExpansionSize))
                return SETTING::gc_Precalculate_MemoryPoolExpansionSize[_IndexThisPool][1];
            else
                return gFN_Calculate_MemoryPoolExpansionSize(_IndexThisPool, 8);
        }

        size_t gFN_Precalculate_MemoryPoolExpansionSize__CountingExpansionSize(UINT32 _Size)
        {
        #pragma warning(push)
        #pragma warning(disable: 4127)
            _Assert(GLOBAL::gc_minAllocationSize % 1024 == 0);
        #pragma warning(pop)
            UINT32 c_DefaultAllocSize = GLOBAL::gc_minAllocationSize / 1024;

            size_t Counting = 0;
            for(size_t i=0; i<_MACRO_ARRAY_COUNT(SETTING::gc_Precalculate_MemoryPoolExpansionSize); i++)
            {
                for(size_t j=0; j<2; j++)
                {
                    const UINT32 _temp = SETTING::gc_Precalculate_MemoryPoolExpansionSize[i][j];
                    const UINT32 val = ((0 ==_temp)? c_DefaultAllocSize : _temp);
                    if(val == _Size)
                        Counting++;
                }
            }
            return Counting;
        }
        namespace{
            BOOL gFN_Test_MemoryPoolsUnitSize()
            {
                _MACRO_STATIC_ASSERT(_MACRO_ARRAY_COUNT(SETTING::gc_Array_MemoryPoolUnitSize) == SETTING::gc_Num_MemoryPool);
                _AssertRelease(SETTING::gc_Array_Limit__Count == SETTING::gc_Array_MinUnit__Count);
                BOOL bFailed = FALSE;

                size_t cnt = 0;
                size_t unitsize = SETTING::gc_minSize_of_MemoryPoolUnitSize;
                if(unitsize != SETTING::gc_Array_MemoryPoolUnitSize[cnt])
                {
                    bFailed = TRUE;
                    _Error(_T("Bad Setting"));
                }
                cnt++;
                
                for(size_t i = 0; i < SETTING::gc_Array_Limit__Count; i++)
                {
                    const size_t limit = SETTING::gc_Array_Limit[i];
                    const size_t unit = SETTING::gc_Array_MinUnit[i];

                    if(unitsize >= limit)
                    {
                        bFailed = TRUE;
                        _Error(_T("Bad Setting"));
                    }
                    do{
                        unitsize += unit;
                        if(SETTING::gc_Array_MemoryPoolUnitSize[cnt] != unitsize || cnt >= SETTING::gc_Num_MemoryPool)
                        {
                            bFailed = TRUE;
                            _Error(_T("Bad Setting"));
                        }
                        cnt++;
                    }while(unitsize < limit);
                }

                return !bFailed;
            }
            BOOL gFN_Test_Precalculated_MemoryPoolPoolExpansionSize()
            {
                // 가능 조합 s0, s1
                // case 1   s0 s1 이 모두 0
                // case 2   s0 s1 모두 0이 아니며 허용된 수치
                //          s0 < s1
                BOOL bFailed = FALSE;
                for(int i=0; i<_MACRO_ARRAY_COUNT(SETTING::gc_Precalculate_MemoryPoolExpansionSize); i++)
                {
                    const UINT32* set = SETTING::gc_Precalculate_MemoryPoolExpansionSize[i];
                    BOOL bInvalid = FALSE;
                    if(0 == set[0] + set[1])
                        ;// 모두 0
                    else if(set[0] > set[1])
                        bInvalid = TRUE; // S > B
                    else if(set[0] && set[1])
                        ;// 모두 0이상
                    else
                        bInvalid = TRUE;// 둘중 하나만 0이 설정된 경우

                    if(!bInvalid)
                    {
                        size_t nUnits[2];
                        for(int s=0; s<2; s++)
                        {
                            if(set[s] % 4)
                            {
                                // 4KB 정렬 확인
                                nUnits[s] = 0;
                                continue;
                            }

                            if(set[s])
                            {
                                const size_t sizeExpansion = set[s]*(size_t)1024 - ::UTIL::MEM::GLOBAL::SETTING::gc_SizeMemoryPoolAllocation_DefaultCost;
                                const size_t Unitsize = ::UTIL::MEM::GLOBAL::SETTING::gc_Array_MemoryPoolUnitSize[i] + sizeof(TDATA_BLOCK_HEADER);
                                nUnits[s] = sizeExpansion / Unitsize;
                            }
                            else
                            {
                                const size_t sizeExpansion = ::UTIL::MEM::GLOBAL::gc_minAllocationSize - ::UTIL::MEM::GLOBAL::SETTING::gc_SizeMemoryPoolAllocation_DefaultCost;
                                const size_t Unitsize = ::UTIL::MEM::GLOBAL::SETTING::gc_Array_MemoryPoolUnitSize[i];
                                nUnits[s] = sizeExpansion / Unitsize;
                            }
                        }
                        if(nUnits[0] < 1 || nUnits[1] < 2
                            || nUnits[0] > nUnits[1])
                            bInvalid = TRUE;
                    }
                    if(bInvalid)
                    {
                        _Error(_T("Bad Setting ExpansionSize(%uKB , %uKB)"), set[0], set[1]);
                        bFailed = TRUE;
                    }
                }
                return !bFailed;
            }
            const BOOL gc_bDontTest_MemoryPoolsUnitSize = gFN_Test_MemoryPoolsUnitSize();
            const BOOL gc_bDoneTest_Precalculate_MemoryPoolPoolExpansionSize = gFN_Test_Precalculated_MemoryPoolPoolExpansionSize();
        }
    #pragma endregion
    }
}
}
#endif