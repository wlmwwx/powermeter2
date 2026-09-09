#include "EnergyStore.h"
#include <LittleFS.h>

namespace es {

static const char* PATH = "/energy.dat";

bool EnergyStore::begin() {
  return LittleFS.begin(true);  // format on fail
}

bool EnergyStore::save(const Snapshot& s) {
  File f = LittleFS.open(PATH, "w");
  if (!f) return false;
  size_t w = f.write((const uint8_t*)&s, sizeof(s));
  f.close();
  return w == sizeof(s);
}

bool EnergyStore::load(Snapshot* out) {
  File f = LittleFS.open(PATH, "r");
  if (!f) return false;
  size_t r = f.read((uint8_t*)out, sizeof(*out));
  f.close();
  return r == sizeof(*out);
}

void EnergyStore::clear() {
  LittleFS.remove(PATH);
}

}  // namespace es
