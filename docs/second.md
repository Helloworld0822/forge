```markdown
# 🔥 Forge 언어 완전 가이드

## 목차
1. [탄생 배경 & 철학](#1-탄생-배경--철학)
2. [3개 언어 핵심 특징](#2-3개-언어-핵심-특징)
3. [BEAM VM 특징](#3-beam-vm-특징)
4. [Forge 런타임 아키텍처](#4-forge-런타임-아키텍처)
5. [언어 핵심 문법](#5-언어-핵심-문법)
6. [성능 최적화 전략](#6-성능-최적화-전략)
7. [컴파일러 파이프라인](#7-컴파일러-파이프라인)
8. [타입 시스템](#8-타입-시스템)
9. [내결함성 & 분산](#9-내결함성--분산)
10. [개발 로드맵](#10-개발-로드맵)
11. [최종 성능 목표](#11-최종-성능-목표)

---

## 1. 탄생 배경 & 철학

기존 언어의 한계:
- Elixir  → 동시성 최고, But GC 있음, 느린 연산
- Rust    → 성능 최고, But 동시성 모델 복잡
- Node.js → I/O 효율적, But 싱글 스레드 한계

세 언어의 장점만 결합:

```
Rust 소유권 + BEAM Actor + io_uring 이벤트 루프
(메모리안전)   (동시성)     (I/O 효율)
```

---

## 2. 3개 언어 핵심 특징

### Elixir

| 특징 | 내용 |
|------|------|
| 동작 환경 | Erlang BEAM VM |
| 동시성 | Actor 모델 (경량 프로세스) |
| 메모리 관리 | 프로세스별 GC |
| 강점 | 수백만 동시 연결, 내결함성 |
| 약점 | GC 존재, CPU 연산 느림 |

**핵심 문법**
```elixir
# 파이프 연산자
result =
  [1, 2, 3, 4, 5]
  |> Enum.map(&(&1 * 2))
  |> Enum.filter(&(&1 > 4))
  |> Enum.sum()

# 패턴 매칭
match response do
  {:ok, data}        -> process(data)
  {:error, :timeout} -> retry()
  {:error, reason}   -> log_error(reason)
end

# Actor (GenServer)
defmodule Counter do
  use GenServer

  def handle_call(:get, _from, state) do
    {:reply, state, state}
  end

  def handle_cast(:increment, state) do
    {:noreply, state + 1}
  end
end
```

### Rust

| 특징 | 내용 |
|------|------|
| 동작 환경 | 네이티브 (LLVM) |
| 동시성 | Send/Sync 트레이트 |
| 메모리 관리 | 소유권 시스템 (GC 없음) |
| 강점 | C/C++ 수준 성능, 메모리 안전 |
| 약점 | 학습 곡선, 느린 컴파일 |

**핵심 문법**
```rust
// 소유권
let s1 = String::from("hello");
let s2 = s1;          // 소유권 이동
// println!("{}", s1) // ❌ 컴파일 에러

// 빌림
fn calculate(s: &String) -> usize { s.len() }

// 참조 규칙
let mut s = String::from("hello");
let r1 = &s;          // 불변 참조 (여러 개 가능)
let r2 = &s;          // ✅
let r3 = &mut s;      // 가변 참조 (하나만 가능)
// let r4 = &mut s;   // ❌ 컴파일 에러

