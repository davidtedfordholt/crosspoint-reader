#pragma once
#include <I18n.h>

#include <string>
#include <vector>

#include "HouseChecklistStore.h"
#include "activities/UiListActivity.h"

/**
 * Two-way offline-first house checklist (X4 Pro, plan tasks B1/B2).
 *
 * Cache-first: rows render instantly from the SD cache; sync happens only
 * when the user activates the "Sync now" row (last row) — Wi-Fi comes up for
 * that explicit moment (WifiSelectionActivity on demand) and is torn down
 * with the stock silentRestart when the screen closes afterwards.
 *
 * Refresh policy (plan contract): FULL refresh on entry and on exit, FAST
 * (partial) per toggle, forced FULL after 8 consecutive partials. No
 * animations, nothing background.
 *
 * Rows are toggle-rows: tap or Confirm flips done state locally, queues the
 * toggle in the SD store (last-writer-wins per item), and posts immediately
 * when Wi-Fi happens to be up. Device-side add/remove of items waits on the
 * server's missing add/delete endpoints (routed; see plan Phase B).
 */
class ChecklistActivity final : public UiListActivity {
 public:
  explicit ChecklistActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  // The sync affordance rides as the last row, so it works on touch and
  // button-only navigation alike.
  int listCount() const override { return static_cast<int>(items.size()) + 1; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return tr(STR_HOUSE_CHECKLIST); }

  void rebuildRowItems();
  void reloadFromCache();
  void toggleItem(int index);
  void beginSync();
  void runSync();  // blocking fetch + queue flush; runs from loop() while State::SYNCING

  std::vector<HouseChecklistItem> items;
  // Row buffers aliasing items' strings (RecentBooksActivity pattern): built
  // on data change, reused on every repaint.
  std::vector<freeink::ui::ListItem> rowItems;
  // Sync-row label buffer (labels alias it across renders).
  char syncLabel[96] = "";

  enum class State { IDLE, SYNCING } state = State::IDLE;
  const char* statusText = nullptr;  // sticky result line shown on the sync row
  int partialsSinceFull = 0;
  bool wifiSessionOpen = false;
};
