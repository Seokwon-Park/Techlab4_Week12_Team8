# Git 소스 인덱싱

`PublishSymbols.bat`는 소스를 공유 폴더에 복사하지 않는다. 빌드 소스와
PDB 체크섬, Git HEAD의 파일을 비교한 뒤 원격 주소, 전체 커밋 해시, 파일 경로,
줄바꿈 방식과 SHA256만 PDB의 `srcsrv` 스트림에 기록한다.

## 빌드를 게시할 때

1. 빌드에 들어갈 소스를 커밋한다. 서브모듈 소스는 해당 서브모듈의 커밋을 사용한다.
2. 빌드한다. `DebugPublishSymbols` / `ReleasePublishSymbols`는 게시 스크립트가 자동 실행된다.
   일반 구성은 `PublishSymbols.bat Debug "용도" --no-pause`처럼 실행한다.
3. **배포 전에 소스 커밋을 원격 저장소에 push한다.** 인덱싱 자체는 push하지 않으며
   원격 커밋 존재 여부를 네트워크로 확인하지 않는다.

빌드 후 수정된 파일, HEAD와 다른 빌드 소스, Git에 없는 일반 소스는 등록을 중단한다.
`Intermediate/Generated/`의 HHT 생성 파일은 PDB 체크섬 검증 후 Git 인덱싱에서 제외한다.
이 생성 파일은 다른 PC에서 소스 서버를 통해 자동 복원할 수 없다. 기존 게시 PDB와 기존 `Sources`
사본은 자동으로 변경하거나 삭제하지 않는다. 새 방식은 재게시한 PDB에 적용된다.

## 디버깅 PC에서 한 번 설정

Git for Windows를 설치하고, 비공개 저장소라면 그 PC의 Git 인증을 먼저 설정한다.
다음 명령은 추출 도구 두 개를 `%LOCALAPPDATA%\HitoriDebug\SourceTools`에 설치하고
사용자 환경 변수 `HITORI_GIT_SOURCE_COMMAND`를 설정한다. 소스는 가져오지 않는다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\SetupGitSourceDebugging.ps1
```

Visual Studio/WinDbg를 다시 시작한다. Visual Studio의 디버깅 옵션에서 Source Server
지원을 활성화하고, 이 프로젝트 PDB의 추출 명령 실행 요청을 확인한다.
WinDbg에서는 소스 경로에 `srv*`를 포함한다. 전역 `srcsrv.ini`의 명령 신뢰 목록은
설정 스크립트가 변경하지 않는다.

디버거가 소스를 요청하면 추출 도구가 필요한 커밋을 shallow fetch한 뒤 **요청된 파일만**
소스 캐시에 쓴다. Git 객체는 `%LOCALAPPDATA%\HitoriDebug\Git`에 보관한다.
커밋 객체를 가져오는 과정에서 다른 파일의 Git 객체도 다운로드될 수 있다.
전체 작업 트리 checkout은 하지 않는다. LF/CRLF를 빌드 당시 방식으로 복원하고
SHA256을 검증한다. 이후에는 캐시를 재사용한다.

## 검증

x64 Visual Studio Developer PowerShell에서:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\TestSourceIndexing.ps1
```

테스트는 `Build\SourceIndexTests`의 로컬 Git 원격만 사용한다. 공유 심볼 저장소,
사용자 환경 변수, 전역 SrcSrv 신뢰 설정을 변경하지 않는다.
