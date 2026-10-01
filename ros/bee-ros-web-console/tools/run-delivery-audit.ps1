param(
    [string]$Board = '192.168.127.10',
    [string]$User = 'sunrise',
    [string]$KeyPath = 'C:\Users\Bee\.ssh\rdk_x5_bee_console_deploy_rsa'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$auditScript = Join-Path $PSScriptRoot 'delivery_audit.py'
$patrolTest = Join-Path $PSScriptRoot 'test_patrol_contract.py'
$remoteStage = '/tmp/bee-delivery-audit'
$sshTarget = "$User@$Board"

if (-not (Test-Path -LiteralPath $auditScript)) {
    throw "Audit script not found: $auditScript"
}
if (-not (Test-Path -LiteralPath $KeyPath)) {
    throw "SSH key not found: $KeyPath"
}

python -m py_compile $auditScript $patrolTest
if ($LASTEXITCODE -ne 0) { throw 'Python syntax check failed' }
if (Get-Command node -ErrorAction SilentlyContinue) {
    node --check (Join-Path $projectRoot 'frontend\live.js')
    if ($LASTEXITCODE -ne 0) { throw 'live.js syntax check failed' }
    node --check (Join-Path $projectRoot 'frontend\login.js')
    if ($LASTEXITCODE -ne 0) { throw 'login.js syntax check failed' }
}

ssh -i $KeyPath -o BatchMode=yes $sshTarget "rm -rf $remoteStage && mkdir -p $remoteStage"
if ($LASTEXITCODE -ne 0) { throw 'Remote staging directory creation failed' }
scp -i $KeyPath -o BatchMode=yes $auditScript "${sshTarget}:$remoteStage/delivery_audit.py"
if ($LASTEXITCODE -ne 0) { throw 'Audit script upload failed' }
scp -i $KeyPath -o BatchMode=yes $patrolTest "${sshTarget}:$remoteStage/test_patrol_contract.py"
if ($LASTEXITCODE -ne 0) { throw 'Patrol contract script upload failed' }
ssh -i $KeyPath -o BatchMode=yes $sshTarget "sudo -n install -m 0755 $remoteStage/delivery_audit.py /opt/bee-ros-web-console/tools/delivery_audit.py && sudo -n install -m 0755 $remoteStage/test_patrol_contract.py /opt/bee-ros-web-console/tools/test_patrol_contract.py && rm -rf $remoteStage && python3 /opt/bee-ros-web-console/tools/delivery_audit.py"
if ($LASTEXITCODE -ne 0) { throw 'Remote delivery audit failed; inspect the failed checks above' }
