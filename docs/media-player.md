# Media Player

MicriOS includes a touch-native Media Player for the ESP32-2432S028 CYD. The
catalog and playback-session model live in shared code so future boards can
reuse them, while SD access, AVI parsing, JPEG drawing, and GPIO audio are
currently implemented only for the CYD.

## SD Card Layout

Format the microSD card as FAT32 and create a `media` folder:

```text
/media/
  Audio/
    Music/
      anthem.wav
  Video/
    Movies/
      Ice Age/
        Ice Age.avi
    TV/
      Example Show/
        Episode 01.avi
```

The player scans `/media` first. If that folder is missing, it scans the root
of the card. Folder rows are shown before media rows and may be nested to any
depth that fits the path-length limit. Only the open directory is scanned, and
up to 64 folders and AVI/WAV files are listed per directory.

## Supported Media

The first player deliberately uses formats that a classic ESP32 can decode
without a large codec stack:

- AVI container with Motion JPEG (`MJPEG`) video and optional PCM audio.
- Audio-only RIFF/WAV container with uncompressed PCM audio.
- Maximum frame size `320x176`.
- PCM audio, 8-bit or 16-bit.
- Recommended audio: signed 16-bit little-endian PCM, mono, `16000 Hz`.
- Recommended frame rate: `12 fps`.

The 16 kHz recommendation keeps files and SD traffic modest. Playback does not
send that rate directly to the CYD DAC: the backend linearly resamples it to a
stable 32 kHz output clock, so existing correctly encoded media does not need
to be converted again.

Other AVI codecs such as H.264, MPEG-4, MP3, or AAC are not decoded by this
version. The player reports an on-screen format error instead of attempting to
play them.

## Audio-Only Diagnostics

WAV playback uses the same SD reader, PCM conversion, stream buffer, I2S DAC,
onboard amplifier, volume setting, and speaker as movie playback, but performs
no JPEG decoding or video transfer. This makes it useful for separating audio
format or hardware problems from video workload.

While a WAV is playing, tap the record artwork to toggle `DISPLAY STRESS 12 FPS`.
The stress pass repeatedly redraws the full media area while leaving the audio
path unchanged. If clean audio becomes crackly only with stress enabled, TFT/SPI
or interrupt pressure is implicated. If both modes sound alike, investigate the
DAC, amplifier, power, wiring, or speaker instead.

Create short comparison files from the same source:

```powershell
ffmpeg -y -ss 10 -t 30 -i ".\source.wav" -vn -ac 1 -ar 16000 -c:a pcm_u8 ".\01-16k-u8.wav"
ffmpeg -y -ss 10 -t 30 -i ".\source.wav" -vn -ac 1 -ar 8000  -c:a pcm_s16le ".\02-8k-s16.wav"
ffmpeg -y -ss 10 -t 30 -i ".\source.wav" -vn -ac 1 -ar 16000 -c:a pcm_s16le ".\03-16k-s16.wav"
ffmpeg -y -ss 10 -t 30 -i ".\source.wav" -vn -ac 1 -ar 22050 -c:a pcm_s16le ".\04-22k-s16.wav"
ffmpeg -y -ss 10 -t 30 -i ".\source.wav" -vn -ac 1 -ar 32000 -c:a pcm_s16le ".\05-32k-s16.wav"
ffmpeg -y -ss 10 -t 30 -i ".\source.wav" -vn -ac 1 -ar 44100 -c:a pcm_s16le ".\06-44k-s16.wav"
```

Interpretation:

- WAV clean but AVI crackly points to video/SD scheduling interference.
- Every WAV crackly in the same way points to the internal DAC, onboard
  amplifier, power, wiring, or speaker.
- PCM8 substantially worse than PCM16 points to quantization.
- Only particular rates failing points to I2S clock or DAC-rate behavior.
- Serial `starvations` or `writeErrors` increasing points to a software pacing
  problem rather than the analog output stage.

## Convert A Video

FFmpeg must be available on `PATH`. From the repository root:

```powershell
.\tools\convert-media.ps1 `
  ".\media-source\input-video.avi" `
  ".\media-output\video-CYD.avi"
```

For a quick 30-second hardware test before encoding a full movie:

```powershell
.\tools\convert-media.ps1 `
  ".\media-source\input-video.avi" `
  ".\media-output\video-CYD-preview.avi" `
  -PreviewSeconds 30
