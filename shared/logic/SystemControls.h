#pragma once

#include <stdint.h>

namespace SystemControls {

enum class Control : uint8_t {
  None,
  Brightness,
  Volume,
  Battery
};

struct State {
  bool implemented = false;
  bool adjustable = false;
  int16_t percent = -1;
};

class Provider {
 public:
  virtual ~Provider() = default;

  virtual State state(Control) const { return {}; }
  virtual void preview(Control, uint8_t) {}
  virtual void commit(Control, uint8_t) {}
};

constexpr uint32_t OVERLAY_TIMEOUT_MS = 5000;
constexpr uint32_t UNIMPLEMENTED_TIMEOUT_MS = 2500;

inline uint8_t clampPercent(int16_t percent) {
  if (percent < 0) return 0;
  if (percent > 100) return 100;
  return static_cast<uint8_t>(percent);
}

inline uint8_t percentFromTrack(int16_t coordinate, int16_t start,
                                int16_t length) {
  if (length <= 1) return 0;
  const int32_t numerator = static_cast<int32_t>(coordinate - start) * 100;
  return clampPercent(static_cast<int16_t>(numerator / (length - 1)));
}

inline uint8_t levelFromPercent(uint8_t percent, uint8_t minimum,
                                uint8_t maximum) {
  if (maximum <= minimum) return minimum;
  return static_cast<uint8_t>(minimum +
      (static_cast<uint16_t>(clampPercent(percent)) * (maximum - minimum) + 50) /
          100);
}

inline uint8_t percentFromLevel(uint8_t level, uint8_t minimum,
                                uint8_t maximum) {
  if (maximum <= minimum || level <= minimum) return 0;
  if (level >= maximum) return 100;
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(level - minimum) * 100 +
       (maximum - minimum) / 2) /
      (maximum - minimum));
}

inline const char* label(Control control) {
  switch (control) {
    case Control::Brightness: return "Brightness";
    case Control::Volume: return "Volume";
    case Control::Battery: return "Battery";
    default: return "";
  }
}

}  // namespace SystemControls
