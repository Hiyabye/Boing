# Boing!

C99 + [raylib](https://www.raylib.com/)로 만든 수직 점프 게임.
["Mega Jump"](https://www.halfbrick.com/games/megajump)(Halfbrick)에서 장르적
영감을 받았으나, 코드·아트·오디오·이름은 모두 독자 제작이다. raylib은
`lib/raylib/`에 git 서브모듈로 고정되어 CMake 서브디렉터리로 소스에서 함께
빌드되므로 사용자가 따로 설치할 의존성이 없다. macOS 11 이상을 지원하는
Universal 2 앱으로 빌드되어 Apple Silicon과 Intel Mac에서 같은 `.app`을 쓴다.
(일반 `git clone`으로는 서브모듈이 비어 있으니 아래 빌드 절차를 따를 것.) 아이콘·글리프 렌더링에는
[nanosvg](https://github.com/memononen/nanosvg)(zlib 라이선스, Mikko Mononen)를
`lib/nanosvg/`에 git 서브모듈로 고정해 쓴다 — 서드파티 코드는 이 둘뿐이다.

[2PG Arcade 1.44MB 콘테스트](https://2pgarcade.com/contest-144mb.html)를 겨냥 —
배포되는 게임(실행 파일 + 런타임 + 에셋)은 **1,474,560바이트(3.5" 플로피 1장)
이하**여야 한다. 게임 에셋은 절차적으로 생성한 레트로 모던 스테레오 음악, 테마별 SFX,
배경을 쓴다. 게임 안의 아이콘도 SVG
소스 문자열을 바이너리에 박아 두고 실행 시 래스터화한다. Finder와 Dock에
필요한 앱 아이콘만 직접 만든 `resources/Boing.icns`로 번들한다. 후처리
셰이더도 GLSL 소스를 문자열로 박아 두고 실행 시 컴파일한다.

## 플레이

코인을 먹으면 위로 튀어 오르고, 화면 아래로 떨어지면 끝나는 끝없는 등반
게임이다. 화살표 키 또는 마우스로 좌우 조작하고, `P` 또는 화면의 일시정지
버튼으로 멈춘 뒤 `P` 또는 `Esc`로 재개한다.
등반 고도에 따라 7개의 레트로 시대 테마(맑은 언덕 → 새노래 하늘 → 밤의 블록 →
네온 미로 → 떨어지는 블록 → 마그마 대장간 → 재부팅 정원)로 화면·음악이 연속
전환된다. 모든 곡은 하나의 독창적인 8음 "Boing" 모티프를 공유하되, 맑은 언덕의
밝은 말렛 팝, 새노래 하늘의 3박자 리디안 하프, 밤의 블록의 느린 도리안 패드,
네온 미로의 신코페이션 일렉트로 펑크, 떨어지는 블록의 화성단음계 브레이크비트,
마그마 대장간의 108 BPM 프리지안 금속 타격·저음 드론·톰 리듬으로 완전히 다르게
편곡된다. 시대가 바뀔 때는
다음 곡을 미리 예약하고 현재 곡의 다음 마디에서 리듬을 걷어 낸 뒤, 공통 모티프를
음색과 조성 사이의 짧은 브리지로 이어 새 곡의 첫 프레임부터 시작한다. 두 곡의 완성된
리듬 섹션이 겹치지 않는다. 떨어지는 블록의 마지막 브리지는 브레이크비트의 고역과
저역을 차례로 녹이고 두 조성이 공유하는 E음을 붙든 뒤, 디지털 모티프를 금속성 대장간
타격음으로 바꾸며 마그마 대장간으로 들어간다. 다음 브리지는 공통 A음을 유지하며 그
타격음을 밝은 말렛으로 식혀 재부팅 정원에 도착한다. 재부팅 정원은 이 말렛 곡을 그대로
이어 쓰므로, 다음 맑은 언덕에서도 재생 위치가 초기화되지 않는다.
메뉴·상점은 같은 박자를 유지한 채 편곡만 자연스럽게
바뀌고, 크레딧과 결과 화면에는 전용 곡이 있다. 콤보가 오르면 테마별 리듬 레이어가
추가되고, 위험하게 추락하면 조성에 맞는 저음 박동이 울리며, 마지막 1/4 구간 전용
멜로디도 실시간 합성된다. HUD는 시대 경계마다 초기화되는 레벨 막대 대신 계속 증가하는
`HEIGHT`를 보여 준다. 한 바퀴를 돈 뒤에도 경계 순간을 표시하지 않고, 새 언덕의 안전한
도입 구간을 지난 다음에만 `CYCLE 2`를 드러낸다.
코인·점프·충돌·각 파워업·마그마 분출 효과음은 현재 테마의 악기 음색과 화면 속 x 위치를 따른다.
각 테마는 여러 속도의
원경과 플레이 영역 가장자리를 스치는 전경을 함께 그려 깊이감을 내되, 캐릭터와
위험물은 가리지 않는다. 경계에서는 화면 위쪽 하늘이 먼저 바뀌고 원경, 중경,
전경 순으로 변화한다. 지형과 강은 가라앉아 바람이 되고, 구름은 별도 시대가 아닌
`Blue Hour` 팔레트를 지나 밤의 섬으로 굳어진다. 별은 먼저 규칙적인 줄에 잠긴 뒤
직각 신호 통로로 휘고, 미로의 불빛은 떨어지는 블록 묶음으로 이어진다. 다음 경계에서
떨어지는 셀과 직선 격자는 처져 빛나는 방울로 녹고, 화산 능선과 용암 강이 있는 마그마
대장간으로 이어진다. 다음 경계에서는 용암 껍질이 검은 능선과 비옥한 흙으로 식고,
불길이던 수로가 강으로 휘며, 떠오르던 불씨가 씨앗이 되고 재가 걷혀 재부팅 정원의 작은
해가 나타난다. 정원을 떠날 때는 셀의 새싹이 묘목과 꽃가루가 되고 해가 넓어져 맑은
언덕으로 이어진다.
바뀌는 것은 그림만이 아니라 **규칙**이다. 맑은 언덕은 기본 점프, 새노래 하늘은 구름 발판이 바람을
타고 좌우로 움직이며, 밤의 블록은 밟은 발판이 잠시 뒤 무너진다. 네온 미로에서는
몬스터가 유령처럼 쫓아오다가 파워 펠릿을 먹으면 도망치며(이때 잡아먹으면 점수가
배로 쌓인다), 떨어지는 블록에서는 발판이 7종 테트로미노(I·O·T·S·Z·L·J)
모양 그대로 아래로 내려오며, 옆·아래에서 맞으면 부딪힌 것으로 친다. 이어지는 마그마
대장간의 현무암 발판은 고정 주기로 어두운 돌에서 빛나는 균열과 불꽃까지 달아오른다.
식었을 때 밟으면 평범하게 튀지만, 충분히 달아오른 순간 밟으면 강한 분출이 캐릭터를 더
높이 쏘아 올리고 그 발판은 영구히 안전한 흑요석으로 식는다. 불꽃 모양과 열린 분화구가
색 없이도 분출 시점을 알린다. 이어지는 재부팅 정원에서는 먼저 뿌리내린 I 셀을 안전하게
보여 준 뒤 더 느린 셀 조각이 내려온다. 윗면에 착지하면 그 자리에서 멈추고 같은 4칸 충돌 모양을
유지한 채 뿌리와 새싹을 틔운다. 옆이나 아래에 부딪히기 전 직접 안전한 길로 바꾸는 것이
이 시대의 규칙이다. 정원 위쪽에서 평범한 언덕 발판이 다시 나타난다. 레벨은 무한하고
7개 테마가 순환하므로 규칙도 함께 순환한다. 이미 생성된 코인·발판·몬스터·파워업은 경계를 지나도
생성 고도의 시대를 유지하며, 파워 펠릿은 네온 미로에서 생성된 유령에게만 작용한다.
각 시대의 초반과 다음 경계 직전에는 위험도가 완만해진다. 첫 바람 발판은 코인 길에
맞춘 넓고 느린 발판이고, 첫 밤 발판은 더 오래 금이 간 채 버티며, 네온 미로에서는
멀리 떨어진 느린 유령을 먼저 보여 준 뒤 코인 길 위에 파워 펠릿을 놓는다. 첫 낙하
블록은 천천히 내려오는 가로 한 줄이라 안전한 윗면과 위험한 옆면을 미리 읽을 수 있다.
첫 마그마 발판과 다음 경계 직전의 발판은 넓고 코인 길에 맞춰진 식은 흑요석이며, 도입과
완화 구간의 열기는 분출 임계값 아래로 제한된다. 죽으면 그 판의 코인이 은행에
적립되고, SHOP에서 캐릭터 색·머리장식·안경·입 액세서리를 각각 골라 사고
자유롭게 조합할 수 있다. 선택한 몸 색상은 메뉴와 버튼 등 UI 강조색에도 즉시
반영된다. 각 카테고리에는 8개 선택지가 있으며, 별도 팝업에서
구매·장착한다. 파워업 업그레이드도 같은 SHOP에서 산다. MISSIONS는 세 탭으로
나뉜다. DAILY는 날짜마다 풀에서 뽑힌 3개 누적 과제, REPEATABLE은 달성 뒤
초과 진행도를 남기고 다시 시작하는 3개 과제, ACHIEVEMENTS는 한 번만 깨는
영구 목표다. 진행도와 보상은 한 판이 끝날 때 자동으로 반영된다.

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
│   ├── main.c            # 게임 루프 + STATES[] 디스패치 + 오디오 라우팅
│   ├── game.c            # 공용 Game 컨텍스트: 상태 전환, 최고점수, 버튼 배치
│   ├── screen_splash.c   # 스플래시 (로고 + 아트·음악 프리로드)
│   ├── screen_menu.c     # 메뉴 (어트랙트 데모, 도움말/미션 오버레이)
│   ├── screen_shop.c     # 상점 (조합형 외형 팝업 + 파워업 업그레이드 카드)
│   ├── screen_credits.c  # 크레딧 (CRT 줌아웃 + 스크롤 릴)
│   ├── screen_play.c     # 플레이/일시정지/게임오버 (HUD, 결과 카드)
│   ├── player.c          # 플레이어 물리(중력, 입력, 스쿼시&스트레치)
│   ├── world.c           # 레벨 생성, 카메라, 점수, 코인/플랫폼/몬스터/파워업/디버프
│   ├── background.c      # 7개 테마의 절차적 원경·시차·전경 깊이 레이어
│   ├── theme.c           # 순수 여정 샘플 + 런타임 팔레트/테마 테이블
│   ├── sound.c           # 절차적 스테레오 음악 + 적응형 레이어 + 테마별 SFX
│   ├── ui.c              # 마우스/키보드 UI(버튼·아이콘·텍스트, 포커스, 커서)
│   ├── svg.c             # 벡터 아이콘 — SVG 소스를 텍스처로 굽고 틴트해 그림
│   ├── font.c            # 벡터 폰트 — 글리프 49개를 아틀라스 하나로 (파일 없음)
│   ├── tex.c             # 런타임 생성 텍스처 — 노이즈/글로우 (데이터 0바이트)
│   ├── post.c            # 후처리 셰이더 — 블룸/안개/CRT/비네트 (GLSL 내장)
│   ├── settings.c        # 설정 화면(프레임률/해상도/음악·SFX 볼륨)
│   ├── persist.c         # save.json 정수 키-값 저장소
│   └── progress.c        # 코인 은행 + 외형/업그레이드/미션
├── include/              # 헤더 (core.h·game.h 외에는 .c와 1:1)
├── resources/            # 직접 만든 macOS 앱 아이콘 원본 + icns
├── cmake/                # Info.plist 템플릿 + 번들 용량 검사
├── lib/raylib/           # raylib (git 서브모듈)
├── lib/nanosvg/          # nanosvg (git 서브모듈, 헤더 온리)
├── lib/nanosvg_impl.c    # nanosvg 구현 TU (서브모듈 바깥, 별도 CMake 타겟)
└── CMakeLists.txt
```

세이브 파일은 `~/Library/Application Support/Boing/save.json`에 런타임
생성되므로 용량 예산에 들지 않는다. 예전 작업 디렉터리의 `save.json`은 새
위치에 파일이 없을 때 한 번 읽고, 다음 저장 때 새 위치로 옮겨진다.

## 빌드 & 실행

```sh
# 처음 받을 때는 서브모듈(raylib, nanosvg)까지 함께 클론:
git clone --recurse-submodules https://github.com/Hiyabye/Boing.git
# 이미 일반 clone 했다면:
git submodule update --init --recursive

cmake -S . -B build      # 최초 1회, 또는 .c 파일 추가/삭제 후
cmake --build build      # ./build/Boing.app 생성
open build/Boing.app
```

`CMakeLists.txt`가 `src/*.c`를 glob하므로 `.c` 파일을 추가/삭제하면
`cmake -S . -B build`를 다시 돌려야 한다. 빌드는 `arm64;x86_64`, 최소 macOS
11.0으로 고정된다. raylib, GLFW, nanosvg는 정적으로 포함되고 Cocoa, IOKit,
CoreVideo, OpenGL 같은 macOS 기본 프레임워크만 동적으로 사용한다. Windows와
Linux는 지원하지 않는다.

clang-format이 있으면 `cmake --build build --target format`으로 소스를
일괄 정렬할 수 있고, clang-tidy가 있으면 빌드마다 자동 실행된다(둘 다
없어도 빌드된다).

## 크기 확인

기본 빌드는 `MinSizeRel` + strip + Hardened Runtime ad-hoc 서명이다. 빌드 마지막에 `.app` 안의
모든 파일을 합산해 1,474,560바이트를 넘으면 실패한다. 현재 Universal 2 번들은
1,454,627바이트이며 19,933바이트가 남는다.

```sh
find build/Boing.app -type f -exec stat -f %z {} \; | \
  awk '{ total += $1 } END { print total }'
file build/Boing.app/Contents/MacOS/Boing
```

## Developer ID 서명과 공증

기본 ad-hoc 서명은 로컬 빌드가 온전한지만 보장하며 Gatekeeper 신뢰나 Apple
공증을 제공하지 않는다. 외부 사용자에게 경고 없이 배포하려면 유료 Apple
Developer Program 가입, Keychain의 `Developer ID Application` 인증서, 공증
자격 증명이 필요하다. 인증서를 준비한 뒤 다음 순서로 ad-hoc 서명을 교체한다.
빌드를 다시 하면 ad-hoc 서명으로 돌아가므로 아래 과정은 최종 빌드에만 실행한다.

```sh
IDENTITY='Developer ID Application: YOUR NAME (TEAMID)'
PROFILE='boing-notary'
APP='build/Boing.app'
ZIP='build/Boing-1.0.0-macOS.zip'

# 최초 한 번: Team ID를 지정하고 앱 암호는 안전한 프롬프트에서 입력
xcrun notarytool store-credentials "$PROFILE" \
  --apple-id 'YOU@example.com' --team-id 'TEAMID'

# Hardened Runtime + 안전한 타임스탬프로 Developer ID 서명
codesign --force --options runtime --timestamp --sign "$IDENTITY" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

# 공증용 zip 제출
rm -f "$ZIP"
ditto -c -k --keepParent "$APP" "$ZIP"
xcrun notarytool submit "$ZIP" --keychain-profile "$PROFILE" --wait

# 공증 티켓을 앱에 붙인 뒤 최종 배포 zip을 다시 생성
xcrun stapler staple "$APP"
xcrun stapler validate "$APP"
spctl --assess --type execute --verbose=4 "$APP"
cmake -DBUNDLE="$APP" -DLIMIT=1474560 -P cmake/CheckBundleSize.cmake
rm -f "$ZIP"
ditto -c -k --keepParent "$APP" "$ZIP"
```
