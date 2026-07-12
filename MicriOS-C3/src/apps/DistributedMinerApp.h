#pragma once

#include <stddef.h>

#include "../../App.h"

namespace MinerCluster {
class MasterEngine;
class SlaveEngine;
struct MasterStats;
struct SlaveStats;
}

class DistributedMinerApp : public App {
  public:
    DistributedMinerApp(uint32_t width, uint32_t height, uint32_t left = 1);
    ~DistributedMinerApp() override;

    bool hasCustomOverlay() const override;
    uint16_t runningRenderIntervalMs() const override;

  protected:
    bool updateStart(uint32_t deltaMs, const ButtonInput& input) override;
    void onAppReset() override;
    void onAppExit() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;

  private:
    enum class Role : uint8_t {
      Master,
      Slave
    };

    enum class MasterPage : uint8_t {
      Status,
      Slaves,
      Local,
      Exit,
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
    void doPageAction();
    void drawMaster(U8G2& u8g2);
    void drawSlave(U8G2& u8g2);
    void drawFit(U8G2& u8g2, int x, int y, const char* text, uint8_t maxWidth) const;
    const char* shortMasterState(const MinerCluster::MasterStats& stats) const;
    const char* shortSlaveState(const MinerCluster::SlaveStats& stats) const;
    void rateText(uint32_t hps, char* out, size_t len) const;

    MinerCluster::MasterEngine* master_ = nullptr;
    MinerCluster::SlaveEngine* slave_ = nullptr;
    Role selectedRole_ = Role::Master;
    Role activeRole_ = Role::Master;
    uint8_t page_ = 0;
    uint32_t left_ = 1;
};
