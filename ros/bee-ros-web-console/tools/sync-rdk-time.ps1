param(
    [string]$HostName = '192.168.127.10',
    [string]$UserName = 'sunrise',
    [string]$IdentityFile = 'C:\Users\Bee\.ssh\rdk_x5_bee_console_deploy_rsa',
    [Nullable[DateTime]]$TrustedUtc,
    [switch]$UseWindowsClock
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $IdentityFile)) {
    throw "SSH identity file not found: $IdentityFile"
}

if (-not $TrustedUtc.HasValue -and -not $UseWindowsClock) {
    throw 'Specify -TrustedUtc explicitly, or pass -UseWindowsClock after confirming the Windows clock is correct.'
}

$selectedUtc = if ($TrustedUtc.HasValue) { $TrustedUtc.Value.ToUniversalTime() } else { [DateTime]::UtcNow }
$trustedEpochMs = [DateTimeOffset]$selectedUtc
$trustedEpochMs = $trustedEpochMs.ToUnixTimeMilliseconds()
$target = "$UserName@$HostName"
$remote = "set -e; sudo -n /usr/local/sbin/bee-time-sync --epoch-ms $trustedEpochMs --source browser; sudo -n systemctl start bee-time-check.service; date -Ins; cat /run/bee-time-guard/status.json; timedatectl | sed -n '1,8p'"

& ssh -i $IdentityFile -o BatchMode=yes -o ConnectTimeout=8 $target $remote
if ($LASTEXITCODE -ne 0) {
    throw "RDK time synchronization failed with exit code $LASTEXITCODE"
}