// Result / Option (Null 없음)
fn divide(a: f64, b: f64) -> Result<f64, String> {
    if b == 0.0 { Err("0으로 나눌 수 없음".into()) }
    else        { Ok(a / b) }
}
```

---

## 3. BEAM VM 특징

```
┌─────────────────────────────────────────────────┐
│                  BEAM VM                        │
│                                                 │
│  Scheduler   Scheduler   Scheduler              │
│  (Core 1)    (Core 2)    (Core N)               │
│  [P1][P2]    [P3][P4]    [P5][P6]               │
│      └──────────┴─────────┘                     │
│          Work Stealing                          │
│                                                 │
│  ETS (메모리DB)  Mnesia (분산DB)  I/O (비동기)  │
│                                                 │
│  Port / NIF (외부 C 코드 인터페이스)             │
└─────────────────────────────────────────────────┘
```

| 항목 | OS Thread | BEAM Process |
|------|-----------|--------------|
| 초기 스택 | ~1-8 MB | ~2 KB |
| 생성 비용 | 수 ms | 수 μs |
| 최대 개수 | 수천 | 수백만 |
| 컨텍스트 스위치 | 무거움 | 매우 가벼움 |
| 공유 메모리 | 있음 | 없음 |
| GC | Shared Heap | Per-Process |
| Stop-the-World | 있음 | 없음 |

**핵심 특징**
- 선점형 스케줄링 (2000 Reduction마다 강제 교체)
- 프로세스별 독립 GC (STW 없음)
- 핫 코드 스와핑 (무중단 배포)
- 투명한 분산 (로컬/원격 동일 API)

---

## 4. Forge 런타임 아키텍처

```
┌─────────────────────────────────────────────────────────┐
│                    Forge Runtime                        │
│                                                         │
│  ┌─────────────────────────────────────────────────┐    │
│  │           Hybrid Scheduler Layer                │    │
│  │                                                 │    │
│  │  Core 1          Core 2          Core N         │    │
│  │ ┌──────────┐   ┌──────────┐   ┌──────────┐     │    │
│  │ │ Run Queue│   │ Run Queue│   │ Run Queue│     │    │
│  │ │CPU Tasks │   │CPU Tasks │   │CPU Tasks │     │    │
│  │ │Event Loop│   │Event Loop│   │Event Loop│     │    │
│  │ │(io_uring)│   │(io_uring)│   │(io_uring)│     │    │
│  │ └──────────┘   └──────────┘   └──────────┘     │    │
│  │              Work Stealing                      │    │
│  └─────────────────────────────────────────────────┘    │
│                                                         │
│  Actor Manager │ Ownership Checker │ io_uring + DPDK   │
│                                                         │
│  Memory: Stack │ Owned Heap │ Arc │ Arena │ Slab        │
└─────────────────────────────────────────────────────────┘
```

**설계 원칙**
1. 코어당 하나의 이벤트 루프 (BEAM + Tokio 결합)
2. 선점형 + 협력형 하이브리드 스케줄링
3. I/O는 항상 Non-blocking (io_uring 기반)
4. 소유권으로 메모리 관리 (GC 없음)
5. Actor 간 Zero-Copy 메시지 패싱

---

## 5. 언어 핵심 문법

### 기본 문법
```forge
// 불변이 기본값
let x = 42
let mut y = 0

// 타입 추론
let name = "Forge"
let scores: Vec<i32> = []

// 파이프 연산자
let result =
    [1, 2, 3, 4, 5]
    |> filter(fn(x) -> x % 2 == 0)
    |> map(fn(x) -> x * x)
    |> sum()
```

### 패턴 매칭
```forge
match response {
    {:ok, data} if data.len() > 0 => process(data),
    {:ok, []}                     => default_value(),
    {:error, :timeout}            => retry(),
    {:error, reason}              => log_error(reason),
    _                             => unreachable()
}

// 리스트 분해
match items {
    []              => "비어있음",
    [single]        => "하나: {single}",
    [first, ..rest] => "첫번째: {first}, 나머지: {rest.len()}개",
}

// 구조체 분해
match user {
    User { age, role: :admin } if age >= 18 => grant_access(),
    User { role: :guest }                   => limited_access(),
    User { banned: true, .. }               => deny_access(),
}
```

### Actor 시스템
```forge
actor Counter {
    state {
        count: u64 = 0,
    }

    handle(:increment)             { state.count += 1; reply(:ok) }
    handle(:increment_by, n: u64)  { state.count += n; reply(:ok) }
    handle(:get)                   { reply(state.count) }
    handle(:reset)                 { state.count = 0;   reply(:ok) }
}

