$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual Studio C tools required'}
$setup=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
Push-Location $repo
try {
    New-Item -ItemType Directory -Force build/host | Out-Null
    $command='call "'+$setup+'" -arch=x64 >nul && cl /nologo /W3 /WX /std:c11 /DPD_TEST /D_CRT_SECURE_NO_WARNINGS /Icore /Iui /Ivendor\zlib tests\test.c core\pixel.c core\storage.c core\files.c core\community.c core\dns_packet.c ui\dropdown.c vendor\zlib\*.c ui\app.c /Fobuild\host\ /Febuild\host\test.exe'
    & $env:ComSpec /d /c $command
    if($LASTEXITCODE -ne 0){throw 'Host compile failed'}
    $httpsCommand='call "'+$setup+'" -arch=x64 >nul && cl /nologo /W3 /EHsc /std:c++14 /D_CRT_SECURE_NO_WARNINGS /Itests\https /Iplatform\3ds tests\https\test.cpp platform\3ds\https_client.cpp /Fobuild\host\ /Febuild\host\https-test.exe'
    & $env:ComSpec /d /c $httpsCommand
    if($LASTEXITCODE -ne 0){throw 'HTTPS host compile failed'}
    & build/host/https-test.exe
    if($LASTEXITCODE -ne 0){throw 'HTTPS checks failed'}
    $fixture=Join-Path $repo ('build/host/run-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $fixture | Out-Null
    & build/host/test.exe $fixture
    if($LASTEXITCODE -ne 0){throw 'Host checks failed'}
} finally { Pop-Location }
