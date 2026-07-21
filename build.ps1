$ErrorActionPreference = "Stop"

$Esp32BoardUrl = "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
$Targets = @(
  @{ Sketch = "MicriOS-C3"; Fqbn = "esp32:esp32:esp32c3:PartitionScheme=huge_app" },
  @{ Sketch = "MicriOS-T-Display"; Fqbn = "esp32:esp32:esp32:PartitionScheme=huge_app" },
  @{ Sketch = "MicriOS-C3-Headless"; Fqbn = "esp32:esp32:esp32c3:PartitionScheme=huge_app" },
  @{ Sketch = "MicriOS-S3-Zero-Headless"; Fqbn = "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=enabled,UploadSpeed=921600" }
)

function Invoke-ArduinoCli {
  param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
  )

  Write-Host ""
  Write-Host "arduino-cli $($Arguments -join ' ')"
  & arduino-cli @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "arduino-cli failed: $($Arguments -join ' ')"
  }
}

function Sync-SharedCode {
  param([string]$Sketch)

  $source = Join-Path $PSScriptRoot "shared"
  $destParent = Join-Path $PSScriptRoot "$Sketch\src"
  $dest = Join-Path $destParent "shared"

  if (-not (Test-Path -LiteralPath $source)) {
    throw "Shared source folder not found: $source"
  }

  $rootFull = [System.IO.Path]::GetFullPath($PSScriptRoot)
  $destFull = [System.IO.Path]::GetFullPath($dest)
  if (-not $destFull.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to sync shared code outside repo: $destFull"
  }

  if (Test-Path -LiteralPath $dest) {
    Remove-Item -LiteralPath $dest -Recurse -Force
  }
  New-Item -ItemType Directory -Path $destParent -Force | Out-Null
  Copy-Item -LiteralPath $source -Destination $dest -Recurse
  Write-Host "Synced shared code -> $Sketch\src\shared"
}

Write-Host "Preparing Arduino CLI dependencies"
$oldErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$configInitOutput = & arduino-cli config init 2>&1
$configInitExit = $LASTEXITCODE
$ErrorActionPreference = $oldErrorActionPreference
if ($configInitExit -ne 0) {
  if ($configInitOutput -match "already exists") {
    Write-Host "Arduino CLI config already exists; continuing"
  } else {
    $configInitOutput | ForEach-Object { Write-Host $_ }
    throw "arduino-cli config init failed"
  }
}

Invoke-ArduinoCli config set board_manager.additional_urls $Esp32BoardUrl
Invoke-ArduinoCli core update-index
Invoke-ArduinoCli core install esp32:esp32
Invoke-ArduinoCli lib install U8g2 NimBLE-Arduino TFT_eSPI ArduinoJson

foreach ($target in $Targets) {
  Sync-SharedCode -Sketch $target.Sketch
}

Push-Location $PSScriptRoot
try {
  foreach ($target in $Targets) {
    Write-Host ""
    Write-Host "Building $($target.Sketch)"
    Invoke-ArduinoCli compile --fqbn $target.Fqbn $target.Sketch
  }
} finally {
  Pop-Location
}

Write-Host ""
Write-Host "All MicriOS targets built successfully"
