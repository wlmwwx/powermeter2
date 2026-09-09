#pragma once
#include <Arduino.h>

namespace es {

struct Snapshot {
  double ep1_kwh = 0;
  double eq1_kvarh = 0;
  double ep2_kwh = 0;
  double eq2_kvarh = 0;
  uint32_t saved_at_ms = 0;
};

class EnergyStore {
 public:
  // Mount filesystem (call once in setup). Returns false on failure.
  static bool begin();

  static bool save(const Snapshot& s);
  static bool load(Snapshot* out);
  static void clear();
};

}  // namespace es
