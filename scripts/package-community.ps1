param([string]$Version='0.3.3',[switch]$PrivateOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$name="PixelDoodle-Community-$Version"
$out=Join-Path $repo 'build'
$privateStage=Join-Path $out "$name-app-private-staging"
$publicStage=Join-Path $out "$name-public-staging"
$privateZip=Join-Path $out "$name-app-private.zip"
$publicZip=Join-Path $out "$name-public.zip"
$reserved=@($privateStage,$privateZip)
if(!$PrivateOnly){$reserved+=@($publicStage,$publicZip)}
foreach($path in $reserved){
    if(Test-Path -LiteralPath $path){throw "Refusing to replace existing package path: $path"}
}

New-Item -ItemType Directory -Path $privateStage | Out-Null
if(!$PrivateOnly){New-Item -ItemType Directory -Path $publicStage | Out-Null}
$privatePhp=Join-Path $privateStage 'community-php'
New-Item -ItemType Directory -Path $privatePhp | Out-Null
foreach($dir in @('app','bootstrap','config','deploy','resources','routes','vendor')){
    Copy-Item -LiteralPath (Join-Path $repo "community-php/$dir") -Destination $privatePhp -Recurse
}
New-Item -ItemType Directory -Force -Path (Join-Path $privatePhp 'database') | Out-Null
foreach($dir in @('database/factories','database/migrations','database/seeders')){
    Copy-Item -LiteralPath (Join-Path $repo "community-php/$dir") -Destination (Join-Path $privatePhp 'database') -Recurse
}
foreach($file in @('database/.gitignore','database/gallery.sql','database/moderation.sql','database/community.sql','database/community-v5.sql')){
    Copy-Item -LiteralPath (Join-Path $repo "community-php/$file") -Destination (Join-Path $privatePhp 'database')
}
foreach($file in @('artisan','composer.json','composer.lock','.env.example')){
    Copy-Item -LiteralPath (Join-Path $repo "community-php/$file") -Destination $privatePhp
}
foreach($dir in @('storage/app/private','storage/app/public','storage/framework/cache/data','storage/framework/cache/locks','storage/framework/sessions','storage/framework/testing','storage/framework/views','storage/logs','bootstrap/cache')){
    New-Item -ItemType Directory -Force -Path (Join-Path $privatePhp $dir) | Out-Null
}
foreach($file in @('storage/app/.gitignore','storage/app/private/.gitignore','storage/app/public/.gitignore','storage/framework/.gitignore','storage/framework/cache/.gitignore','storage/framework/cache/data/.gitignore','storage/framework/cache/locks/.gitignore','storage/framework/sessions/.gitignore','storage/framework/testing/.gitignore','storage/framework/views/.gitignore','storage/logs/.gitignore','bootstrap/cache/.gitignore')){
    $source=Join-Path $repo "community-php/$file"
    if(Test-Path -LiteralPath $source){$dest=Join-Path $privatePhp $file;New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null;Copy-Item -LiteralPath $source -Destination $dest}
}

# Never ship machine-specific compiled config/routes or provider caches.
Get-ChildItem -LiteralPath (Join-Path $privatePhp 'bootstrap/cache') -File | Where-Object { $_.Extension -eq '.php' } | ForEach-Object { Remove-Item -LiteralPath $_.FullName }

$privateCommunity=Join-Path $privateStage 'community'
New-Item -ItemType Directory -Path $privateCommunity | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'community/public') -Destination $privateCommunity -Recurse
Copy-Item -LiteralPath (Join-Path $repo 'docs/SETUP-GUIDE.md') -Destination $privateStage
Copy-Item -LiteralPath (Join-Path $repo 'docs/DISCORD-REVIEW-BOT.md') -Destination $privateStage
Copy-Item -LiteralPath (Join-Path $repo 'docs/HANDS-ON-CHECKLIST.md') -Destination $privateStage
Copy-Item -LiteralPath (Join-Path $repo 'community-php/README.md') -Destination (Join-Path $privateStage 'README-PHP.md')

$helper=@'
@echo off
setlocal
title Pixel Doodle Community updater
set "APP_DIR=%~dp0community-php"
if not exist "%APP_DIR%\artisan" (echo Could not find community-php\artisan beside this script.& goto failed)
if not exist "%APP_DIR%\.env" (echo Copy .env.example to .env and configure it first.& echo This script never creates or overwrites .env or APP_KEY.& goto failed)
where php >nul 2>&1
if errorlevel 1 (echo PHP CLI was not found on PATH.& goto failed)
echo.
echo This clears the config cache and runs Laravel plus gallery migrations.
echo Back up both private databases before continuing. The gallery migration
echo upgrades schema 0-4 to schema 5 without changing the auth database.
echo Your .env, APP_KEY, current pairing setting and device grants are preserved.
echo.
choice /C YN /N /M "Continue? [Y/N] "
if errorlevel 2 goto cancelled
pushd "%APP_DIR%"
php artisan config:clear
if errorlevel 1 goto command_failed
php artisan route:clear
if errorlevel 1 goto command_failed
php artisan view:clear
if errorlevel 1 goto command_failed
php artisan package:discover
if errorlevel 1 goto command_failed
php artisan migrate --force
if errorlevel 1 goto command_failed
php artisan gallery:migrate
if errorlevel 1 goto command_failed
php artisan config:clear
if errorlevel 1 goto command_failed
popd
echo.
echo Migration commands completed. Review the hands-on checklist before enabling
echo new handheld permissions. Restart Apache if opcode caching hides the update.
goto done
:command_failed
set "RESULT=%ERRORLEVEL%"
popd
echo An update command failed with exit code %RESULT%; inspect logs before retrying.
goto failed
:cancelled
echo Update cancelled; no commands were run.
goto done
:failed
echo No further update steps were run.
:done
echo.
pause
endlocal
'@
Set-Content -LiteralPath (Join-Path $privateStage 'Update-Community.bat') -Value $helper -Encoding Ascii
$publicNote=if($PrivateOnly){'The public webroot package is not included in this private-only archive.'}else{'The accompanying public webroot ZIP contains only the Apache front controller and configuration.'}
$release=@"
# Pixel Doodle Community $Version

