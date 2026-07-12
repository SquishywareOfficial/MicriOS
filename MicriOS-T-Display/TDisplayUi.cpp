#include "TDisplayUi.h"

#include <Preferences.h>

namespace {
constexpr const char* UI_PREF_NS = "tdui";
constexpr const char* TEXT_SIZE_KEY = "text";
Preferences uiPrefs;
}

namespace TDisplayUi {

TextSize loadTextSize() {
  uiPrefs.begin(UI_PREF_NS, true);
  const uint8_t raw = uiPrefs.getUChar(TEXT_SIZE_KEY, static_cast<uint8_t>(TextSize::Compact));
  uiPrefs.end();
  return raw == static_cast<uint8_t>(TextSize::Large) ? TextSize::Large : TextSize::Compact;
}

void saveTextSize(TextSize size) {
  uiPrefs.begin(UI_PREF_NS, false);
  uiPrefs.putUChar(TEXT_SIZE_KEY, static_cast<uint8_t>(size));
  uiPrefs.end();
}

TextSize nextTextSize(TextSize size) {
  return size == TextSize::Compact ? TextSize::Large : TextSize::Compact;
}

const char* textSizeLabel(TextSize size) {
  return size == TextSize::Large ? "Large" : "Compact";
}

}
