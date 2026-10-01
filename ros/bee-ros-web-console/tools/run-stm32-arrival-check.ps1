param(
    [string]$HostName = '192.168.127.10',
    [string]$UserName = 'sunrise',
    [string]$IdentityFile = 'C:\Users\Bee\.ssh\rdk_x5_bee_console_deploy_rsa',
    [string]$Device = '/dev/ttyS5',
    [int]$Baud = 115200,
    [double]$ProbeSeconds = 0
)

$ErrorActionPreference = 'Stop'
$target = "$UserName@$HostName"
$remoteCommand = "cd /opt/bee-ros-web-console/tools && python3 stm32_arrival_readiness.py --device '$Device' --baud $Baud --probe-seconds $ProbeSeconds --output /opt/bee-ros-web-console/data/stm32-arrival-readiness.json"
& ssh -i $IdentityFile -o BatchMode=yes -o ConnectTimeout=8 $target $remoteCommand
if ($LASTEXITCODE -ne 0) { throw "STM32 arrival check failed with exit code $LASTEXITCODE" }
