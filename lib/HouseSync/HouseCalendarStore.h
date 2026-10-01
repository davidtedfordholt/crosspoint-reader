#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <string>
#include <vector>

struct HouseCalendarItem {
  std::string uid;
  std::string summary;
  std::string location;
  int64_t start = 0;  // epoch seconds UTC instant; display applies device TZ
  int64_t end = 0;    // epoch seconds; 0 = not given
  bool allDay = false;
};

/**
 * Read-only ICS calendar cache (X4 Pro, plan tasks C1/C2). Written by
 * HouseSyncClient::fetchCalendar(), rendered by CalendarActivity. Naive
 * parser per the plan: VEVENT/DTSTART/DTEND/SUMMARY/LOCATION/UID only;
 * recurring events appear once at their first occurrence; TZID events are
 * treated as device-local wall time (UTC→local only is the documented
 * ceiling).
 */
class HouseCalendarStore : public PersistableStore<HouseCalendarStore> {
 private:
  std::vector<HouseCalendarItem> events;  // sorted by start ascending
  int64_t lastFetch = 0;

  HouseCalendarStore() = default;
  ~HouseCalendarStore() = default;
  friend class PersistableStore<HouseCalendarStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/house/calendar.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // Naive ICS text -> events (sorted by start). Best-effort: malformed
  // events are skipped, err carries the reason on total failure.
  static bool parseIcs(const std::string& body, std::vector<HouseCalendarItem>& out, std::string& err);

  const std::vector<HouseCalendarItem>& getEvents() const { return events; }
  void replaceEvents(std::vector<HouseCalendarItem> fresh) {
    events = std::move(fresh);
    saveToFile();
  }
  void markFetched(int64_t t) {
    lastFetch = t;
    saveToFile();
  }
  int64_t getLastFetch() const { return lastFetch; }
};

#define HOUSE_CALENDAR HouseCalendarStore::getInstance()
