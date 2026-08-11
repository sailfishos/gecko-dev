/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.importESModule(
  "resource://gre/modules/Services.sys.mjs"
);

function loadInitialContent() {
  const encodedURI = new URLSearchParams(window.location.search).get("uri");
  if (!encodedURI) {
    return;
  }

  let initialURI;
  try {
    const bytes = ChromeUtils.base64URLDecode(encodedURI, {
      padding: "reject",
    });
    initialURI = new TextDecoder().decode(bytes);
  } catch (error) {
    console.error("Invalid EmbedLite initial content URI", error);
    return;
  }

  const browser = document.getElementById("content");
  browser.fixupAndLoadURIString(initialURI, {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
}

window.addEventListener("load", loadInitialContent, { once: true });
