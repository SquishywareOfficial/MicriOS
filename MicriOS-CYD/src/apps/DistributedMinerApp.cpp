#include "DistributedMinerApp.h"

#include <TFT_eSPI.h>

#include "../../CydFramebuffer.h"
#include "../../CydHardware.h"
#include "../../CydUi.h"
#include "../shared/logic/MinerClusterLogic.h"

namespace {
constexpr const char* HOSTNAME = "MicriDeck-Cluster";

template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}

template <typename Canvas>
void drawFit(Canvas& canvas, const String& text, int x, int y, int maxWidth, uint8_t size, uint16_t color) {
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(size);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(CydUi::fitText(canvas, text, maxWidth), x, y);
}

String rateLabel(uint32_t hps) {
  char text[20];
  if (hps >= 1000000UL) {
    snprintf(text, sizeof(text), "%.2f MH/s", hps / 1000000.0f);
  } else if (hps >= 1000UL) {
    snprintf(text, sizeof(text), "%.1f KH/s", hps / 1000.0f);
  } else {
    snprintf(text, sizeof(text), "%lu H/s", static_cast<unsigned long>(hps));
  }
  return String(text);
}
}

DistributedMinerApp::DistributedMinerApp(uint32_t width, uint32_t height)
    : App("Distributed Miner", width, height), cluster_(new MinerCluster::MasterEngine()) {}

DistributedMinerApp::~DistributedMinerApp() {
  stopAll();
  delete cluster_;
}

bool DistributedMinerApp::hasCustomOverlay() const {
  return true;
}

uint16_t DistributedMinerApp::runningRenderIntervalMs() const {
  return 500;
}

uint16_t DistributedMinerApp::staticRenderIntervalMs() const {
  return 0;
}

bool DistributedMinerApp::wantsImmediateRender() const {
  if (phase() == AppPhase::Start && (!startIntroPageRendered_ || startIntroPage() != lastStartIntroPage_)) {
    return true;
  }
  return dirty_;
}

void DistributedMinerApp::debugStartCluster() {
  switchToMasterForDebug();
  if (!cluster_->running()) {
    cluster_->start(HOSTNAME);
    forceClear();
  }
  debugPrintStats("debug-start");
}

void DistributedMinerApp::debugStopCluster() {
  switchToMasterForDebug();
  if (cluster_->running()) {
    cluster_->stop();
    forceClear();
  }
  debugPrintStats("debug-stop");
}

void DistributedMinerApp::debugStartPairing() {
  switchToMasterForDebug();
  if (!cluster_->running()) {
    cluster_->start(HOSTNAME);
  }
  cluster_->startPairing();
  forceClear();
  debugPrintStats("debug-pair");
}

void DistributedMinerApp::debugSetLocalMining(bool enabled) {
  switchToMasterForDebug();
  cluster_->setLocalMining(enabled);
  forceClear();
  debugPrintStats(enabled ? "local-on" : "local-off");
}

void DistributedMinerApp::debugResetCluster() {
  switchToMasterForDebug();
  cluster_->resetClusterIdentity();
  forceClear();
  debugPrintStats("cluster-reset");
}

void DistributedMinerApp::debugPrintStats(const char* reason) {
  printStatsLine(cluster_->stats(), reason);
}

bool DistributedMinerApp::updateStart(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  if (b1.click) {
    selectedRole_ = selectedRole_ == Role::Master ? Role::Slave : Role::Master;
    markDirty();
  } else if (b2.click || b1.longPress) {
    startRunning();
  }
  return true;
}

namespace {
constexpr TouchUi::Rect ROLE_MASTER = {20, 49, 130, 102};
constexpr TouchUi::Rect ROLE_SLAVE = {170, 49, 130, 102};
constexpr TouchUi::Rect ROLE_START = {90, 163, 140, 38};
constexpr TouchUi::Rect CLUSTER_PREV = {8, 174, 52, 28};
constexpr TouchUi::Rect CLUSTER_ACTION = {66, 166, 188, 36};
constexpr TouchUi::Rect CLUSTER_NEXT = {260, 174, 52, 28};
constexpr TouchUi::Rect CLUSTER_CANCEL = {20, 151, 132, 43};
constexpr TouchUi::Rect CLUSTER_CONFIRM = {168, 151, 132, 43};
}

