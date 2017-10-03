# MemoryPool
c++ / Windows xp~ / Visual Studio 2015~

## 프로젝트 우선순위
1. 스레드 세이프
2. 메모리 논리적 주소, 물리적 공간의 단편화 방지
3. 부가적인 메모리 소모비용의 최소화
4. 속도
5. 범용성을 위해 사용하지 않는 메모리는 가능한 해제를 시도

## 사용 요구사항
* Visual Studio 2015 C++
* 사용자의 C++ 기초지식

## 실행 요구사항
* Visual Studio 2015 C++ 또는,
* Visual Studio 2015용 Visual C++ 재배포 가능 패키지
* https://www.microsoft.com/ko-kr/download/details.aspx?id=48145

## 구성
* Core_x86.dll	Windows XP 이상
* Core_x64.dll	Windows Vista 이상
* ※ 문자 집합 : 유니코드

* * *

## 라이브러리 연결
### 방법1
**Project_C/Library Public Version/Core Library BETA 2.0** 을 사용

초기화를 처리할 `cpp` 파일에서 다음 파일을 포함(`include`)
```c++
./User/Core_import.h
```
초기화
```c++
::CORE::Connect_CORE(...)
```
종료(명시적 종료 시점이 필요할때만 사용)
```c++
::CORE::Disconnect_CORE
```    
사용을 위해서는 다음 파일을 포함(include)
```c++
./User/Core_include.h
```
### 방법2
`Project_C/SourceCode/Engine` 라이브러리 `import` 또는 코드를 참고

* * *

## 인터페이스
인터페이스는 다음 파일들을 참고
* `Core_Interface.h`
* `MemoryPool_Interface.h`
* `System_Information_Interface.h`

디버깅용 로그 필요시 다음 파일을 참고하여 객체를 정의하여, 초기화시 인스턴스를 전달해야 함
* `LogWriter_Interface.h`
* `pLogSystem`: 중요한 오류등을 기록
* `pLogDebug`: 일반 디버깅 정보

* * *

## 코드내 사용 방법
(다음의 방법들은 모두 혼용사용이 가능합니다) 
 
#1) 이미 작성된 코드에 쉽게 적용하는 방법
메모리풀을 적용하려는 객체는 `CMemoryPoolResource`를 상속받습니다. 
※ 주의 : 자식 클래스까지 모두 메모리풀의 영향을 받습니다. 

```c++
예:)
class CTest1 : public CMemoryPoolResource {
...

CTest1* p = new Ctest1(...);
delete p;
예:) 
struct TTest1 : CMemoryPoolResource {
...

TTest1* p = new TTest1;
delete p;
```
`new delete` 를 이용하여 사용합니다. 
 
#2) 객체단위에 대하여 만약 상속을 사용하지 않고 특정상황에만 사용하고 싶다면 다음의 매크로를 사용합니다. (`CMemoryPoolResource`를 상속 받은 타입또한 가능합니다
```c++
// ■ 매크로 버전 : 할당자 / 소멸자 비 호출 
_MACRO_ALLOC__FROM_MEMORYPOOL(Address) 
_MACRO_FREE__FROM_MEMORYPOOL(Address) 

// ■ 매크로 버전 : 할당자 / 소멸자 호출 
_MACRO_NEW__FROM_MEMORYPOOL(Address, Constructor) 
_MACRO_DELETE__FROM_MEMORYPOOL(Address) 

예:)
TTest1* p1 = nullptr;
_MACRO_ALLOC__FROM_MEMORYPOOL(p1) 
_MACRO_FREE__FROM_MEMORYPOOL(p2) 

TTest1* p2 = nullptr;
_MACRO_NEW__FROM_MEMORYPOOL(p1, TTest1(...));
_MACRO_DELETE__FROM_MEMORYPOOL(p1);

```
#3) 가변크기 사용
만약 할당하려는 크기가 가변적(예를 들어 문자열 버퍼) 이라면 메모리풀관리자에 직접 접근하여 다음의 메소드를 사용합니다 
```c++
IMemoryPool_Manager::mFN_Get_Memory 
IMemoryPool_Manager::mFN_Return_Memory 
IMemoryPool_Manager::mFN_Get_Memory__AlignedCacheSize       // 캐시라인 정렬 버전 
IMemoryPool_Manager::mFN_Return_Memory__AlignedCacheSize    // 캐시라인 정렬 버전 

// ■ 매크로 버전 : 할당자 / 소멸자 비 호출 
// ※ malloc / free 를 대신하기에 적합한 방법입니다. 
_MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC 
_MACRO_FREE__FROM_MEMORYPOOL_GENERIC

예:)
TTest1* p1 = _MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC(sizeof(TTest1));
...
_MACRO_FREE__FROM_MEMORYPOOL_GENERIC(p1);
```

