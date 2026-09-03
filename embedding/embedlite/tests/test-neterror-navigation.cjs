/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Run with node test-neterror-navigation.cjs /path/to/patched/gecko.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

assert.ok(process.argv[2], "Supply the patched Gecko source directory");
let source = fs.readFileSync(path.join(process.argv[2],
  "toolkit/actors/NetErrorParent.sys.mjs"), "utf8");
source = source.replace(/^import .*;\n/gm, "").replace(/^export /gm, "");
const principal = {};
const scope = {
  JSWindowActorParent: class {},
  AppConstants: { MOZ_BUILD_APP: "mobile/sailfishos" },
  ChromeUtils: { defineESModuleGetters() {} },
  Services: { scriptSecurityManager: { getSystemPrincipal() { return principal; } } }
};
vm.createContext(scope);
vm.runInContext(source + "\nglobalThis.Actor = EscapablePageParent;", scope);

for (const kind of ["native", "absent", "desktop"]) {
  for (const index of [0, 1]) {
    for (const allowGoingBack of [false, true]) {
      const calls = [];
      const navigation = target => ({
        goBack() { calls.push([target, "back"]); },
        fixupAndLoadURIString(url, options) {
          assert.equal(options.triggeringPrincipal, principal);
          calls.push([target, url]);
        }
      });
      const actor = new scope.Actor();
      actor.browsingContext = { parent: null, top: {
        crossGroupOpener: null, sessionHistory: { index },
        ...navigation("context")
      } };
      const browser = kind === "absent" ? null : {
        documentGlobal: {}, frameLoader: { messageManager: {} }
      };
      if (kind === "desktop") {
        Object.assign(browser, navigation("browser"), { canGoBack: index > 0 });
      }
      actor.leaveErrorPage(browser, allowGoingBack);
      assert.deepEqual(calls, [[kind === "desktop" ? "browser" : "context",
        index > 0 && allowGoingBack ? "back" : "about:blank"]]);
    }
  }
}

const actor = new scope.Actor();
actor.browsingContext = { parent: null, top: { crossGroupOpener: {} } };
let closed = false;
actor.leaveErrorPage({
  documentGlobal: { CustomEvent: class {
    constructor(type, options) { this.type = type; this.bubbles = options.bubbles; }
  } },
  dispatchEvent(event) {
    assert.equal(event.type, "DOMWindowClose");
    assert.equal(event.bubbles, true);
    closed = true;
  }
});
assert.equal(closed, true, "Native popup errors must still close the popup");
console.log("Error-page navigation tests passed");
