# Boing!

C99 + [raylib](https://www.raylib.com/)로 만든 수직 점프 게임.
["Mega Jump"](https://www.halfbrick.com/games/megajump)(Halfbrick)에서 장르적
영감을 받았으나, 코드·아트·오디오·이름은 모두 독자 제작이다. raylib은
`lib/raylib/`에 git 서브모듈로 고정되어 CMake 서브디렉터리로 소스에서 함께
빌드되므로 따로 설치할 시스템 의존성이 없다. (일반 `git clone`으로는
서브모듈이 비어 있으니 아래 빌드 절차를 따를 것.) 아이콘·글리프 렌더링에는
[nanosvg](https://github.com/memononen/nanosvg)(zlib 라이선스, Mikko Mononen)를
`lib/nanosvg/`에 git 서브모듈로 고정해 쓴다 — 서드파티 코드는 이 둘뿐이다.

[2PG Arcade 1.44MB 콘테스트](https://2pgarcade.com/contest-144mb.html)를 겨냥 —
배포되는 게임(실행 파일 + 런타임 + 에셋)은 **1,474,560바이트(3.5" 플로피 1장)
이하**여야 한다. 에셋 파일 대신 절차적 생성(칩튠, SFX, 배경)을 쓴다. 아이콘도
파일이 아니라 SVG 소스 문자열을 바이너리에 박아 두고 실행 시 래스터화한다.
후처리 셰이더도 같은 방식으로 GLSL 소스를 문자열로 박아 두고 실행 시
컴파일한다.

## 플레이

코인을 먹으면 위로 튀어 오르고, 화면 아래로 떨어지면 끝나는 끝없는 등반
게임이다. 화살표 키 또는 마우스로 좌우 조작, `P` 또는 `Esc`로 일시정지.
등반 고도에 따라 5개의 레트로 시대 테마(맑은 언덕 → 새노래 하늘 → 밤의 블록 →
네온 미로 → 떨어지는 블록)로 화면·음악이 연속 전환된다. 바뀌는 것은 그림만이
아니라 **규칙**이다. 맑은 언덕은 기본 점프, 새노래 하늘은 구름 발판이 바람을
타고 좌우로 움직이며, 밤의 블록은 밟은 발판이 잠시 뒤 무너진다. 네온 미로에서는
몬스터가 유령처럼 쫓아오다가 파워 펠릿을 먹으면 도망치며(이때 잡아먹으면 점수가
배로 쌓인다), 떨어지는 블록에서는 발판이 7종 테트로미노(I·O·T·S·Z·L·J)
모양 그대로 아래로 내려오며, 옆·아래에서 맞으면 부딪힌 것으로 친다. 레벨은 무한하고 테마는 순환하므로
규칙도 함께 순환한다. 죽으면 그 판의 코인이 은행에
적립되고, SHOP에서 스킨과 파워업 업그레이드를 살 수 있다. 스킨마다 모자·안경
같은 액세서리가 함께 붙는다.

메뉴·상점 등 모든 화면은 마우스 없이도 조작된다. 화살표 키로 포커스 링을
옮기고 `Enter`로 누르며, 화면 아래에 해당 안내가 표시된다. 마우스 포인터도
OS 화살표 대신 게임 자체 커서로 그린다.

화면은 매 프레임 오프스크린 타깃에 그린 뒤 후처리 셰이더 하나를 거쳐 나온다 —
비네트는 상시, 블룸은 네온 구간에서 차오르고, 안개 디버프는 블러 + 흐르는
노이즈, 크레딧 화면은 브라운관 배럴 왜곡·색수차·형광체 마스크가 걸린다.

## 구조

화면(FSM 상태)마다 파일 하나, 그 아래에 재사용 서브시스템이 깔린다.

```
Boing/
├── src/
│   ├── main.c            # 게임 루프 + STATES[] 디스패치 + 오디오 프리로드
│   ├── game.c            # 공용 Game 컨텍스트: 상태 전환, 최고점수, 버튼 배치
│   ├── screen_splash.c   # 스플래시 (로고 + 글리프·텍스처 프리로드)
│   ├── screen_menu.c     # 메뉴 (어트랙트 데모, 도움말/미션 오버레이)
│   ├── screen_shop.c     # 상점 (스킨 + 파워업 업그레이드 카드)
│   ├── screen_credits.c  # 크레딧 (CRT 줌아웃 + 스크롤 릴)
│   ├── screen_play.c     # 플레이/일시정지/게임오버 (HUD, 결과 카드)
│   ├── player.c          # 플레이어 물리(중력, 입력, 스쿼시&스트레치)
│   ├── world.c           # 레벨 생성, 카메라, 점수, 코인/플랫폼/몬스터/파워업/디버프
│   ├── background.c      # 절차적 배경 - 5개 테마가 등반에 따라 연속 전환
│   ├── theme.c           # 런타임 팔레트/테마 테이블(THEMES[])
│   ├── sound.c           # 절차적 칩튠 + SFX (트랙 7개, 샘플 파일 없음)
│   ├── ui.c              # 마우스/키보드 UI(버튼·아이콘·텍스트, 포커스, 커서)
│   ├── svg.c             # 벡터 아이콘 — SVG 소스를 텍스처로 굽고 틴트해 그림
│   ├── font.c            # 벡터 폰트 — 글리프 49개를 아틀라스 하나로 (파일 없음)
│   ├── tex.c             # 런타임 생성 텍스처 — 노이즈/글로우 (데이터 0바이트)
│   ├── post.c            # 후처리 셰이더 — 블룸/안개/CRT/비네트 (GLSL 내장)
│   ├── settings.c        # 설정 화면(프레임률/해상도/음악·SFX 볼륨)
│   ├── persist.c         # save.json 정수 키-값 저장소
│   └── progress.c        # 코인 은행 + 스킨/업그레이드/미션
├── include/              # 헤더 (core.h·game.h 외에는 .c와 1:1)
├── lib/raylib/           # raylib (git 서브모듈)
├── lib/nanosvg/          # nanosvg (git 서브모듈, 헤더 온리)
├── lib/nanosvg_impl.c    # nanosvg 구현 TU (서브모듈 바깥, 별도 CMake 타겟)
└── CMakeLists.txt
```

세이브 파일 `save.json`은 실행 파일 옆에 런타임 생성되므로 용량 예산에 들지
않는다.

## 빌드 & 실행

```sh
# 처음 받을 때는 서브모듈(raylib, nanosvg)까지 함께 클론:
git clone --recurse-submodules https://github.com/Hiyabye/Boing.git
# 이미 일반 clone 했다면:
git submodule update --init --recursive

cmake -S . -B build      # 최초 1회, 또는 .c 파일 추가/삭제 후
cmake --build build      # ./build/Boing 생성
./build/Boing
```

`CMakeLists.txt`가 `src/*.c`를 glob하므로 `.c` 파일을 추가/삭제하면
`cmake -S . -B build`를 다시 돌려야 한다. macOS에서는 Cocoa/IOKit/CoreVideo/
OpenGL 프레임워크가 자동으로 링크된다.

clang-format이 있으면 `cmake --build build --target format`으로 소스를
일괄 정렬할 수 있고, clang-tidy가 있으면 빌드마다 자동 실행된다(둘 다
없어도 빌드된다).

## 크기 확인

기본 빌드가 `MinSizeRel` + strip이라 `build/Boing`이 곧 배포 크기다:

```sh
ls -l build/Boing   # stripped ~567KB (580,856 B), 한도 1,474,560 B
```