bool DistributedMinerApp::handleTouch(const TouchUi::TouchSample& sample,
                                      const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;
  if (phase() == AppPhase::Start) {
    if (ROLE_MASTER.contains(point)) hit = 0;
    if (ROLE_SLAVE.contains(point)) hit = 1;
    if (ROLE_START.contains(point)) hit = 2;
  } else if (confirmDestructive_) {
    if (CLUSTER_CANCEL.contains(point)) hit = 0;
    if (CLUSTER_CONFIRM.contains(point)) hit = 1;
  } else {
    if (CLUSTER_PREV.contains(point)) hit = 0;
    if (CLUSTER_ACTION.contains(point)) hit = 1;
    if (CLUSTER_NEXT.contains(point)) hit = 2;
  }

  const auto result = touchCapture_.update(sample, event, hit);
  if (result.pressed || result.released) markDirty();
  if (!result.activated) {
    if (phase() == AppPhase::Running && !confirmDestructive_ &&
        event.released &&
        (event.swipe == TouchUi::SwipeDirection::Left ||
         event.swipe == TouchUi::SwipeDirection::Right)) {
      const uint8_t count = activeRole_ == Role::Master
          ? static_cast<uint8_t>(MasterPage::Count)
          : static_cast<uint8_t>(SlavePage::Count);
      page_ = event.swipe == TouchUi::SwipeDirection::Left
          ? (page_ + 1) % count
          : (page_ == 0 ? count - 1 : page_ - 1);
      forceClear();
      return true;
    }
    return result.consumed;
  }

  if (phase() == AppPhase::Start) {
    if (result.id == 0) selectedRole_ = Role::Master;
    if (result.id == 1) selectedRole_ = Role::Slave;
    if (result.id == 2) startRunning();
    touchCapture_.reset();
    markDirty();
    return true;
  }

  if (confirmDestructive_) {
    if (result.id == 1) {
      if (activeRole_ == Role::Master) cluster_->resetClusterIdentity();
      else if (slave_ != nullptr) slave_->clearPairing();
    }
    confirmDestructive_ = false;
    touchCapture_.reset();
    forceClear();
    return true;
  }

  const uint8_t count = activeRole_ == Role::Master
      ? static_cast<uint8_t>(MasterPage::Count)
      : static_cast<uint8_t>(SlavePage::Count);
  if (result.id == 0) {
    page_ = page_ == 0 ? count - 1 : page_ - 1;
  } else if (result.id == 2) {
    page_ = (page_ + 1) % count;
  } else if (activeRole_ == Role::Master) {
    const MasterPage page = static_cast<MasterPage>(page_);
    if (page == MasterPage::Dashboard || page == MasterPage::Pool) {
      if (cluster_->running()) cluster_->stop();
      else cluster_->start(HOSTNAME);
    } else if (page == MasterPage::Pairing || page == MasterPage::Slaves) {
      if (!cluster_->running()) cluster_->start(HOSTNAME);
      cluster_->startPairing();
    } else if (page == MasterPage::Controls) {
      cluster_->setLocalMining(!cluster_->localMiningEnabled());
    } else if (page == MasterPage::Reset) {
      confirmDestructive_ = true;
    }
  } else {
    const SlavePage page = static_cast<SlavePage>(page_);
    if (page == SlavePage::Clear) confirmDestructive_ = true;
    if (page == SlavePage::Exit) requestExitToMenu();
  }
  touchCapture_.reset();
  forceClear();
  return true;
}

void DistributedMinerApp::onAppReset() {
  stopAll();
  activeRole_ = selectedRole_;
  page_ = 0;
  lastSerialStatsMs_ = 0;
  confirmDestructive_ = false;
  touchCapture_.reset();
  if (activeRole_ == Role::Slave) {
    slave_ = new MinerCluster::SlaveEngine();
    slave_->begin();
  }
  forceClear();
}

void DistributedMinerApp::onAppExit() {
  stopAll();
}

void DistributedMinerApp::stopAll() {
  if (cluster_ != nullptr) {
    cluster_->stop();
  }
  if (slave_ != nullptr) {
    slave_->stop();
    delete slave_;
    slave_ = nullptr;
  }
}

void DistributedMinerApp::switchToMasterForDebug() {
  if (activeRole_ != Role::Master) {
    if (slave_ != nullptr) {
      slave_->stop();
      delete slave_;
      slave_ = nullptr;
    }
    activeRole_ = Role::Master;
    selectedRole_ = Role::Master;
    page_ = 0;
  }
}

void DistributedMinerApp::markDirty() {
  dirty_ = true;
}

void DistributedMinerApp::forceClear() {
  dirty_ = true;
  forceScreenClear_ = true;
}

void DistributedMinerApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  const uint32_t now = millis();
  if (slave_ != nullptr) {
    slave_->update();
  }

  if (activeRole_ == Role::Master && cluster_->running() && (lastSerialStatsMs_ == 0 || now - lastSerialStatsMs_ >= 2000)) {
    lastSerialStatsMs_ = now;
    printStatsLine(cluster_->stats(), "tick");
  }

  if (b2.click) {
    const uint8_t count = activeRole_ == Role::Master
                              ? static_cast<uint8_t>(MasterPage::Count)
                              : static_cast<uint8_t>(SlavePage::Count);
    page_ = (page_ + 1) % count;
    forceClear();
    return;
  }

  if (activeRole_ == Role::Master && b1.click) {
    const MasterPage page = static_cast<MasterPage>(page_);
    if (page == MasterPage::Dashboard) {
      if (cluster_->running()) cluster_->stop();
      else cluster_->start(HOSTNAME);
      forceClear();
    } else if (page == MasterPage::Controls) {
      cluster_->setLocalMining(!cluster_->localMiningEnabled());
      forceClear();
    } else if (page == MasterPage::Pairing) {
      if (!cluster_->running()) cluster_->start(HOSTNAME);
      cluster_->startPairing();
      forceClear();
    } else if (page == MasterPage::Reset) {
      cluster_->resetClusterIdentity();
      forceClear();
    }
    return;
  }

  if (activeRole_ == Role::Master && b1.longPress) {
    const MasterPage page = static_cast<MasterPage>(page_);
    if (page == MasterPage::Reset) {
      cluster_->resetClusterIdentity();
    } else if (page == MasterPage::Controls) {
      if (cluster_->running()) cluster_->stop();
      else cluster_->start(HOSTNAME);
    } else {
      if (!cluster_->running()) cluster_->start(HOSTNAME);
      cluster_->startPairing();
    }
    forceClear();
  } else if (activeRole_ == Role::Slave && b1.longPress) {
    const SlavePage page = static_cast<SlavePage>(page_);
    if (page == SlavePage::Clear && slave_ != nullptr) {
      slave_->clearPairing();
      forceClear();
    } else if (page == SlavePage::Exit) {
      requestExitToMenu();
    }
  }
}

void DistributedMinerApp::printStatsLine(const MinerCluster::MasterStats& stats, const char* reason) {
  Serial.print("[cluster] reason=");
  Serial.print(reason);
  Serial.print(" state=");
  Serial.print(MinerCluster::masterStateLabel(stats.state));
  Serial.print(" total_khps=");
  Serial.print(stats.totalHps / 1000.0f, 1);
  Serial.print(" local_khps=");
  Serial.print(stats.localHps / 1000.0f, 1);
  Serial.print(" slave_khps=");
  Serial.print(stats.slaveHps / 1000.0f, 1);
  Serial.print(" slaves=");
  Serial.print(stats.slaveCount);
  Serial.print(" local=");
  Serial.print(stats.localMining ? "on" : "off");
  Serial.print(" jobs=");
  Serial.print(stats.jobs);
  Serial.print(" submitted=");
  Serial.print(stats.submitted);
  Serial.print(" accepted=");
  Serial.print(stats.accepted);
  Serial.print(" rejected=");
  Serial.print(stats.rejected);
  Serial.print(" channel=");
  Serial.print(stats.channel);
  if (stats.lastError[0]) {
    Serial.print(" error=");
    Serial.print(stats.lastError);
  }
  Serial.println();
}

const char* DistributedMinerApp::pageTitle() const {
  if (activeRole_ == Role::Slave) {
    switch (static_cast<SlavePage>(page_)) {
      case SlavePage::Work: return "Slave Work";
      case SlavePage::Debug: return "Slave Debug";
      case SlavePage::Clear: return "Clear Pairing";
      case SlavePage::Exit: return "Exit Slave";
      case SlavePage::Status:
      default: return "Miner Slave";
    }
  }

  switch (static_cast<MasterPage>(page_)) {
    case MasterPage::Slaves: return "Cluster Slaves";
    case MasterPage::Pool: return "Cluster Pool";
    case MasterPage::Pairing: return "Pair Slaves";
    case MasterPage::Controls: return "Cluster Control";
    case MasterPage::Reset: return "Reset Cluster";
    case MasterPage::Dashboard:
    default: return "Distributed Miner";
  }
}

