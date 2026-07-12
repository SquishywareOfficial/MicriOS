#include "DistributedMinerApp.h"

#include <Arduino.h>
#include <U8g2lib.h>

#define FEMTO_CLUSTER_SERIAL_DEBUG 1
#include "../shared/logic/MinerClusterLogic.h"

namespace {
constexpr const char* HOSTNAME = "MicriDeck-C3-Cluster";
}

DistributedMinerApp::DistributedMinerApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Distributed Miner", width, height), left_(left) {}

DistributedMinerApp::~DistributedMinerApp() {
  stopAll();
}

bool DistributedMinerApp::hasCustomOverlay() const {
  return true;
}

uint16_t DistributedMinerApp::runningRenderIntervalMs() const {
  return 500;
}

bool DistributedMinerApp::updateStart(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;
  if (input.click) {
    selectedRole_ = selectedRole_ == Role::Master ? Role::Slave : Role::Master;
  } else if (input.longPress) {
    startRunning();
  }
  return true;
}

void DistributedMinerApp::onAppReset() {
  stopAll();
  activeRole_ = selectedRole_;
  page_ = 0;

  if (activeRole_ == Role::Master) {
    master_ = new MinerCluster::MasterEngine();
  } else {
    slave_ = new MinerCluster::SlaveEngine();
    slave_->begin();
  }
}

void DistributedMinerApp::onAppExit() {
  stopAll();
}

void DistributedMinerApp::stopAll() {
  if (master_ != nullptr) {
    master_->stop();
    delete master_;
    master_ = nullptr;
  }
  if (slave_ != nullptr) {
    slave_->stop();
    delete slave_;
    slave_ = nullptr;
  }
}

void DistributedMinerApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;
  if (slave_ != nullptr) {
    slave_->update();
  }

  if (input.click) {
    if (activeRole_ == Role::Master) {
      page_ = (page_ + 1) % static_cast<uint8_t>(MasterPage::Count);
    } else {
      page_ = (page_ + 1) % static_cast<uint8_t>(SlavePage::Count);
    }
  }

  if (input.longPress) {
    doPageAction();
  }
}

void DistributedMinerApp::doPageAction() {
  if (activeRole_ == Role::Master) {
    if (master_ == nullptr) return;
    const MasterPage page = static_cast<MasterPage>(page_);
    if (page == MasterPage::Status) {
      if (master_->running()) master_->stop();
      else master_->start(HOSTNAME);
    } else if (page == MasterPage::Slaves) {
      if (!master_->running()) master_->start(HOSTNAME);
      master_->startPairing();
    } else if (page == MasterPage::Local) {
      master_->setLocalMining(!master_->localMiningEnabled());
    } else if (page == MasterPage::Exit) {
      requestExitToMenu();
    }
  } else {
    if (slave_ == nullptr) return;
    const SlavePage page = static_cast<SlavePage>(page_);
    if (page == SlavePage::Clear) {
      slave_->clearPairing();
    } else if (page == SlavePage::Exit) {
      requestExitToMenu();
    }
  }
}

void DistributedMinerApp::rateText(uint32_t hps, char* out, size_t len) const {
  if (hps >= 1000000UL) {
    snprintf(out, len, "%.1fM", hps / 1000000.0f);
  } else if (hps >= 1000UL) {
    snprintf(out, len, "%.1fK", hps / 1000.0f);
  } else {
    snprintf(out, len, "%lu", static_cast<unsigned long>(hps));
  }
}

const char* DistributedMinerApp::shortMasterState(const MinerCluster::MasterStats& stats) const {
  switch (stats.state) {
    case MinerCluster::MasterState::Idle: return "Idle";
    case MinerCluster::MasterState::NoWifi: return "No WiFi";
    case MinerCluster::MasterState::ConnectingWifi: return "WiFi...";
    case MinerCluster::MasterState::WifiFailed: return "WiFi fail";
    case MinerCluster::MasterState::RadioFailed: return "Radio fail";
    case MinerCluster::MasterState::ConnectingPool: return "Pool...";
    case MinerCluster::MasterState::PoolFailed: return "Pool fail";
    case MinerCluster::MasterState::Subscribing: return "Sub...";
    case MinerCluster::MasterState::Mining: return "Mining";
    case MinerCluster::MasterState::Stopping: return "Stopping";
    case MinerCluster::MasterState::Error: return "Error";
  }
  return "?";
}

const char* DistributedMinerApp::shortSlaveState(const MinerCluster::SlaveStats& stats) const {
  switch (stats.state) {
    case MinerCluster::SlaveState::Searching: return "Search";
    case MinerCluster::SlaveState::Pairing: return "Pairing";
    case MinerCluster::SlaveState::Paired: return "Paired";
    case MinerCluster::SlaveState::Mining: return "Mining";
    case MinerCluster::SlaveState::Error: return "Error";
  }
  return "?";
}

void DistributedMinerApp::drawFit(U8G2& u8g2, int x, int y, const char* text, uint8_t maxWidth) const {
  char clipped[24];
  strncpy(clipped, text, sizeof(clipped) - 1);
  clipped[sizeof(clipped) - 1] = '\0';
  while (clipped[0] != '\0' && u8g2.getStrWidth(clipped) > maxWidth) {
    clipped[strlen(clipped) - 1] = '\0';
  }
  u8g2.drawStr(x, y, clipped);
}

