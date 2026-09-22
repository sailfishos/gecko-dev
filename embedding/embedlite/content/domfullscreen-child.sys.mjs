/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class EmbedLiteDOMFullscreenChild extends JSWindowActorChild {
  _orientation() {
    const type = this.contentWindow?.screen.orientation.type;
    return type?.startsWith("landscape") ? "landscape" : "portrait";
  }

  receiveMessage(message) {
    const windowUtils = this.contentWindow?.windowUtils;
    if (!windowUtils) {
      this.sendAsyncMessage("EmbedLiteDOMFullscreen:Exit", {});
      return;
    }

    switch (message.name) {
      case "EmbedLiteDOMFullscreen:Entered":
        this._lastOrientation = this._orientation();
        if (!windowUtils.handleFullscreenRequests() &&
            !this.document.fullscreenElement) {
          this.sendAsyncMessage("EmbedLiteDOMFullscreen:Exit", {});
        }
        break;
      case "EmbedLiteDOMFullscreen:CleanUp":
        if (this.document.fullscreenElement) {
          const restoreViewSize =
            this._orientation() === this._lastOrientation;
          windowUtils.exitFullscreen(!restoreViewSize);
        } else {
          this.sendAsyncMessage("EmbedLiteDOMFullscreen:Exited", {});
        }
        break;
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "MozDOMFullscreen:Request":
        this.sendAsyncMessage("EmbedLiteDOMFullscreen:Request", {});
        break;
      case "MozDOMFullscreen:Entered":
        this.sendAsyncMessage("EmbedLiteDOMFullscreen:Entered", {});
        break;
      case "MozDOMFullscreen:Exit":
        this.sendAsyncMessage("EmbedLiteDOMFullscreen:Exit", {});
        break;
      case "MozDOMFullscreen:Exited":
        this.sendAsyncMessage("EmbedLiteDOMFullscreen:Exited", {});
        break;
    }
  }
}
