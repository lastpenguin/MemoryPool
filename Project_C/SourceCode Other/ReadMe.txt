Calculator_MemoryPool_Demand
	메모리풀들(유닛크기 단위별)의 VirtualAlloc 단위 크기 계산에 사용
	공유하기 위해 정해진 청크 크기들을 후보로 두고,
	사용자가 사용가능한 영역 크기를 이용하여 사용하지 못하는 영역의 낭비율을 계산,
	각 메모리풀 별, 적합한 후보(청크크기)를 계산
Core_UseTest
	Core Library 사용 테스트
	