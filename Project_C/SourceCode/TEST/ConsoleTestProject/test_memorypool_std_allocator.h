#include "../../Output/Engine_include.h"
#include "../../Output/Engine_import.h"

#pragma warning(disable: 4996)


//#include <hash_map>
//#include <hash_set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <queue>
#include <deque>


//#include <functional>
// 0 : default allocator
const size_t g_nLoop  = 6;
const size_t g_nDatas = 1000 * 1000;

typedef size_t _TYPE;
typedef size_t _TYPE_KEY;

//auto cmpless = [](const _TYPE_KEY& a, const _TYPE_KEY& b) -> bool {return a < b;};
namespace POOL{
    ::ENGINE::CEngine g_Engine;
    typedef std::list<_TYPE, ::UTIL::MEM::TAllocator<_TYPE>> _TYPE_Container_list;
    typedef std::vector<_TYPE, ::UTIL::MEM::TAllocator<_TYPE>> _TYPE_Container_vector;
    typedef std::deque<_TYPE, ::UTIL::MEM::TAllocator<int>> _TYPE_Container_deque;
    typedef std::set<_TYPE_KEY, std::less<_TYPE_KEY>, ::UTIL::MEM::TAllocator<_TYPE_KEY>> _TYPE_Container_set;
    typedef std::map<_TYPE_KEY, _TYPE, std::less<_TYPE_KEY>, ::UTIL::MEM::TAllocator<std::pair<const _TYPE_KEY, _TYPE>>> _TYPE_Container_map;
    typedef std::unordered_map<_TYPE_KEY, _TYPE, std::hash<_TYPE_KEY>, std::equal_to<_TYPE_KEY>, ::UTIL::MEM::TAllocator<std::pair<const _TYPE_KEY, _TYPE>>> _TYPE_Container_unordered_map;
}
namespace DEFAULT{
    typedef std::list<_TYPE> _TYPE_Container_list;
    typedef std::vector<_TYPE> _TYPE_Container_vector;
    typedef std::deque<_TYPE> _TYPE_Container_deque;
    typedef std::set<_TYPE_KEY, std::less<_TYPE_KEY>> _TYPE_Container_set;
    typedef std::map<_TYPE_KEY, _TYPE, std::less<_TYPE_KEY>> _TYPE_Container_map;
    typedef std::unordered_map<_TYPE_KEY, _TYPE, std::hash<_TYPE_KEY>, std::equal_to<_TYPE_KEY>> _TYPE_Container_unordered_map;
}

namespace{
    template<typename T>
    void test_list(T* pCon)
    {
        T& l = *pCon;
        for(size_t i=0; i<g_nDatas; i++)
            l.push_front(i);
        for(size_t i=0; i<g_nDatas; i++)
            l.pop_back();
    }
    template<typename T>
    void test_vector(T* pCon)
    {
        T& v = *pCon;
        for(size_t i=0; i<g_nDatas; i++)
            v.push_back(i);
        for(size_t i=0; i<g_nDatas; i++)
            v.pop_back();
    }
    template<typename T>
    void test_deque(T* pCon)
    {
        T& d = *pCon;
        for(size_t i=0; i<g_nDatas; i++)
            d.push_back(i);
        for(size_t i=0; i<g_nDatas; i++)
            d.pop_back();
    }
    template<typename T>
    void test_set(T* pCon)
    {
        T& s = *pCon;
        for(size_t i=0; i<g_nDatas; i++)
            s.insert(i);
        for(size_t i=0; i<g_nDatas; i++)
            s.erase(i);
    }
    template<typename T>
    void test_map(T* pCon)
    {
        T& m = *pCon;
        for(size_t i=0; i<g_nDatas; i++)
            m.insert(std::make_pair(i, i));
        for(size_t i=0; i<g_nDatas; i++)
            m.erase(i);
    }
    template<typename T>
    void test_unordered_map(T* pCon)
    {
        T& m = *pCon;
        for(size_t i=0; i<g_nDatas; i++)
            m.insert(std::make_pair(i, i));
        for(size_t i=0; i<g_nDatas; i++)
            m.erase(i);
    }

    template<typename TFN1, typename T1, typename TFN2, typename T2>
    void testcon(TFN1 pFN1, T1* pContainer1, TFN2 pFN2, T2* pContainer2, LPCSTR name)
    {
        for(size_t nLoop=0; nLoop<g_nLoop; nLoop++)
        {
            auto s1 = __rdtsc();
            pFN1(pContainer1);
            auto e1 = __rdtsc();

            auto s2 = __rdtsc();
            pFN2(pContainer2);
            auto e2 = __rdtsc();

            auto Elapse1 = e1-s1;
            auto Elapse2 = e2-s2;

            NUMBERFMTA nFmt = { 0, 0, 3, ".", ",", 1 };
            char szOUT1[128], szOUT2[128];
            char szTemp[128];
            sprintf(szTemp, "%llu", (UINT64)(Elapse1));
            ::GetNumberFormatA(NULL, NULL, szTemp, &nFmt, szOUT1, 128);
            sprintf(szTemp, "%llu", (UINT64)(Elapse2));
            ::GetNumberFormatA(NULL, NULL, szTemp, &nFmt, szOUT2, 128);

            printf("%16s(%3Iu) Cost : %16s %16s %3.2f%%\n", name, nLoop
                , szOUT1, szOUT2
                , (float)(Elapse2*100/Elapse1));
        }
    }
}


void test()
{
    ::SetThreadAffinityMask(::GetCurrentThread(), 1);
    printf("Container : Default Allocator VS Memory Pool\n");
    DEFAULT::_TYPE_Container_list l1;
    DEFAULT::_TYPE_Container_vector v1;
    DEFAULT::_TYPE_Container_deque d1;
    DEFAULT::_TYPE_Container_set s1;
    DEFAULT::_TYPE_Container_map m1;
    DEFAULT::_TYPE_Container_unordered_map um1;
    POOL::_TYPE_Container_list l2;
    POOL::_TYPE_Container_vector v2;
    POOL::_TYPE_Container_deque d2;
    POOL::_TYPE_Container_set s2;
    POOL::_TYPE_Container_map m2;
    POOL::_TYPE_Container_unordered_map um2;

    testcon(test_list<decltype(l1)>, &l1, test_list<decltype(l2)>, &l2, "list");
    testcon(test_vector<decltype(v1)>, &v1, test_vector<decltype(v2)>, &v2, "vector");
    testcon(test_deque<decltype(d1)>, &d1, test_deque<decltype(d2)>, &d2, "deque");
    testcon(test_set<decltype(s1)>, &s1, test_set<decltype(s2)>, &s2, "set");
    testcon(test_map<decltype(m1)>, &m1, test_map<decltype(m2)>, &m2, "map");
    testcon(test_unordered_map<decltype(um1)>, &um1, test_unordered_map<decltype(um2)>, &um2, "unordered map");

    system("pause");
}