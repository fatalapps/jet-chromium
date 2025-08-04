// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ephemeraltab;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a {@link
 * EphemeralTabCoordinator}.
 */
@NullMarked
public class EphemeralTabCoordinatorSupplier
        extends UnownedUserDataSupplier<EphemeralTabCoordinator> {
    private static final UnownedUserDataKey<EphemeralTabCoordinatorSupplier> KEY =
            new UnownedUserDataKey<>(EphemeralTabCoordinatorSupplier.class);

    /**
     * Return {@link EphemeralTabCoordinator} supplier associated with the given {@link
     * WindowAndroid}.
     */
    public static @Nullable ObservableSupplier<EphemeralTabCoordinator> from(
            WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /** Constructs a EphemeralTabCoordinator and attaches it to the {@link WindowAndroid}. */
    public EphemeralTabCoordinatorSupplier() {
        super(KEY);
    }
}