uint16_t DistributedMinerApp::stateColor(MinerCluster::MasterState state) const {
  switch (state) {
    case MinerCluster::MasterState::Mining: return TFT_GREEN;
    case MinerCluster::MasterState::Idle: return TFT_CYAN;
    case MinerCluster::MasterState::Stopping: return TFT_ORANGE;
    case MinerCluster::MasterState::NoWifi:
    case MinerCluster::MasterState::WifiFailed:
    case MinerCluster::MasterState::RadioFailed:
    case MinerCluster::MasterState::PoolFailed:
    case MinerCluster::MasterState::Error: return TFT_RED;
    default: return TFT_YELLOW;
  }
}

uint16_t DistributedMinerApp::slaveStateColor(MinerCluster::SlaveState state) const {
  switch (state) {
    case MinerCluster::SlaveState::Mining: return TFT_GREEN;
    case MinerCluster::SlaveState::Paired: return TFT_CYAN;
    case MinerCluster::SlaveState::Pairing: return TFT_YELLOW;
    case MinerCluster::SlaveState::Searching: return TFT_ORANGE;
    case MinerCluster::SlaveState::Error: return TFT_RED;
  }
  return TFT_LIGHTGREY;
}

template <typename Canvas>
void DistributedMinerApp::drawPageControls(Canvas& canvas, const char* action,
                                           uint16_t color) {
  CydUi::touchAction(canvas, CLUSTER_PREV, "<", TFT_BLUE, false,
                     touchCapture_.activeId() == 0);
  if (action != nullptr) {
    CydUi::touchAction(canvas, CLUSTER_ACTION, action,
                       color == 0 ? TFT_CYAN : color, true,
                       touchCapture_.activeId() == 1);
  }
  CydUi::touchAction(canvas, CLUSTER_NEXT, ">", TFT_BLUE, false,
                     touchCapture_.activeId() == 2);
}

template <typename Canvas>
void DistributedMinerApp::drawDashboard(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  const uint16_t color = stateColor(stats.state);
  CydUi::header(canvas, pageTitle(), color, MinerCluster::masterStateLabel(stats.state));
  CydUi::largeValue(canvas, rateLabel(stats.totalHps), 55, color);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(String("Local ") + rateLabel(stats.localHps) + (stats.localMining ? " on" : " off"), 16, 126);
  canvas.drawString(String("Slaves ") + stats.slaveCount + "  " + rateLabel(stats.slaveHps), 16, 148);
  drawPageControls(canvas, cluster_->running() ? "Stop cluster" : "Start cluster",
                   cluster_->running() ? TFT_ORANGE : TFT_GREEN);
}

template <typename Canvas>
void DistributedMinerApp::drawSlaves(Canvas& canvas) {
  CydUi::header(canvas, pageTitle(), TFT_CYAN);
  MinerCluster::SlaveRecord slave;
  bool any = false;
  for (uint8_t row = 0; row < 6; row++) {
    if (!cluster_->slaveAt(row, slave)) break;
    any = true;
    const int y = 43 + row * 22;
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.drawString(MinerCluster::macSuffix(slave.mac), 12, y);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawString(rateLabel(slave.hashrate), 92, y);
    drawFit(canvas, slave.status, 206, y, 100, 1, TFT_LIGHTGREY);
  }
  if (!any) {
    CydUi::centered(canvas, "No slaves yet", 52, 2, TFT_LIGHTGREY);
    CydUi::centered(canvas, "Use pair page", 78, 1, TFT_CYAN);
  }
  drawPageControls(canvas, "Pair new slave", TFT_CYAN);
}

template <typename Canvas>
void DistributedMinerApp::drawPool(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  const MinerCluster::MinerConfig config = MinerCluster::loadMinerConfig();
  CydUi::header(canvas, pageTitle(), stateColor(stats.state));
  drawFit(canvas, String("WiFi: ") + (stats.ssid[0] ? stats.ssid : "-"), 14, 45, 292, 1, TFT_WHITE);
  drawFit(canvas, String("Pool: ") + config.poolHost + ":" + config.poolPort, 14, 72, 292, 1, TFT_CYAN);
  drawFit(canvas, String("Jobs: ") + stats.jobs + "  Diff: " + String(stats.poolDifficulty, 5), 14, 99, 292, 1, TFT_LIGHTGREY);
  drawFit(canvas, String("OK/Rej: ") + stats.accepted + "/" + stats.rejected, 14, 126, 292, 1, TFT_GREEN);
  drawFit(canvas, String("Worker: ") + MinerCluster::workerName(), 14, 153, 292, 1, TFT_YELLOW);
  drawPageControls(canvas, cluster_->running() ? "Stop cluster" : "Start cluster",
                   cluster_->running() ? TFT_ORANGE : TFT_GREEN);
}

