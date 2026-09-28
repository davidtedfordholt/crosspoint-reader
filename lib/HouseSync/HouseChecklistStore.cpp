#include "HouseChecklistStore.h"

namespace {
// Items are read as const char* per the PersistableStore flash-bloat note.
std::vector<HouseChecklistItem> parseItems(JsonVariantConst arr) {
  std::vector<HouseChecklistItem> out;
  for (JsonVariantConst it : arr.as<JsonArrayConst>()) {
    HouseChecklistItem item;
    item.id = it["id"] | "";
    if (item.id.empty()) continue;
    item.done = it["done"] | false;
    // Server items may name the text field "label"; accept common alternates.
    const char* label = it["label"] | (it["text"] | (it["title"] | ""));
    item.label = label;
    out.push_back(std::move(item));
  }
  return out;
}
}  // namespace

void HouseChecklistStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["items"].to<JsonArray>();
  for (const auto& it : items) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = it.id;
    o["label"] = it.label;
    o["done"] = it.done;
  }
  JsonArray pend = doc["pendingToggles"].to<JsonArray>();
  for (const auto& it : pendingToggles) {
    JsonObject o = pend.add<JsonObject>();
    o["id"] = it.id;
    o["label"] = it.label;
    o["done"] = it.done;
  }
  doc["lastFetch"] = lastFetch;
}

bool HouseChecklistStore::fromJson(JsonVariantConst doc) {
  items = parseItems(doc["items"]);
  pendingToggles = parseItems(doc["pendingToggles"]);
  lastFetch = doc["lastFetch"] | static_cast<int64_t>(0);
  return true;
}

void HouseChecklistStore::replaceItems(std::vector<HouseChecklistItem> fresh) {
  items = std::move(fresh);
  // Re-apply queued offline toggles so a refetch can't eat them.
  for (const auto& pend : pendingToggles) {
    for (auto& it : items) {
      if (it.id == pend.id) it.done = pend.done;
    }
  }
  saveToFile();
}

bool HouseChecklistStore::toggleItem(const std::string& id) {
  for (auto& it : items) {
    if (it.id != id) continue;
    it.done = !it.done;
    // Last-writer-wins per item: one pending entry per id, replaced on re-toggle.
    bool queued = false;
    for (auto& pend : pendingToggles) {
      if (pend.id == id) {
        pend.done = it.done;
        queued = true;
        break;
      }
    }
    if (!queued) pendingToggles.push_back({id, it.label, it.done});
    saveToFile();
    return it.done;
  }
  return false;
}

void HouseChecklistStore::clearPending(const std::string& id) {
  for (auto it = pendingToggles.begin(); it != pendingToggles.end(); ++it) {
    if (it->id == id) {
      pendingToggles.erase(it);
      saveToFile();
      return;
    }
  }
}
