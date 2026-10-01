#include "HouseCalendarStore.h"

#include <Logging.h>

#include <algorithm>

// Same persistable-store JSON conventions as HouseChecklistStore: read
// strings as const char* to avoid ArduinoJson flash bloat.
void HouseCalendarStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["events"].to<JsonArray>();
  for (const auto& ev : events) {
    JsonObject o = arr.add<JsonObject>();
    o["uid"] = ev.uid;
    o["summary"] = ev.summary;
    if (!ev.location.empty()) o["location"] = ev.location;
    o["start"] = ev.start;
    o["end"] = ev.end;
    o["allDay"] = ev.allDay;
  }
  doc["lastFetch"] = lastFetch;
}

bool HouseCalendarStore::fromJson(JsonVariantConst doc) {
  events.clear();
  for (JsonVariantConst it : doc["events"].as<JsonArrayConst>()) {
    HouseCalendarItem ev;
    ev.uid = it["uid"] | "";
    ev.summary = it["summary"] | "";
    ev.location = it["location"] | "";
    ev.start = it["start"] | (int64_t)0;
    ev.end = it["end"] | (int64_t)0;
    ev.allDay = it["allDay"] | false;
    if (ev.summary.empty() && ev.start == 0) continue;
    events.push_back(std::move(ev));
  }
  std::sort(events.begin(), events.end(),
            [](const HouseCalendarItem& a, const HouseCalendarItem& b) { return a.start < b.start; });
  lastFetch = doc["lastFetch"] | (int64_t)0;
  return true;
}

bool HouseCalendarStore::parseIcs(const std::string& body, std::vector<HouseCalendarItem>& out, std::string& err) {
  return house_ics::parse(body, out, err);
}

