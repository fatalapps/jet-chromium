// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "exclusive_access_manager_android.h"

#include <cstddef>

#include "chrome/android/chrome_jni_headers/ExclusiveAccessManager_jni.h"
#include "chrome/browser/ui/android/exclusive_access/exclusive_access_context_android.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

ExclusiveAccessManagerAndroid::ExclusiveAccessManagerAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_eam,
    const jni_zero::JavaRef<jobject>& j_fullscreen_manager,
    const jni_zero::JavaRef<jobject>& j_activity_tab_provider)
    : eac_(std::make_unique<ExclusiveAccessContextAndroid>(
          env,
          j_fullscreen_manager,
          j_activity_tab_provider)),
      eam_(eac_.get()) {
  j_eam_.Reset(j_eam);
}

ExclusiveAccessManagerAndroid::~ExclusiveAccessManagerAndroid() = default;

void ExclusiveAccessManagerAndroid::EnterFullscreenModeForTab(
    JNIEnv* env,
    jlong requesting_frame,
    bool prefersNavigationBar,
    bool prefersStatusBar) {
  eam_.fullscreen_controller()->EnterFullscreenModeForTab(
      reinterpret_cast<content::RenderFrameHost*>(requesting_frame));
}

void ExclusiveAccessManagerAndroid::ExitFullscreenModeForTab(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jweb_contents) {
  content::WebContents* wc =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(wc != nullptr);
  eam_.fullscreen_controller()->ExitFullscreenModeForTab(wc);
}

bool ExclusiveAccessManagerAndroid::IsFullscreenForTabOrPending(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jweb_contents) {
  content::WebContents* wc =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(wc != nullptr);
  auto state = eam_.fullscreen_controller()->GetFullscreenState(wc);

  return state.target_mode == content::FullscreenMode::kContent ||
         state.target_mode == content::FullscreenMode::kPseudoContent;
}

bool ExclusiveAccessManagerAndroid::PreHandleKeyboardEvent(
    JNIEnv* env,
    jlong nativeKeyEvent) {
  return eam_.HandleUserKeyEvent(
      *reinterpret_cast<input::NativeWebKeyboardEvent*>(nativeKeyEvent));
}

void ExclusiveAccessManagerAndroid::RequestKeyboardLock(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jweb_contents,
    bool escKeyLocked) {
  content::WebContents* wc =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK_NE(wc, nullptr);
  eam_.keyboard_lock_controller()->RequestKeyboardLock(wc, escKeyLocked);
}

void ExclusiveAccessManagerAndroid::CancelKeyboardLockRequest(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jweb_contents) {
  content::WebContents* wc =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK_NE(wc, nullptr);
  eam_.keyboard_lock_controller()->CancelKeyboardLockRequest(wc);
}

void ExclusiveAccessManagerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

jlong JNI_ExclusiveAccessManager_Init(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& jeam,
    const jni_zero::JavaParamRef<jobject>& j_fullscreen_manager,
    const jni_zero::JavaParamRef<jobject>& j_activity_tab_provider) {
  ExclusiveAccessManagerAndroid* content = new ExclusiveAccessManagerAndroid(
      env, jeam, j_fullscreen_manager, j_activity_tab_provider);
  return reinterpret_cast<intptr_t>(content);
}
