// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {client, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

function convertUint8ArrayToBase64(uint8Array: Uint8Array): string {
  let binaryString = '';
  for (let i = 0; i < uint8Array.length; i++) {
    binaryString += String.fromCharCode(uint8Array[i]!);
  }
  return btoa(binaryString);
}

$.executeAction.addEventListener('click', async () => {
  logMessage('Starting Execute Action');

  // The action proto is expected to be a Actions proto, which is binary
  // serialized and then base64 encoded.
  const protoBytes = Uint8Array.fromBase64($.actionProtoEncodedText.value);
  try {
    const actionResult = await client!.browser!.performActions!
                         (protoBytes.buffer as ArrayBuffer);
    $.actionStatus.innerText = `Finished Execute Action. Result code ${
        convertUint8ArrayToBase64(new Uint8Array(actionResult))}.`;
  } catch (error) {
    $.actionStatus.innerText = `Error in Execute Action: ${error}`;
  }
});

$.createActorTask.addEventListener('click', async () => {
  logMessage('Starting Create Actor Task');
  try {
    const taskId = await client!.browser!.createTask!();
    $.actorTaskId.value = taskId.toString();
    $.actionStatus.innerText = `Created task with ID: ${taskId}`;
  } catch (error) {
    $.actionStatus.innerText = `Error in Create Actor Task: ${error}`;
  }
});

$.stopActorTask.addEventListener('click', () => {
  logMessage('Starting Stop Actor Task');
  const taskIdStr = $.actorTaskId.value;
  if (taskIdStr) {
    const taskId = parseInt(taskIdStr, 10);
    if (isNaN(taskId)) {
      $.actionStatus.innerText = `Invalid task ID: ${taskIdStr}`;
      return;
    }
    client!.browser!.stopActorTask!(taskId);
    $.actionStatus.innerText = `Stopped task with ID: ${taskId}`;
  } else {
    client!.browser!.stopActorTask!();
    $.actionStatus.innerText = 'Stopped most recent task.';
  }
});
