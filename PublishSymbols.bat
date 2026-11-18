@echo off
chcp 65001 > nul
rem 빌드된 HitoriEditor.exe/.pdb를 심볼 창고에 등록한다.
rem 내 PC를 떠나는 빌드(배포, 시연, 팀원 전달)에만 실행한다.
rem 사용법: PublishSymbols.bat [Debug|Release|Executable ...] ["용도"]  (기본값: Debug)
rem 용도를 인자로 주지 않으면 실행 중에 입력받는다.
setlocal
cd /d "%~dp0"

rem ① 구성
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"
set "BIN_DIR=Build\Bin\%CONFIG%-windows-x86_64"

rem ①-2 용도 (인자가 없으면 입력받고, Enter만 누르면 생략)
set "NOTE=%~2"
if "%NOTE%"=="" set /p "NOTE=용도 (예: 11/14 중간시연): "
set "COMMENT=%CONFIG%"
if not "%NOTE%"=="" set "COMMENT=%CONFIG% - %NOTE%"

rem ② 도구와 결과물 확인
set "SYMSTORE=%ProgramFiles(x86)%\Windows Kits\10\Debuggers\x64\symstore.exe"
if not exist "%SYMSTORE%" goto :NoSymstore
if not exist "%BIN_DIR%\HitoriEditor.pdb" goto :NoBuild


rem 기본 저장소는 팀 공유 폴더다.
rem HITORI_SYMBOL_STORE가 설정된 PC에서는 해당 경로를 먼저 사용한다.
rem 첫 저장소에 등록하지 못하면 현재 사용자의 로컬 저장소에 저장한다.

rem ③ 심볼 저장소
set "LOCAL_STORE=%USERPROFILE%\HitoriDebug\Symbols"
set "STORE=\\DESKTOP-TVQKHMM\HitoriSymbols"
if defined HITORI_SYMBOL_STORE set "STORE=%HITORI_SYMBOL_STORE%"

rem ④ 커밋 해시 (커밋 안 된 변경이 있으면 -dirty 표시)
set "COMMIT=unknown"
for /f %%H in ('git rev-parse --short HEAD 2^>nul') do set "COMMIT=%%H"
git diff --quiet HEAD 2>nul || set "COMMIT=%COMMIT%-dirty"

rem ⑤ 심볼 등록
echo [심볼 등록] %COMMENT% / %COMMIT%

rem 먼저 선택된 저장소에 EXE와 PDB를 등록한다.
rem 첫 시도의 상세 출력은 숨기고, 실패하면 로컬로 전환했다는 메시지를 남긴다.
rem 로컬 등록까지 실패하면 빌드 후 이벤트에 실패 코드를 반환한다.
call :RegisterSymbols >nul 2>&1
if not errorlevel 1 goto :Done

if /I "%STORE%"=="%LOCAL_STORE%" goto :Fail
echo [심볼 등록] %STORE% 등록 실패. 로컬 저장소로 다시 시도합니다.
set "STORE=%LOCAL_STORE%"
call :RegisterSymbols
if errorlevel 1 goto :Fail

:Done
echo 완료: %STORE%
if /I not "%~3"=="--no-pause" pause
exit /b 0

:NoSymstore
echo [오류] symstore.exe가 없다. Windows SDK에 Debugging Tools for Windows를 설치한다.
if /I not "%~3"=="--no-pause" pause
exit /b 1

:NoBuild
echo [오류] %BIN_DIR%에 빌드 결과물이 없다. %CONFIG% 구성으로 먼저 빌드한다.
if /I not "%~3"=="--no-pause" pause
exit /b 1

:Fail
echo [오류] symstore 등록에 실패했다.
if /I not "%~3"=="--no-pause" pause
exit /b 1

:RegisterSymbols
rem 현재 STORE 경로에 EXE와 PDB를 등록한다.
rem 저장소 폴더가 없으면 생성을 시도하고, 사용할 수 없으면 실패를 반환한다.
rem EXE와 PDB는 별도로 등록된다. PDB 등록이 실패하면 EXE만 남을 수 있다.
rem 호출부는 로컬 저장소로 전환할 때 두 파일을 모두 다시 등록한다.
if not exist "%STORE%\." mkdir "%STORE%"
if not exist "%STORE%\." exit /b 1

"%SYMSTORE%" add /f "%BIN_DIR%\HitoriEditor.exe" /s "%STORE%" /t Hitori /v "%COMMIT%" /c "%COMMENT%"
if errorlevel 1 exit /b 1

"%SYMSTORE%" add /f "%BIN_DIR%\HitoriEditor.pdb" /s "%STORE%" /t Hitori /v "%COMMIT%" /c "%COMMENT%"
if errorlevel 1 exit /b 1
exit /b 0