/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class EmbedLiteDOMFullscreenParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "EmbedLiteDOMFullscreen:Request":
        this.manager.fullscreen = true;
        this.sendAsyncMessage("EmbedLiteDOMFullscreen:Entered", {});
        break;
      case "EmbedLiteDOMFullscreen:Entered":
        this.manager.fullscreen = true;
        break;
      case "EmbedLiteDOMFullscreen:Exit":
        this.manager.fullscreen = false;
        this.sendAsyncMessage("EmbedLiteDOMFullscreen:CleanUp", {});
        break;
      case "EmbedLiteDOMFullscreen:Exited":
        this.manager.fullscreen = false;
        break;
    }
  }
}
