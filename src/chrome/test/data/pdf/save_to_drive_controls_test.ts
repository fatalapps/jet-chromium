// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

const SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;
type SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;

const tests = [
  /**
   * Test that the toolbar shows an option to save the edited PDF to Google
   * Drive if available.
   */
  async function testEditedPdfOption() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element = document.createElement('viewer-save-to-drive-controls');
    document.body.appendChild(element);

    // Do not show the menu if there are no edits.
    let onSave = eventToPromise('save-to-drive', element);
    element.$.save.click();
    let e: CustomEvent<SaveRequestType> = await onSave;
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);

    // Set editing mode. Now, the menu should open.
    element.hasEdits = true;
    element.$.save.click();
    await eventToPromise('save-menu-shown-for-testing', element);
    chrome.test.assertTrue(element.$.menu.open);

    // Click on "Edited".
    const buttons = element.shadowRoot.querySelectorAll('button');
    onSave = eventToPromise('save-to-drive', element);
    buttons[0]!.click();
    e = await onSave;
    chrome.test.assertEq(SaveRequestType.EDITED, e.detail);

    // Click the button again to re-open menu and click on "Original".
    element.$.save.click();
    await eventToPromise('save-menu-shown-for-testing', element);
    chrome.test.assertTrue(element.$.menu.open);
    onSave = eventToPromise('save-to-drive', element);
    buttons[1]!.click();
    e = await onSave;
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);

    chrome.test.succeed();
  },

  /**
   * Test that the upload progress ring is shown and updated correctly.
   */
  async function testShowUploadProgress() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element = document.createElement('viewer-save-to-drive-controls');
    document.body.appendChild(element);
    await microtasksFinished();

    // Initially the button should be in non-uploading state
    chrome.test.assertEq('pdf:add-to-drive', element.$.save.ironIcon);
    chrome.test.assertFalse(
        !!element.shadowRoot.querySelector('circular-progress-ring'));

    // Once we start showing the upload progress, the icon will change
    element.showUploadProgress(10);
    await microtasksFinished();
    chrome.test.assertEq('pdf:arrow-upward-alt', element.$.save.ironIcon);
    chrome.test.assertTrue(
        !!element.shadowRoot.querySelector('circular-progress-ring'));


    // If we reset the button, it should go back to the initial state
    element.reset();
    await microtasksFinished();
    chrome.test.assertEq('pdf:add-to-drive', element.$.save.ironIcon);
    chrome.test.assertFalse(
        !!element.shadowRoot.querySelector('circular-progress-ring'));

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
