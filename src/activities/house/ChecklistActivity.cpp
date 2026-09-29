#include "ChecklistActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <cstdio>
#include <memory>

#include "HouseSettings.h"
#include "HouseSyncClient.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

// Plan refresh contract: force a full refresh after 8 consecutive partials.
static constexpr int MAX_CONSECUTIVE_PARTIALS = 8;

ChecklistActivity::ChecklistActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("HouseChecklist", renderer, mappedInput, /*wantsTouchLongPress=*/false) {}

void ChecklistActivity::onEnter() {
  UiListActivity::onEnter();
  reloadFromCache();
  // Entry boundary: clear whatever ghosting the previous screen left.
  renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
}

void ChecklistActivity::onExit() {
  Activity::onExit();
  // Exit boundary: hand the full-refresh wave to the next screen's repaint.
  renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
  // rowItems' label pointers alias items' strings; drop both together.
  rowItems.clear();
  items.clear();

  if (wifiSessionOpen && WiFi.getMode() != WIFI_MODE_NULL) {
    // Stock on-demand-Wi-Fi teardown (ClockSyncActivity pattern): the radio
    // only lives during the explicit sync moment, and the fragmentation it
    // leaves behind is cleared by the silent restart the reader already does.
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void ChecklistActivity::reloadFromCache() {
  items = HOUSE_CHECKLIST.getItems();
  rebuildRowItems();
}

void ChecklistActivity::rebuildRowItems() {
  rowItems.clear();
  rowItems.reserve(items.size() + 1);
  for (const auto& it : items) {
    fui::ListItem item;
    item.label = it.label.c_str();
    item.toggle = true;
    item.toggleChecked = it.done;
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
  fui::ListItem sync;
  sync.label = syncLabel;
  sync.actionValue = static_cast<int16_t>(items.size());
  rowItems.push_back(sync);
}

void ChecklistActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  // Sync-row label reflects live state (recomputed per repaint; buffers live
  // on the activity, ListItem just aliases).
  if (state == State::SYNCING) {
    snprintf(syncLabel, sizeof(syncLabel), "%s", tr(STR_HOUSE_SYNCING));
  } else if (statusText != nullptr) {
    snprintf(syncLabel, sizeof(syncLabel), "%s", statusText);
  } else if (const auto pending = HOUSE_CHECKLIST.getPendingToggles().size(); pending > 0) {
    snprintf(syncLabel, sizeof(syncLabel), tr(STR_HOUSE_PENDING_FMT), static_cast<int>(pending));
  } else {
    snprintf(syncLabel, sizeof(syncLabel), "%s", tr(STR_HOUSE_SYNC));
  }

  if (items.empty()) {
    screen.centeredText(tr(STR_HOUSE_EMPTY), screen.theme().bodyText);
    screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  }

  // rowItems was built by reloadFromCache()/rebuildRowItems() and aliases
  // activity-owned strings (items' labels + syncLabel above), so only data
  // changes rebuild it — repaints reuse it, like RecentBooksActivity.

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

void ChecklistActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  if (index == static_cast<int>(items.size())) {
    beginSync();
    return;
  }
  toggleItem(index);
}

void ChecklistActivity::toggleItem(const int index) {
  // Copy the id: the store mutates while we touch the row snapshot.
  const std::string id = items[index].id;
  items[index].done = HOUSE_CHECKLIST.toggleItem(id);
  rowItems[index].toggleChecked = items[index].done;

  // If the radio happens to be up, skip the queue round-trip.
  if (WiFi.status() == WL_CONNECTED) {
    std::string err;
    if (HouseSyncClient::postToggle(id, items[index].done, err)) {
      HOUSE_CHECKLIST.clearPending(id);
    }
  }

  if (++partialsSinceFull >= MAX_CONSECUTIVE_PARTIALS) {
    renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
    partialsSinceFull = 0;
  }
  requestUpdate();
}

void ChecklistActivity::beginSync() {
  if (!HOUSE_SETTINGS.hasServerConfig()) {
    statusText = tr(STR_HOUSE_NOT_CONFIGURED);
    requestUpdate();
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    state = State::SYNCING;
    requestUpdate();
    return;
  }
  // Explicit sync moment: bring the radio up on demand (stock pattern).
  wifiSessionOpen = true;
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           state = State::SYNCING;
                           requestUpdate();
                         });
}

void ChecklistActivity::runSync() {
  std::string err;
  bool ok = HouseSyncClient::fetchChecklist(err);
  if (ok) ok = HouseSyncClient::flushPending(err);
  statusText = ok ? tr(STR_HOUSE_SYNCED) : tr(STR_HOUSE_SYNC_FAILED);
  if (ok) reloadFromCache();
  state = State::IDLE;
  requestUpdate();
}

void ChecklistActivity::loop() {
  if (state == State::SYNCING) {
    // First tick: push the "Syncing…" frame, then block on the network work
    // (ClockSyncActivity pattern).
    requestUpdateAndWait();
    runSync();
    return;
  }
  UiListActivity::loop();
}