// 사용
let counter = spawn Counter()
counter <- :increment           // fire-and-forget
let n   = counter <? :get       // 응답 대기 (동기)
let f   = counter <?> :increment // 비동기
let res = await f
```

### Zero-Copy 메시지 패싱
```forge
actor Producer {
    async fn run() {
        let data = Vec<u8>::with_capacity(1024 * 1024)
        fill_data(&mut data)

        consumer <- move data  // 소유권 이전 (복사 없음!)
        // 이후 data 사용 불가
    }
}

actor Consumer {
    handle(:data, data: owned Vec<u8>) {
        process(data)
        // 스코프 종료 시 자동 해제
    }
}

// 불변 공유 (Arc)
let shared = Arc::new(large_dataset)
reader1 <- {:read, shared.clone()}  // 참조 카운트만 증가
reader2 <- {:read, shared.clone()}
reader3 <- {:read, shared.clone()}
```

### 비동기 처리
```forge
// async/await
async fn fetch_user(id: u64) -> Result<User, Error> {
    let res  = http.get("/users/{id}").await?
    let user = res.json::<User>().await?
    Ok(user)
}

// 동시 실행 (join)
let (user, posts, comments) = join!(
    fetch_user(1),
    fetch_posts(1),
    fetch_comments(1)
).await?

// 타임아웃 (select)
select! {
    result = task        => Ok(result),
    _      = sleep(5000) => Err(:timeout),
}

// 스트림 처리
stream
|> async_map(parse_packet)
|> async_filter(is_valid)
|> async_for_each(fn(packet) async {
    handle_packet(packet).await
})
.await
```

### Supervisor (내결함성)
```forge
supervisor AppSupervisor {
    strategy: :one_for_one,  // 죽은 자식만 재시작
    max_restarts: 3,
    max_seconds: 5,

    children: [
        {DatabasePool, size: 20},
        {HttpServer,   port: 8080},
        {MetricsActor},
    ]
}

// 재시작 시 상태 복구
actor DatabasePool {
    on_start(size: u32) {
        state.connections = create_connections(size)
    }

    on_restart(prev_state: Option<State>) {
        match prev_state {
            Some(s) => restore_from(s),
            None    => on_start(state.size),
        }
    }
}
```

### 실전 예시 - HTTP 서버
```forge
router AppRouter {
    get  "/users"     => UserController.index
    get  "/users/:id" => UserController.show
    post "/users"     => UserController.create

    scope "/api/v1" {
        pipe_through [:auth, :rate_limit]
        get "/profile" => ProfileController.show
    }

    websocket "/ws" => WebSocketHandler
}

module UserController {
    async fn show(req: Request) -> Response {
        let id = req.params.get("id")?

        match db.find_user(id).await {
            Ok(user)       => Response.json(user),
            Err(:not_found) => Response.status(404),
            Err(e)         => Response.status(500).body(e.message),
        }
    }
}

middleware Auth {
    async fn call(req: Request, next: Handler) -> Response {
        match req.headers.get("Authorization") {
            Some(token) if validate_token(token).await =>
                next.call(req).await,
            _ =>
                Response.status(401).json({:error, "Unauthorized"}),
        }
    }
}
```

---

## 6. 성능 최적화 전략

### 최적화 레이어 구조

```
Layer 7│ 애플리케이션: Actor 핀닝, 배치 처리, 핫 코드 교체
Layer 6│ 런타임: 하이브리드 스케줄러, Work Stealing, Lock-Free
Layer 5│ 메모리: Arena/Slab 할당자, Zero-Copy, Object Pool
Layer 4│ I/O: io_uring SQPOLL, DPDK, Zero-Copy 전송
Layer 3│ 컴파일러: 컴파일 타임 계산, SIMD, PGO+LTO
Layer 2│ CPU: AVX-512, 브랜치 힌트, 프리패칭, 파이프라이닝
Layer 1│ 하드웨어: NUMA 토폴로지, 캐시 계층, 하드웨어 카운터
```

### 캐시 친화적 데이터 구조
```forge
// ❌ AoS (Array of Structs) - 캐시 비효율
struct Particle { x: f32, y: f32, z: f32, w: f32 }
let particles: Vec<Particle>