template <typename Canvas>
void DistributedMinerApp::drawPairing(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  const bool radioReady = stats.channel != 0;
  const uint16_t color = stats.pairing && radioReady ? TFT_GREEN : (stats.pairing ? TFT_YELLOW : TFT_ORANGE);
  CydUi::header(canvas, pageTitle(), color, MinerCluster::masterStateLabel(stats.state));
  if (stats.pairing && radioReady) {
    CydUi::centered(canvas, "Pairing Open", 39, 2, TFT_GREEN);
    CydUi::centered(canvas, String("Channel ") + stats.channel, 66, 2, TFT_CYAN);
  } else if (stats.pairing) {
    CydUi::centered(canvas, "Radio Waiting", 39, 2, TFT_YELLOW);
    CydUi::centered(canvas, "Need WiFi first", 67, 1, TFT_LIGHTGREY);
  } else {
    CydUi::centered(canvas, "Pairing Closed", 43, 2, TFT_ORANGE);
    CydUi::centered(canvas, "Open for 60 seconds", 70, 1, TFT_LIGHTGREY);
  }
  CydUi::centered(canvas, String(stats.slaveCount) + " slaves", 96, 1, TFT_LIGHTGREY);
  drawPageControls(canvas, "Open pairing", TFT_CYAN);
}

template <typename Canvas>
void DistributedMinerApp::drawControls(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  CydUi::header(canvas, pageTitle(), TFT_YELLOW);
  CydUi::row(canvas, 48, stats.localMining ? "Local Mining: On" : "Local Mining: Off", true, TFT_YELLOW);
  CydUi::row(canvas, 91, cluster_->running() ? "Cluster: Running" : "Cluster: Stopped", false, TFT_CYAN);
  CydUi::centered(canvas, "Tap below to toggle", 142, 1, TFT_LIGHTGREY);
  drawPageControls(canvas, stats.localMining ? "Local mining: On"
                                              : "Local mining: Off",
                   stats.localMining ? TFT_GREEN : TFT_ORANGE);
}

