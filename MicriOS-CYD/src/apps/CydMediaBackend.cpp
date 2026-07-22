#include "CydMediaBackend.h"

#include <driver/i2s.h>
#include <esp_heap_caps.h>

#include "../../CydHardware.h"

namespace {
constexpr uint32_t fourcc(char a, char b, char c, char d) {
  return static_cast<uint32_t>(static_cast<uint8_t>(a)) |
         (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8) |
         (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

constexpr uint32_t FCC_RIFF = fourcc('R', 'I', 'F', 'F');
constexpr uint32_t FCC_LIST = fourcc('L', 'I', 'S', 'T');
constexpr uint32_t FCC_AVI = fourcc('A', 'V', 'I', ' ');
constexpr uint32_t FCC_AVIX = fourcc('A', 'V', 'I', 'X');
constexpr uint32_t FCC_WAVE = fourcc('W', 'A', 'V', 'E');
constexpr uint32_t FCC_MOVI = fourcc('m', 'o', 'v', 'i');
constexpr uint32_t FCC_AVIH = fourcc('a', 'v', 'i', 'h');
constexpr uint32_t FCC_STRH = fourcc('s', 't', 'r', 'h');
constexpr uint32_t FCC_STRF = fourcc('s', 't', 'r', 'f');
constexpr uint32_t FCC_VIDS = fourcc('v', 'i', 'd', 's');
constexpr uint32_t FCC_AUDS = fourcc('a', 'u', 'd', 's');
constexpr uint32_t FCC_MJPG = fourcc('M', 'J', 'P', 'G');
constexpr uint32_t FCC_FMT = fourcc('f', 'm', 't', ' ');
constexpr uint32_t FCC_DATA = fourcc('d', 'a', 't', 'a');
constexpr uint32_t FCC_IDX1 = fourcc('i', 'd', 'x', '1');
constexpr uint16_t WAVE_FORMAT_PCM = 1;
constexpr i2s_port_t AUDIO_PORT = I2S_NUM_0;

TFT_eSPI* jpegTarget = nullptr;

bool hasChunkSuffix(uint32_t id, char third, char fourth) {
  return static_cast<char>((id >> 16) & 0xff) == third &&
         static_cast<char>((id >> 24) & 0xff) == fourth;
}

int drawJpegBlock(JPEGDRAW* draw) {
  if (jpegTarget == nullptr || draw == nullptr) return 0;
  if (draw->y >= CydMediaBackend::VIDEO_HEIGHT) return 1;
  const int16_t sourceWidth = draw->iWidth;
  int16_t width = draw->iWidthUsed;
  int16_t height = draw->iHeight;
  if (draw->x + width > CydMediaBackend::VIDEO_WIDTH) {
    width = CydMediaBackend::VIDEO_WIDTH - draw->x;
  }
  if (draw->y + height > CydMediaBackend::VIDEO_HEIGHT) {
    height = CydMediaBackend::VIDEO_HEIGHT - draw->y;
  }
  if (width > 0 && height > 0) {
    if (width == sourceWidth) {
      jpegTarget->pushImage(draw->x, draw->y, width, height, draw->pPixels);
    } else {
      for (int16_t row = 0; row < height; ++row) {
        jpegTarget->pushImage(draw->x, draw->y + row, width, 1,
                              draw->pPixels + row * sourceWidth);
      }
    }
  }
  return 1;
}

uint32_t littleU32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

uint16_t littleU16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) |
         (static_cast<uint16_t>(bytes[1]) << 8);
}
}  // namespace

CydMediaBackend::CydMediaBackend() : sdSpi_(VSPI) {}

CydMediaBackend::~CydMediaBackend() { end(); }

bool CydMediaBackend::begin() {
  if (mounted_) return true;
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  sdSpi_.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  constexpr uint8_t MOUNT_ATTEMPTS = 4;
  for (uint8_t attempt = 0; attempt < MOUNT_ATTEMPTS && !mounted_; ++attempt) {
    if (attempt > 0) delay(100U + attempt * 80U);
    mounted_ = SD.begin(PIN_SD_CS, sdSpi_, SD_FREQUENCY, "/sd", 4, false);
    if (!mounted_) {
      Serial.printf("[media] SD mount attempt %u/%u failed\n", attempt + 1,
                    MOUNT_ATTEMPTS);
      SD.end();
      digitalWrite(PIN_SD_CS, HIGH);
    }
  }
  if (!mounted_) {
    sdSpi_.end();
    session_.fail("SD card not found");
    return false;
  }
  session_.browsing();
  Serial.printf("[media] SD mounted size=%llu MB\n",
                static_cast<unsigned long long>(SD.cardSize() / 1048576ULL));
  return true;
}

void CydMediaBackend::end() {
  stopAudio();
  if (file_) file_.close();
  clearAviSeekIndex();
  if (jpegBuffer_ != nullptr) {
    heap_caps_free(jpegBuffer_);
    jpegBuffer_ = nullptr;
  }
  jpegCapacity_ = 0;
  pendingJpegBytes_ = 0;
  audioChunkRemaining_ = 0;
  audioChunkAlignedEnd_ = 0;
  framePending_ = false;
  if (mounted_) {
    SD.end();
    sdSpi_.end();
  }
  mounted_ = false;
  session_.browsing();
}

bool CydMediaBackend::scan(MediaPlayerLogic::Catalog& catalog) {
  if (!begin()) return false;
  MediaPlayerLogic::copyText(rootPath_, sizeof(rootPath_), "/media");
  if (!loadDirectory(rootPath_, catalog)) {
    MediaPlayerLogic::copyText(rootPath_, sizeof(rootPath_), "/");
    if (!loadDirectory(rootPath_, catalog)) return false;
  }
  return true;
}

