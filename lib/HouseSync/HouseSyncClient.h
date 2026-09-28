#pragma once
#include <string>
#include <vector>

#include "HouseChecklistStore.h"

/**
 * HTTP client for the house checklist endpoints (kaw server, plan Phase 0).
 *
 *   GET  /api/house/checklist/<name>?device=..&token=..   -> {"items":[...]}
 *   POST /api/house/checklist/<name>/item/<id>            (form: device, token, done)
 *         -> {"ok":true,"item":{...}}
 *
 * Uses SecureHttpClient (freeink SecureNet): plain http today, TLS-capable if
 * the house server ever fronts https. One-shot calls — the caller brings Wi-Fi
 * up beforehand (on-demand pattern) and tears it down after.
 */
class HouseSyncClient {
 public:
  // Fetch the named checklist from the server, cache it to SD (re-applying
  // pending offline toggles) and stamp lastFetch. Returns false + err on any
  // failure; the SD cache is left untouched unless the fetch succeeded.
  static bool fetchChecklist(std::string& err);

  // POST one toggle to the server. done is the desired final state (the
  // device-side item has already been flipped locally).
  static bool postToggle(const std::string& id, bool done, std::string& err);

  // Drain the pending queue: POST each queued toggle, clearing entries the
  // server accepted. Stops at the first failure (Wi-Fi likely gone).
  // Returns true when the queue is empty afterwards.
  static bool flushPending(std::string& err);

  static int getLastHttpCode() { return lastHttpCode; }

 private:
  static int lastHttpCode;
};