template <typename Canvas>
void DistributedMinerApp::drawReset(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  CydUi::header(canvas, pageTitle(), TFT_RED, MinerCluster::masterStateLabel(stats.state));
  CydUi::centered(canvas, "Forget slaves", 38, 2, TFT_ORANGE);
  CydUi::centered(canvas, "New cluster id", 64, 2, TFT_CYAN);
  CydUi::centered(canvas, stats.localMining ? "Local mining kept ON" : "Local mining kept OFF", 95, 1, TFT_LIGHTGREY);
  drawPageControls(canvas, "Reset cluster", TFT_RED);
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveStatus(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  const uint16_t color = slaveStateColor(stats.state);
  CydUi::header(canvas, pageTitle(), color, MinerCluster::slaveStateLabel(stats.state));
  CydUi::largeValue(canvas, stats.paired ? "Linked" : "Seeking", 39, color);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(String("Channel ") + stats.channel, 16, 84);
  canvas.drawString(String("Master ") + (stats.paired ? MinerCluster::macSuffix(stats.masterMac) : String("---")), 16, 99);
  drawPageControls(canvas);
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveWork(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  const uint16_t color = slaveStateColor(stats.state);
  CydUi::header(canvas, pageTitle(), color);
  CydUi::largeValue(canvas, rateLabel(stats.hashrate), 39, color);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(String("Jobs ") + stats.jobs + "  Done " + stats.completed, 16, 84);
  canvas.drawString(String("Total ") + static_cast<unsigned long>(stats.totalHashes / 1000ULL) + " KH", 16, 99);
  drawPageControls(canvas);
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveDebug(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  CydUi::header(canvas, pageTitle(), TFT_YELLOW, MinerCluster::slaveStateLabel(stats.state));
  drawFit(canvas, String("Assign ") + stats.currentAssignmentId + "  Nonces " + stats.assignmentSize, 14, 48, 292, 1, TFT_CYAN);
  drawFit(canvas, String("Done ") + stats.assignmentDone + "  Result " + stats.lastResultHashes, 14, 78, 292, 1, TFT_WHITE);
  drawFit(canvas, String("Last result ") + stats.lastResultMs + " ms", 14, 108, 292, 1, TFT_LIGHTGREY);
  drawFit(canvas, String("Best diff ") + String(stats.bestDifficulty, 6), 14, 138, 292, 1, TFT_GREEN);
  drawPageControls(canvas);
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveClear(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  CydUi::header(canvas, pageTitle(), TFT_ORANGE, MinerCluster::slaveStateLabel(stats.state));
  CydUi::centered(canvas, stats.paired ? "Saved Master" : "No Master", 38, 2, stats.paired ? TFT_CYAN : TFT_LIGHTGREY);
  CydUi::centered(canvas, stats.paired ? MinerCluster::macSuffix(stats.masterMac) : "Seeking pair", 66, 2, TFT_WHITE);
  CydUi::centered(canvas, "Remove the saved master", 96, 1, TFT_LIGHTGREY);
  drawPageControls(canvas, "Clear pairing", TFT_RED);
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveExit(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  CydUi::header(canvas, pageTitle(), TFT_RED, MinerCluster::slaveStateLabel(stats.state));
  CydUi::centered(canvas, "Exit", 42, 3, TFT_WHITE);
  CydUi::centered(canvas, "Return to MicriOS", 82, 1, TFT_LIGHTGREY);
  drawPageControls(canvas, "Exit", TFT_RED);
}

template <typename Canvas>
void DistributedMinerApp::drawFrame(Canvas& canvas) {
  CydUi::clear(canvas);
  if (confirmDestructive_) {
    CydUi::confirmation(canvas,
                        activeRole_ == Role::Master ? "Reset Cluster"
                                                    : "Clear Pairing",
                        activeRole_ == Role::Master
                            ? "Forget every paired slave?"
                            : "Forget the saved master?",
                        CLUSTER_CANCEL, CLUSTER_CONFIRM,
                        activeRole_ == Role::Master ? "Reset" : "Clear");
    dirty_ = false;
    return;
  }
  if (activeRole_ == Role::Slave) {
    const MinerCluster::SlaveStats stats = slave_ != nullptr ? slave_->stats() : MinerCluster::SlaveStats();
    switch (static_cast<SlavePage>(page_)) {
      case SlavePage::Work:
        drawSlaveWork(canvas, stats);
        break;
      case SlavePage::Debug:
        drawSlaveDebug(canvas, stats);
        break;
      case SlavePage::Clear:
        drawSlaveClear(canvas, stats);
        break;
      case SlavePage::Exit:
        drawSlaveExit(canvas, stats);
        break;
      case SlavePage::Status:
      default:
        drawSlaveStatus(canvas, stats);
        break;
    }
  } else {
    const MinerCluster::MasterStats stats = cluster_->stats();
    switch (static_cast<MasterPage>(page_)) {
      case MasterPage::Slaves:
        drawSlaves(canvas);
        break;
      case MasterPage::Pool:
        drawPool(canvas, stats);
        break;
      case MasterPage::Pairing:
        drawPairing(canvas, stats);
        break;
      case MasterPage::Controls:
        drawControls(canvas, stats);
        break;
      case MasterPage::Reset:
        drawReset(canvas, stats);
        break;
      case MasterPage::Dashboard:
      default:
        drawDashboard(canvas, stats);
        break;
    }
  }
  dirty_ = false;
}

void DistributedMinerApp::drawRunning(TFT_eSPI& tft) {
  CydHardware::applyDisplayOrientation(tft);
  if (forceScreenClear_) {
    forceScreenClear_ = false;
  }
  drawBuffered(tft, width, height, [this](auto& canvas) { drawFrame(canvas); });
}

void DistributedMinerApp::drawStart(TFT_eSPI& tft) {
  CydHardware::applyDisplayOrientation(tft);
  drawBuffered(tft, width, height, [this](auto& canvas) {
    CydUi::clear(canvas);
    CydUi::header(canvas, "Distributed Miner", TFT_ORANGE);
    CydUi::touchAction(canvas, ROLE_MASTER, "Master", TFT_CYAN,
                       selectedRole_ == Role::Master,
                       touchCapture_.activeId() == 0);
    CydUi::touchAction(canvas, ROLE_SLAVE, "Slave", TFT_GREEN,
                       selectedRole_ == Role::Slave,
                       touchCapture_.activeId() == 1);
    CydUi::touchAction(canvas, ROLE_START, "Start", TFT_ORANGE, true,
                       touchCapture_.activeId() == 2);
  });
  lastStartIntroPage_ = startIntroPage();
  startIntroPageRendered_ = true;
  dirty_ = false;
}
