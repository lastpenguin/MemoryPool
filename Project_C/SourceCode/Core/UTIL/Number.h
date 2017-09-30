#pragma once
// 언더플로우/오버플로우가 없는 카운팅
// TCounting_UINT32
// TCounting_UINT64
// TCounting_LockFree_UINT32
// TCounting_LockFree_UINT64
// TCounting_UT (TCounting_UINT32 / TCounting_UINT64)
// TCounting_LockFree_UT (TCounting_LockFree_UINT32 / TCounting_LockFree_UINT64)
//----------------------------------------------------------------
//  사용 주의
//  LockFree 계열 64바이트 이상은 윈도우 Vista / Server 2008 에서만 실행가능
//----------------------------------------------------------------
namespace UTIL{
    namespace NUMBER{
    #pragma pack(push, 4)
        template<typename T, T _Min, T _Max>
        class TCounting_T{
        public:
            _MACRO_STATIC_ASSERT(_Min < _Max);
            typedef typename T _ValueType;
            static const T c_Min = _Min;
            static const T c_Max = _Max;

            TCounting_T() :m_Val(((c_Min <= 0 && c_Max <= 0)? 0 : c_Min)){}
            TCounting_T(const T& val){ operator = (val); }
            TCounting_T(const TCounting_T& r){ operator = (r.m_Val); }
            TCounting_T& operator = (const TCounting_T& r){ operator = (r.m_Val); }

            operator T() const
            {
                return m_Val;
            }
            T operator = (T val)
            {
                _Assert(c_Min <= val && val <= c_Max);
                if(val < c_Min)
                    m_Val = c_Min;
                else if(c_Max < val)
                    m_Val = c_Max;
                else
                    m_Val = val;
                return m_Val;
            }
            T operator ++ ()
            {
                if(m_Val < _Max)
                    m_Val++;
                return m_Val;
            }
            T operator ++ (int)
            {
                T r = m_Val;
                if(m_Val < _Max)
                    m_Val++;
                return r;
            }
            T operator -- ()
            {
                if(m_Val > _Min)
                    m_Val--;
                return m_Val;
            }
            T operator -- (int)
            {
                T r = m_Val;
                if(m_Val > _Min)
                    m_Val--;
                return r;
            }
            T operator + (T val) const
            {
                T temp(m_Val);
                temp += val;
                return temp;
            }
            T operator - (T val) const
            {
                T temp(m_Val);
                temp -= val;
                return temp;
            }
            T operator += (T val)
            {
                if(0 == val)
                    return m_Val;

                if(0 < val)
                {
                    // +
                    const T temp = m_Val;
                    if(temp == _Max)
                        return temp;

                    T result = temp + val;
                    if(_Max < result || result <= temp)
                    {
                        m_Val = _Max;
                        return _Max;
                    }
                    else
                    {
                        m_Val = result;
                        return result;
                    }
                }
                else
                {
                    // -
                    const T temp = m_Val;
                    if(temp == _Min)
                        return temp;

                    T result = temp + val;
                    if(result < _Min || temp <= result)
                    {
                        m_Val = _Min;
                        return _Min;
                    }
                    else
                    {
                        m_Val = result;
                        return result;
                    }
                }
            }
            T operator -= (T val)
            {
                if(0 == val)
                    return m_Val;

                if(0 < val)
                {
                    // -
                    const T temp = m_Val;
                    if(temp == _Min)
                        return temp;

                    T result = temp - val;
                    if(result < _Min || temp <= result)
                    {
                        m_Val = _Min;
                        return _Min;
                    }
                    else
                    {
                        m_Val = result;
                        return result;
                    }
                }
                else
                {
                    // +
                    const T temp = m_Val;
                    if(temp == _Max)
                        return temp;

                    T result = temp - val;
                    if(_Max < result || result <= temp)
                    {
                        m_Val = _Max;
                        return _Max;
                    }
                    else
                    {
                        m_Val = result;
                        return result;
                    }
                }
            }

        private:
            T m_Val;
        };
        typedef typename TCounting_T<UINT32, (UINT32)0, MAXUINT32> TCounting_UINT32;
        typedef typename TCounting_T<UINT64, (UINT64)0, MAXUINT64> TCounting_UINT64;
        _MACRO_STATIC_ASSERT(sizeof(TCounting_UINT32) == sizeof(UINT32));
        _MACRO_STATIC_ASSERT(sizeof(TCounting_UINT64) == sizeof(UINT64));

        template<typename T, T _Min, T _Max>
        class CCounting_LockFree_T{
        public:
            _MACRO_STATIC_ASSERT(sizeof(long) == sizeof(int));
            _MACRO_STATIC_ASSERT(_Min < _Max);
            typedef typename T _ValueType;
            static const T c_Min = _Min;
            static const T c_Max = _Max;

            CCounting_LockFree_T() :m_Val(((c_Min <= 0 && c_Max <= 0)? 0 : c_Min)){}
            CCounting_LockFree_T(const T& val){ operator = (val); }
            CCounting_LockFree_T(const CCounting_LockFree_T& r){ operator = (r.m_Val); }
            CCounting_LockFree_T& operator = (const CCounting_LockFree_T& r){ operator = (r.m_Val); }

