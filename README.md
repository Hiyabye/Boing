# Mega Jump

C99 + [raylib](https://www.raylib.com/)로 만든 수직 점프 게임. raylib은
`lib/raylib/`에 벤더링되어 CMake 서브디렉터리로 소스에서 함께 빌드되므로
따로 설치할 시스템 의존성이 없다.

[2PG Arcade 1.44MB 콘테스트](https://2pgarcade.com/contest-144mb.html)를 겨냥 —
배포되는 게임(실행 파일 + 런타임 + 에셋)은 **1,474,560바이트(3.5" 플로피 1장)
이하**여야 한다. 에셋 파일 대신 절차적 생성(칩튠, SFX, 배경)을 쓴다.

## 구조

```
1.44MB/
├── src/                # 구현부
│   ├── main.c          # 게임 루프 + GameState 머신, HUD/오버레이
│   ├── player.c        # 플레이어 물리(중력, 입력, 스쿼시&스트레치)
│   ├── world.c         # 레벨 생성, 카메라, 점수, 코인/플랫폼/몬스터/파워업
│   ├── background.c    # 절차적 하늘/구름/태양 배경
│   └── sound.c         # 절차적 칩튠 + SFX (샘플 파일 없음)
├── include/            # 헤더 (core.h, theme.h는 헤더 온리)
├── lib/raylib/         # 벤더링된 raylib
└── CMakeLists.txt
```

## 빌드 & 실행

```sh
cmake -S . -B build      # 최초 1회, 또는 .c 파일 추가/삭제 후
cmake --build build      # ./build/Megajump 생성
./build/Megajump
```

`CMakeLists.txt`가 `src/*.c`를 glob하므로 `.c` 파일을 추가/삭제하면
`cmake -S . -B build`를 다시 돌려야 한다. macOS에서는 Cocoa/IOKit/CoreVideo/
OpenGL 프레임워크가 자동으로 링크된다.

## 크기 확인

기본 빌드는 최적화가 없어 ~2.2MB다. 실제 크기는 크기 최적화 빌드로 잰다:

```sh
cmake -S . -B build-minsize -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-minsize
strip build-minsize/Megajump
ls -l build-minsize/Megajump   # stripped ~1.03MB
```