#기타1 : STL Container Allocator
`STL Container` 의 `Allocator` 교체 할 경우 다음을 사용 
```c++
TAllocator 
```
이것은 `vector` 같은 매우 가변적 크기 할당에 사용하지 않는 것이 좋습니다 

#기타2 : 
만약 캐시라인 크기인 64B단위로 정렬된 주소를 얻고 싶다면
단순하게는 64B의 배수단위로 요구를 해도 됩니다
 
### 고급 사용 방법
파일 `MemoryPool_Interface.h`의 사용 설명을 참고 하십시오

Memory Pool Interface
::UTIL::MEM::IMemoryPool

Memory Pool Manager Interface
::UTIL::MEM::IMemoryPool_Manager
Instance
::UTIL::g_pMem

* * *
## 작성자 코멘트
### 혹시나 분석을 시도하려는 분을 위하여...
메모리풀 코드는 프로젝트Core의 폴더 UTIL 내에 있습니다.
그중에, V2 폴더에 있는 것이 최근 버전입니다(2015년까지 작업)
파일당 라인수는 분할을 하지 않아 조금 불편하게 느낄수도 있지만
가급적 객체단위로 namespace로 감싸놓아 VS의 숨기기 기능을 이용하면 보기 불편하지 않을겁니다
이런식으로요
```c++
namespace UTIL{
namespace MEM{
class OBJ1{
}
}
}

namespace UTIL{
namespace MEM{
class OBJ2{
}
}
}
```

이 메모리풀 내부에 사용된 락은 몽땅 스핀락입니다
(물론 스핀 횟수에 제한을 두어 스레드스위칭은 시도합니다)
(수정하고 싶다면 잠금이 사용된 부분은 사실 많지 않으니 쉽게 하실 수 있습니다)


이 메모리풀은 게임같은 곳에 사용시 LFH, TBB에 비해 좀더 메모리를 절약합니다
(아마도요, 계속 해서 LFH, TBB 와 비교해왔거든요)
LFH에 비해 좀더 다양한 풀로 분할했습니다
기본 청크 단위는 256KB이며(아마 제 기억이 맞을 겁니다), 이것을 다시 몇 개의 그룹으로 분할하여 여러 스레드 경쟁을 줄입니다
실제 virtualalloc 시에는 256KB * x 단위로 한번에 더 많이 할당합니다(여러개의 청크를 한번에 생성합니다)
그리고 이 청크들은 각 단위사이즈를 사용하는 풀에서 필요시 분할해서 가져가 사용합니다
여기서 왜 윈도우 메모리 예약단위인 64KB가 아닌 256KB 냐고 묻는다면 순전히 메모리 절약을 위해서 입니다
여기서 256KB 정렬문제로 좀 귀찮은 처리를 했구요

만약 사용자가 자신의 프로젝트에 맞게끔 수정하고 싶다면 다음 스프레드시트 파일을 참고하세요
* 메모리풀 할당크기 계산.ods
* 메모리풀 예약단위 계산.ods

### 이 메모리풀은 8논리코어 이하에 최적화 되어있습니다
(사실 혼자 쓰려 했는데 라이젠때문에 메인스트림이 12, 16 스레드로 바뀌어서 오픈소스화 합니다)

### 질문은 lastpenguin83@gmail.com 으로 하시거나 이곳에 하십시오

2015년 이후로 수정하지 않아 기억나지 않는 부분이 많습니다.
이 부분이 왜 이렇게 했나 궁금한 부분은 다음의 파일을 참고 해주세요
* 메모리풀 개선.txt (V1)
* 메모리풀 개선V2.txt