            operator T() const
            {
                return m_Val;
            }
            T operator = (T val)
            {
                _Assert(c_Min <= val && val <= c_Max);
                if(val < c_Min)
                {
                    m_Val = c_Min;
                    return c_Min;
                }
                else if(c_Max < val)
                {
                    m_Val = c_Max;
                    return c_Max;
                }
                else
                {
                    m_Val = val;
                    return val;
                }
            }
            T operator ++ ()
            {
                for(;;)
                {
                    const T temp = m_Val;
                    if(temp == _Max)
                        return temp;
                    if(temp == CompareExchange<T>(&m_Val, temp+1, temp))
                        return temp+1;
                }
            }
            T operator ++ (int)
            {
                for(;;)
                {
                    const T temp = m_Val;
                    if(temp == _Max)
                        return temp;
                    if(temp == CompareExchange<T>(&m_Val, temp+1, temp))
                        return temp;
                }
            }
            T operator -- ()
            {
                for(;;)
                {
                    const T temp = m_Val;
                    if(temp == _Min)
                        return temp;
                    if(temp == CompareExchange<T>(&m_Val, temp-1, temp))
                        return temp-1;
                }
            }
            T operator -- (int)
            {
                for(;;)
                {
                    const T temp = m_Val;
                    if(temp == _Min)
                        return temp;
                    if(temp == CompareExchange<T>(&m_Val, temp-1, temp))
                        return temp;
                }
            }

            T operator += (T val)
            {
                if(0 == val)
                    return m_Val;

                if(0 < val)
                {
                    // +
                    for(;;)
                    {
                        const T temp = m_Val;
                        if(temp == _Max)
                            return temp;

                        T result = temp + val;
                        if(_Max < result || result <= temp)
                        {
                            if(temp == CompareExchange<T>(&m_Val, _Max, temp))
                                return _Max;
                        }
                        else
                        {
                            if(temp == CompareExchange<T>(&m_Val, result, temp))
                                return result;
                        }
                    }
                }
                else
                {
                    // -
                    for(;;)
                    {
                        const T temp = m_Val;
                        if(temp == _Min)
                            return temp;

                        T result = temp + val;
                        if(result < _Min || temp <= result)
                        {
                            if(temp == CompareExchange<T>(&m_Val, _Min, temp))
                                return _Min;
                        }
                        else
                        {
                            if(temp == CompareExchange<T>(&m_Val, result, temp))
                                return result;
                        }
                    }
                }
            }
            T operator -= (T val)
            {
                if(0 == val)
                    return m_Val;

                if(0 < val)
                {
                    // -
                    for(;;)
                    {
                        const T temp = m_Val;
                        if(temp == _Min)
                            return temp;

                        T result = temp - val;
                        if(result < _Min || temp <= result)
                        {
                            if(temp == CompareExchange<T>(&m_Val, _Min, temp))
                                return _Min;
                        }
                        else
                        {
                            if(temp == CompareExchange<T>(&m_Val, result, temp))
                                return result;
                        }
                    }
                }
                else
                {
                    // +
                    for(;;)
                    {
                        const T temp = m_Val;
                        if(temp == _Max)
                            return temp;

                        T result = temp - val;
                        if(_Max < result || result <= temp)
                        {
                            if(temp == CompareExchange<T>(&m_Val, _Max, temp))
                                return _Max;
                        }
                        else
                        {
                            if(temp == CompareExchange<T>(&m_Val, result, temp))
                                return result;
                        }
                    }
                }
            }

        private:
            template<typename _T>
            __forceinline _T CompareExchange(volatile _T* pDestination, _T Exchange, _T Comperand)
            {
                return _InterlockedCompareExchange(pDestination, Exchange, Comperand);
            }
            template<>
            __forceinline int CompareExchange<int>(volatile int* pDestination, int Exchange, int Comperand)
            {
                return _InterlockedCompareExchange((volatile long*)pDestination, Exchange, Comperand);
            }
            template<>
            __forceinline __int64 CompareExchange<__int64>(volatile __int64* pDestination, __int64 Exchange, __int64 Comperand)
            {
                return _InterlockedCompareExchange64(pDestination, Exchange, Comperand);
            }
            template<>
            __forceinline SHORT CompareExchange<SHORT>(volatile SHORT* pDestination, SHORT Exchange, SHORT Comperand)
            {
                return _InterlockedCompareExchange16(pDestination, Exchange, Comperand);
            }
            template<>
            __forceinline USHORT CompareExchange<USHORT>(volatile USHORT* pDestination, USHORT Exchange, USHORT Comperand)
            {
                return _InterlockedCompareExchange16((volatile SHORT*)pDestination, (SHORT)Exchange, (SHORT)Comperand);
            }

        private:
            volatile T m_Val;
        };
        typedef typename CCounting_LockFree_T<UINT32, 0, MAXUINT32> TCounting_LockFree_UINT32;
        typedef typename CCounting_LockFree_T<UINT64, 0, MAXUINT64> TCounting_LockFree_UINT64;
        _MACRO_STATIC_ASSERT(sizeof(TCounting_LockFree_UINT32) == sizeof(UINT32));
        _MACRO_STATIC_ASSERT(sizeof(TCounting_LockFree_UINT64) == sizeof(UINT64));
    #pragma pack(pop)
    }
}

namespace UTIL{
    namespace NUMBER{
    #if __X86
        typedef TCounting_UINT32 TCounting_UT;
        typedef TCounting_LockFree_UINT32 TCounting_LockFree_UT;
    #elif __X64
        typedef TCounting_UINT64 TCounting_UT;
        typedef TCounting_LockFree_UINT64 TCounting_LockFree_UT;
    #endif
    }
}