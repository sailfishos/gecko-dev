/* This Source Code Form is subject to the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FinderParent } from "resource://gre/modules/FinderParent.sys.mjs";

const searches = new WeakMap();

export function findInPage(browser, data, emit) {
  let state = searches.get(browser);
  if (!state) {
    state = { pending: Promise.resolve(), sequence: 0 };
    searches.set(browser, state);
  }
  const sequence = ++state.sequence;
  if (!data.again || !data.text) state.resetSequence = sequence;
  const document = browser.browsingContext?.currentWindowGlobal;
  const current = () => sequence === state.sequence && document &&
    document === browser.browsingContext?.currentWindowGlobal;

  // Keep find-again searches in order, and discard replies after navigation
  // or cancellation. FinderParent maintains the cross-frame search position.
  const pending = state.pending.then(async () => {
    if (!document || document !== browser.browsingContext?.currentWindowGlobal ||
        sequence < state.resetSequence) return;
    if (state.document !== document) {
      if (!data.text) return;
      state.finder = new FinderParent(browser);
      state.document = document;
    }
    const finder = state.finder;
    if (!data.text) {
      finder.sendMessageToAllContexts("Finder:RemoveSelection");
      finder.onFindbarClose();
      state.document = null;
      return;
    }
    const listener = {
      onFindResult(result) { if (current()) emit({ r: result.result }); },
      onHighlightFinished() {},
      onMatchesCountResult() {},
    };
    finder.addResultListener(listener);
    try {
      await finder.doFind(!!data.again, {
        searchString: data.text,
        findBackwards: !!data.backwards,
        linksOnly: false,
        drawOutline: true,
      });
    } finally {
      finder.removeResultListener(listener);
    }
  });
  state.pending = pending.catch(() => {});
  return pending;
}