```

The script scales without stretching, pads unused space black, converts video
to 12 fps MJPEG, and converts audio to mono 16 kHz signed 16-bit PCM. It first
measures the mono downmix, then applies two-pass EBU R128 normalization to
`-20 LUFS` with a `-2 dBTP` peak ceiling and an `11 LU` loudness-range target.
Measuring after the mono downmix is important because source files vary greatly
in mastering level and may contain stereo or 5.1 audio. A simple fixed gain can
leave quiet films quiet or clip louder films.

The analysis pass means a full movie takes longer to convert, but produces much
more consistent volume on the CYD. The defaults may be overridden with
`-AudioLoudness` and `-AudioTruePeak`, although the standard MicriOS media set
should retain `-20` and `-2`. Keeping 16-bit precision until the final DAC
conversion also makes low volume substantially cleaner than attenuating an
already 8-bit source. MJPEG files are much larger than modern compressed video,
so a feature-length conversion can occupy hundreds of megabytes.

The converted file can also be tested on the PC with VLC or:

```powershell
ffplay ".\media-output\video-CYD-preview.avi"
```

## CYD Controls

- Tap a blue folder row to open it, or a grey AVI/WAV row to play it.
- **Up** opens the parent folder. The small path in the header is informational
  and is not itself a navigation control.
- **Previous/Next** move through pages of entries in the current folder.
- Tap the video to pause or resume.
- **Pause/Play** controls playback.
- **Jump To** pauses playback and opens a scrub bar. Dragging changes only the
  target time; releasing performs one seek, renders the selected video frame,
  and remains paused until **Play** is tapped.
- **List** returns to the SD catalog.
- Tap the speaker in the MicriOS system bar to adjust persisted volume. New
  installations default to 15 percent to protect against an unexpectedly loud
  CYD speaker/amplifier.
- While the volume bar is open, tap its speaker icon to toggle persisted mute.
  Muting does not change the saved volume, and the normal speaker icon gains a
  red slash while muted.
- The MicriOS system-bar **Exit** control closes the player.

## CYD Hardware Notes

The common ESP32-2432S028 SD slot uses `CS 5`, `CLK 18`, `MOSI 23`, and
`MISO 19`. Media Player owns that hardware SPI bus while open. CYD touch is
read with target-local software SPI on its existing pins, avoiding the SD and
touch controllers fighting over the same ESP32 SPI host.

The app retries SD initialization briefly before reporting a missing card.
This accommodates cards that need a little longer to become ready after the
SPI bus is started.

Audio uses the classic ESP32 built-in DAC through I2S on `GPIO26`, which is
the audio output used by the common CYD speaker connector. A dedicated audio
task buffers signed 16-bit PCM independently of SD/JPEG rendering, applies the
saved volume, linearly resamples it to 32 kHz, then converts it to the DAC's
8-bit samples. Serial diagnostics report buffer starvations and I2S write
errors.
An experimental 4x noise-shaped output sounded substantially worse through
the CYD analog amplifier and must not be restored without isolated hardware
testing. Older unsigned 8-bit PCM files remain supported. The actual
speaker/amplifier wiring and comfortable volume still require hardware
validation on the specific CYD revision.

The CYD's SC8002B speaker amplifier is permanently enabled and AC-coupled to
GPIO26. MicriOS therefore drives GPIO26 low whenever I2S/DAC audio is shut
down instead of leaving the amplifier input floating. The backlight PWM also
runs at 25 kHz rather than the previously audible 5 kHz, reducing switching
whine coupled into the onboard analog amplifier. Do not place a resistor
directly across the two speaker terminals: this is a bridged amplifier output,
not a ground-referenced signal.

The original ESP32 I2S peripheral has a mono FIFO-packing limitation. The CYD
backend therefore configures `I2S_CHANNEL_FMT_RIGHT_LEFT` and duplicates every
mono sample into both slots of each stereo frame, while only the left internal
DAC output is routed to GPIO26. The physical CYD also produced unreliable 8 and
16 kHz playback clocks even after correcting frame packing, while 22.05, 32,
and 44.1 kHz sources ran at the expected speed. The backend now keeps the DAC
at a known-good 32 kHz and linearly resamples every supported source rate in
the audio task.

Playback remains sequential between seeks. WAV seeking converts the selected
time directly to an aligned PCM byte offset. For AVI, the backend reads the
container's standard `idx1` packet table in buffered blocks the first time a
movie is scrubbed. It retains one video offset every 30 frames, using only a
few kilobytes even for a feature-length movie. Each later seek jumps to the
nearest retained offset, scans at most about 2.5 seconds of packets, decodes
the selected frame once, and remains paused. Scrubbing never scans or decodes
while the finger is moving. Existing AVI files produced by the conversion
script already contain `idx1` and do not need to be re-encoded.

The AVI parser queues bounded PCM blocks into a stream buffer and only the
audio task writes I2S/DAC data. This keeps normal playback touch-responsive
and prevents JPEG or SD operations from directly interrupting DAC output.