bool CydMediaBackend::enterDirectory(
    const char* path, MediaPlayerLogic::Catalog& catalog) {
  if (!mounted_ || path == nullptr || path[0] != '/') return false;
  return loadDirectory(path, catalog);
}

bool CydMediaBackend::goUp(MediaPlayerLogic::Catalog& catalog) {
  if (atRoot()) return false;
  char parent[MediaPlayerLogic::MAX_PATH_LENGTH];
  MediaPlayerLogic::copyText(parent, sizeof(parent), currentPath_);
  char* slash = strrchr(parent, '/');
  if (slash == nullptr || slash == parent) {
    MediaPlayerLogic::copyText(parent, sizeof(parent), rootPath_);
  } else {
    *slash = '\0';
    if (strlen(parent) < strlen(rootPath_) ||
        strncmp(parent, rootPath_, strlen(rootPath_)) != 0) {
      MediaPlayerLogic::copyText(parent, sizeof(parent), rootPath_);
    }
  }
  return loadDirectory(parent, catalog);
}

bool CydMediaBackend::atRoot() const {
  return strcmp(currentPath_, rootPath_) == 0;
}

bool CydMediaBackend::loadDirectory(
    const char* path, MediaPlayerLogic::Catalog& catalog) {
  catalog.clear();
  if (!scanDirectory(path, catalog)) return false;
  MediaPlayerLogic::copyText(currentPath_, sizeof(currentPath_), path);
  catalog.sort();
  session_.browsing();
  Serial.printf("[media] found %u item(s) in %s\n", catalog.count(),
                currentPath_);
  return true;
}

bool CydMediaBackend::scanDirectory(const char* path,
                                    MediaPlayerLogic::Catalog& catalog) {
  File directory = SD.open(path);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    return false;
  }
  File entry = directory.openNextFile();
  while (entry && catalog.count() < MediaPlayerLogic::MAX_ITEMS) {
    if (entry.isDirectory()) {
      catalog.addDirectory(entry.path());
    } else {
      catalog.add(entry.path(), static_cast<uint32_t>(entry.size()));
    }
    entry.close();
    entry = directory.openNextFile();
  }
  directory.close();
  return true;
}

bool CydMediaBackend::open(const char* path) {
  if (!mounted_ && !begin()) return false;
  stopAudio();
  if (file_) file_.close();
  clearAviSeekIndex();
  session_.opening();
  metadata_ = {};
  moviStart_ = 0;
  moviEnd_ = 0;
  idx1DataStart_ = 0;
  idx1Size_ = 0;
  waveDataStart_ = 0;
  waveDataSize_ = 0;
  waveDrainStartedMs_ = 0;
  decodedFrames_ = 0;
  droppedFrames_ = 0;
  framePending_ = false;
  pendingJpegBytes_ = 0;
  audioChunkRemaining_ = 0;
  audioChunkAlignedEnd_ = 0;
  audioStarvations_ = 0;
  audioWriteErrors_ = 0;
  audioSamplesWritten_ = 0;
  waveAudioOnly_ = false;
  lastAudioDiagnosticMs_ = millis();
  nextFrameDueUs_ = micros();
  MediaPlayerLogic::titleFromPath(path, currentTitle_, sizeof(currentTitle_));

  file_ = SD.open(path, FILE_READ);
  if (!file_) {
    fail("Unable to open media file");
    return false;
  }
  if (!parseHeader()) return false;
  if (!waveAudioOnly_ && !metadata_.motionJpeg) {
    fail("AVI video must be MJPEG");
    return false;
  }
  if (!waveAudioOnly_ &&
      (metadata_.width > VIDEO_WIDTH || metadata_.height > VIDEO_HEIGHT)) {
    fail("Video must fit 320x176");
    return false;
  }
  if (metadata_.audioSampleRate > 0 && !metadata_.pcmAudio) {
    fail("Audio must be PCM");
    return false;
  }
  if (metadata_.pcmAudio && metadata_.audioBits != 8 &&
      metadata_.audioBits != 16) {
    fail("Audio must be 8/16-bit PCM");
    return false;
  }

  if (metadata_.pcmAudio && !beginAudio()) {
    fail("Unable to start audio");
    return false;
  }
  if (waveAudioOnly_) {
    file_.seek(waveDataStart_);
    audioChunkRemaining_ = waveDataSize_;
    audioChunkAlignedEnd_ = waveDataStart_ + waveDataSize_;
  } else {
    file_.seek(moviStart_);
  }
  session_.playing(metadata_);
  Serial.printf(
      "[media] open %s %ux%u %.2f fps audio=%luHz/%ubit/%uch duration=%lums\n",
      currentTitle_, metadata_.width, metadata_.height,
      1000000.0f / metadata_.frameDurationUs,
      static_cast<unsigned long>(metadata_.audioSampleRate),
      metadata_.audioBits, metadata_.audioChannels,
      static_cast<unsigned long>(metadata_.durationMs));
  return true;
}

void CydMediaBackend::closePlayback() {
  stopAudio();
  if (file_) file_.close();
  clearAviSeekIndex();
  framePending_ = false;
  pendingJpegBytes_ = 0;
  audioChunkRemaining_ = 0;
  audioChunkAlignedEnd_ = 0;
  session_.browsing();
}

