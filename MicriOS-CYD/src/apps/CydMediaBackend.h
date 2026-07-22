#pragma once

#include <Arduino.h>
#include <FS.h>
using fs::File;
#include <JPEGDEC.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include "../shared/logic/MediaPlayerLogic.h"

class CydMediaBackend {
 public:
  static constexpr int16_t VIDEO_WIDTH = 320;
  static constexpr int16_t VIDEO_HEIGHT = 176;

  CydMediaBackend();
  ~CydMediaBackend();

  bool begin();
  void end();
  bool scan(MediaPlayerLogic::Catalog& catalog);
  bool enterDirectory(const char* path, MediaPlayerLogic::Catalog& catalog);
  bool goUp(MediaPlayerLogic::Catalog& catalog);
  bool atRoot() const;
  bool open(const char* path);
  void closePlayback();
  bool service(TFT_eSPI& tft);
  void pause();
  void resume();
  void restart();
  bool seekTo(uint32_t targetMs, TFT_eSPI& tft);
  void setMuted(bool muted);

  bool mounted() const { return mounted_; }
  const char* mountedPath() const { return currentPath_; }
  MediaPlayerLogic::Session& session() { return session_; }
  const MediaPlayerLogic::Session& session() const { return session_; }
  const char* currentTitle() const { return currentTitle_; }
  uint32_t decodedFrames() const { return decodedFrames_; }
  uint32_t droppedFrames() const { return droppedFrames_; }
  uint32_t audioStarvations() const { return audioStarvations_; }
  uint32_t audioWriteErrors() const { return audioWriteErrors_; }
  bool audioOnly() const { return waveAudioOnly_; }

 private:
  static constexpr uint8_t PIN_SD_CS = 5;
  static constexpr uint8_t PIN_SD_CLK = 18;
  static constexpr uint8_t PIN_SD_MOSI = 23;
  static constexpr uint8_t PIN_SD_MISO = 19;
  static constexpr uint32_t SD_FREQUENCY = 20000000;
  static constexpr uint32_t MAX_JPEG_BYTES = 96 * 1024;
  static constexpr size_t AUDIO_STREAM_BYTES = 32 * 1024;
  static constexpr uint32_t DAC_OUTPUT_RATE = 32000;
  static constexpr uint16_t AVI_SEEK_STRIDE_FRAMES = 30;

  bool scanDirectory(const char* path, MediaPlayerLogic::Catalog& catalog);
  bool loadDirectory(const char* path, MediaPlayerLogic::Catalog& catalog);
  bool parseHeader();
  bool parseWaveHeader();
  bool readNextChunk(TFT_eSPI& tft);
  bool readVideoChunk(TFT_eSPI& tft, uint32_t size);
  bool readAudioChunk(uint32_t size);
  bool decodePendingFrame(TFT_eSPI& tft);
  bool beginAudio();
  bool seekWave(uint32_t targetMs);
  bool seekAvi(uint32_t targetMs, TFT_eSPI& tft);
  bool locateAviIndex();
  bool buildAviSeekIndex();
  bool resolveAviIndexBase(uint32_t firstOffset, uint32_t firstChunkId);
  bool chunkIdMatches(uint32_t position, uint32_t expectedId);
  void clearAviSeekIndex();
  void stopAudio();
  void resetAudioPipeline(bool discardQueuedAudio);
  void writeDacSilence();
  static void audioTaskEntry(void* context);
  void audioTaskLoop();
  void fail(const char* message);
  bool seekAligned(uint32_t dataStart, uint32_t size);
  bool readBytes(void* destination, size_t size);
  bool readChunkHeader(uint32_t& id, uint32_t& size);
  uint32_t readU32();
  uint16_t readU16();

  SPIClass sdSpi_;
  File file_;
  JPEGDEC jpeg_;
  MediaPlayerLogic::Session session_;
  MediaPlayerLogic::Metadata metadata_;
  uint32_t moviStart_ = 0;
  uint32_t moviEnd_ = 0;
  uint32_t idx1DataStart_ = 0;
  uint32_t idx1Size_ = 0;
  uint32_t aviIndexOffsetBase_ = 0;
  uint32_t* aviSeekOffsets_ = nullptr;
  uint32_t aviSeekIndexCount_ = 0;
  bool aviSeekIndexAttempted_ = false;
  uint32_t waveDataStart_ = 0;
  uint32_t waveDataSize_ = 0;
  uint32_t waveDrainStartedMs_ = 0;
  uint32_t nextFrameDueUs_ = 0;
  uint32_t decodedFrames_ = 0;
  uint32_t droppedFrames_ = 0;
  uint8_t* jpegBuffer_ = nullptr;
  uint32_t jpegCapacity_ = 0;
  uint32_t pendingJpegBytes_ = 0;
  uint32_t audioChunkRemaining_ = 0;
  uint32_t audioChunkAlignedEnd_ = 0;
  bool mounted_ = false;
  bool audioReady_ = false;
  bool audioInstalled_ = false;
  bool framePending_ = false;
  bool waveAudioOnly_ = false;
  volatile bool audioTaskRunning_ = false;
  volatile bool audioShouldPlay_ = false;
  volatile uint32_t audioResetGeneration_ = 0;
  volatile uint32_t audioStarvations_ = 0;
  volatile uint32_t audioWriteErrors_ = 0;
  volatile uint32_t audioSamplesWritten_ = 0;
  StreamBufferHandle_t audioStream_ = nullptr;
  TaskHandle_t audioTask_ = nullptr;
  uint32_t lastAudioDiagnosticMs_ = 0;
  char rootPath_[MediaPlayerLogic::MAX_PATH_LENGTH] = {};
  char currentPath_[MediaPlayerLogic::MAX_PATH_LENGTH] = {};
  char currentTitle_[MediaPlayerLogic::MAX_TITLE_LENGTH] = {};
};