void DistributedMinerApp::drawMaster(U8G2& u8g2) {
  if (master_ == nullptr) return;
  const MinerCluster::MasterStats stats = master_->stats();
  char rate[14];
  rateText(stats.totalHps, rate, sizeof(rate));

  u8g2.setFont(u8g2_font_4x6_tr);
  const MasterPage page = static_cast<MasterPage>(page_);
  if (page == MasterPage::Status) {
    u8g2.drawStr(left_ + 2, 8, "Dist Miner M");
    u8g2.drawStr(left_ + 2, 18, shortMasterState(stats));
    char line[24];
    snprintf(line, sizeof(line), "%sH/s S%u", rate, stats.slaveCount);
    drawFit(u8g2, left_ + 2, 28, line, width - 4);
    u8g2.drawStr(left_ + 2, 37, master_->running() ? "Hold stop" : "Hold start");
  } else if (page == MasterPage::Slaves) {
    u8g2.drawStr(left_ + 2, 8, "Slaves");
    char line[24];
    snprintf(line, sizeof(line), "Count %u", stats.slaveCount);
    u8g2.drawStr(left_ + 2, 18, line);
    rateText(stats.slaveHps, rate, sizeof(rate));
    snprintf(line, sizeof(line), "%sH/s ch%u", rate, stats.channel);
    drawFit(u8g2, left_ + 2, 28, line, width - 4);
    u8g2.drawStr(left_ + 2, 37, "Hold pair");
  } else if (page == MasterPage::Local) {
    u8g2.drawStr(left_ + 2, 8, "Local Mine");
    u8g2.drawStr(left_ + 2, 18, stats.localMining ? "ON" : "OFF");
    rateText(stats.localHps, rate, sizeof(rate));
    char line[24];
    snprintf(line, sizeof(line), "%sH/s", rate);
    u8g2.drawStr(left_ + 2, 28, line);
    u8g2.drawStr(left_ + 2, 37, "Hold toggle");
  } else {
    u8g2.drawStr(left_ + 2, 12, "Exit");
    u8g2.drawStr(left_ + 2, 25, "Hold menu");
    u8g2.drawStr(left_ + 2, 37, "Tap page");
  }
}

void DistributedMinerApp::drawSlave(U8G2& u8g2) {
  if (slave_ == nullptr) return;
  const MinerCluster::SlaveStats stats = slave_->stats();
  char rate[14];
  rateText(stats.hashrate, rate, sizeof(rate));

  u8g2.setFont(u8g2_font_4x6_tr);
  const SlavePage page = static_cast<SlavePage>(page_);
  if (page == SlavePage::Status) {
    u8g2.drawStr(left_ + 2, 8, "Dist Miner S");
    u8g2.drawStr(left_ + 2, 18, shortSlaveState(stats));
    char line[24];
    const bool linked = stats.state == MinerCluster::SlaveState::Paired || stats.state == MinerCluster::SlaveState::Mining;
    snprintf(line, sizeof(line), "ch%u %s", stats.channel, linked ? "linked" : (stats.paired ? "seek" : "new"));
    drawFit(u8g2, left_ + 2, 28, line, width - 4);
    u8g2.drawStr(left_ + 2, 37, "Tap page");
  } else if (page == SlavePage::Work) {
    u8g2.drawStr(left_ + 2, 8, "Work");
    char line[24];
    snprintf(line, sizeof(line), "%sH/s", rate);
    u8g2.drawStr(left_ + 2, 18, line);
    snprintf(line, sizeof(line), "J%lu C%lu", static_cast<unsigned long>(stats.jobs), static_cast<unsigned long>(stats.completed));
    drawFit(u8g2, left_ + 2, 28, line, width - 4);
    u8g2.drawStr(left_ + 2, 37, "Tap page");
  } else if (page == SlavePage::Debug) {
    u8g2.drawStr(left_ + 2, 8, "Debug");
    char line[24];
    snprintf(line, sizeof(line), "A%lu N%lu", static_cast<unsigned long>(stats.currentAssignmentId), static_cast<unsigned long>(stats.assignmentSize));
    drawFit(u8g2, left_ + 2, 18, line, width - 4);
    snprintf(line, sizeof(line), "D%lu R%lu", static_cast<unsigned long>(stats.assignmentDone), static_cast<unsigned long>(stats.lastResultHashes));
    drawFit(u8g2, left_ + 2, 28, line, width - 4);
    snprintf(line, sizeof(line), "%lums", static_cast<unsigned long>(stats.lastResultMs));
    drawFit(u8g2, left_ + 2, 37, line, width - 4);
  } else if (page == SlavePage::Clear) {
    u8g2.drawStr(left_ + 2, 8, "Pairing");
    u8g2.drawStr(left_ + 2, 18, stats.paired ? "Saved master" : "No master");
    u8g2.drawStr(left_ + 2, 28, "Hold clear");
    u8g2.drawStr(left_ + 2, 37, "Tap page");
  } else {
    u8g2.drawStr(left_ + 2, 12, "Exit");
    u8g2.drawStr(left_ + 2, 25, "Hold menu");
    u8g2.drawStr(left_ + 2, 37, "Tap page");
  }
}

void DistributedMinerApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  if (activeRole_ == Role::Master) {
    drawMaster(u8g2);
  } else {
    drawSlave(u8g2);
  }
}

void DistributedMinerApp::drawStart(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 2, 10, "Dist Miner");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(left_ + 2, 22, selectedRole_ == Role::Master ? "Role: Master" : "Role: Slave");
  u8g2.drawStr(left_ + 2, 36, "Tap role Hold go");
}