bool CydMediaBackend::parseHeader() {
  file_.seek(0);
  if (readU32() != FCC_RIFF) {
    fail("Not a RIFF media file");
    return false;
  }
  readU32();
  const uint32_t containerType = readU32();
  if (containerType == FCC_WAVE) {
    waveAudioOnly_ = true;
    return parseWaveHeader();
  }
  if (containerType != FCC_AVI) {
    fail("Not an AVI/WAV file");
    return false;
  }

  uint32_t currentStreamType = 0;
  while (file_.available() >= 8) {
    uint32_t id = 0;
    uint32_t size = 0;
    if (!readChunkHeader(id, size)) break;
    const uint32_t dataStart = file_.position();

    if (id == FCC_LIST || id == FCC_RIFF) {
      const uint32_t listType = readU32();
      if (listType == FCC_MOVI) {
        moviStart_ = file_.position();
        moviEnd_ = dataStart + size;
        break;
      }
      if (id == FCC_RIFF && listType != FCC_AVIX) {
        seekAligned(dataStart, size);
      }
      continue;
    }

    if (id == FCC_AVIH && size >= 40) {
      uint8_t header[40] = {};
      if (!readBytes(header, sizeof(header))) break;
      metadata_.frameDurationUs = littleU32(header);
      metadata_.frameCount = littleU32(header + 16);
      metadata_.width = static_cast<uint16_t>(littleU32(header + 32));
      metadata_.height = static_cast<uint16_t>(littleU32(header + 36));
    } else if (id == FCC_STRH && size >= 36) {
      uint8_t stream[36] = {};
      if (!readBytes(stream, sizeof(stream))) break;
      currentStreamType = littleU32(stream);
      const uint32_t handler = littleU32(stream + 4);
      const uint32_t scale = littleU32(stream + 20);
      const uint32_t rate = littleU32(stream + 24);
      const uint32_t length = littleU32(stream + 32);
      if (currentStreamType == FCC_VIDS) {
        metadata_.motionJpeg = handler == FCC_MJPG;
        if (scale > 0 && rate > 0) {
          metadata_.frameDurationUs =
              static_cast<uint32_t>((1000000ULL * scale) / rate);
        }
        if (metadata_.frameCount == 0) metadata_.frameCount = length;
      }
    } else if (id == FCC_STRF && currentStreamType == FCC_AUDS && size >= 16) {
      uint8_t format[16] = {};
      if (!readBytes(format, sizeof(format))) break;
      metadata_.pcmAudio = littleU16(format) == WAVE_FORMAT_PCM;
      metadata_.audioChannels = static_cast<uint8_t>(littleU16(format + 2));
      metadata_.audioSampleRate = littleU32(format + 4);
      metadata_.audioBits = littleU16(format + 14);
    }
    seekAligned(dataStart, size);
  }

  if (moviStart_ == 0) {
    fail("AVI has no movi data");
    return false;
  }
  if (metadata_.frameDurationUs == 0) metadata_.frameDurationUs = 83333;
  if (metadata_.frameCount > 0) {
    metadata_.durationMs = static_cast<uint32_t>(
        (static_cast<uint64_t>(metadata_.frameCount) *
         metadata_.frameDurationUs) /
        1000ULL);
  }
  locateAviIndex();
  return true;
}

bool CydMediaBackend::parseWaveHeader() {
  while (file_.available() >= 8) {
    uint32_t id = 0;
    uint32_t size = 0;
    if (!readChunkHeader(id, size)) break;
    const uint32_t dataStart = file_.position();

    if (id == FCC_FMT && size >= 16) {
      uint8_t format[16] = {};
      if (!readBytes(format, sizeof(format))) break;
      metadata_.pcmAudio = littleU16(format) == WAVE_FORMAT_PCM;
      metadata_.audioChannels = static_cast<uint8_t>(littleU16(format + 2));
      metadata_.audioSampleRate = littleU32(format + 4);
      metadata_.audioBits = littleU16(format + 14);
    } else if (id == FCC_DATA) {
      waveDataStart_ = dataStart;
      waveDataSize_ = size;
      break;
    }
    seekAligned(dataStart, size);
  }

  if (!metadata_.pcmAudio || waveDataStart_ == 0 ||
      metadata_.audioSampleRate == 0 || metadata_.audioChannels == 0) {
    fail("WAV must contain PCM audio");
    return false;
  }
  const uint32_t bytesPerFrame =
      metadata_.audioChannels * (metadata_.audioBits / 8U);
  if (bytesPerFrame == 0) {
    fail("Invalid WAV format");
    return false;
  }
  metadata_.durationMs = static_cast<uint32_t>(
      (static_cast<uint64_t>(waveDataSize_) * 1000ULL) /
      (static_cast<uint64_t>(metadata_.audioSampleRate) * bytesPerFrame));
  return true;
}

bool CydMediaBackend::service(TFT_eSPI& tft) {
  if (session_.state() != MediaPlayerLogic::State::Playing) return false;
  if (audioReady_ && millis() - lastAudioDiagnosticMs_ >= 10000U) {
    lastAudioDiagnosticMs_ = millis();
    const size_t queued = audioStream_ == nullptr
                              ? 0
                              : xStreamBufferBytesAvailable(audioStream_);
    Serial.printf(
        "[media] audio queued=%u bytes starvations=%lu writeErrors=%lu\n",
        static_cast<unsigned>(queued),
        static_cast<unsigned long>(audioStarvations_),
        static_cast<unsigned long>(audioWriteErrors_));
  }
  if (waveAudioOnly_) {
    session_.setPosition(static_cast<uint32_t>(
        (static_cast<uint64_t>(audioSamplesWritten_) * 1000ULL) /
        metadata_.audioSampleRate));
    if (audioChunkRemaining_ > 0) {
      waveDrainStartedMs_ = 0;
      return readAudioChunk(0);
    }
    const size_t queued = audioStream_ == nullptr
                              ? 0
                              : xStreamBufferBytesAvailable(audioStream_);
    if (queued > 0) {
      waveDrainStartedMs_ = 0;
      return false;
    }
    if (waveDrainStartedMs_ == 0) {
      waveDrainStartedMs_ = millis();
      return false;
    }
    if (millis() - waveDrainStartedMs_ >= 250U) {
      stopAudio();
      session_.setPosition(metadata_.durationMs);
      session_.finish();
      return true;
    }
    return false;
  }
  if (framePending_ && !decodePendingFrame(tft)) return false;
  if (audioChunkRemaining_ > 0) return readAudioChunk(0);
  return readNextChunk(tft);
}