// ✅ SoA (Struct of Arrays) - SIMD 친화적
struct Particles {
    x: Vec<f32>,  // [x0, x1, x2, ...] 연속 메모리
    y: Vec<f32>,
    z: Vec<f32>,
    w: Vec<f32>,
}

// ✅ 핫/콜드 데이터 분리
#[align(64)]          // 캐시 라인 정렬
struct HotData {      // 자주 접근
    id: u64,
    score: f64,
    flags: u32,
    _pad: [u8; 44],   // 64바이트 맞춤
}

struct ColdData {     // 가끔 접근
    name: String,
    description: String,
    metadata: HashMap<String, String>,
}
```

### 메모리 할당자
```forge
// 1. Arena 할당자 (요청당 단 한 번의 free)
async fn handle_request(req: Request) -> Response {
    using ArenaAllocator::new(64 * 1024) {
        let parsed   = parse_request(req)   // arena에서 할당
        let result   = query_db(parsed)     // arena에서 할당
        let response = build_response(result)
        response
    }  // 블록 종료 시 전체 한 번에 해제!
}

// 2. Slab 할당자 (동일 크기 객체, O(1) 할당/해제)
allocator SlabAllocator<T> {
    free_list: LockFreeStack<*mut T>

    fn alloc() -> *mut T {
        self.free_list.pop()
            .unwrap_or_else(|| allocate_new_slab())
    }
    fn free(ptr: *mut T) {
        self.free_list.push(ptr)  // 즉시 재사용
    }
}

// 3. 버퍼 풀 (크기별 버킷)
pool BufferPool {
    buckets: [
        {size: 512,     count: 10000},
        {size: 4096,    count: 5000},
        {size: 65536,   count: 1000},
        {size: 1048576, count: 100},
    ]
}
```

### io_uring Zero-Syscall I/O
```forge
// 기존 epoll: syscall 2번 (epoll_wait + read)
// io_uring:  syscall 0번 (공유 링 버퍼로 직접 통신)

io_engine IoUring {
    sq_entries: 4096,
    cq_entries: 8192,
    flags: [
        .sqpoll,   // 커널 폴링 스레드 (syscall 완전 제거)
        .io_poll,  // 폴링 기반 I/O
    ]
}

// Zero-Copy 파일 전송
async fn send_file(file: File, socket: Socket) {
    // 기존: File → [커널버퍼] → [유저버퍼] → [소켓버퍼] → 전송
    // splice: File → [커널버퍼] → 전송 (복사 1번 절약!)
    io_uring.splice(file.fd, socket.fd, file.size).await
}
```

### SIMD 벡터화
```forge
// 스칼라: 1번에 1개
// AVX2:   1번에 8개 (f32 기준)
// AVX-512: 1번에 16개

#[target_feature(avx512f)]
#[vectorize(width = 16)]
fn sum(data: &[f32]) -> f32 {
    data.iter().sum()  // 컴파일러가 자동으로 SIMD 변환
}

// 직접 SIMD
fn add_vectors(a: &[f32; 8], b: &[f32; 8]) -> [f32; 8] {
    simd256 {
        let va = load256(a)
        let vb = load256(b)
        store256(vadd_f32(va, vb))
    }
}
```

### Lock-Free 자료구조
```forge
// CAS 기반 Lock-Free Queue
struct LockFreeQueue<T> {
    head: Atomic<*mut Node<T>>,
    tail: Atomic<*mut Node<T>>,
}

