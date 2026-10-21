$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual Studio C tools required'}
$setup=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
Push-Location $repo
try {
 New-Item -ItemType Directory -Force build/https-live | Out-Null
 $command='call "'+$setup+'" -arch=x64 >nul && cl /nologo /O2 /EHsc /std:c++14 /D_CRT_SECURE_NO_WARNINGS /DMBEDTLS_NET_C /DMBEDTLS_CONFIG_FILE=\"doodle_mbedtls_config.h\" /Itests\https /Iplatform\3ds /Ivendor\mbedtls\include tests\https\live.cpp platform\3ds\https_client.cpp vendor\mbedtls\library\*.c /Fobuild\https-live\ /Febuild\https-live\probe.exe /link bcrypt.lib ws2_32.lib'
 & $env:ComSpec /d /c $command
 if($LASTEXITCODE -ne 0){throw 'Live HTTPS probe compile failed'}
 & build/https-live/probe.exe
 if($LASTEXITCODE -ne 0){throw 'Live HTTPS probe failed'}
} finally { Pop-Location }
