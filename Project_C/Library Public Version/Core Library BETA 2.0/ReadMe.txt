https://github.com/lastpenguin/MemoryPool
lastpenguin83@gmail.com

사용 요구사항
	visual studio 2015 C++

실행 요구사항
	visual studio 2015 C++ 또는,
	Visual Studio 2015용 Visual C++ 재배포 가능 패키지
		https://www.microsoft.com/ko-kr/download/details.aspx?id=48145
//----------------------------------------------------------------
구성
	Core_x86.dll	Windows XP 이상
	Core_x64.dll	Windows Vista 이상
	※ 문자 집합 : 유니코드
//----------------------------------------------------------------
사용 방법
	초기화를 처리할 cpp 파일에서 다음 파일을 포함(include)
		./User/Core_import.h
	초기화
		::CORE::Connect_CORE(...)
	종료(명시적 종료 시점이 필요할때만 사용)
		::CORE::Disconnect_CORE

	사용을 위해서는 다음 파일을 포함(include)
		./User/Core_include.h
//----------------------------------------------------------------
인터페이스는 다음 파일들을 참조
	Core_Interface.h
	MemoryPool_Interface.h
	System_Information_Interface.h

디버깅용 로그 필요시 다음 파일을 참조하여 객체를 정의하여, 초기화시 인스턴스를 전달해야 함
	LogWriter_Interface.h

	pLogSystem	중요한 오류등을 기록
	pLogDebug	일반 디버깅 정보
