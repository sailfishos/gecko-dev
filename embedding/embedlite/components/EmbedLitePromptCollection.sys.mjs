/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

/**
 * Implements nsIPromptCollection for EmbedLite.
 */
export class EmbedLitePromptCollection {
  confirmRepost(browsingContext) {
    let brandName;
    try {
      brandName = this.stringBundles.brand.GetStringFromName("brandShortName");
    } catch (exception) {
      // Use the generic prompt text when branding is unavailable.
    }

    let message;
    let resendLabel;
    try {
      if (brandName) {
        message = this.stringBundles.app.formatStringFromName(
          "confirmRepostPrompt",
          [brandName]
        );
      } else {
        message = this.stringBundles.app.GetStringFromName(
          "confirmRepostPrompt"
        );
      }
      resendLabel =
        this.stringBundles.app.GetStringFromName("resendButton.label");
    } catch (exception) {
      console.error("Failed to get strings from appstrings.properties");
      return false;
    }

    let contentViewer = browsingContext?.docShell?.contentViewer;
    let modalType = contentViewer?.isTabModalPromptAllowed
      ? Ci.nsIPromptService.MODAL_TYPE_CONTENT
      : Ci.nsIPromptService.MODAL_TYPE_WINDOW;
    let buttonFlags =
      (Ci.nsIPromptService.BUTTON_TITLE_IS_STRING *
        Ci.nsIPromptService.BUTTON_POS_0) |
      (Ci.nsIPromptService.BUTTON_TITLE_CANCEL *
        Ci.nsIPromptService.BUTTON_POS_1);
    let buttonPressed = Services.prompt.confirmExBC(
      browsingContext,
      modalType,
      null,
      message,
      buttonFlags,
      resendLabel,
      null,
      null,
      null,
      {}
    );

    return buttonPressed === 0;
  }

  async asyncBeforeUnloadCheck(browsingContext) {
    let title;
    let message;
    let leaveLabel;
    let stayLabel;

    try {
      title = this.stringBundles.dom.GetStringFromName("OnBeforeUnloadTitle");
      message = this.stringBundles.dom.GetStringFromName(
        "OnBeforeUnloadMessage2"
      );
      leaveLabel = this.stringBundles.dom.GetStringFromName(
        "OnBeforeUnloadLeaveButton"
      );
      stayLabel = this.stringBundles.dom.GetStringFromName(
        "OnBeforeUnloadStayButton"
      );
    } catch (exception) {
      console.error("Failed to get strings from dom.properties");
      return false;
    }

    let contentViewer = browsingContext?.docShell?.contentViewer;

    if (
      (contentViewer && !contentViewer.isTabModalPromptAllowed) ||
      !browsingContext.ancestorsAreCurrent
    ) {
      console.error("Can't prompt from inactive content viewer");
      return true;
    }

    let buttonFlags =
      Ci.nsIPromptService.BUTTON_POS_0_DEFAULT |
      (Ci.nsIPromptService.BUTTON_TITLE_IS_STRING *
        Ci.nsIPromptService.BUTTON_POS_0) |
      (Ci.nsIPromptService.BUTTON_TITLE_IS_STRING *
        Ci.nsIPromptService.BUTTON_POS_1);

    let result = await Services.prompt.asyncConfirmEx(
      browsingContext,
      Services.prompt.MODAL_TYPE_CONTENT,
      title,
      message,
      buttonFlags,
      leaveLabel,
      stayLabel,
      null,
      null,
      false,
      { inPermitUnload: true }
    );

    return (
      result.QueryInterface(Ci.nsIPropertyBag2).get("buttonNumClicked") == 0
    );
  }

  confirmFolderUpload(browsingContext, directoryName) {
    let title;
    let message;
    let acceptLabel;

    try {
      title = this.stringBundles.dom.GetStringFromName(
        "FolderUploadPrompt.title"
      );
      message = this.stringBundles.dom.formatStringFromName(
        "FolderUploadPrompt.message",
        [directoryName]
      );
      acceptLabel = this.stringBundles.dom.GetStringFromName(
        "FolderUploadPrompt.acceptButtonLabel"
      );
    } catch (exception) {
      console.error("Failed to get strings from dom.properties");
      return false;
    }

    let buttonFlags =
      Services.prompt.BUTTON_TITLE_IS_STRING *
        Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1 +
      Services.prompt.BUTTON_POS_1_DEFAULT;

    return (
      Services.prompt.confirmExBC(
        browsingContext,
        Services.prompt.MODAL_TYPE_TAB,
        title,
        message,
        buttonFlags,
        acceptLabel,
        null,
        null,
        null,
        {}
      ) === 0
    );
  }
}

const BUNDLES = {
  dom: "chrome://global/locale/dom/dom.properties",
  app: "chrome://global/locale/appstrings.properties",
  brand: "chrome://branding/locale/brand.properties",
};

EmbedLitePromptCollection.prototype.stringBundles = {};

for (const [bundleName, bundleUrl] of Object.entries(BUNDLES)) {
  XPCOMUtils.defineLazyGetter(
    EmbedLitePromptCollection.prototype.stringBundles,
    bundleName,
    function () {
      let bundle = Services.strings.createBundle(bundleUrl);
      if (!bundle) {
        throw new Error("String bundle for prompt not present!");
      }
      return bundle;
    }
  );
}

EmbedLitePromptCollection.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIPromptCollection",
]);
