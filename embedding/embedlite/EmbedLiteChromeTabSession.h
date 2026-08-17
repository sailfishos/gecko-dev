/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_CHROME_TAB_SESSION_H
#define EMBED_LITE_CHROME_TAB_SESSION_H

#include <stdint.h>

namespace mozilla {
namespace embedlite {

struct EmbedLiteChromeHistoryEntry
{
  // These strings are borrowed for the duration of RestoreTabs().
  const char* location;
  const char16_t* title;
};

struct EmbedLiteChromeRestoredTab
{
  // The history array and its strings are borrowed for the duration of
  // RestoreTabs().
  uint64_t persistentId;
  const EmbedLiteChromeHistoryEntry* history;
  uint32_t historyCount;
  int32_t selectedHistoryIndex;
};

struct EmbedLiteChromeTabSnapshot
{
  uint64_t id;
  uint64_t persistentId;
  uint64_t locationRevision;
  const char* location;
  const char16_t* title;
  bool loading;
  bool closing;
  bool discarded;
  bool canGoBack;
  bool canGoForward;
  int32_t progress;
  int64_t current;
  int64_t total;
};

class EmbedLiteChromeTabSessionListener
{
public:
  // The array and strings are borrowed and only valid for the duration of
  // this callback. Revisions increase monotonically for this chrome window.
  virtual void OnTabsChanged(uint64_t aRevision, uint64_t aSelectedTabId,
                             const EmbedLiteChromeTabSnapshot* aTabs,
                             uint32_t aTabCount) = 0;

  // The session is no longer usable after this callback returns.
  virtual void ChromeTabSessionDestroyed() = 0;

protected:
  virtual ~EmbedLiteChromeTabSessionListener() = default;
};

class EmbedLiteChromeTabSession
{
public:
  // This does not take ownership. Passing nullptr detaches the listener
  // synchronously and prevents subsequent callbacks.
  virtual void SetTabListener(
    EmbedLiteChromeTabSessionListener* aListener) = 0;

  // These results only report whether IPC accepted the request. Gecko owns
  // tab IDs and publishes the authoritative result in OnTabsChanged().
  // RestoreTabs() is a one-shot operation and may be sent before the chrome
  // window reports that it has initialized.
  virtual bool RestoreTabs(const EmbedLiteChromeRestoredTab* aTabs,
                           uint32_t aTabCount,
                           int32_t aSelectedTabIndex) = 0;
  virtual bool NewTab(const char* aURL, uint64_t aPersistentId,
                      bool aFromExternal, bool aInBackground) = 0;
  virtual bool AssociateTab(uint64_t aTabId,
                            uint64_t aPersistentId) = 0;
  virtual bool SelectTab(uint64_t aTabId) = 0;
  virtual bool CloseTab(uint64_t aTabId) = 0;

protected:
  virtual ~EmbedLiteChromeTabSession() = default;
};

} // namespace embedlite
} // namespace mozilla

#endif