// False Sharing 방지
struct SharedCounters {
    counter_a: AtomicU64,
    _pad_a: [u8; 56],    // 64 - 8 = 56 바이트 패딩
    counter_b: AtomicU64,
    _pad_b: [u8; 56],    // 별도 캐시 라인 보장
}
```

### 컴파일 타임 계산
```forge
// 런타임 비용 → 컴파일 타임으로 이동
const LOOKUP_TABLE: [u32; 256] = comptime {
    let mut table = [0u32; 256]
    for i in 0..256 { table[i] = crc32_compute(i) }
    table
}

const ROUTES: CompileTimeMap<&str, Handler> = comptime_map! {
    "/api/users"    => handle_users,
    "/api/posts"    => handle_posts,
    "/api/comments" => handle_comments,
    // 완벽한 해시 함수 컴파일 타임 생성!
}

const EMAIL_REGEX: CompiledRegex = compile_regex!(
    r"^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$"
)
```

### NUMA 인식 스케줄링
```forge
topology {
    affinity_group :user_service {
        actors: [UserActor, AuthActor, SessionActor],
        cores: 0..7,
        numa_node: 0,   // 같은 NUMA 노드 → 메모리 접근 빠름
    }

    affinity_group :db_service {
        actors: [DbPoolActor, QueryActor, CacheActor],
        cores: 8..15,
        numa_node: 0,
    }
}
```

---

## 7. 컴파일러 파이프라인

```
소스 (.fg)
    │
    ▼
Lexer        → 토큰화
    │
    ▼
Parser       → AST 생성
    │
    ▼
Type Checker → 타입 추론 + 소유권 분석 + 라이프타임 분석
    │
    ▼
Effect Analyzer → 부작용 추적 (IO | Async | Unsafe)
    │
    ▼
MIR          → 중간 표현 (인라이닝, 상수 폴딩, 데드코드 제거)
    │
    ▼
LLVM Backend → 네이티브 코드 생성 (SIMD, PGO, LTO, BOLT)
    │
    ▼
네이티브 바이너리
```

**컴파일러 최적화 플래그**
```forge
compiler_flags {
    lto: .full,             // 전체 링크 타임 최적화
    pgo: .use("pgo.dat"),   // 프로파일 기반 최적화 (+15~30%)
    bolt: true,             // 바이너리 레이아웃 최적화
    target_cpu: .native,    // 현재 CPU에 특화
    optimize: .aggressive,
}
```

---

## 8. 타입 시스템

```forge
// 효과 시스템 (부작용 타입 추적)
fn pure_fn(x: i32)  -> i32           // 순수 함수
fn io_fn(x: i32)    -> i32 | IO      // I/O 부작용
fn async_fn(x: i32) -> i32 | Async   // 비동기
fn unsafe_fn(x: i32)-> i32 | Unsafe  // 안전하지 않음

// Null 없음
type Option<T> = Some(T) | None
type Result<T> = Ok(T)   | Err(Error)

// 대수적 데이터 타입
union NetworkEvent {
    Connected    { addr: IpAddr, port: u16 },
    Disconnected { reason: String },
    DataReceived { bytes: Vec<u8> },
    Error        { code: u32, msg: String },
}

// 트레이트 (Zero-Cost 추상화)
trait Serialize {
    fn serialize(self) -> Bytes
    fn deserialize(bytes: Bytes) -> Result<Self, Error>
}

// 컴파일 타임 크기 검증
compile_assert sizeof(HotData) <= 64   // 캐시 라인 이하 보장
compile_assert alignof(HotData) == 64  // 캐시 라인 정렬 보장

// 제네릭 + 트레이트 바운드
fn process<T: Serialize + Send + Sync>(value: T) -> Bytes {
    value.serialize()
}
```

---

## 9. 내결함성 & 분산

```forge
// Supervisor 전략
// :one_for_one  → 죽은 자식만 재시작
// :one_for_all  → 하나 죽으면 전체 재시작
// :rest_for_one → 죽은 것 이후 프로세스 재시작

