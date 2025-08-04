// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './composebox.mojom-webui.js';

export interface ComposeboxProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  searchboxHandler: SearchboxPageHandlerRemote;
  searchboxCallbackRouter: SearchboxPageCallbackRouter;
}
export class ComposeboxProxyImpl implements ComposeboxProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  searchboxHandler: SearchboxPageHandlerRemote;
  searchboxCallbackRouter: SearchboxPageCallbackRouter;
  constructor(
      handler: PageHandlerRemote, callbackRouter: PageCallbackRouter,
      searchboxHandler: SearchboxPageHandlerRemote,
      searchboxCallbackRouter: SearchboxPageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
    this.searchboxHandler = searchboxHandler;
    this.searchboxCallbackRouter = searchboxCallbackRouter;
  }
  static getInstance(): ComposeboxProxyImpl {
    if (instance) {
      return instance;
    }
    // Composebox connection variables.
    const callbackRouter = new PageCallbackRouter();
    const handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    // Searchbox connection variables.
    const searchboxHandler = new SearchboxPageHandlerRemote();
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    factory.createPageHandler(
        callbackRouter.$.bindNewPipeAndPassRemote(),
        handler.$.bindNewPipeAndPassReceiver(),
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote(),
        searchboxHandler.$.bindNewPipeAndPassReceiver());
    instance = new ComposeboxProxyImpl(
        handler, callbackRouter, searchboxHandler, searchboxCallbackRouter);
    return instance;
  }
  static setInstance(newInstance: ComposeboxProxy) {
    instance = newInstance;
  }
}
let instance: ComposeboxProxy|null = null;
