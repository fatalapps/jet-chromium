// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={1600x1200}
// META: --wndow-size=800,600

(async function(testRunner) {
  const {dp} =
      await testRunner.startBlank(`Tests fullscreen browser window size.`);

  await dp.Page.enable();

  async function getWindowSize(windowId) {
    const {result: {bounds}} = await dp.Browser.getWindowBounds({windowId});
    return {width: bounds.width, height: bounds.height};
  }

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();

  await dp.Browser.setWindowBounds(
      {windowId, bounds: {windowState: 'fullscreen'}});
  await dp.Page.onceFrameResized();

  const fullscreenSize = await getWindowSize(windowId);
  testRunner.log(`${fullscreenSize.width}x${fullscreenSize.height}`);

  testRunner.completeTest();
});