bool CydMediaBackend::readNextChunk(TFT_eSPI& tft) {
  for (uint8_t skipped = 0; skipped < 8; ++skipped) {
    if (!file_ || !file_.available() ||
        (moviEnd_ > moviStart_ && file_.position() >= moviEnd_)) {
      stopAudio();
      session_.finish();
      return true;
    }
    uint32_t id = 0;
    uint32_t size = 0;
    if (!readChunkHeader(id, size)) {
      stopAudio();
      session_.finish();
      return true;
    }
    const uint32_t dataStart = file_.position();
    if (id == FCC_LIST || id == FCC_RIFF) {
      readU32();
      continue;
    }
    if (hasChunkSuffix(id, 'd', 'c') || hasChunkSuffix(id, 'd', 'b')) {
      return readVideoChunk(tft, size);
    }
    if (hasChunkSuffix(id, 'w', 'b')) {
      return readAudioChunk(size);
    }
    if (!seekAligned(dataStart, size)) {
      fail("AVI read error");
      return true;
    }
  }
  return false;
}

bool CydMediaBackend::readVideoChunk(TFT_eSPI& tft, uint32_t size) {
  const uint32_t dataStart = file_.position();
  if (size == 0 || size > MAX_JPEG_BYTES) {
    ++droppedFrames_;
    seekAligned(dataStart, size);
    return false;
  }
  if (jpegCapacity_ < size) {
    const uint32_t capacity = (size + 4095U) & ~4095U;
    uint8_t* replacement = static_cast<uint8_t*>(
        heap_caps_realloc(jpegBuffer_, capacity, MALLOC_CAP_8BIT));
    if (replacement == nullptr) {
      fail("Not enough RAM for frame");
      return true;
    }
    jpegBuffer_ = replacement;
    jpegCapacity_ = capacity;
  }
  if (!readBytes(jpegBuffer_, size)) {
    fail("Truncated video frame");
    return true;
  }
  seekAligned(dataStart, size);
  pendingJpegBytes_ = size;
  framePending_ = true;
  return decodePendingFrame(tft);
}

bool CydMediaBackend::decodePendingFrame(TFT_eSPI& tft) {
  if (!framePending_) return false;
  const uint32_t nowUs = micros();
  if (static_cast<int32_t>(nowUs - nextFrameDueUs_) < 0) {
    return false;
  }

  if (decodedFrames_ > 0 &&
      static_cast<int32_t>(nowUs - nextFrameDueUs_) >
          static_cast<int32_t>(metadata_.frameDurationUs * 2U)) {
    ++droppedFrames_;
  } else {
    CydHardware::applyDisplayOrientation(tft);
    const bool previousSwap = tft.getSwapBytes();
    tft.setSwapBytes(false);
    jpegTarget = &tft;
    if (jpeg_.openRAM(jpegBuffer_, pendingJpegBytes_, drawJpegBlock)) {
      // TFT_eSPI's direct SPI image path consumes display-order bytes. Ask
      // JPEGDEC for big-endian RGB565 and leave TFT byte swapping disabled.
      jpeg_.setPixelType(RGB565_BIG_ENDIAN);
      const int16_t x = (VIDEO_WIDTH - jpeg_.getWidth()) / 2;
      const int16_t y = (VIDEO_HEIGHT - jpeg_.getHeight()) / 2;
      jpeg_.decode(x, y, 0);
      jpeg_.close();
      ++decodedFrames_;
    } else {
      ++droppedFrames_;
    }
    jpegTarget = nullptr;
    tft.setSwapBytes(previousSwap);
  }
  framePending_ = false;
  pendingJpegBytes_ = 0;
  nextFrameDueUs_ += metadata_.frameDurationUs;
  session_.setPosition(static_cast<uint32_t>(
      (static_cast<uint64_t>(decodedFrames_ + droppedFrames_) *
       metadata_.frameDurationUs) /
      1000ULL));
  return true;
}

bool CydMediaBackend::readAudioChunk(uint32_t size) {
  if (size > 0) {
    const uint32_t dataStart = file_.position();
    audioChunkRemaining_ = size;
    audioChunkAlignedEnd_ = dataStart + size + (size & 1U);
  }
  if (audioChunkRemaining_ == 0) return false;

  const uint8_t channels = metadata_.audioChannels == 0
                               ? 1
                               : metadata_.audioChannels;
  const size_t bytesPerChannel = metadata_.audioBits == 16 ? 2U : 1U;
  const size_t frameBytes = channels * bytesPerChannel;
  uint8_t raw[768];
  int16_t pcm[768];
  size_t bytes = audioChunkRemaining_ > sizeof(raw)
                     ? sizeof(raw)
                     : audioChunkRemaining_;

  if (audioReady_ && audioStream_ != nullptr) {
    const size_t streamSpace = xStreamBufferSpacesAvailable(audioStream_);
    const size_t framesThatFit = streamSpace / sizeof(int16_t);
    const size_t inputThatFits = framesThatFit * frameBytes;
    if (inputThatFits < bytes) bytes = inputThatFits;
    bytes -= bytes % frameBytes;
    if (bytes == 0) return false;
  }

  if (file_.read(raw, bytes) != bytes) {
    audioChunkRemaining_ = 0;
    fail("Truncated audio data");
    return true;
  }
  if (audioReady_ && audioStream_ != nullptr) {
    size_t samples = 0;
    if (metadata_.audioBits == 8) {
      for (size_t index = 0; index < bytes; index += channels) {
        pcm[samples++] = static_cast<int16_t>(
            (static_cast<int16_t>(raw[index]) - 128) << 8);
      }
    } else {
      const size_t stride = channels * 2U;
      for (size_t index = 0; index + 1 < bytes; index += stride) {
        pcm[samples++] = static_cast<int16_t>(
            static_cast<uint16_t>(raw[index]) |
            (static_cast<uint16_t>(raw[index + 1]) << 8));
      }
    }
    if (samples > 0) {
      const size_t expected = samples * sizeof(int16_t);
      const size_t queued =
          xStreamBufferSend(audioStream_, pcm, expected, 0);
      if (queued != expected) ++audioWriteErrors_;
    }
  }
  audioChunkRemaining_ -= bytes;
  if (audioChunkRemaining_ == 0) file_.seek(audioChunkAlignedEnd_);
  return false;
}

