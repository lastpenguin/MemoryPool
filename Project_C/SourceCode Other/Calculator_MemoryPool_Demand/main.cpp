#include <stdio.h>
#include <iostream>
#include <vector>
#include <algorithm>

// pool 단위크기 B
struct TPoolSize{
    int iStart;
    int iRise;
    int nStep;
};
TPoolSize g_Input[]={
    // 6KB 까지는 256KB S 형식 정렬이 적은 메모리를 사용한다
    {6400, 256, 8},
    {8704, 512, 16},
    {17*1024, 1024, 48},
    {72*1024, 8*1024, 56},
    {576*1024, 64*1024, 8},
    {1024*1024 + 512*1024, 512*1024, 18}
};
const int gc_nInput = sizeof(g_Input) / sizeof(g_Input[0]);

const int gc_sizeHeader_per_Unit = 64;
const int gc_defaultCost = 128+64;



struct TResult{
    int   sizeIndex = 0;

    int   sizeKB = 0;;
    int   nUnits = 0;
    float fRate  = 0.f;
    bool operator < (const TResult& r)
    {
        return fRate < r.fRate;
    }
};
struct TResultEnd{
    static const int c_MaxResult = 5;
    
    int PoolSize;
    int nResult = 0;
    TResult data[c_MaxResult];
};
std::vector<TResultEnd> g_Result;


const int gc_minUnits_per_Alloc = 4;

// 단위는 KB
const int gc_Array_TestSize[] = {
    256, 512, 1024, 2048, 4096, 8192
};
const int gc_cnt_Test = sizeof(gc_Array_TestSize) / sizeof(gc_Array_TestSize[0]);

// 64KB에서 시작하여 목표치를 만족하는 것이 있다면 작은 크기부터 순서대로
// 만약 그렇지 못하다면 최대 테스트 크기까지 확인하여, 비율순위로
std::vector<TResult> temp(gc_cnt_Test);
void BuildData(int iSizePool)
{
    const int iRealUnitSize = iSizePool + gc_sizeHeader_per_Unit;
    int sizeIndex = 0;
    for(auto& data : temp)
    {
        const int sizeAlloc = gc_Array_TestSize[sizeIndex] * 1024;

        const int nUnits = (sizeAlloc-gc_defaultCost) / iRealUnitSize;
        const int iUselessSize = sizeAlloc - /*iRealUnitSize*/iSizePool * nUnits;
        const float fRateUseless = iUselessSize / (float)sizeAlloc;

        data.sizeIndex = sizeIndex;
        data.sizeKB = gc_Array_TestSize[sizeIndex];
        data.nUnits = nUnits;
        data.fRate  = fRateUseless;
        if(nUnits < gc_minUnits_per_Alloc)
            data.fRate = 1.f;

        sizeIndex++;
    }
}
bool GetData(TResultEnd& r, float fGoal, bool isSorted = false)
{
    for(const auto& data : temp)
    {
        if(data.fRate < fGoal)
        {
            auto& slot = r.data[r.nResult];
            slot = data;

            if(++r.nResult == TResultEnd::c_MaxResult)
                break;
        }
        else if(isSorted && data.fRate > fGoal)
        {
            break;
        }
    }
    return 0 < r.nResult;
}
int main()
{
    int nPool = 0;
    for(const auto t : g_Input)
        nPool += t.nStep;

    g_Result.resize(nPool);


    auto iter = g_Result.begin();
    for(const auto t : g_Input)
    {
        for(int iStep=0; iStep<t.nStep; iStep++)
        {
            TResultEnd& r = *iter;
            r.PoolSize = t.iStart + (t.iRise * iStep);
            
            BuildData(r.PoolSize);
            if(!GetData(r, 0.05f))
            {
                std::sort(temp.begin(), temp.end());
                GetData(r, 0.10f, true);
            }
            iter++;
        }
    }

    struct TStats_TestSize{
        int size;
        int cntUsing = 0;
        bool operator < (const TStats_TestSize& r) const{
            return cntUsing < r.cntUsing;
        }
    }array_stats_testsize[gc_cnt_Test];
    for(int i=0; i<gc_cnt_Test; i++)
        array_stats_testsize[i].size = gc_Array_TestSize[i];


    for(const auto& r : g_Result)
    {
        for(int i=0; i<r.nResult; i++)
        {
            const int index = r.data[i].sizeIndex;
            array_stats_testsize[index].cntUsing++;
        }
    }

    int __id = 104;
    for(const auto& r : g_Result)
    {
        printf("Pool[%9d] ID: %d    ", r.PoolSize, __id++);
        if(r.nResult == 0)
        {
            printf("Not Found\n");
            continue;
        }
        for(int i=0; i<r.nResult; i++)
        {
            printf("%6d[%4d %4.2f%%]  "
                , r.data[i].sizeKB
                , r.data[i].nUnits
                , r.data[i].fRate * 100.f);
        }
        printf("\n");
    }
    printf("================\n");
    for(const auto& d : array_stats_testsize)
    {
        printf("[%6d] %d\n", d.size, d.cntUsing);
    }

    printf("================\n");
    std::sort(&array_stats_testsize[0], &array_stats_testsize[gc_cnt_Test]);
    for(int i=gc_cnt_Test-1; i>=0; i--)
    {
        const auto& d = array_stats_testsize[i];
        printf("Rank %3d = [%6d] %d\n", gc_cnt_Test-i
            , d.size, d.cntUsing);
    }

    system("pause");
    return 0;
}