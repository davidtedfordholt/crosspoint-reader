#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <string>
#include <vector>

/**
 * Offline-first SD cache for the house checklist (X4 Pro, plan task B1/B2).
 *
 * Cached at /.crosspoint/house/checklist.json — the device renders from here
 * whenever Wi-Fi is down. Queued toggles ride along in the same file so an
 * offline check-off survives a reboot; the queue drains on the next sync
 * (last-writer-wins per item — the whole merge strategy, see plan).
 */
struct HouseChecklistItem {
  std::string id;
  std::string label;
  bool done = false;
};

class HouseChecklistStore : public PersistableStore<HouseChecklistStore> {
 private:
  std::vector<HouseChecklistItem> items;
  // Toggles made offline, not yet acknowledged by the server. One entry per
  // item id; a second toggle of the same item replaces its entry.
  std::vector<HouseChecklistItem> pendingToggles;
  int64_t lastFetch = 0;  // unix seconds of last successful server fetch, 0 = never

  // Private constructor for singleton
  HouseChecklistStore() = default;
  ~HouseChecklistStore() = default;

  friend class PersistableStore<HouseChecklistStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/house/checklist.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  const std::vector<HouseChecklistItem>& getItems() const { return items; }
  const std::vector<HouseChecklistItem>& getPendingToggles() const { return pendingToggles; }
  int64_t getLastFetch() const { return lastFetch; }
  void markFetched(int64_t unixSeconds) { lastFetch = unixSeconds; saveToFile(); }

  // Replace the cached list with a fresh server snapshot, then re-apply any
  // pending offline toggles so a queued check-off isn't lost to a refetch.
  void replaceItems(std::vector<HouseChecklistItem> fresh);

  // Apply a toggle locally and (if offline / unacked) queue it for the server.
  // Returns the new done state of the item. No-ops on unknown id.
  bool toggleItem(const std::string& id);

  // Drop one pending entry once the server has acknowledged it.
  void clearPending(const std::string& id);
};

#define HOUSE_CHECKLIST HouseChecklistStore::getInstance()
