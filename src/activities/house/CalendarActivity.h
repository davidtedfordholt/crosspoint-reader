#pragma once
#include <I18n.h>

#include <string>
#include <vector>

#include "HouseCalendarStore.h"
#include "activities/UiListActivity.h"

/**
 * Read-only house calendar (X4 Pro, plan tasks C1/C2). Renders the SD cache
 * day-grouped over the next 30 days; "Refresh" row = the one explicit Wi-Fi
 * moment (same pattern as ChecklistActivity). When Wi-Fi happens to be up on
 * entry, a cache older than an hour refetches by itself (plan's hourly cap).
 *
 * Recurring events appear once at their first occurrence; TZID times are
 * treated as device-local wall time — documented plan ceilings.
 */
class CalendarActivity final : public UiListActivity {
 public:
  explicit CalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  int listCount() const override { return static_cast<int>(visible.size()) + 1; }  // + refresh row
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return tr(STR_HOUSE_CALENDAR); }

  void reloadFromCache();
  void rebuildRows();
  void beginRefresh();
  void runRefresh();

  // Upcoming window: today's still-running events, next 30 days, capped rows.
  static constexpr int64_t WINDOW_DAYS = 30;
  static constexpr size_t MAX_ROWS = 60;
  // Plan C1: refetch at most hourly.
  static constexpr int64_t REFETCH_INTERVAL = 60 * 60;

  std::vector<HouseCalendarItem> visible;
  // Row/string buffers aliasing activity-owned storage across repaints.
  std::vector<freeink::ui::ListItem> rowItems;
  std::vector<std::string> dayLabels;
  std::vector<std::string> startLabels;
  char refreshLabel[96] = "";

  enum class State { IDLE, SYNCING } state = State::IDLE;
  const char* statusText = nullptr;
  bool wifiSessionOpen = false;
};
