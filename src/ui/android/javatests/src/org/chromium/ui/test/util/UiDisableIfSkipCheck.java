// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.content.Context;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisableIfSkipCheck;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Checks for conditional disables. Currently only includes checks against a few device form factor
 * values.
 */
public class UiDisableIfSkipCheck extends DisableIfSkipCheck {
    private final Context mTargetContext;

    public UiDisableIfSkipCheck(Context targetContext) {
        mTargetContext = targetContext;
    }

    @Override
    protected boolean deviceTypeApplies(String type) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    switch (type) {
                        case DeviceFormFactor.PHONE:
                            return !isDesktopBuild() && !isTablet();
                        case DeviceFormFactor.ONLY_TABLET:
                            return !isDesktopBuild() && isTablet();
                        case DeviceFormFactor.DESKTOP:
                            return isDesktopBuild();
                        case DeviceFormFactor.TABLET_OR_DESKTOP:
                            return isTablet();
                        default:
                            return false;
                    }
                });
    }

    private boolean isDesktopBuild() {
        return DeviceInfo.isDesktop();
    }

    private boolean isTablet() {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mTargetContext);
    }
}