bool CydMediaBackend::beginAudio() {
  stopAudio();
  if (metadata_.audioSampleRate < 8000 || metadata_.audioSampleRate > 48000) {
    return false;
  }
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX |
                                        I2S_MODE_DAC_BUILT_IN);
  // The legacy built-in DAC produced incorrect clocks for 8/16 kHz streams on
  // the CYD. Keep the peripheral at a known-good rate and resample in software.
  config.sample_rate = DAC_OUTPUT_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  // Original ESP32 hardware scrambles 8/16-bit mono FIFO packing. Use stereo
  // frames and duplicate each mono sample, matching Espressif's workaround.
  // GPIO26 still receives only the enabled left DAC channel.
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_MSB;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 12;
  config.dma_buf_len = 256;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;
  const esp_err_t installed =
      i2s_driver_install(AUDIO_PORT, &config, 0, nullptr);
  if (installed != ESP_OK) {
    Serial.printf("[media] I2S install failed: %d\n", installed);
    return false;
  }
  audioInstalled_ = true;
  if (i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN) != ESP_OK) {
    stopAudio();
    return false;
  }
  const esp_err_t clockResult =
      i2s_set_clk(AUDIO_PORT, DAC_OUTPUT_RATE,
                  I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  if (clockResult != ESP_OK) {
    Serial.printf("[media] I2S clock setup failed: %d\n", clockResult);
    stopAudio();
    return false;
  }
  const float actualRate = i2s_get_clk(AUDIO_PORT);
  Serial.printf(
      "[media] I2S source=%luHz output=%luHz actual=%.2fHz\n",
      static_cast<unsigned long>(metadata_.audioSampleRate),
      static_cast<unsigned long>(DAC_OUTPUT_RATE), actualRate);
  audioStream_ = xStreamBufferCreate(AUDIO_STREAM_BYTES, sizeof(int16_t));
  if (audioStream_ == nullptr) {
    Serial.println("[media] audio stream allocation failed");
    stopAudio();
    return false;
  }
  audioTaskRunning_ = true;
  audioShouldPlay_ = true;
  const BaseType_t taskCreated = xTaskCreatePinnedToCore(
      audioTaskEntry, "mediaAudio", 6144, this, 3, &audioTask_, 0);
  if (taskCreated != pdPASS) {
    Serial.println("[media] audio task creation failed");
    audioTaskRunning_ = false;
    stopAudio();
    return false;
  }
  audioReady_ = true;
  writeDacSilence();
  Serial.printf("[media] audio task ready rate=%luHz buffer=%u\n",
                static_cast<unsigned long>(metadata_.audioSampleRate),
                static_cast<unsigned>(AUDIO_STREAM_BYTES));
  return true;
}

void CydMediaBackend::stopAudio() {
  audioReady_ = false;
  audioShouldPlay_ = false;
  audioTaskRunning_ = false;
  if (audioTask_ != nullptr) xTaskNotifyGive(audioTask_);
  const uint32_t waitStarted = millis();
  while (audioTask_ != nullptr && millis() - waitStarted < 250U) delay(1);
  if (audioTask_ != nullptr) {
    vTaskDelete(audioTask_);
    audioTask_ = nullptr;
  }
  if (audioStream_ != nullptr) {
    vStreamBufferDelete(audioStream_);
    audioStream_ = nullptr;
  }
  if (audioInstalled_) {
    writeDacSilence();
    i2s_stop(AUDIO_PORT);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
    i2s_driver_uninstall(AUDIO_PORT);
  }
  audioInstalled_ = false;
  CydHardware::holdAudioIdle();
}

void CydMediaBackend::pause() {
  session_.pause();
  if (audioReady_) {
    audioShouldPlay_ = false;
    writeDacSilence();
    i2s_stop(AUDIO_PORT);
  }
}

void CydMediaBackend::resume() {
  session_.resume();
  nextFrameDueUs_ = micros();
  if (audioReady_) {
    i2s_start(AUDIO_PORT);
    audioShouldPlay_ = true;
    if (audioTask_ != nullptr) xTaskNotifyGive(audioTask_);
  }
}

void CydMediaBackend::restart() {
  if (!file_ || (!waveAudioOnly_ && moviStart_ == 0) ||
      (waveAudioOnly_ && waveDataStart_ == 0)) {
    return;
  }
  if (metadata_.pcmAudio && !audioReady_ && !beginAudio()) {
    fail("Unable to restart audio");
    return;
  }
  resetAudioPipeline(true);
  if (waveAudioOnly_) {
    file_.seek(waveDataStart_);
    audioChunkRemaining_ = waveDataSize_;
    audioChunkAlignedEnd_ = waveDataStart_ + waveDataSize_;
    waveDrainStartedMs_ = 0;
  } else {
    file_.seek(moviStart_);
    audioChunkRemaining_ = 0;
    audioChunkAlignedEnd_ = 0;
  }
  decodedFrames_ = 0;
  droppedFrames_ = 0;
  audioSamplesWritten_ = 0;
  framePending_ = false;
  pendingJpegBytes_ = 0;
  nextFrameDueUs_ = micros();
  session_.playing(metadata_);
}