supervisor AppSupervisor {
    strategy: :one_for_one,
    max_restarts: 3,
    max_seconds: 5,

    children: [
        {DatabasePool, size: 20},
        {HttpServer,   port: 8080},
        {MetricsActor},
    ]
}

// 핫 코드 스와핑 (무중단 배포)
#[hot_reload]
actor RequestHandler {
    // 프로덕션 서비스 중단 없이 코드 교체!
}

// 분산 클러스터 (BEAM 스타일)
Node.connect(:"node_b@192.168.1.2")
send({:process, :"node_b@192.168.1.2"}, {:hello, "world"})
// 로컬 / 원격 완전히 동일한 API!
```

---

## 10. 개발 로드맵

| Phase | 기간 | 목표 |
|-------|------|------|
| Phase 1 | 0~6개월 | Lexer/Parser, 타입 시스템, 소유권 검사기, LLVM 백엔드 |
| Phase 2 | 6~12개월 | io_uring 이벤트 루프, Actor 시스템, 하이브리드 스케줄러, 표준 라이브러리 |
| Phase 3 | 12~18개월 | 패키지 매니저, HTTP 프레임워크, DB 드라이버, LSP/포매터 |
| Phase 4 | 18~24개월 | SIMD 자동 벡터화, PGO 파이프라인, 핫 코드 스와핑, 분산 클러스터 |

---

## 11. 최종 성능 목표

| 항목 | Node.js | Go | Elixir | Rust | Forge |
|------|---------|----|--------|------|-------|
| HTTP RPS | 80K | 150K | 100K | 300K | **2,000K** |
| 동시 연결 | 10K | 100K | 1M | 500K | **10M+** |
| 메모리/연결 | 10KB | 8KB | 2KB | 1KB | **256B** |
| P99 레이턴시 | 10ms | 2ms | 5ms | 1ms | **100μs** |
| GC 중단 | O | O | X | X | **X** |
| 핫 코드 교체 | X | X | O | X | **O** |
| 분산 내장 | X | X | O | X | **O** |
| 메모리 안전 | O | O | O | O | **O** |

**최적화 단계별 성능 향상**

| 최적화 단계 | RPS |
|------------|-----|
| 기준 (Rust 기본) | 100,000 |
| + io_uring SQPOLL | 200,000 |
| + Arena 할당자 | 280,000 |
| + Lock-Free 구조 | 350,000 |
| + SIMD 최적화 | 500,000 |
| + 배치 처리 | 700,000 |
| + PGO + LTO | 850,000 |
| + DPDK 네트워킹 | 1,500,000 |
| + NUMA + 핀닝 | **2,000,000** |

---

## 핵심 원칙 요약

### Rust에서 가져온 것
- 소유권 시스템 (GC 없는 메모리 안전)
- Zero-Cost 추상화
- 트레이트 기반 다형성
- 컴파일 타임 안전성 검사

### Elixir에서 가져온 것
- Actor 모델 (경량 프로세스)
- 패턴 매칭 + 파이프 연산자
- Supervisor 내결함성
- 핫 코드 스와핑

### 이벤트 루프에서 가져온 것
- io_uring Non-blocking I/O
- 코어당 이벤트 루프
- Zero-Copy I/O

### Forge만의 것
- 효과 시스템 (부작용 타입 추적)
- 하이브리드 스케줄러 (선점 + 협력)
- NUMA 인식 Actor 핀닝
- 컴파일 타임 계산 극대화
- Arena 메모리 모델

---

> 한 줄 요약:
> "GC 없이, 수백만 Actor를, Zero-Syscall I/O로, 컴파일 타임에 안전하게"
```

위 내용을 전체 복사 후 `forge_guide.md` 로 저장하시면 됩니다! 😊
