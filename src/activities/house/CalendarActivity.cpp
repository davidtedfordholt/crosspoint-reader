#include "CalendarActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <memory>

#include "HouseSettings.h"
#include "HouseSyncClient.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

CalendarActivity::CalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("HouseCalendar", renderer, mappedInput, /*wantsTouchLongPress=*/false) {}

void CalendarActivity::onEnter() {
  UiListActivity::onEnter();
  reloadFromCache();
  // Entry boundary: clear whatever ghosting the previous screen left.
  renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
  // Opportunistic hourly refetch only when the radio is already up — never
  // blocks entry, never brings Wi-Fi up on its own.
  const int64_t now = static_cast<int64_t>(time(nullptr));
  if (WiFi.status() == WL_CONNECTED && !HOUSE_SETTINGS.getIcsUrl().empty() &&
      now - HOUSE_CALENDAR.getLastFetch() >= REFETCH_INTERVAL) {
    state = State::SYNCING;
    requestUpdate();
  }
}

void CalendarActivity::onExit() {
  Activity::onExit();
  // Exit boundary: hand the full-refresh wave to the next screen's repaint.
  renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
  rowItems.clear();
  visible.clear();
  dayLabels.clear();
  startLabels.clear();

  if (wifiSessionOpen && WiFi.getMode() != WIFI_MODE_NULL) {
    // Stock on-demand-Wi-Fi teardown (ClockSyncActivity pattern).
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void CalendarActivity::reloadFromCache() {
  const auto& all = HOUSE_CALENDAR.getEvents();
  const int64_t now = static_cast<int64_t>(time(nullptr));
  const int64_t horizon = now + WINDOW_DAYS * 86400;
  visible.clear();
  for (const auto& ev : all) {
    const int64_t finish = ev.end > 0 ? ev.end : (ev.allDay ? ev.start + 86400 : ev.start + 60 * 60);
    if (finish < now - 12 * 3600) continue;  // let today's just-ended events linger
    if (ev.start > horizon) break;           // sorted by start
    visible.push_back(ev);
    if (visible.size() >= MAX_ROWS) break;
  }
  rebuildRows();
}

void CalendarActivity::rebuildRows() {
  rowItems.clear();
  dayLabels.clear();
  startLabels.clear();
  rowItems.reserve(visible.size() + 1);
  dayLabels.reserve(visible.size());
  startLabels.reserve(visible.size());

  int lastY = -1, lastM = -1, lastD = -1;
  for (const auto& ev : visible) {
    fui::ListItem item;
    item.label = ev.summary.c_str();
    if (!ev.location.empty()) item.subtitle = ev.location.c_str();

    const time_t start = static_cast<time_t>(ev.start);
    std::tm local{};
    localtime_r(&start, &local);
    char buf[48];
    // Day group heading when the local day changes.
    if (local.tm_year != lastY || local.tm_mon != lastM || local.tm_mday != lastD) {
      lastY = local.tm_year;
      lastM = local.tm_mon;
      lastD = local.tm_mday;
      std::strftime(buf, sizeof(buf), "%a %d %b", &local);
      dayLabels.push_back(buf);
      item.sectionHeading = dayLabels.back().c_str();
    }
    if (ev.allDay) {
      startLabels.emplace_back(tr(STR_HOUSE_ALL_DAY));
    } else {
      std::strftime(buf, sizeof(buf), "%H:%M", &local);
      startLabels.push_back(buf);
    }
    item.value = startLabels.back().c_str();
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }

  fui::ListItem refresh;
  refresh.label = refreshLabel;
  refresh.actionValue = static_cast<int16_t>(visible.size());
  rowItems.push_back(refresh);
}

void CalendarActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (state == State::SYNCING) {
    snprintf(refreshLabel, sizeof(refreshLabel), "%s", tr(STR_HOUSE_SYNCING));
  } else if (statusText != nullptr) {
    snprintf(refreshLabel, sizeof(refreshLabel), "%s", statusText);
  } else if (HOUSE_SETTINGS.getIcsUrl().empty()) {
    snprintf(refreshLabel, sizeof(refreshLabel), "%s", tr(STR_HOUSE_NOT_CONFIGURED));
  } else {
    snprintf(refreshLabel, sizeof(refreshLabel), "%s", tr(STR_HOUSE_SYNC));
  }

  if (visible.empty()) {
    screen.centeredText(tr(STR_HOUSE_NO_EVENTS), screen.theme().bodyText);
    screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  }

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // read-only: rows don't activate
  syncListViewport(screen, props, /*hasSubtitle=*/true);
  screen.list(props);
}

void CalendarActivity::activateIndex(const int index) {
  if (index == static_cast<int>(visible.size())) {
    app.clearTapFlash();
    beginRefresh();
  }
}

void CalendarActivity::beginRefresh() {
  if (HOUSE_SETTINGS.getIcsUrl().empty()) {
    statusText = tr(STR_HOUSE_NOT_CONFIGURED);
    requestUpdate();
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    state = State::SYNCING;
    requestUpdate();
    return;
  }
  // Explicit refresh moment: bring the radio up on demand (stock pattern).
  wifiSessionOpen = true;
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           state = State::SYNCING;
                           requestUpdate();
                         });
}

void CalendarActivity::runRefresh() {
  std::string err;
  statusText = HouseSyncClient::fetchCalendar(err) ? tr(STR_HOUSE_SYNCED) : tr(STR_HOUSE_SYNC_FAILED);
  reloadFromCache();
  state = State::IDLE;
  requestUpdate();
}

void CalendarActivity::loop() {
  if (state == State::SYNCING) {
    // First tick: push the "Syncing…" frame, then block on the network work.
    requestUpdateAndWait();
    runRefresh();
    return;
  }
  UiListActivity::loop();
}