bool CydMediaBackend::seekTo(uint32_t targetMs, TFT_eSPI& tft) {
  if (!file_ || metadata_.durationMs == 0) return false;
  if (targetMs > metadata_.durationMs) targetMs = metadata_.durationMs;
  if (metadata_.pcmAudio && !audioReady_ && !beginAudio()) {
    fail("Unable to restart audio");
    return false;
  }

  pause();
  resetAudioPipeline(true);
  audioChunkRemaining_ = 0;
  audioChunkAlignedEnd_ = 0;
  framePending_ = false;
  pendingJpegBytes_ = 0;
  waveDrainStartedMs_ = 0;

  const bool sought = waveAudioOnly_ ? seekWave(targetMs)
                                     : seekAvi(targetMs, tft);
  if (!sought && session_.state() != MediaPlayerLogic::State::Error) {
    fail("Unable to seek media");
  }
  return sought;
}

bool CydMediaBackend::seekWave(uint32_t targetMs) {
  const uint32_t bytesPerSample = metadata_.audioBits / 8U;
  const uint32_t bytesPerFrame = metadata_.audioChannels * bytesPerSample;
  if (bytesPerFrame == 0 || metadata_.audioSampleRate == 0) return false;

  uint64_t targetSample =
      (static_cast<uint64_t>(targetMs) * metadata_.audioSampleRate) / 1000ULL;
  uint64_t byteOffset = targetSample * bytesPerFrame;
  if (byteOffset > waveDataSize_) byteOffset = waveDataSize_;
  byteOffset -= byteOffset % bytesPerFrame;
  targetSample = byteOffset / bytesPerFrame;
  if (!file_.seek(waveDataStart_ + static_cast<uint32_t>(byteOffset))) {
    return false;
  }

  audioChunkRemaining_ = waveDataSize_ - static_cast<uint32_t>(byteOffset);
  audioChunkAlignedEnd_ = waveDataStart_ + waveDataSize_;
  audioSamplesWritten_ = static_cast<uint32_t>(targetSample);
  const uint32_t snappedMs = static_cast<uint32_t>(
      (targetSample * 1000ULL) / metadata_.audioSampleRate);
  session_.seekPaused(snappedMs);
  nextFrameDueUs_ = micros();
  Serial.printf("[media] seek WAV target=%lums offset=%lu\n",
                static_cast<unsigned long>(snappedMs),
                static_cast<unsigned long>(byteOffset));
  return true;
}

bool CydMediaBackend::locateAviIndex() {
  idx1DataStart_ = 0;
  idx1Size_ = 0;
  if (!file_ || moviEnd_ <= moviStart_) return false;

  const uint32_t restorePosition = file_.position();
  uint32_t probe = (moviEnd_ + 1U) & ~1U;
  for (uint8_t chunk = 0; chunk < 8 && probe + 8U <= file_.size(); ++chunk) {
    if (!file_.seek(probe)) break;
    uint32_t id = 0;
    uint32_t size = 0;
    if (!readChunkHeader(id, size)) break;
    const uint32_t dataStart = file_.position();
    if (id == FCC_IDX1 && size >= 16U) {
      const uint32_t available = static_cast<uint32_t>(file_.size()) - dataStart;
      idx1DataStart_ = dataStart;
      idx1Size_ = min(size, available) & ~15U;
      break;
    }
    probe = (dataStart + size + 1U) & ~1U;
  }
  file_.seek(restorePosition);
  if (idx1DataStart_ != 0) {
    Serial.printf("[media] AVI idx1 entries=%lu bytes=%lu\n",
                  static_cast<unsigned long>(idx1Size_ / 16U),
                  static_cast<unsigned long>(idx1Size_));
  } else {
    Serial.println("[media] AVI has no usable idx1 seek table");
  }
  return idx1DataStart_ != 0;
}

bool CydMediaBackend::chunkIdMatches(uint32_t position,
                                     uint32_t expectedId) {
  if (!file_ || position + 4U > file_.size() || !file_.seek(position)) {
    return false;
  }
  return readU32() == expectedId;
}

bool CydMediaBackend::resolveAviIndexBase(uint32_t firstOffset,
                                          uint32_t firstChunkId) {
  const uint32_t candidates[] = {
      moviStart_ >= 4U ? moviStart_ - 4U : 0U,
      moviStart_,
      0U,
      moviStart_ >= 12U ? moviStart_ - 12U : 0U,
      moviStart_ >= 8U ? moviStart_ - 8U : 0U,
  };
  for (uint32_t base : candidates) {
    if (base <= UINT32_MAX - firstOffset &&
        chunkIdMatches(base + firstOffset, firstChunkId)) {
      aviIndexOffsetBase_ = base;
      return true;
    }
  }
  return false;
}

