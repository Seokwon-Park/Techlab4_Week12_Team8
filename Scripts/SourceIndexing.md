# 소스 사본 기반 PDB 소스 인덱싱

`PublishSymbols.bat`는 빌드 당시 소스 사본을 심볼 저장소의 `Sources` 폴더에
보관하고, PDB의 `srcsrv` 스트림에 해당 사본의 경로를 기록한다.
Git 커밋 조회, fetch, checkout 및 디버깅용 Git 설정은 사용하지 않는다.
게시 설명에 붙이는 Git 커밋 표시는 참고 정보이며 소스 조회에는 사용하지 않는다.

## 저장 구조

```text
\\DESKTOP-TVQKHMM\HitoriSymbols\
  HitoriEditor.exe\...
  HitoriEditor.pdb\...
  Sources\<파일 내용 SHA256>\<파일명>
```

파일 내용이 같으면 기존 사본을 재사용하고, 내용이 다르면 다른 해시 폴더에 저장한다.
빌드 소스를 PDB 체크섬과 비교한 뒤 모든 파일이 일치해야 사본 저장을 시작한다.
기존 사본이 손상되면 게시를 실패시키며 덮어쓰지 않는다.
Git에 커밋되지 않은 파일도 빌드 당시 내용과 일치하면 보관할 수 있다.
`Intermediate/Generated/` 생성 헤더도 포함한다. 저장소 밖의 SDK 파일,
`Source/ThirdParty/`, `Build/`의 중간 산출물은 제외한다.

## 게시

Windows SDK의 Debugging Tools for Windows가 필요하다.
`DebugPublishSymbols` / `ReleasePublishSymbols` 빌드 또는 다음 명령으로 게시한다.

```bat
PublishSymbols.bat Debug "용도" --no-pause
```

기본 저장소는 `\\DESKTOP-TVQKHMM\HitoriSymbols`이며 `HITORI_SYMBOL_STORE`로
변경할 수 있다. 공유 저장소에 게시하지 못하면 `%USERPROFILE%\HitoriDebug\Symbols`로
대체하고, PDB의 소스 경로도 해당 저장소의 `Sources`로 다시 기록한다.
소스 체크섬 불일치나 파일 누락은 대체 저장소로 재시도하지 않고 중단한다.
로컬 저장소의 소스 경로는 다른 PC에서 그대로 접근할 수 없으므로 팀 배포 전에
공유 저장소에 정상 게시되었는지 확인한다.

## 디버깅

디버깅 PC에서 공유 심볼 저장소를 설정하고 Visual Studio의 Source Server 지원을
활성화한다. WinDbg에서는 소스 경로에 `srv*`를 포함한다. 해당 PC에서 PDB에 기록된
`Sources` 경로에 읽기 권한이 있어야 한다. 소스 조회를 위한 별도 명령은 실행하지 않는다.

기존 Git 방식으로 게시된 PDB는 자동 변환되지 않는다. 새로 인덱싱해 게시한 PDB부터
사본 방식을 사용한다. 이 변경은 기존 공유 저장소와 사용자 환경 변수를 변경하지 않는다.
이전 `HITORI_GIT_SOURCE_COMMAND` 설정은 새 PDB에서 사용하지 않는다.

## 검증

x64 Visual Studio Developer PowerShell에서 다음 명령을 실행한다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\TestSourceIndexing.ps1
```

`Build/SourceIndexTests` 안의 격리된 소스와 저장소로 체크섬, 생성 헤더, 공백 경로,
중복 게시, 원본 삭제 후 조회, 누락·변경·손상 거부, 대체 경로와 배치 게시를 확인한다.
팀 공유 저장소에는 게시하지 않는다.