This private app package includes the current Laravel application and community
website assets. This test overlay changes device API authentication responses:
missing, expired, or revoked grants return HTTP 401; valid grants missing a
required scope return HTTP 403. It introduces no schema or configuration change.

This is a local test handoff. It was not deployed. $publicNote
Read SETUP-GUIDE.md and HANDS-ON-CHECKLIST.md before any installation. Preserve
the existing .env, APP_KEY, auth/gallery databases, assets and device grants.
"@
Set-Content -LiteralPath (Join-Path $privateStage 'RELEASE-NOTES.md') -Value $release -Encoding UTF8

if(!$PrivateOnly){
    Copy-Item -LiteralPath (Join-Path $repo 'community-php/deploy/index.php') -Destination (Join-Path $publicStage 'index.php')
    Copy-Item -LiteralPath (Join-Path $repo 'community-php/deploy/.htaccess') -Destination (Join-Path $publicStage '.htaccess')
    Copy-Item -LiteralPath (Join-Path $repo 'community-php/deploy/apache.conf') -Destination (Join-Path $publicStage 'apache.conf')
}
$tar='C:\Windows\System32\tar.exe'
& $tar -a -c -f $privateZip -C $privateStage .
if($LASTEXITCODE -ne 0){throw 'Private app archive creation failed.'}
if(!$PrivateOnly){
    & $tar -a -c -f $publicZip -C $publicStage .
    if($LASTEXITCODE -ne 0){throw 'Public webroot archive creation failed.'}
}

$verify=Join-Path $out "$name-extracted-verification"
if(Test-Path -LiteralPath $verify){throw "Refusing to replace verification path: $verify"}
New-Item -ItemType Directory -Path $verify | Out-Null
Expand-Archive -LiteralPath $privateZip -DestinationPath (Join-Path $verify 'private')
$required=@(
    (Join-Path $verify 'private/community-php/artisan'),
    (Join-Path $verify 'private/community-php/vendor/autoload.php'),
    (Join-Path $verify 'private/community-php/database/community-v5.sql'),
    (Join-Path $verify 'private/community/public/index.html'),
    (Join-Path $verify 'private/community/public/app.js'),
    (Join-Path $verify 'private/Update-Community.bat')
)
if(!$PrivateOnly){
    Expand-Archive -LiteralPath $publicZip -DestinationPath (Join-Path $verify 'public')
    $required+=@((Join-Path $verify 'public/index.php'),(Join-Path $verify 'public/apache.conf'))
}
foreach($path in $required){if(!(Test-Path -LiteralPath $path)){throw "Package verification missing required path: $path"}}
$sensitiveName='^(?:\.env|.*\.(?:sqlite(?:3)?|db|log)|(?:identity|recovery|seed|credential)(?:[-_].*)?\.(?:txt|bin))$'
if(!$PrivateOnly -and (Get-ChildItem (Join-Path $verify 'public') -Recurse -File | Where-Object { $_.Name -match '^(?:artisan|autoload\.php)$' -or $_.Name -match $sensitiveName })){throw 'Public ZIP contains private runtime material.'}
if(Get-ChildItem (Join-Path $verify 'private') -Recurse -File | Where-Object { $_.Name -match $sensitiveName }){throw 'Private ZIP contains credentials, databases, or logs.'}
$privateHash=(Get-FileHash -LiteralPath $privateZip -Algorithm SHA256).Hash
if($PrivateOnly){@("$privateHash  $([IO.Path]::GetFileName($privateZip))") | Set-Content -LiteralPath (Join-Path $out "$name-SHA256SUMS.txt") -Encoding Ascii}
else{$publicHash=(Get-FileHash -LiteralPath $publicZip -Algorithm SHA256).Hash;@("$privateHash  $([IO.Path]::GetFileName($privateZip))","$publicHash  $([IO.Path]::GetFileName($publicZip))") | Set-Content -LiteralPath (Join-Path $out "$name-SHA256SUMS.txt") -Encoding Ascii}
if($PrivateOnly){Get-Item $privateZip | Select-Object FullName,Length}else{Get-Item $privateZip,$publicZip | Select-Object FullName,Length}
Get-Content (Join-Path $out "$name-SHA256SUMS.txt")
Write-Output "Extracted package tree verified at $verify"
