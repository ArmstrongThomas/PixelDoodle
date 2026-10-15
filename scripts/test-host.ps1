$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual Studio C tools required'}
$setup=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/host | Out-Null
    $command='call "'+$setup+'" -arch=x64 >nul && cl /nologo /W3 /WX /std:c11 /DPD_TEST /D_CRT_SECURE_NO_WARNINGS /Icore /Iui /Ivendor\zlib tests\test.c core\pixel.c core\storage.c core\files.c vendor\zlib\*.c ui\app.c /Fobuild\host\ /Febuild\host\test.exe'
    & $env:ComSpec /d /c $command
    if($LASTEXITCODE -ne 0){throw 'Host compile failed'}
    $fixture=Join-Path $repo ('build/host/run-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $fixture | Out-Null
    & build/host/test.exe $fixture
    if($LASTEXITCODE -ne 0){throw 'Host checks failed'}
} finally { Pop-Location }
