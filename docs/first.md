# Hybrid Lightweight Process + Coroutine 언어 설계 아이디어 정리

## 1. 개요

이 설계는 **Elixir/Erlang의 경량 프로세스(Actor 모델)**와 **Coroutine 기반 동시성 모델**을 결합하는 새로운 실행 모델을 목표로 한다.

기존 모델:

* Actor 모델 → 안전한 격리와 장애 복구
* Coroutine 모델 → 낮은 비용의 동시 실행

두 모델의 장점을 결합하여:

> "프로세스는 상태와 장애 격리를 담당하고, 코루틴은 프로세스 내부의 실행 단위를 담당한다."

라는 구조를 만든다.

---

# 2. 기존 모델의 한계

## Elixir/Erlang 경량 프로세스

장점:

* 강력한 격리
* 메시지 기반 통신
* 장애 복구(Supervisor)
* 수백만 개의 동시 프로세스 가능

단점:

* 프로세스마다 Heap, Mailbox 관리 비용 발생
* 작은 작업도 프로세스 단위 생성 필요
* 메시지 전달 비용 존재

## Coroutine 모델

장점:

* 매우 낮은 실행 전환 비용
* I/O 작업 처리 효율 우수
* 자연스러운 비동기 코드 작성

단점:

* 공유 메모리 문제
* Race Condition 위험
* 장애 격리 부족

---

# 3. 새로운 실행 모델

## 기본 구조

```
Scheduler

    |

Light Process

    |
    +-- Coroutine
    +-- Coroutine
    +-- Coroutine
```

### 역할 분리

| 구성 요소         | 역할               |
| ------------- | ---------------- |
| Light Process | 상태 소유, 격리, 장애 단위 |
| Coroutine     | 실제 작업 실행 단위      |
| Scheduler     | CPU와 실행 순서 관리    |
| Mailbox       | Process 간 통신     |

---

# 4. 동작 방식

예:

```
Server Process

├ Request Coroutine
├ Database Coroutine
├ Network Coroutine
└ Timer Coroutine
```

하나의 프로세스 내부에서 여러 작업이 실행된다.

기존 Actor:

```
Request 1 → Process 1
Request 2 → Process 2
Request 3 → Process 3
```

새 모델:

```
Server Process

├ Request 1 Coroutine
├ Request 2 Coroutine
└ Request 3 Coroutine
```

---

# 5. 실행 규칙

안전성을 위해:

1. Process는 상태의 유일한 소유자이다.
2. Coroutine은 Process 내부에서만 실행된다.
3. Process 간 데이터 전달은 메시지 기반이다.
4. Coroutine은 명시적인 yield/await 지점에서 실행을 양보한다.
5. 공유 데이터는 기본적으로 불변(Immutable)이다.

---

# 6. 메모리 관리 방향

## 목표

* Garbage Collector 제거
* Rust 수준의 메모리 안정성
* 낮은 Latency

가능한 모델:

```
Process

├ Owned Memory
├ Immutable Shared Data
└ Move Data
```

규칙:

* Process 내부 → 자유 사용
* Process 외부 이동 → Ownership 이동
* 공유 → Immutable만 허용

Rust Borrow Checker 전체를 가져오기보다 단순화된 Ownership 모델을 목표로 한다.

---

# 7. GC 없는 설계의 장점

장점:

* GC Pause 제거
* 일정한 응답 시간
* 낮은 메모리 사용량
* 서버 Latency 감소

특히:

* WebSocket 서버
* 게임 서버
* 실시간 서비스
* 고성능 API 서버

에 적합하다.

---

# 8. Scheduler 설계

목표:

M:N 구조

```
CPU Thread

    |

Scheduler

    |

Light Process

    |

Coroutine
```

필요 기능:

* Work Stealing
* Coroutine Scheduling
* Process Migration
* 우선순위 관리

---

# 9. Supervisor 모델

Elixir의 장점을 유지한다.

구조:

```
Supervisor

    |

Process

    |

Coroutine
```

가능한 정책:

* Coroutine만 재시작
* Process 전체 재시작
* 하위 작업 전체 종료

---

# 10. 웹 서버 적용 예상

특히 적합한 분야:

* 대규모 WebSocket
* 채팅
* 게임 서버
* 실시간 API
* 마이크로서비스

잠재적 장점:

* 적은 메모리 사용
* 높은 동시 접속
* 낮은 Latency
* 장애 격리

---

# 11. 예상 성능 잠재력

가정:

* 최적화된 Scheduler
* 효율적인 Memory Model
* Zero Copy Message
* 낮은 Runtime Overhead

예상:

## 동시 연결

| 기술           |         예상 |
| ------------ | ---------: |
| Node.js      |   50만~150만 |
| Go           |  100만~300만 |
| Elixir       |  200만~500만 |
| Hybrid Model | 300만~800만+ |

최적화가 잘 된 경우:

* 1000만 동시 연결 가능성

---

# 12. 기존 연구와 비교

## Pony

비슷한 점:

* Actor
* 메모리 안전성
* GC 제거

차이:

* Coroutine 모델 없음

## Verona

비슷한 점:

* Ownership
* Region Memory
* GC 없는 설계

차이:

* Actor 중심 아님
* Coroutine 실행 모델 없음

## Rust

가져올 요소:

* Ownership
* Zero Cost Abstraction
* Memory Safety

## Elixir

가져올 요소:

* Process
* Mailbox
* Supervisor
* 장애 복구

---

# 13. 가장 중요한 설계 방향

문법보다 먼저 정의해야 할 것:

1. 실행 모델
2. Process 생명주기
3. Coroutine 생명주기
4. Scheduler 구조
5. Memory Ownership 규칙
6. Message 전달 방식
7. 장애 복구 정책

---

# 14. 핵심 철학

최종 목표:

```
Actor의 안전성

+

Coroutine의 효율

+

Rust의 메모리 안정성

=

새로운 서버용 동시성 모델
```

---

# 결론

이 설계는 단순히 Elixir와 Coroutine을 합치는 것이 아니라,

"경량 프로세스 내부에 여러 실행 흐름을 안전하게 넣는 새로운 런타임 모델"

을 만드는 방향이다.

기존 Actor 모델은 안정성을 위해 작은 프로세스를 많이 만들었고,
Coroutine 모델은 효율성을 위해 하나의 실행 공간에서 많은 작업을 처리했다.

새 모델은:

```
작은 격리 단위
+
내부의 많은 경량 작업
```

이라는 구조를 목표로 한다.

성공의 핵심은 기능 추가가 아니라:

* 단순한 메모리 규칙
* 예측 가능한 실행 모델
* 낮은 런타임 비용
* 쉬운 개발 경험

을 유지하는 것이다.
