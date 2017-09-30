# MemoryPool
c++ / Windows xp~ / Visual Studio 2015~

## 프로젝트 우선순위
1. 스레드 세이프
2. 부가적인 메모리 소모비용의 최소화
3. 속도
4. 범용성을 위해 사용하지 않는 메모리는 가능한 해제를 시도

## 사용 요구사항
Visual Studio 2015 C++

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
인터페이스는 다음 파일들을 참조
* `Core_Interface.h`
* `MemoryPool_Interface.h`
* `System_Information_Interface.h`

디버깅용 로그 필요시 다음 파일을 참조하여 객체를 정의하여, 초기화시 인스턴스를 전달해야 함
* `LogWriter_Interface.h`
* `pLogSystem`: 중요한 오류등을 기록
* `pLogDebug`: 일반 디버깅 정보

* * *

## 코드내 사용 방법
(다음의 방법들은 모두 혼용사용이 가능합니다) 
 
#1) 메모리풀을 적용하려는 객체는 `CMemoryPoolResource`를 상속받습니다. 
```c++
예:)
class CTest1 : public CMemoryPoolResource 
예:) 
struct TTest1 : CMemoryPoolResource 
```
`new delete` 를 이용하여 사용합니다. 

※ 이미 작성된 코드에 쉽게 적용하는데 유리합니다. 

※ 주의 : 자식 클래스까지 모두 메모리풀의 영향을 받습니다. 

 
#2) 객체단위에 대하여 만약 상속을 사용하지 않고 특정상황에만 사용하고 싶다면 다음의 매크로를 사용합니다. (`CMemoryPoolResource`를 상속 받은 타입또한 가능합니다
```c++
// ■ 매크로 버전 : 할당자 / 소멸자 비 호출 
_MACRO_ALLOC__FROM_MEMORYPOOL(Address) 
_MACRO_FREE__FROM_MEMORYPOOL(Address) 

// ■ 매크로 버전 : 할당자 / 소멸자 호출 
_MACRO_NEW__FROM_MEMORYPOOL(Address, Constructor) 
_MACRO_DELETE__FROM_MEMORYPOOL(Address) 
```
#3) 만약 할당하려는 크기가 가변적(예를 들어 문자열 버퍼) 이라면 메모리풀관리자에 직접 접근하여 다음의 메소드를 사용합니다 
```c++
IMemoryPool_Manager::mFN_Get_Memory 
IMemoryPool_Manager::mFN_Return_Memory 
IMemoryPool_Manager::mFN_Get_Memory__AlignedCacheSize       // 캐시라인 정렬 버전 
IMemoryPool_Manager::mFN_Return_Memory__AlignedCacheSize    // 캐시라인 정렬 버전 

// ■ 매크로 버전 : 할당자 / 소멸자 비 호출 
// ※ malloc / free 를 대신하기에 적합한 방법입니다. 
_MACRO_ALLOC__FROM_MEMORYPOOL_GENERIC 
_MACRO_FREE__FROM_MEMORYPOOL_GENERIC 
```

#기타 : 
`STL Container` 의 `Allocator` 교체 할 경우 다음을 사용 
```c++
TAllocator 
```
이것은 `vector` 같은 매우 가변적 크기 할당에 사용하지 않는 것이 좋습니다 
 