bool CydMediaBackend::buildAviSeekIndex() {
  if (aviSeekIndexAttempted_) {
    return aviSeekOffsets_ != nullptr && aviSeekIndexCount_ > 0;
  }
  aviSeekIndexAttempted_ = true;
  if ((idx1DataStart_ == 0 && !locateAviIndex()) ||
      metadata_.frameCount == 0) {
    return false;
  }

  const uint32_t capacity =
      (metadata_.frameCount + AVI_SEEK_STRIDE_FRAMES - 1U) /
      AVI_SEEK_STRIDE_FRAMES;
  if (capacity == 0 || capacity > SIZE_MAX / sizeof(uint32_t)) return false;

  uint32_t* offsets = static_cast<uint32_t*>(heap_caps_malloc(
      capacity * sizeof(uint32_t), MALLOC_CAP_8BIT));
  constexpr size_t READ_BYTES = 4096;
  uint8_t* buffer = static_cast<uint8_t*>(
      heap_caps_malloc(READ_BYTES, MALLOC_CAP_8BIT));
  if (offsets == nullptr || buffer == nullptr) {
    if (offsets != nullptr) heap_caps_free(offsets);
    if (buffer != nullptr) heap_caps_free(buffer);
    Serial.println("[media] not enough RAM for AVI seek index");
    return false;
  }

  const uint32_t startedMs = millis();
  uint32_t remaining = idx1Size_;
  uint32_t videoFrame = 0;
  uint32_t stored = 0;
  uint32_t firstOffset = 0;
  uint32_t firstChunkId = 0;
  bool readOk = file_.seek(idx1DataStart_);
  while (readOk && remaining >= 16U) {
    const size_t batch = min(static_cast<uint32_t>(READ_BYTES), remaining);
    const size_t bytesRead = file_.read(buffer, batch);
    if (bytesRead != batch) {
      readOk = false;
      break;
    }
    for (size_t offset = 0; offset + 16U <= bytesRead; offset += 16U) {
      const uint32_t chunkId = littleU32(buffer + offset);
      if (!hasChunkSuffix(chunkId, 'd', 'c') &&
          !hasChunkSuffix(chunkId, 'd', 'b')) {
        continue;
      }
      const uint32_t chunkOffset = littleU32(buffer + offset + 8U);
      if (videoFrame == 0) {
        firstOffset = chunkOffset;
        firstChunkId = chunkId;
      }
      if ((videoFrame % AVI_SEEK_STRIDE_FRAMES) == 0 && stored < capacity) {
        offsets[stored++] = chunkOffset;
      }
      ++videoFrame;
    }
    remaining -= batch;
    delay(0);
  }
  heap_caps_free(buffer);

  if (!readOk || stored == 0 ||
      !resolveAviIndexBase(firstOffset, firstChunkId)) {
    heap_caps_free(offsets);
    Serial.println("[media] AVI idx1 table could not be resolved");
    return false;
  }

  aviSeekOffsets_ = offsets;
  aviSeekIndexCount_ = stored;
  Serial.printf(
      "[media] AVI sparse seek index ready frames=%lu points=%lu RAM=%luB time=%lums\n",
      static_cast<unsigned long>(videoFrame),
      static_cast<unsigned long>(aviSeekIndexCount_),
      static_cast<unsigned long>(aviSeekIndexCount_ * sizeof(uint32_t)),
      static_cast<unsigned long>(millis() - startedMs));
  return true;
}

void CydMediaBackend::clearAviSeekIndex() {
  if (aviSeekOffsets_ != nullptr) {
    heap_caps_free(aviSeekOffsets_);
    aviSeekOffsets_ = nullptr;
  }
  aviSeekIndexCount_ = 0;
  aviSeekIndexAttempted_ = false;
  aviIndexOffsetBase_ = 0;
  idx1DataStart_ = 0;
  idx1Size_ = 0;
}

bool CydMediaBackend::seekAvi(uint32_t targetMs, TFT_eSPI& tft) {
  if (moviStart_ == 0 || metadata_.frameDurationUs == 0 ||
      !buildAviSeekIndex()) {
    return false;
  }

  uint32_t targetFrame = static_cast<uint32_t>(
      (static_cast<uint64_t>(targetMs) * 1000ULL) /
      metadata_.frameDurationUs);
  if (metadata_.frameCount > 0 && targetFrame >= metadata_.frameCount) {
    targetFrame = metadata_.frameCount - 1U;
  }

  uint32_t slot = targetFrame / AVI_SEEK_STRIDE_FRAMES;
  if (slot >= aviSeekIndexCount_) slot = aviSeekIndexCount_ - 1U;
  const uint32_t indexedOffset = aviSeekOffsets_[slot];
  if (aviIndexOffsetBase_ > UINT32_MAX - indexedOffset ||
      !file_.seek(aviIndexOffsetBase_ + indexedOffset)) {
    return false;
  }

  uint32_t videoIndex = slot * AVI_SEEK_STRIDE_FRAMES;
  uint32_t chunksScanned = 0;
  while (file_.available() &&
         (moviEnd_ <= moviStart_ || file_.position() < moviEnd_)) {
    uint32_t id = 0;
    uint32_t size = 0;
    if (!readChunkHeader(id, size)) break;
    const uint32_t dataStart = file_.position();

    if (id == FCC_LIST || id == FCC_RIFF) {
      if (size < 4 || file_.available() < 4) return false;
      readU32();
      continue;
    }

    if (hasChunkSuffix(id, 'd', 'c') || hasChunkSuffix(id, 'd', 'b')) {
      if (videoIndex >= targetFrame) {
        decodedFrames_ = videoIndex;
        droppedFrames_ = 0;
        audioSamplesWritten_ = static_cast<uint32_t>(
            (static_cast<uint64_t>(videoIndex) * metadata_.frameDurationUs *
             metadata_.audioSampleRate) /
            1000000ULL);
        nextFrameDueUs_ = micros();
        readVideoChunk(tft, size);
        if (session_.state() == MediaPlayerLogic::State::Error) return false;
        const uint32_t snappedMs = static_cast<uint32_t>(
            (static_cast<uint64_t>(videoIndex) * metadata_.frameDurationUs) /
            1000ULL);
        session_.seekPaused(snappedMs);
        nextFrameDueUs_ = micros();
        Serial.printf(
            "[media] seek AVI target=%lums frame=%lu indexSlot=%lu chunks=%lu\n",
                      static_cast<unsigned long>(snappedMs),
                      static_cast<unsigned long>(videoIndex),
                      static_cast<unsigned long>(slot),
                      static_cast<unsigned long>(chunksScanned));
        return true;
      }
      ++videoIndex;
    }

    if (!seekAligned(dataStart, size)) return false;
    if ((++chunksScanned & 0xffU) == 0) delay(0);
  }
  return false;
}

void CydMediaBackend::setMuted(bool muted) {
  session_.setMuted(muted);
  if (audioReady_) {
    audioShouldPlay_ = true;
    resetAudioPipeline(true);
    if (muted) {
      writeDacSilence();
    }
    if (audioTask_ != nullptr) {
      xTaskNotifyGive(audioTask_);
    }
  }
  nextFrameDueUs_ = micros();
}

