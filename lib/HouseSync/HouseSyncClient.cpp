#include "HouseSyncClient.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <SecureHttpClient.h>

#include "HouseSettings.h"
#include "HouseCalendarStore.h"

using freeink::SecureHttpClient;

int HouseSyncClient::lastHttpCode = 0;

namespace {
// POST body is form-encoded per the kaw endpoint contract; done values are
// JSON-style lowercase bools.
std::string formBody(const std::string& done) {
  std::string body = "device=";
  body += HOUSE_SETTINGS.getDevice();
  body += "&token=";
  body += HOUSE_SETTINGS.getToken();
  body += "&done=";
  body += done;
  return body;
}

std::string itemPostUrl(const std::string& id) {
  std::string url = HOUSE_SETTINGS.getServerUrl();
  while (!url.empty() && url.back() == '/') url.pop_back();
  url += "/api/house/checklist/";
  url += HOUSE_SETTINGS.getChecklistName();
  url += "/item/";
  url += id;
  return url;
}
}  // namespace

bool HouseSyncClient::fetchChecklist(std::string& err) {
  if (!HOUSE_SETTINGS.hasServerConfig()) {
    err = "house server not configured";
    return false;
  }

  SecureHttpClient http;
  if (!http.begin(HOUSE_SETTINGS.getChecklistUrl())) {
    err = "bad checklist url";
    return false;
  }
  http.addHeader("Accept", "application/json");
  lastHttpCode = http.GET();
  if (lastHttpCode != 200) {
    err = "http " + std::to_string(lastHttpCode);
    http.end();
    return false;
  }
  const std::string body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    err = "bad json from server";
    return false;
  }
  std::vector<HouseChecklistItem> fresh;
  for (JsonVariantConst it : doc["items"].as<JsonArrayConst>()) {
    HouseChecklistItem item;
    item.id = it["id"] | "";
    if (item.id.empty()) continue;
    item.done = it["done"] | false;
    const char* label = it["label"] | (it["text"] | (it["title"] | ""));
    item.label = label;
    fresh.push_back(std::move(item));
  }
  HOUSE_CHECKLIST.replaceItems(std::move(fresh));
  HOUSE_CHECKLIST.markFetched(static_cast<int64_t>(time(nullptr)));
  LOG_INF("HOUSE", "checklist fetched: %zu items", HOUSE_CHECKLIST.getItems().size());
  return true;
}

bool HouseSyncClient::postToggle(const std::string& id, bool done, std::string& err) {
  if (!HOUSE_SETTINGS.hasServerConfig()) {
    err = "house server not configured";
    return false;
  }

  SecureHttpClient http;
  if (!http.begin(itemPostUrl(id))) {
    err = "bad toggle url";
    return false;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  lastHttpCode = http.POST(formBody(done ? "true" : "false"));
  const bool ok = lastHttpCode == 200;
  if (!ok) err = "http " + std::to_string(lastHttpCode);
  http.end();
  return ok;
}

bool HouseSyncClient::fetchCalendar(std::string& err) {
  if (HOUSE_SETTINGS.getIcsUrl().empty()) {
    err = "calendar url not configured";
    return false;
  }

  SecureHttpClient http;
  if (!http.begin(HOUSE_SETTINGS.getIcsUrl())) {
    err = "bad calendar url";
    return false;
  }
  lastHttpCode = http.GET();
  if (lastHttpCode != 200) {
    err = "http " + std::to_string(lastHttpCode);
    http.end();
    return false;
  }
  const std::string body = http.getString();
  http.end();

  std::vector<HouseCalendarItem> events;
  if (!HouseCalendarStore::parseIcs(body, events, err)) return false;
  HOUSE_CALENDAR.replaceEvents(std::move(events));
  HOUSE_CALENDAR.markFetched(static_cast<int64_t>(time(nullptr)));
  LOG_INF("HOUSE", "calendar fetched: %zu events", HOUSE_CALENDAR.getEvents().size());
  return true;
}

bool HouseSyncClient::flushPending(std::string& err) {
  for (const auto& pend : HOUSE_CHECKLIST.getPendingToggles()) {
    if (!postToggle(pend.id, pend.done, err)) {
      LOG_ERR("HOUSE", "flush stopped on item %s: %s", pend.id.c_str(), err.c_str());
      return false;
    }
    HOUSE_CHECKLIST.clearPending(pend.id);
  }
  return true;
}
