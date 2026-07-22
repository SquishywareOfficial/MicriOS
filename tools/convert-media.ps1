param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$InputFile,

    [Parameter(Mandatory = $true, Position = 1)]
    [string]$OutputFile,

    [ValidateRange(0, 86400)]
    [int]$PreviewSeconds = 0,

    [ValidateRange(1, 30)]
    [int]$FramesPerSecond = 12,

    [ValidateRange(2, 31)]
    [int]$JpegQuality = 10,

    [ValidateRange(-30, -12)]
    [double]$AudioLoudness = -20,

    [ValidateRange(-6, -1)]
    [double]$AudioTruePeak = -2
)

$ErrorActionPreference = "Stop"

$ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
$ffprobe = (Get-Command ffprobe -ErrorAction Stop).Source
$inputPath = (Resolve-Path -LiteralPath $InputFile).Path
$outputPath = [System.IO.Path]::GetFullPath($OutputFile)
$outputDirectory = Split-Path -Parent $outputPath
if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$culture = [System.Globalization.CultureInfo]::InvariantCulture
$targetLoudness = $AudioLoudness.ToString("0.##", $culture)
$targetTruePeak = $AudioTruePeak.ToString("0.##", $culture)
$audioStream = & $ffprobe -v error -select_streams a:0 -show_entries stream=index -of csv=p=0 -- $inputPath
if ($LASTEXITCODE -ne 0) {
    throw "ffprobe could not inspect the input audio stream"
}
$hasAudio = -not [string]::IsNullOrWhiteSpace(($audioStream -join ""))

$audioFilter = $null
if ($hasAudio) {
    $analysisFilter = "aformat=channel_layouts=mono,loudnorm=I=${targetLoudness}:LRA=11:TP=${targetTruePeak}:print_format=json"
    $analysisArguments = @(
        "-hide_banner",
        "-nostats",
        "-i", $inputPath
    )
    if ($PreviewSeconds -gt 0) {
        $analysisArguments += @("-t", $PreviewSeconds.ToString())
    }
    $analysisArguments += @(
        "-map", "0:a:0",
        "-vn",
        "-sn",
        "-dn",
        "-af", $analysisFilter,
        "-f", "null",
        "NUL"
    )

    Write-Host "Measuring mono audio loudness..."
    $analysisLog = [System.IO.Path]::GetTempFileName()
    $previousErrorAction = $ErrorActionPreference
    try {
        # Windows PowerShell wraps normal FFmpeg stderr as NativeCommandError.
        $ErrorActionPreference = "Continue"
        & $ffmpeg @analysisArguments 2> $analysisLog | Out-Null
        $analysisExitCode = $LASTEXITCODE
        $ErrorActionPreference = $previousErrorAction
        if ($analysisExitCode -ne 0) {
            throw "FFmpeg loudness analysis failed:`n$(Get-Content -LiteralPath $analysisLog -Raw)"
        }

        $analysisText = Get-Content -LiteralPath $analysisLog -Raw
        $jsonMatch = [regex]::Match($analysisText, '(?s)\{\s*"input_i".*?\}')
        if (-not $jsonMatch.Success) {
            throw "FFmpeg did not return loudness measurements"
        }
        $measurement = $jsonMatch.Value | ConvertFrom-Json
        if ($measurement.input_i -eq "-inf" -or $measurement.input_tp -eq "-inf") {
            throw "The input audio is silent or cannot be measured"
        }
    }
    finally {
        $ErrorActionPreference = $previousErrorAction
        if (Test-Path -LiteralPath $analysisLog) {
            Remove-Item -LiteralPath $analysisLog -Force
        }
    }

    $audioFilter = "aformat=channel_layouts=mono,loudnorm=I=${targetLoudness}:LRA=11:TP=${targetTruePeak}" +
        ":measured_I=$($measurement.input_i):measured_LRA=$($measurement.input_lra)" +
        ":measured_TP=$($measurement.input_tp):measured_thresh=$($measurement.input_thresh)" +
        ":offset=$($measurement.target_offset):linear=true:print_format=summary"
    Write-Host "Audio: $($measurement.input_i) LUFS -> $targetLoudness LUFS, peak ceiling $targetTruePeak dBTP"
}

$videoFilter = "fps=$FramesPerSecond,scale=320:176:force_original_aspect_ratio=decrease,pad=320:176:(ow-iw)/2:(oh-ih)/2:black"
$arguments = @(
    "-hide_banner",
    "-y",
    "-i", $inputPath
)
if ($PreviewSeconds -gt 0) {
    $arguments += @("-t", $PreviewSeconds.ToString())
}
$arguments += @(
    "-map", "0:v:0",
    "-vf", $videoFilter,
    "-c:v", "mjpeg",
    "-q:v", $JpegQuality.ToString(),
    "-pix_fmt", "yuvj420p"
)
if ($hasAudio) {
    $arguments += @(
        "-map", "0:a:0",
        "-af", $audioFilter,
        "-c:a", "pcm_s16le",
        "-ar", "16000",
        "-ac", "1"
    )
}
$arguments += @(
    "-max_interleave_delta", "0",
    "-f", "avi",
    $outputPath
)

Write-Host "Encoding MicriOS CYD media..."
Write-Host "Input : $inputPath"
Write-Host "Output: $outputPath"
& $ffmpeg @arguments
if ($LASTEXITCODE -ne 0) {
    throw "ffmpeg failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "Done. Copy the AVI to the SD card's /media folder."
& $ffprobe -v error -show_entries stream=index,codec_name,width,height,r_frame_rate,sample_rate,channels -show_entries format=duration,size -of default=noprint_wrappers=1 $outputPath
