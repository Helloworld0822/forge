# Hylo

**Hybrid Lightweight Process + Coroutine** 언어 — Elixir/Erlang 스타일 경량 프로세스와 코루틴을 결합한 AOT 컴파일 언어입니다.

Hylo 소스(`.hy`)는 C 코드로 컴파일된 뒤 네이티브 바이너리로 빌드됩니다. GC 없이 예측 가능한 실행 모델을 목표로 합니다.

## 특징

- **Light Process** — 상태 소유·격리·장애 복구 단위 (`process`)
- **Coroutine** — 프로세스 내부 경량 실행 흐름 (`coroutine`, `spawn`, `yield`)
- **AOT 컴파일** — `.hy` → `.c` → 네이티브 바이너리
- **표준 모듈** — I/O, 문자열, 수학, 파일, TCP/UDP, HTTP, JSON
- **사용자 라이브러리** — `library` / `export` / `import`로 정적 라이브러리 빌드
- **Supervisor** — Elixir 스타일 장애 복구 정책

## 요구 사항

- GCC 또는 Clang (C11)
- CMake 3.16+
- Linux (네트워킹 모듈은 POSIX 소켓 기준)

## 빌드

```bash
git clone https://github.com/Helloworld0822/hylo.git
cd hylo

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 실행

```bash
./build/bin/hello
./build/bin/coroutines
./build/bin/stdlib_demo
./build/bin/use_library
./build/bin/http_server   # curl http://127.0.0.1:8080
./build/bin/tcp_echo      # 포트 9000
```

## 언어 예시

```hylo
process main {
    coroutine worker(id: int) {
        println("start", id);
        yield;
        println("done", id);
    }

    spawn worker(1);
    spawn worker(2);
}
```

## 표준 모듈

`import`로 사용합니다. 표준 모듈은 `libhylo_std`에 포함됩니다.

| 모듈 | 설명 |
|------|------|
| `io` | `print`, `eprint`, `eprintln` |
| `strings` | `str_len`, `str_concat`, `str_eq`, … |
| `math` | `abs_i`, `min_i`, `max_i`, `pow_i`, … |
| `time` | `time_now_ms`, `sleep_ms` |
| `fs` | `fs_read`, `fs_write`, `fs_exists` |
| `os` | `os_exit`, `os_getenv`, `os_argc`, `os_argv` |
| `tcp` | `tcp_listen`, `tcp_connect`, `tcp_send`, `tcp_recv` |
| `udp` | `udp_bind`, `udp_send`, `udp_recv` |
| `http` | `http_get`, `http_post`, `http_listen`, … |
| `json` | `json_get_string`, `json_get_int`, … |

```hylo
import io;
import tcp;

process main {
    let server: int = tcp_listen(8080);
    println("listening");
}
```

## 사용자 라이브러리

### 라이브러리 정의

```hylo
library greeting {
    import strings;

    export fn hello(name: string): string {
        return str_concat("Hello, ", name);
    }
}
```

### 컴파일

```bash
./build/bin/hylo --lib libs/greeting/greeting.hy \
    -o greeting.c --header greeting.h
```

### 사용

```hylo
import greeting;

process main {
    println(greeting.hello("Hylo"));
}
```

CMake에서는 `cmake/HyloLibrary.cmake`의 `hylo_add_library()`를 사용합니다.

## 프로젝트 구조

```
hylo/
├── compiler/       # Hylo 컴파일러 (lexer, parser, codegen)
├── runtime/        # 프로세스·코루틴·스케줄러
├── stdlib/         # 표준 모듈 C 구현
├── include/        # 런타임 및 stdlib 헤더
├── libs/           # 사용자 라이브러리 예제
├── examples/       # 예제 프로그램
├── cmake/          # CMake 헬퍼
└── docs/           # 설계 문서
```

## 컴파일러 사용법

```bash
# 실행 파일용 C 생성
hylo app.hy -o app.c

# 라이브러리용 C + 헤더 생성
hylo --lib lib.hy -o lib.c --header lib.h
```

## 기여

이슈와 PR을 환영합니다.

1. Fork 후 브랜치 생성
2. 변경 사항 커밋
3. PR 제출

## 라이선스

[MIT License](LICENSE) — 자유롭게 사용·수정·배포할 수 있습니다.

## 설계 문서

자세한 실행 모델과 설계 방향은 [docs/first.md](docs/first.md)를 참고하세요.
