#include "DistributedMinerApp.h"

#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"
#define MICRI_CLUSTER_DEFAULT_LOCAL_MINING 0
#include "../shared/logic/MinerClusterLogic.h"

namespace {
constexpr const char* HOSTNAME = "MicriDeck-Cluster";
constexpr const char* DISPLAY_PREF_NS = "distminer";
constexpr const char* DISPLAY_PREF_KEY = "portrait";
constexpr const char* ROLE_PREF_KEY = "role";
constexpr int16_t LANDSCAPE_WIDTH = 240;
constexpr int16_t LANDSCAPE_HEIGHT = 135;
constexpr int16_t PORTRAIT_WIDTH = 135;
constexpr int16_t PORTRAIT_HEIGHT = 240;
constexpr int16_t PORTRAIT_HEADER_HEIGHT = 28;
constexpr int16_t PORTRAIT_FOOTER_TOP = 221;

template <typename Drawer>
void drawBuffered(TFT_eSPI& tft, uint32_t width, uint32_t height, Drawer drawer) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawer);
}

template <typename Canvas>
void drawFit(Canvas& canvas, const String& text, int x, int y, int maxWidth, uint8_t size, uint16_t color) {
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(size);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(TDisplayUi::fitText(canvas, text, maxWidth), x, y);
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

template <typename Canvas>
void portraitHeader(Canvas& canvas, const String& title, uint16_t color, const String& status = "") {
  canvas.fillRect(0, 0, PORTRAIT_WIDTH, PORTRAIT_HEADER_HEIGHT, TFT_BLACK);
  canvas.drawFastHLine(0, PORTRAIT_HEADER_HEIGHT - 1, PORTRAIT_WIDTH, TFT_DARKGREY);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(TDisplayUi::fitText(canvas, title, PORTRAIT_WIDTH - 10), 5, 5);
  if (status.length() > 0) {
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString(TDisplayUi::fitText(canvas, status, PORTRAIT_WIDTH - 10), 5, 16);
  }
}

template <typename Canvas>
void portraitFooter(Canvas& canvas, const String& text) {
  canvas.fillRect(0, PORTRAIT_FOOTER_TOP, PORTRAIT_WIDTH, PORTRAIT_HEIGHT - PORTRAIT_FOOTER_TOP, TFT_BLACK);
  canvas.drawFastHLine(0, PORTRAIT_FOOTER_TOP, PORTRAIT_WIDTH, TFT_DARKGREY);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(TDisplayUi::fitText(canvas, text, PORTRAIT_WIDTH - 10), 5, PORTRAIT_FOOTER_TOP + 6);
}

template <typename Canvas>
void portraitCentered(Canvas& canvas, const String& text, int16_t y, uint8_t size, uint16_t color) {
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextFont(1);
  canvas.setTextColor(color, TFT_BLACK);
  const int16_t availableWidth = PORTRAIT_WIDTH - 8;
  uint8_t fittedSize = max<uint8_t>(1, size);
  canvas.setTextSize(fittedSize);
  while (fittedSize > 1 && canvas.textWidth(text) > availableWidth) {
    fittedSize--;
    canvas.setTextSize(fittedSize);
  }
  String fitted = text;
  while (fitted.length() > 0 && canvas.textWidth(fitted) > availableWidth) fitted.remove(fitted.length() - 1);
  canvas.drawString(fitted, max<int16_t>(4, (PORTRAIT_WIDTH - canvas.textWidth(fitted)) / 2), y);
}

template <typename Canvas>
void portraitLine(Canvas& canvas, const String& text, int16_t y, uint16_t color = TFT_WHITE) {
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(TDisplayUi::fitText(canvas, text, PORTRAIT_WIDTH - 10), 5, y);
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

bool DistributedMinerApp::startsRunningImmediately() const {
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
  loadDisplayPreference();
  if (b1.click) {
    selectedRole_ = selectedRole_ == Role::Master ? Role::Slave : Role::Master;
    markDirty();
  } else if (b2.click || b1.longPress) {
    startRunning();
  }
  return true;
}

void DistributedMinerApp::onAppReset() {
  loadDisplayPreference();
  stopAll();
  activeRole_ = selectedRole_;
  Serial.printf("[cluster-ui] restored role=%s layout=%s\n",
                activeRole_ == Role::Master ? "master" : "slave",
                portraitMode_ ? "portrait" : "landscape");
  page_ = 0;
  lastSerialStatsMs_ = 0;
  if (activeRole_ == Role::Slave) {
    slave_ = new MinerCluster::SlaveEngine();
    slave_->begin();
  }
  forceClear();
}

void DistributedMinerApp::onAppExit() {
  stopAll();
  displayPreferenceLoaded_ = false;
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

void DistributedMinerApp::loadDisplayPreference() {
  if (displayPreferenceLoaded_) return;
  Preferences prefs;
  prefs.begin(DISPLAY_PREF_NS, true);
  portraitMode_ = prefs.getBool(DISPLAY_PREF_KEY, false);
  selectedRole_ = prefs.getUChar(ROLE_PREF_KEY, static_cast<uint8_t>(Role::Master)) ==
                          static_cast<uint8_t>(Role::Slave)
                      ? Role::Slave
                      : Role::Master;
  prefs.end();
  displayPreferenceLoaded_ = true;
}

void DistributedMinerApp::saveDisplayPreference() {
  Preferences prefs;
  prefs.begin(DISPLAY_PREF_NS, false);
  prefs.putBool(DISPLAY_PREF_KEY, portraitMode_);
  prefs.end();
}

void DistributedMinerApp::saveRolePreference() {
  Preferences prefs;
  prefs.begin(DISPLAY_PREF_NS, false);
  prefs.putUChar(ROLE_PREF_KEY, static_cast<uint8_t>(selectedRole_));
  prefs.end();
}

void DistributedMinerApp::switchRole(Role role) {
  if (activeRole_ == role && selectedRole_ == role) return;
  stopAll();
  selectedRole_ = role;
  activeRole_ = role;
  saveRolePreference();
  Serial.printf("[cluster-ui] saved role=%s\n", activeRole_ == Role::Master ? "master" : "slave");
  page_ = 0;
  lastSerialStatsMs_ = 0;
  if (activeRole_ == Role::Slave) {
    slave_ = new MinerCluster::SlaveEngine();
    slave_->begin();
  }
  forceClear();
}

void DistributedMinerApp::toggleDisplayOrientation() {
  loadDisplayPreference();
  portraitMode_ = !portraitMode_;
  saveDisplayPreference();
  forceClear();
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
  } else if (activeRole_ == Role::Slave && slave_ != nullptr &&
             (lastSerialStatsMs_ == 0 || now - lastSerialStatsMs_ >= 2000)) {
    lastSerialStatsMs_ = now;
    printSlaveStatsLine(slave_->stats(), "tick");
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
    } else if (page == MasterPage::Role) {
      switchRole(Role::Slave);
    } else if (page == MasterPage::Display) {
      toggleDisplayOrientation();
    } else if (page == MasterPage::Reset) {
      cluster_->resetClusterIdentity();
      forceClear();
    }
    return;
  }

  if (activeRole_ == Role::Slave && b1.click) {
    const SlavePage page = static_cast<SlavePage>(page_);
    if (page == SlavePage::Role) {
      switchRole(Role::Master);
      return;
    }
  }

  if (activeRole_ == Role::Master && b1.longPress) {
    const MasterPage page = static_cast<MasterPage>(page_);
    if (page == MasterPage::Reset) {
      cluster_->resetClusterIdentity();
    } else if (page == MasterPage::Display) {
      toggleDisplayOrientation();
      return;
    } else if (page == MasterPage::Role) {
      switchRole(Role::Slave);
      return;
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
  Serial.print(" rx_drop=");
  Serial.print(stats.rxQueueDrops);
  Serial.print(" assign_retry=");
  Serial.print(stats.deliveryRetries);
  Serial.print(" heap=");
  Serial.print(stats.freeHeap);
  Serial.print(" stack=");
  Serial.print(stats.taskStackHighWater);
  if (stats.lastError[0]) {
    Serial.print(" error=");
    Serial.print(stats.lastError);
  }
  Serial.println();
}

void DistributedMinerApp::printSlaveStatsLine(const MinerCluster::SlaveStats& stats, const char* reason) {
  Serial.print("[cluster-slave] reason=");
  Serial.print(reason);
  Serial.print(" state=");
  Serial.print(MinerCluster::slaveStateLabel(stats.state));
  Serial.print(" raw_khps=");
  Serial.print(stats.hashrate / 1000.0f, 1);
  Serial.print(" effective_khps=");
  Serial.print(stats.effectiveHashrate / 1000.0f, 1);
  Serial.print(" assign=");
  Serial.print(stats.currentAssignmentId);
  Serial.print(" progress=");
  Serial.print(stats.assignmentDone);
  Serial.print('/');
  Serial.print(stats.assignmentSize);
  Serial.print(" rx_drop=");
  Serial.print(stats.rxQueueDrops);
  Serial.print(" result_retry=");
  Serial.print(stats.resultRetries);
  Serial.print(" idle_ms=");
  Serial.print(stats.idleMs);
  Serial.print(" heap=");
  Serial.print(stats.freeHeap);
  Serial.print(" stack=");
  Serial.print(stats.taskStackHighWater);
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
      case SlavePage::Role: return "Miner Role";
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
    case MasterPage::Role: return "Miner Role";
    case MasterPage::Display: return "Display Layout";
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
void DistributedMinerApp::drawDashboard(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  const uint16_t color = stateColor(stats.state);
  TDisplayUi::header(canvas, pageTitle(), color, MinerCluster::masterStateLabel(stats.state));
  TDisplayUi::largeValue(canvas, rateLabel(stats.totalHps), 39, color);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(String("Local ") + rateLabel(stats.localHps) + (stats.localMining ? " on" : " off"), 16, 84);
  canvas.drawString(String("Slaves ") + stats.slaveCount + "  " + rateLabel(stats.slaveHps), 16, 99);
  TDisplayUi::footer(canvas, cluster_->running() ? "B1 stop  B2 page  B1 hold pair" : "B1 start  B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawSlaves(Canvas& canvas) {
  TDisplayUi::header(canvas, pageTitle(), TFT_CYAN);
  MinerCluster::SlaveRecord slave;
  bool any = false;
  for (uint8_t row = 0; row < 4; row++) {
    if (!cluster_->slaveAt(row, slave)) break;
    any = true;
    const int y = 34 + row * 21;
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.drawString(MinerCluster::macSuffix(slave.mac), 12, y);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawString(rateLabel(slave.effectiveHashrate > 0 ? slave.effectiveHashrate : slave.hashrate), 73, y);
    drawFit(canvas, slave.status, 158, y, 70, 1, TFT_LIGHTGREY);
  }
  if (!any) {
    TDisplayUi::centered(canvas, "No slaves yet", 52, 2, TFT_LIGHTGREY);
    TDisplayUi::centered(canvas, "Use pair page", 78, 1, TFT_CYAN);
  }
  TDisplayUi::footer(canvas, "B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawPool(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  const MinerCluster::MinerConfig config = MinerCluster::loadMinerConfig();
  TDisplayUi::header(canvas, pageTitle(), stateColor(stats.state));
  drawFit(canvas, String("WiFi: ") + (stats.ssid[0] ? stats.ssid : "-"), 14, 34, 210, 1, TFT_WHITE);
  drawFit(canvas, String("Pool: ") + config.poolHost + ":" + config.poolPort, 14, 50, 210, 1, TFT_CYAN);
  drawFit(canvas, String("Jobs: ") + stats.jobs + "  Diff: " + String(stats.poolDifficulty, 5), 14, 66, 210, 1, TFT_LIGHTGREY);
  drawFit(canvas, String("OK/Rej: ") + stats.accepted + "/" + stats.rejected, 14, 82, 210, 1, TFT_GREEN);
  drawFit(canvas, String("Worker: ") + MinerCluster::workerName(), 14, 98, 210, 1, TFT_YELLOW);
  TDisplayUi::footer(canvas, "B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawPairing(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  const bool radioReady = stats.channel != 0;
  const uint16_t color = stats.pairing && radioReady ? TFT_GREEN : (stats.pairing ? TFT_YELLOW : TFT_ORANGE);
  TDisplayUi::header(canvas, pageTitle(), color, MinerCluster::masterStateLabel(stats.state));
  if (stats.pairing && radioReady) {
    TDisplayUi::centered(canvas, "Pairing Open", 39, 2, TFT_GREEN);
    TDisplayUi::centered(canvas, String("Channel ") + stats.channel, 66, 2, TFT_CYAN);
  } else if (stats.pairing) {
    TDisplayUi::centered(canvas, "Radio Waiting", 39, 2, TFT_YELLOW);
    TDisplayUi::centered(canvas, "Need WiFi first", 67, 1, TFT_LIGHTGREY);
  } else {
    TDisplayUi::centered(canvas, "Pairing Closed", 43, 2, TFT_ORANGE);
    TDisplayUi::centered(canvas, "B1 opens 60 sec", 70, 1, TFT_LIGHTGREY);
  }
  TDisplayUi::centered(canvas, String(stats.slaveCount) + " slaves", 96, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 pair  B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawControls(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  TDisplayUi::header(canvas, pageTitle(), TFT_YELLOW);
  TDisplayUi::row(canvas, 38, stats.localMining ? "Local Mining: On" : "Local Mining: Off", true, TFT_YELLOW);
  TDisplayUi::row(canvas, 65, cluster_->running() ? "Cluster: Running" : "Cluster: Stopped", false, TFT_CYAN);
  TDisplayUi::centered(canvas, "B1 toggle local", 96, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 local  B1 hold start/stop");
}

template <typename Canvas>
void DistributedMinerApp::drawRole(Canvas& canvas) {
  TDisplayUi::header(canvas, pageTitle(), TFT_MAGENTA);
  TDisplayUi::centered(canvas, activeRole_ == Role::Master ? "Master" : "Slave", 42, 3, TFT_WHITE);
  TDisplayUi::centered(canvas, activeRole_ == Role::Master ? "B1 switch to Slave" : "B1 switch to Master",
                       87, 1, TFT_CYAN);
  TDisplayUi::centered(canvas, "Choice is saved", 101, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 switch  B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawDisplay(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  TDisplayUi::header(canvas, pageTitle(), TFT_MAGENTA, MinerCluster::masterStateLabel(stats.state));
  TDisplayUi::centered(canvas, portraitMode_ ? "Portrait" : "Landscape", 47, 2, TFT_WHITE);
  TDisplayUi::centered(canvas, portraitMode_ ? "135 x 240" : "240 x 135", 82, 1, TFT_CYAN);
  TDisplayUi::centered(canvas, "Saved for this app", 98, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 toggle  B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawReset(Canvas& canvas, const MinerCluster::MasterStats& stats) {
  TDisplayUi::header(canvas, pageTitle(), TFT_RED, MinerCluster::masterStateLabel(stats.state));
  TDisplayUi::centered(canvas, "Forget slaves", 38, 2, TFT_ORANGE);
  TDisplayUi::centered(canvas, "New cluster id", 64, 2, TFT_CYAN);
  TDisplayUi::centered(canvas, stats.localMining ? "Local mining kept ON" : "Local mining kept OFF", 95, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 reset  B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveStatus(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  const uint16_t color = slaveStateColor(stats.state);
  TDisplayUi::header(canvas, pageTitle(), color, MinerCluster::slaveStateLabel(stats.state));
  TDisplayUi::largeValue(canvas, stats.paired ? "Linked" : "Seeking", 39, color);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(String("Channel ") + stats.channel, 16, 84);
  canvas.drawString(String("Master ") + (stats.paired ? MinerCluster::macSuffix(stats.masterMac) : String("---")), 16, 99);
  TDisplayUi::footer(canvas, "B2 page  B2 hold exit");
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveWork(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  const uint16_t color = slaveStateColor(stats.state);
  TDisplayUi::header(canvas, pageTitle(), color);
  TDisplayUi::largeValue(canvas, rateLabel(stats.hashrate), 39, color);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(String("Jobs ") + stats.jobs + "  Done " + stats.completed, 16, 84);
  canvas.drawString(String("Total ") + static_cast<unsigned long>(stats.totalHashes / 1000ULL) + " KH", 16, 99);
  TDisplayUi::footer(canvas, "B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveDebug(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  TDisplayUi::header(canvas, pageTitle(), TFT_YELLOW, MinerCluster::slaveStateLabel(stats.state));
  drawFit(canvas, String("Assign ") + stats.currentAssignmentId + "  Nonces " + stats.assignmentSize, 14, 36, 210, 1, TFT_CYAN);
  drawFit(canvas, String("Done ") + stats.assignmentDone + "  Result " + stats.lastResultHashes, 14, 54, 210, 1, TFT_WHITE);
  drawFit(canvas, String("Last result ") + stats.lastResultMs + " ms", 14, 72, 210, 1, TFT_LIGHTGREY);
  drawFit(canvas, String("Best diff ") + String(stats.bestDifficulty, 6), 14, 90, 210, 1, TFT_GREEN);
  TDisplayUi::footer(canvas, "B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveClear(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  TDisplayUi::header(canvas, pageTitle(), TFT_ORANGE, MinerCluster::slaveStateLabel(stats.state));
  TDisplayUi::centered(canvas, stats.paired ? "Saved Master" : "No Master", 38, 2, stats.paired ? TFT_CYAN : TFT_LIGHTGREY);
  TDisplayUi::centered(canvas, stats.paired ? MinerCluster::macSuffix(stats.masterMac) : "Seeking pair", 66, 2, TFT_WHITE);
  TDisplayUi::centered(canvas, "Hold B1 to forget", 96, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 hold clear  B2 page");
}

template <typename Canvas>
void DistributedMinerApp::drawSlaveExit(Canvas& canvas, const MinerCluster::SlaveStats& stats) {
  TDisplayUi::header(canvas, pageTitle(), TFT_RED, MinerCluster::slaveStateLabel(stats.state));
  TDisplayUi::centered(canvas, "Exit", 42, 3, TFT_WHITE);
  TDisplayUi::centered(canvas, "Hold B1 or B2", 82, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(canvas, "B1 hold exit  B2 hold exit");
}

template <typename Canvas>
void DistributedMinerApp::drawFrame(Canvas& canvas) {
  TDisplayUi::clear(canvas);
  if (activeRole_ == Role::Slave) {
    const MinerCluster::SlaveStats stats = slave_ != nullptr ? slave_->stats() : MinerCluster::SlaveStats();
    switch (static_cast<SlavePage>(page_)) {
      case SlavePage::Work:
        drawSlaveWork(canvas, stats);
        break;
      case SlavePage::Debug:
        drawSlaveDebug(canvas, stats);
        break;
      case SlavePage::Role:
        drawRole(canvas);
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
      case MasterPage::Role:
        drawRole(canvas);
        break;
      case MasterPage::Display:
        drawDisplay(canvas, stats);
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

template <typename Canvas>
void DistributedMinerApp::drawPortraitFrame(Canvas& canvas) {
  canvas.fillScreen(TFT_BLACK);
  if (activeRole_ == Role::Slave) {
    const MinerCluster::SlaveStats stats = slave_ != nullptr ? slave_->stats() : MinerCluster::SlaveStats();
    const uint16_t color = slaveStateColor(stats.state);
    switch (static_cast<SlavePage>(page_)) {
      case SlavePage::Work:
        portraitHeader(canvas, "Slave Work", color, MinerCluster::slaveStateLabel(stats.state));
        portraitCentered(canvas, rateLabel(stats.hashrate), 43, 2, color);
        portraitLine(canvas, String("Effective ") + rateLabel(stats.effectiveHashrate), 82, TFT_CYAN);
        portraitLine(canvas, String("Jobs ") + stats.jobs + "  Done " + stats.completed, 103);
        portraitLine(canvas, String("Assign ") + stats.currentAssignmentId, 124, TFT_LIGHTGREY);
        portraitLine(canvas, String(stats.assignmentDone) + "/" + stats.assignmentSize, 145, TFT_LIGHTGREY);
        portraitLine(canvas, String("Total ") + static_cast<unsigned long>(stats.totalHashes / 1000ULL) + " KH", 166, TFT_GREEN);
        portraitFooter(canvas, "B2 page");
        break;
      case SlavePage::Role:
        portraitHeader(canvas, "Miner Role", TFT_MAGENTA);
        portraitCentered(canvas, "Slave", 62, 3, TFT_WHITE);
        portraitCentered(canvas, "B1 switch to Master", 126, 1, TFT_CYAN);
        portraitCentered(canvas, "Choice is saved", 150, 1, TFT_LIGHTGREY);
        portraitFooter(canvas, "B1 switch  B2 page");
        break;
      case SlavePage::Debug:
        portraitHeader(canvas, "Slave Debug", TFT_YELLOW, MinerCluster::slaveStateLabel(stats.state));
        portraitLine(canvas, String("Raw ") + rateLabel(stats.hashrate), 38, TFT_WHITE);
        portraitLine(canvas, String("Effective ") + rateLabel(stats.effectiveHashrate), 58, TFT_CYAN);
        portraitLine(canvas, String("RX drops ") + stats.rxQueueDrops, 78, TFT_ORANGE);
        portraitLine(canvas, String("Result retry ") + stats.resultRetries, 98, TFT_ORANGE);
        portraitLine(canvas, String("Idle ") + stats.idleMs + " ms", 118, TFT_LIGHTGREY);
        portraitLine(canvas, String("Heap ") + stats.freeHeap, 138, TFT_LIGHTGREY);
        portraitLine(canvas, String("Stack ") + stats.taskStackHighWater, 158, TFT_LIGHTGREY);
        portraitLine(canvas, String("Best ") + String(stats.bestDifficulty, 5), 178, TFT_GREEN);
        portraitFooter(canvas, "B2 page");
        break;
      case SlavePage::Clear:
        portraitHeader(canvas, "Clear Pairing", TFT_ORANGE, MinerCluster::slaveStateLabel(stats.state));
        portraitCentered(canvas, stats.paired ? "Saved Master" : "No Master", 54, 2,
                         stats.paired ? TFT_CYAN : TFT_LIGHTGREY);
        portraitCentered(canvas, stats.paired ? MinerCluster::macSuffix(stats.masterMac) : "Seeking", 92, 2, TFT_WHITE);
        portraitCentered(canvas, "Hold B1 to forget", 143, 1, TFT_LIGHTGREY);
        portraitFooter(canvas, "B1 hold clear");
        break;
      case SlavePage::Exit:
        portraitHeader(canvas, "Exit Slave", TFT_RED, MinerCluster::slaveStateLabel(stats.state));
        portraitCentered(canvas, "Exit", 70, 3, TFT_WHITE);
        portraitCentered(canvas, "Hold B1 or B2", 126, 1, TFT_LIGHTGREY);
        portraitFooter(canvas, "Hold to exit");
        break;
      case SlavePage::Status:
      default:
        portraitHeader(canvas, "Miner Slave", color, MinerCluster::slaveStateLabel(stats.state));
        portraitCentered(canvas, stats.paired ? "Linked" : "Seeking", 54, 3, color);
        portraitLine(canvas, String("Channel ") + stats.channel, 112, TFT_CYAN);
        portraitLine(canvas, String("Master ") + (stats.paired ? MinerCluster::macSuffix(stats.masterMac) : String("---")), 134);
        portraitLine(canvas, String("Uptime ") + stats.uptimeSeconds + " sec", 156, TFT_LIGHTGREY);
        portraitFooter(canvas, "B2 page  hold exit");
        break;
    }
    dirty_ = false;
    return;
  }

  const MinerCluster::MasterStats stats = cluster_->stats();
  const uint16_t color = stateColor(stats.state);
  switch (static_cast<MasterPage>(page_)) {
    case MasterPage::Slaves: {
      portraitHeader(canvas, "Cluster Slaves", TFT_CYAN, String(stats.slaveCount) + " active");
      MinerCluster::SlaveRecord slaveRecord;
      bool any = false;
      for (uint8_t row = 0; row < MinerCluster::MAX_SLAVES; ++row) {
        if (!cluster_->slaveAt(row, slaveRecord)) break;
        any = true;
        const int16_t y = 33 + row * 22;
        canvas.setTextDatum(TL_DATUM);
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_CYAN, TFT_BLACK);
        canvas.drawString(MinerCluster::macSuffix(slaveRecord.mac), 4, y);
        canvas.setTextColor(TFT_WHITE, TFT_BLACK);
        const uint32_t effective = slaveRecord.effectiveHashrate > 0 ? slaveRecord.effectiveHashrate : slaveRecord.hashrate;
        canvas.drawString(TDisplayUi::fitText(canvas, rateLabel(effective), 52), 44, y);
        canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        canvas.drawString(TDisplayUi::fitText(canvas, slaveRecord.status, 34), 99, y);
        canvas.drawFastHLine(4, y + 16, PORTRAIT_WIDTH - 8, TFT_DARKGREY);
      }
      if (!any) {
        portraitCentered(canvas, "No slaves yet", 72, 2, TFT_LIGHTGREY);
        portraitCentered(canvas, "Open Pairing", 111, 1, TFT_CYAN);
      }
      portraitFooter(canvas, "B2 next page");
      break;
    }
    case MasterPage::Pool: {
      const MinerCluster::MinerConfig config = MinerCluster::loadMinerConfig();
      portraitHeader(canvas, "Cluster Pool", color, MinerCluster::masterStateLabel(stats.state));
      portraitLine(canvas, String("WiFi ") + (stats.ssid[0] ? stats.ssid : "-"), 36);
      portraitLine(canvas, String("Pool ") + config.poolHost, 58, TFT_CYAN);
      portraitLine(canvas, String("Port ") + config.poolPort, 78, TFT_CYAN);
      portraitLine(canvas, String("Jobs ") + stats.jobs, 101, TFT_LIGHTGREY);
      portraitLine(canvas, String("OK / Rej ") + stats.accepted + " / " + stats.rejected, 123, TFT_GREEN);
      portraitLine(canvas, String("Diff ") + String(stats.poolDifficulty, 5), 145, TFT_LIGHTGREY);
      portraitLine(canvas, String("Worker ") + MinerCluster::workerName(), 167, TFT_YELLOW);
      portraitFooter(canvas, "B2 next page");
      break;
    }
    case MasterPage::Pairing:
      portraitHeader(canvas, "Pair Slaves", stats.pairing ? TFT_GREEN : TFT_ORANGE,
                     MinerCluster::masterStateLabel(stats.state));
      portraitCentered(canvas, stats.pairing ? "Pairing Open" : "Pairing Closed", 54, 2,
                       stats.pairing ? TFT_GREEN : TFT_ORANGE);
      portraitCentered(canvas, String("Channel ") + stats.channel, 94, 2, TFT_CYAN);
      portraitCentered(canvas, String(stats.slaveCount) + " slaves", 132, 2, TFT_WHITE);
      portraitCentered(canvas, stats.pairing ? "Open for 60 sec" : "B1 opens pairing", 176, 1, TFT_LIGHTGREY);
      portraitFooter(canvas, "B1 pair  B2 page");
      break;
    case MasterPage::Controls:
      portraitHeader(canvas, "Cluster Control", TFT_YELLOW, MinerCluster::masterStateLabel(stats.state));
      portraitCentered(canvas, stats.localMining ? "Local Mining ON" : "Local Mining OFF", 55, 2,
                       stats.localMining ? TFT_GREEN : TFT_ORANGE);
      portraitCentered(canvas, cluster_->running() ? "Cluster Running" : "Cluster Stopped", 96, 2,
                       cluster_->running() ? TFT_CYAN : TFT_LIGHTGREY);
      portraitLine(canvas, "B1 toggles local", 146, TFT_LIGHTGREY);
      portraitLine(canvas, "Hold B1 start/stop", 166, TFT_LIGHTGREY);
      portraitFooter(canvas, "B1 local  B2 page");
      break;
    case MasterPage::Role:
      portraitHeader(canvas, "Miner Role", TFT_MAGENTA);
      portraitCentered(canvas, "Master", 62, 3, TFT_WHITE);
      portraitCentered(canvas, "B1 switch to Slave", 126, 1, TFT_CYAN);
      portraitCentered(canvas, "Choice is saved", 150, 1, TFT_LIGHTGREY);
      portraitFooter(canvas, "B1 switch  B2 page");
      break;
    case MasterPage::Display:
      portraitHeader(canvas, "Display Layout", TFT_MAGENTA, MinerCluster::masterStateLabel(stats.state));
      portraitCentered(canvas, portraitMode_ ? "Portrait" : "Landscape", 62, 2, TFT_WHITE);
      portraitCentered(canvas, portraitMode_ ? "135 x 240" : "240 x 135", 102, 2, TFT_CYAN);
      portraitCentered(canvas, "Saved for this app", 148, 1, TFT_LIGHTGREY);
      portraitFooter(canvas, "B1 toggle  B2 page");
      break;
    case MasterPage::Reset:
      portraitHeader(canvas, "Reset Cluster", TFT_RED, MinerCluster::masterStateLabel(stats.state));
      portraitCentered(canvas, "Forget Slaves", 54, 2, TFT_ORANGE);
      portraitCentered(canvas, "New Cluster ID", 92, 2, TFT_CYAN);
      portraitCentered(canvas, stats.localMining ? "Local mining kept ON" : "Local mining kept OFF", 140, 1,
                       TFT_LIGHTGREY);
      portraitCentered(canvas, "B1 resets", 175, 1, TFT_RED);
      portraitFooter(canvas, "B1 reset  B2 page");
      break;
    case MasterPage::Dashboard:
    default: {
      portraitHeader(canvas, "Distributed Miner", color, MinerCluster::masterStateLabel(stats.state));
      const String totalRate = rateLabel(stats.totalHps);
      canvas.setTextDatum(TL_DATUM);
      canvas.setTextSize(2);
      if (canvas.textWidth(totalRate) > PORTRAIT_WIDTH - 10) canvas.setTextSize(1);
      canvas.setTextColor(color, TFT_BLACK);
      canvas.drawString(totalRate, max<int16_t>(5, (PORTRAIT_WIDTH - canvas.textWidth(totalRate)) / 2), 45);
      portraitLine(canvas, String("Slaves ") + stats.slaveCount, 86, TFT_CYAN);
      portraitLine(canvas, String("Slave ") + rateLabel(stats.slaveHps), 107);
      portraitLine(canvas, String("Local ") + (stats.localMining ? "ON " : "OFF ") + rateLabel(stats.localHps), 128,
                   stats.localMining ? TFT_GREEN : TFT_LIGHTGREY);
      portraitLine(canvas, String("Jobs ") + stats.jobs + "  OK " + stats.accepted, 150, TFT_LIGHTGREY);
      portraitLine(canvas, String("Retries ") + stats.deliveryRetries, 172, TFT_ORANGE);
      portraitFooter(canvas, cluster_->running() ? "B1 stop  B2 page" : "B1 start  B2 page");
      break;
    }
  }
  dirty_ = false;
}

template <typename Canvas>
void DistributedMinerApp::drawPortraitStart(Canvas& canvas) {
  canvas.fillScreen(TFT_BLACK);
  if (showStartPromptPage()) {
    portraitHeader(canvas, "Distributed Miner", TFT_ORANGE);
    portraitCentered(canvas, selectedRole_ == Role::Master ? "Master" : "Slave", 63, 3, TFT_WHITE);
    portraitCentered(canvas, "B1 changes role", 126, 1, TFT_LIGHTGREY);
    portraitCentered(canvas, "B2 starts", 147, 1, TFT_CYAN);
    portraitFooter(canvas, "B1 role  B2 start");
  } else if (showStartScorePage()) {
    portraitHeader(canvas, "Cluster Mining", TFT_YELLOW);
    portraitCentered(canvas, "Master", 55, 2, TFT_CYAN);
    portraitCentered(canvas, "assigns work", 81, 1, TFT_LIGHTGREY);
    portraitCentered(canvas, "Slave", 119, 2, TFT_GREEN);
    portraitCentered(canvas, "joins cluster", 145, 1, TFT_LIGHTGREY);
    portraitFooter(canvas, selectedRole_ == Role::Master ? "Selected: Master" : "Selected: Slave");
  } else {
    portraitHeader(canvas, "Distributed Miner", TFT_ORANGE);
    canvas.drawCircle(PORTRAIT_WIDTH / 2, 78, 26, TFT_ORANGE);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(4);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawString(selectedRole_ == Role::Master ? "M" : "S", PORTRAIT_WIDTH / 2 - 12, 58);
    for (int i = 0; i < 3; ++i) {
      const int16_t x = 27 + i * 31;
      canvas.drawRect(x, 130, 20, 28, TFT_CYAN);
      canvas.drawFastHLine(x + 4, 151, 12, TFT_GREEN);
    }
    portraitFooter(canvas, "B1 role  B2 start");
  }
}

void DistributedMinerApp::drawRunning(TFT_eSPI& tft) {
  loadDisplayPreference();
  tft.setRotation(portraitMode_ ? 0 : 1);
  if (forceScreenClear_) {
    forceScreenClear_ = false;
  }
  if (portraitMode_) {
    drawBuffered(tft, PORTRAIT_WIDTH, PORTRAIT_HEIGHT, [this](auto& canvas) { drawPortraitFrame(canvas); });
  } else {
    drawBuffered(tft, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT, [this](auto& canvas) { drawFrame(canvas); });
  }
}

void DistributedMinerApp::drawStart(TFT_eSPI& tft) {
  loadDisplayPreference();
  tft.setRotation(portraitMode_ ? 0 : 1);
  if (portraitMode_) {
    drawBuffered(tft, PORTRAIT_WIDTH, PORTRAIT_HEIGHT, [this](auto& canvas) { drawPortraitStart(canvas); });
  } else {
    drawBuffered(tft, LANDSCAPE_WIDTH, LANDSCAPE_HEIGHT, [this](auto& canvas) {
    TDisplayUi::clear(canvas);
    if (showStartPromptPage()) {
      TDisplayUi::header(canvas, "Distributed Miner", TFT_ORANGE);
      TDisplayUi::centered(canvas, selectedRole_ == Role::Master ? "Master" : "Slave", 46, 3, TFT_WHITE);
      TDisplayUi::centered(canvas, "B1 role  B2 start", 87, 1, TFT_LIGHTGREY);
      TDisplayUi::footer(canvas, "B1 role  B2 start");
    } else if (showStartScorePage()) {
      TDisplayUi::header(canvas, "Cluster Mining", TFT_YELLOW);
      TDisplayUi::centered(canvas, "Master assigns work", 47, 2, TFT_CYAN);
      TDisplayUi::centered(canvas, "Slave joins a cluster", 78, 1, TFT_LIGHTGREY);
      TDisplayUi::footer(canvas, selectedRole_ == Role::Master ? "Selected: Master" : "Selected: Slave");
    } else {
      TDisplayUi::header(canvas, "Distributed Miner", TFT_ORANGE);
      canvas.drawCircle(90, 66, 20, TFT_ORANGE);
      canvas.drawString(selectedRole_ == Role::Master ? "M" : "S", 84, 57, 4);
      for (int i = 0; i < 3; i++) {
        const int x = 140 + i * 22;
        canvas.drawRect(x, 52, 16, 22, TFT_CYAN);
        canvas.drawFastHLine(x + 3, 69, 10, TFT_GREEN);
      }
      TDisplayUi::footer(canvas, "B1 role  B2 start");
    }
    });
  }
  lastStartIntroPage_ = startIntroPage();
  startIntroPageRendered_ = true;
  dirty_ = false;
}
