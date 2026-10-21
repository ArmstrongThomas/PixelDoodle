$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$inputPath=Join-Path $repo 'discord-webhook.txt'
if(!(Test-Path -LiteralPath $inputPath)){throw 'Private webhook build input is missing.'}
$url=[IO.File]::ReadAllText($inputPath).Trim()
if($url -cnotmatch '\Ahttps://discord\.com(/api/webhooks/[0-9]+/[A-Za-z0-9_-]{20,})\z'){
    throw 'Private webhook build input is invalid.'
}
$path=$Matches[1]
[IO.File]::WriteAllText((Join-Path $repo 'private_webhook.h'),"#define PD_DISCORD_WEBHOOK_PATH `"$path`?wait=true`"`n")
