#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

/**
 * House server connection settings for the X4 Pro (checklist + calendar).
 *
 * Lives in its own PersistableStore file (/.crosspoint/house.json), following
 * the KOReader credential store precedent for server credentials — keeps the
 * fork diff out of upstream's frequently-churning settings struct so rebases
 * stay cheap.
 *
 * Auth mirrors the kaw house server: every call carries the device name and
 * its per-device token (query params on GET, form fields on POST).
 */
class HouseSettings : public PersistableStore<HouseSettings> {
 private:
  std::string serverUrl;                 // e.g. "http://10.0.0.5:8337" (no trailing slash)
  std::string device;                    // device name registered on the house server
  std::string token;                     // per-device token
  std::string checklistName = "pocket";  // named list on the server
  std::string icsUrl;                    // calendar feed (Phase C)

  // Private constructor for singleton
  HouseSettings() = default;
  ~HouseSettings() = default;

  friend class PersistableStore<HouseSettings>;

 public:
  static const char* getFilePath() { return "/.crosspoint/house.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // True when server + device + token are all set (client refuses to run otherwise)
  bool hasServerConfig() const {
    return !serverUrl.empty() && !device.empty() && !token.empty() && !checklistName.empty();
  }

  void setServerUrl(const std::string& url) { serverUrl = url; saveToFile(); }
  void setDevice(const std::string& d) { device = d; saveToFile(); }
  void setToken(const std::string& t) { token = t; saveToFile(); }
  void setChecklistName(const std::string& n) { checklistName = n; saveToFile(); }
  void setIcsUrl(const std::string& u) { icsUrl = u; saveToFile(); }

  const std::string& getServerUrl() const { return serverUrl; }
  const std::string& getDevice() const { return device; }
  const std::string& getToken() const { return token; }
  const std::string& getChecklistName() const { return checklistName; }
  const std::string& getIcsUrl() const { return icsUrl; }

  // Full GET URL for the checklist endpoint, with auth in the query string
  std::string getChecklistUrl() const;
};

#define HOUSE_SETTINGS HouseSettings::getInstance()
