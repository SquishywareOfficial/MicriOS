#pragma once

#include "../../App.h"

class TFT_eSPI;

namespace MinerCluster {
class MasterEngine;
class SlaveEngine;
struct MasterStats;
struct SlaveStats;
enum class MasterState : uint8_t;
enum class SlaveState : uint8_t;
}

class DistributedMinerApp : public App {
public:
  DistributedMinerApp(uint32_t width, uint32_t height);
  ~DistributedMinerApp() override;

  bool hasCustomOverlay() const override;
  uint16_t runningRenderIntervalMs() const override;
  uint16_t staticRenderIntervalMs() const override;
  bool wantsImmediateRender() const override;

  void debugStartCluster();
  void debugStopCluster();
  void debugStartPairing();
  void debugSetLocalMining(bool enabled);
  void debugResetCluster();
  void debugPrintStats(const char* reason = "debug");

protected:
  bool updateStart(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
  void onAppReset() override;
  void onAppExit() override;
  void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
  void drawRunning(TFT_eSPI& tft) override;
  void drawStart(TFT_eSPI& tft) override;

private:
  enum class Role : uint8_t {
    Master,
    Slave
  };

  enum class MasterPage : uint8_t {
    Dashboard,
    Slaves,
    Pool,
    Pairing,
    Controls,
    Reset,
    Count
  };

  enum class SlavePage : uint8_t {
    Status,
    Work,
    Debug,
    Clear,
    Exit,
    Count
  };

  void stopAll();
  void switchToMasterForDebug();
  void markDirty();
  void forceClear();
  void printStatsLine(const MinerCluster::MasterStats& stats, const char* reason);
  const char* pageTitle() const;
  uint16_t stateColor(MinerCluster::MasterState state) const;
  uint16_t slaveStateColor(MinerCluster::SlaveState state) const;

  template <typename Canvas>
  void drawFrame(Canvas& canvas);
  template <typename Canvas>
  void drawDashboard(Canvas& canvas, const MinerCluster::MasterStats& stats);
  template <typename Canvas>
  void drawSlaves(Canvas& canvas);
  template <typename Canvas>
  void drawPool(Canvas& canvas, const MinerCluster::MasterStats& stats);
  template <typename Canvas>
  void drawPairing(Canvas& canvas, const MinerCluster::MasterStats& stats);
  template <typename Canvas>
  void drawControls(Canvas& canvas, const MinerCluster::MasterStats& stats);
  template <typename Canvas>
  void drawReset(Canvas& canvas, const MinerCluster::MasterStats& stats);
  template <typename Canvas>
  void drawSlaveStatus(Canvas& canvas, const MinerCluster::SlaveStats& stats);
  template <typename Canvas>
  void drawSlaveWork(Canvas& canvas, const MinerCluster::SlaveStats& stats);
  template <typename Canvas>
  void drawSlaveDebug(Canvas& canvas, const MinerCluster::SlaveStats& stats);
  template <typename Canvas>
  void drawSlaveClear(Canvas& canvas, const MinerCluster::SlaveStats& stats);
  template <typename Canvas>
  void drawSlaveExit(Canvas& canvas, const MinerCluster::SlaveStats& stats);

  MinerCluster::MasterEngine* cluster_;
  MinerCluster::SlaveEngine* slave_ = nullptr;
  Role selectedRole_ = Role::Master;
  Role activeRole_ = Role::Master;
  uint8_t page_ = 0;
  bool dirty_ = true;
  bool forceScreenClear_ = true;
  bool startIntroPageRendered_ = false;
  uint8_t lastStartIntroPage_ = 255;
  uint32_t lastSerialStatsMs_ = 0;
};