void CydMediaBackend::resetAudioPipeline(bool discardQueuedAudio) {
  ++audioResetGeneration_;
  if (discardQueuedAudio && audioStream_ != nullptr) {
    if (audioTask_ != nullptr) vTaskSuspend(audioTask_);
    xStreamBufferReset(audioStream_);
    if (audioTask_ != nullptr) vTaskResume(audioTask_);
  }
}

void CydMediaBackend::writeDacSilence() {
  if (!audioInstalled_) return;
  uint16_t midpoint[64];
  for (uint16_t& sample : midpoint) sample = 0x8000;
  size_t written = 0;
  i2s_write(AUDIO_PORT, midpoint, sizeof(midpoint), &written,
            pdMS_TO_TICKS(20));
}

void CydMediaBackend::audioTaskEntry(void* context) {
  static_cast<CydMediaBackend*>(context)->audioTaskLoop();
}

void CydMediaBackend::audioTaskLoop() {
  int16_t input[192];
  uint16_t output[256 * 2];
  uint32_t resetGeneration = audioResetGeneration_;
  uint32_t lastDataMs = millis();
  bool primed = false;
  bool starvationReported = false;
  bool havePreviousSample = false;
  int16_t previousSample = 0;
  uint64_t sourceIndex = 0;
  uint64_t nextOutputPosition = 0;
  const uint64_t outputStep =
      (static_cast<uint64_t>(metadata_.audioSampleRate) << 32) /
      DAC_OUTPUT_RATE;

  while (audioTaskRunning_) {
    if (!audioShouldPlay_ || audioStream_ == nullptr) {
      primed = false;
      starvationReported = false;
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
      continue;
    }

    const size_t bytes = xStreamBufferReceive(
        audioStream_, input, sizeof(input), pdMS_TO_TICKS(20));
    if (!audioTaskRunning_) break;
    if (bytes == 0) {
      if (primed && !starvationReported && millis() - lastDataMs >= 120U) {
        ++audioStarvations_;
        starvationReported = true;
      }
      continue;
    }

    const size_t samples = bytes / sizeof(int16_t);
    if (samples == 0) continue;
    if (!primed || resetGeneration != audioResetGeneration_) {
      resetGeneration = audioResetGeneration_;
      primed = true;
      havePreviousSample = false;
      sourceIndex = 0;
      nextOutputPosition = 0;
    }
    lastDataMs = millis();
    starvationReported = false;

    const uint8_t volume =
        (CydHardware::audioMuted() || session_.muted())
            ? 0
            : CydHardware::audioVolume();
    size_t outputFrames = 0;
    auto flushOutput = [&]() {
      if (outputFrames == 0) return;
      size_t written = 0;
      const size_t outputBytes = outputFrames * 2U * sizeof(uint16_t);
      const esp_err_t result = i2s_write(AUDIO_PORT, output, outputBytes,
                                         &written, pdMS_TO_TICKS(100));
      if (result != ESP_OK || written != outputBytes) ++audioWriteErrors_;
      outputFrames = 0;
    };
    auto appendOutput = [&](int16_t sample) {
      const int32_t scaled = (static_cast<int32_t>(sample) * volume) / 100;
      const uint8_t dacSample =
          static_cast<uint8_t>((scaled + 32768) >> 8);
      const uint16_t dacWord = static_cast<uint16_t>(dacSample) << 8;
      output[outputFrames * 2] = dacWord;
      output[outputFrames * 2 + 1] = dacWord;
      if (++outputFrames == 256) flushOutput();
    };

    for (size_t index = 0; index < samples; ++index) {
      const int16_t currentSample = input[index];
      if (!havePreviousSample) {
        appendOutput(currentSample);
        previousSample = currentSample;
        havePreviousSample = true;
        nextOutputPosition = outputStep;
        continue;
      }

      ++sourceIndex;
      const uint64_t segmentStart = (sourceIndex - 1U) << 32;
      const uint64_t segmentEnd = sourceIndex << 32;
      while (nextOutputPosition <= segmentEnd) {
        const uint64_t fraction = nextOutputPosition - segmentStart;
        const int64_t delta = static_cast<int32_t>(currentSample) -
                              static_cast<int32_t>(previousSample);
        const int32_t interpolated =
            static_cast<int32_t>(previousSample) +
            static_cast<int32_t>((delta * static_cast<int64_t>(fraction)) >>
                                 32);
        appendOutput(static_cast<int16_t>(interpolated));
        nextOutputPosition += outputStep;
      }
      previousSample = currentSample;
    }
    flushOutput();
    audioSamplesWritten_ += samples;
  }

  audioTask_ = nullptr;
  vTaskDelete(nullptr);
}

void CydMediaBackend::fail(const char* message) {
  Serial.printf("[media] error: %s\n", message);
  stopAudio();
  session_.fail(message);
}

bool CydMediaBackend::seekAligned(uint32_t dataStart, uint32_t size) {
  const uint32_t next = dataStart + size + (size & 1U);
  return file_.seek(next);
}

bool CydMediaBackend::readBytes(void* destination, size_t size) {
  return file_.read(static_cast<uint8_t*>(destination), size) == size;
}

bool CydMediaBackend::readChunkHeader(uint32_t& id, uint32_t& size) {
  if (file_.available() < 8) return false;
  id = readU32();
  size = readU32();
  return true;
}

uint32_t CydMediaBackend::readU32() {
  uint8_t bytes[4] = {};
  if (!readBytes(bytes, sizeof(bytes))) return 0;
  return littleU32(bytes);
}

uint16_t CydMediaBackend::readU16() {
  uint8_t bytes[2] = {};
  if (!readBytes(bytes, sizeof(bytes))) return 0;
  return littleU16(bytes);
}
