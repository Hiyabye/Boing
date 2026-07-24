# Boing!

C99 + [raylib](https://www.raylib.com/)로 만든 수직 점프 게임.
["Mega Jump"](https://www.halfbrick.com/games/megajump)(Halfbrick)에서 장르적
영감을 받았으나, 코드·아트·오디오·이름은 모두 독자 제작이다. raylib은
`lib/raylib/`에 git 서브모듈로 고정되어 CMake 서브디렉터리로 소스에서 함께
빌드되므로 따로 설치할 시스템 의존성이 없다. (일반 `git clone`으로는
서브모듈이 비어 있으니 아래 빌드 절차를 따를 것.)

[2PG Arcade 1.44MB 콘테스트](https://2pgarcade.com/contest-144mb.html)를 겨냥 —
배포되는 게임(실행 파일 + 런타임 + 에셋)은 **1,474,560바이트(3.5" 플로피 1장)
이하**여야 한다. 에셋 파일 대신 절차적 생성(칩튠, SFX, 배경)을 쓴다.

## 구조

```
Boing/
├── src/                # 구현부
│   ├── main.c          # 게임 루프 + GameState 머신, HUD/오버레이
│   ├── player.c        # 플레이어 물리(중력, 입력, 스쿼시&스트레치)
│   ├── world.c         # 레벨 생성, 카메라, 점수, 코인/플랫폼/몬스터/파워업/디버프
│   ├── background.c    # 절차적 배경 — 4개 레트로 시대 테마가 등반에 따라 연속 전환
│   ├── theme.c         # 런타임 팔레트/테마 테이블(THEMES[]) — 장면이 이어서 모핑
│   ├── sound.c         # 절차적 칩튠 + SFX (레벨별 4트랙, 샘플 파일 없음)
│   └── progress.c      # 코인 은행 + 스킨/업그레이드/미션 해금(SHOP 화면), 저장 지속
├── include/            # 헤더 (core.h는 헤더 온리; 나머지는 .c와 1:1)
├── lib/raylib/         # raylib (git 서브모듈)
└── CMakeLists.txt
```

## 빌드 & 실행

```sh
# 처음 받을 때는 서브모듈(raylib)까지 함께 클론:
git clone --recurse-submodules https://github.com/Hiyabye/Boing.git
# 이미 일반 clone 했다면:
git submodule update --init

cmake -S . -B build      # 최초 1회, 또는 .c 파일 추가/삭제 후
cmake --build build      # ./build/Boing 생성
./build/Boing
```

`CMakeLists.txt`가 `src/*.c`를 glob하므로 `.c` 파일을 추가/삭제하면
`cmake -S . -B build`를 다시 돌려야 한다. macOS에서는 Cocoa/IOKit/CoreVideo/
OpenGL 프레임워크가 자동으로 링크된다.

## 크기 확인

기본 빌드가 `MinSizeRel` + strip이라 `build/Boing`이 곧 배포 크기다:

```sh
ls -l build/Boing   # stripped ~500KB (512,168 B), 한도 1,474,560 B
```
