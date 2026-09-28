#include "HouseSettings.h"

#include <cctype>

// Same persistable-store JSON conventions as upstream stores: read strings as
// const char* (see PersistableStore.h note on ArduinoJson serializer bloat).
void HouseSettings::toJson(JsonDocument& doc) const {
  doc["serverUrl"] = serverUrl;
  doc["device"] = device;
  doc["token"] = token;
  doc["checklistName"] = checklistName;
  doc["icsUrl"] = icsUrl;
}

bool HouseSettings::fromJson(JsonVariantConst doc) {
  serverUrl = doc["serverUrl"] | "";
  device = doc["device"] | "";
  token = doc["token"] | "";
  checklistName = doc["checklistName"] | "pocket";
  icsUrl = doc["icsUrl"] | "";
  return true;
}

namespace {
// Percent-encode everything outside the RFC 3986 unreserved set, so tokens
// with '&', '=', '+' or spaces survive the query string.
std::string urlEncode(const std::string& value) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size());
  for (const char c : value) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}
}  // namespace

std::string HouseSettings::getChecklistUrl() const {
  std::string url = serverUrl;
  while (!url.empty() && url.back() == '/') url.pop_back();
  url += "/api/house/checklist/";
  url += checklistName;
  url += "?device=";
  url += urlEncode(device);
  url += "&token=";
  url += urlEncode(token);
  return url;
}
