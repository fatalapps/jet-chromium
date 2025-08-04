// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.ContextThemeWrapper;

import androidx.annotation.DrawableRes;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifierJni;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.paint_preview.TabbedPaintPreview;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.OmniboxUrlEmphasizerJni;
import org.chromium.url.GURL;

/** Unit tests for the LocationBarModel. */
@RunWith(BaseRobolectricTestRunner.class)
public class LocationBarModelUnitTest {
    private static final LocationBarModel.OfflineStatus OFFLINE_STATUS =
            new LocationBarModel.OfflineStatus() {
                @Override
                public boolean isShowingTrustedOfflinePage(Tab tab) {
                    return false;
                }

                @Override
                public boolean isOfflinePage(Tab tab) {
                    return false;
                }
            };

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mIncognitoTabMock;
    @Mock private Tab mIncognitoNonPrimaryTabMock;
    @Mock private Tab mRegularTabMock;

    @Mock private Profile mRegularProfileMock;
    @Mock private Profile mPrimaryOtrProfileMock;
    @Mock private Profile mNonPrimaryOtrProfileMock;

    @Mock private LocationBarDataProvider.Observer mLocationBarDataObserver;
    @Mock private LocationBarModel.Natives mLocationBarModelJni;
    @Mock private ChromeAutocompleteSchemeClassifier.Natives mChromeAutocompleteSchemeClassifierJni;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private OmniboxUrlEmphasizerJni mOmniboxUrlEmphasizerJni;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabbedPaintPreview mTabbedPaintPreview;

    private final UserDataHost mUserDataHost = new UserDataHost();

    @Spy
    public LocationBarModel mLocationBarModel =
            new LocationBarModel(
                    new ContextThemeWrapper(
                            ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight),
                    NewTabPageDelegate.EMPTY,
                    url -> url.getSpec(),
                    OFFLINE_STATUS);

    private final GURL mExampleGurl = new GURL("http://www.example.com/");

    @Before
    public void setUp() {
        ChromeAutocompleteSchemeClassifierJni.setInstanceForTesting(
                mChromeAutocompleteSchemeClassifierJni);
        LocationBarModelJni.setInstanceForTesting(mLocationBarModelJni);
        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);
        OmniboxUrlEmphasizerJni.setInstanceForTesting(mOmniboxUrlEmphasizerJni);

        when(mPrimaryOtrProfileMock.isOffTheRecord()).thenReturn(true);
        when(mNonPrimaryOtrProfileMock.isOffTheRecord()).thenReturn(true);

        mUserDataHost.setUserData(TabbedPaintPreview.USER_DATA_KEY, mTabbedPaintPreview);
        when(mRegularTabMock.getProfile()).thenReturn(mRegularProfileMock);
        when(mRegularTabMock.getUserDataHost()).thenReturn(mUserDataHost);

        when(mIncognitoTabMock.isIncognito()).thenReturn(true);
        when(mIncognitoTabMock.getProfile()).thenReturn(mPrimaryOtrProfileMock);

        when(mIncognitoNonPrimaryTabMock.isIncognito()).thenReturn(true);
        when(mIncognitoNonPrimaryTabMock.getProfile()).thenReturn(mNonPrimaryOtrProfileMock);

        when(mLocationBarModelJni.init(any())).thenReturn(123L);

        // Bypass OmniboxUrlEmphasizer testing - this code always returns the displayText.
        doReturn(false).when(mLocationBarModel).shouldEmphasizeUrl();
        doAnswer(inv -> (CharSequence) inv.getArgument(1))
                .when(mLocationBarModel)
                .getOrCreateUrlBarDataStyledDisplayText(any(), any(), anyBoolean());
    }

    @After
    public void tearDown() {
        mLocationBarModel.destroy();
    }

    @Test
    public void getProfile_RegularTab_ReturnsRegularProfile() {
        mLocationBarModel.setTab(mRegularTabMock, mRegularProfileMock);
        Profile profile = mLocationBarModel.getProfile();
        Assert.assertEquals(mRegularProfileMock, profile);
    }

    @Test
    public void getProfile_IncognitoTab_ReturnsPrimaryOtrProfile() {
        mLocationBarModel.setTab(mIncognitoTabMock, mPrimaryOtrProfileMock);
        Profile otrProfile = mLocationBarModel.getProfile();
        Assert.assertEquals(mPrimaryOtrProfileMock, otrProfile);
    }

    @Test
    public void getProfile_IncognitoTab_ReturnsNonPrimaryOtrProfile() {
        mLocationBarModel.setTab(mIncognitoNonPrimaryTabMock, mNonPrimaryOtrProfileMock);
        Profile otrProfile = mLocationBarModel.getProfile();
        Assert.assertEquals(mNonPrimaryOtrProfileMock, otrProfile);
    }

    @Test
    public void getProfile_NullTab() {
        mLocationBarModel.setTab(null, mRegularProfileMock);
        Assert.assertEquals(mRegularProfileMock, mLocationBarModel.getProfile());

        mLocationBarModel.setTab(null, mPrimaryOtrProfileMock);
        Assert.assertEquals(mPrimaryOtrProfileMock, mLocationBarModel.getProfile());

        mLocationBarModel.setTab(null, mNonPrimaryOtrProfileMock);
        Assert.assertEquals(mNonPrimaryOtrProfileMock, mLocationBarModel.getProfile());
    }

    @Test
    public void testObserversNotified_titleChange() {
        mLocationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onTitleChanged();

        mLocationBarModel.notifyTitleChanged();
        verify(mLocationBarDataObserver).onTitleChanged();

        mLocationBarModel.setTab(mRegularTabMock, mRegularProfileMock);
        verify(mLocationBarDataObserver, times(2)).onTitleChanged();

        mLocationBarModel.removeObserver(mLocationBarDataObserver);
        mLocationBarModel.notifyTitleChanged();
        verify(mLocationBarDataObserver, times(2)).onTitleChanged();
    }

    @Test
    public void testObserversNotified_urlChange() {
        mLocationBarModel.initializeWithNative();
        mLocationBarModel.addObserver(mLocationBarDataObserver);

        doReturn(mExampleGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong());
        mLocationBarModel.updateVisibleGurl();

        // The visible url should be cached and hasn't changed, so onUrlChanged shouldn't be called
        mLocationBarModel.notifyUrlChanged(false);
        verify(mLocationBarDataObserver, never()).onUrlChanged(Mockito.anyBoolean());

        // Setting to a new tab with a different url
        GURL exampleGurl2 = new GURL("http://www.example2.com/");
        doReturn(exampleGurl2)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong());
        mLocationBarModel.setTab(mRegularTabMock, mRegularProfileMock);
        verify(mLocationBarDataObserver).onTabChanged(null);
        verify(mLocationBarDataObserver, times(1)).onUrlChanged(true);

        // Setting to another new tab with a different url
        GURL exampleGurl3 = new GURL("http://www.example3.com/");
        Tab regularTabMock2 = Mockito.mock(Tab.class);
        when(regularTabMock2.getProfile()).thenReturn(mRegularProfileMock);
        doReturn(exampleGurl3)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong());
        mLocationBarModel.setTab(regularTabMock2, mRegularProfileMock);
        verify(mLocationBarDataObserver).onTabChanged(mRegularTabMock);
        verify(mLocationBarDataObserver, times(2)).onUrlChanged(true);

        // onUrlChanged() won't be called after calling removeObserver().
        mLocationBarModel.removeObserver(mLocationBarDataObserver);
        mLocationBarModel.notifyUrlChanged(false);
        verify(mLocationBarDataObserver, never()).onUrlChanged(false);
    }

    @Test
    public void testObserversNotified_ntpLoaded() {
        mLocationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onNtpStartedLoading();

        mLocationBarModel.notifyNtpStartedLoading();
        verify(mLocationBarDataObserver).onNtpStartedLoading();
    }

    @Test
    public void testObserversNotified_setIsShowingTabSwitcher() {
        mLocationBarModel.initializeWithNative();
        mLocationBarModel.addObserver(mLocationBarDataObserver);
        doReturn(mExampleGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong());
        mLocationBarModel.updateVisibleGurl();

        verify(mLocationBarDataObserver, never()).onTitleChanged();
        verify(mLocationBarDataObserver, never()).onUrlChanged(Mockito.anyBoolean());
        verify(mLocationBarDataObserver, never()).onPrimaryColorChanged();
        verify(mLocationBarDataObserver, never()).onSecurityStateChanged();

        mLocationBarModel.updateForNonStaticLayout();

        // The omnibox is not showing, and we have not switched to a new tab yet, so don't expect
        // notifications of a url change
        verify(mLocationBarDataObserver, never()).onUrlChanged(Mockito.anyBoolean());
        Assert.assertEquals(mLocationBarModel.getCurrentGurl(), mExampleGurl);

        verify(mLocationBarDataObserver).onTitleChanged();
        verify(mLocationBarDataObserver).onPrimaryColorChanged();
        verify(mLocationBarDataObserver).onSecurityStateChanged();
    }

    @Test
    public void testSpannableCache() {
        mLocationBarModel.setTab(mRegularTabMock, mRegularProfileMock);
        doReturn(true).when(mRegularTabMock).isInitialized();
        mLocationBarModel.initializeWithNative();

        doReturn(mExampleGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong());
        doReturn(mExampleGurl.getSpec())
                .when(mLocationBarModelJni)
                .getFormattedFullURL(Mockito.anyLong());
        doReturn(mExampleGurl.getSpec())
                .when(mLocationBarModelJni)
                .getURLForDisplay(Mockito.anyLong());
        Assert.assertTrue(mLocationBarModel.updateVisibleGurl());
        Assert.assertFalse("Update should be suppressed", mLocationBarModel.updateVisibleGurl());

        // URL changed, cache is invalid.
        GURL exampleGurl2 = new GURL("http://www.example2.com/");
        doReturn(exampleGurl2)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong());
        doReturn(exampleGurl2.getSpec())
                .when(mLocationBarModelJni)
                .getFormattedFullURL(Mockito.anyLong());
        doReturn(exampleGurl2.getSpec())
                .when(mLocationBarModelJni)
                .getURLForDisplay(Mockito.anyLong());
        Assert.assertTrue("New url should notify", mLocationBarModel.updateVisibleGurl());
        Assert.assertFalse(
                "Update should be suppressed again", mLocationBarModel.updateVisibleGurl());
    }

    @Test
    public void testBuildUrlBarData_withoutNative() {
        doReturn(true).when(mLocationBarModel).shouldEmphasizeUrl();

        var data =
                mLocationBarModel.buildUrlBarData(
                        new GURL("https://www.abc.xyz"),
                        /* isOfflinePage= */ false,
                        /* displayText= */ "Alphabet",
                        /* editingText= */ null);

        Assert.assertEquals("https://www.abc.xyz", data.displayText);
    }

    @Test
    public void testBuildUrlBarData_emptyDisplayText() {
        mLocationBarModel.initializeWithNative();

        var data =
                mLocationBarModel.buildUrlBarData(
                        new GURL("https://www.abc.xyz"),
                        /* isOfflinePage= */ false,
                        /* displayText= */ null,
                        /* editingText= */ null);

        Assert.assertEquals("https://www.abc.xyz", data.displayText);
    }

    @Test
    public void testBuildUrlBarData_nonEmptyDisplayTextWithNoEmphasis() {
        mLocationBarModel.initializeWithNative();

        var data =
                mLocationBarModel.buildUrlBarData(
                        new GURL("https://www.abc.xyz"),
                        /* isOfflinePage= */ false,
                        /* displayText= */ "Alphabet",
                        /* editingText= */ null);

        Assert.assertEquals("https://www.abc.xyz", data.displayText);
    }

    @Test
    public void testBuildUrlBarData_nonEmptyDisplayTextWithEmphasis() {
        mLocationBarModel.initializeWithNative();
        doReturn(true).when(mLocationBarModel).shouldEmphasizeUrl();

        var data =
                mLocationBarModel.buildUrlBarData(
                        new GURL("https://www.abc.xyz"),
                        /* isOfflinePage= */ false,
                        /* displayText= */ "Alphabet",
                        /* editingText= */ null);

        Assert.assertEquals("Alphabet", data.displayText);
    }

    @Test
    public void testGetSecurityIconResource_ReadingModePage() {
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        when(mRegularTabMock.getUrl())
                .thenReturn(new GURL(UrlConstants.DISTILLER_SCHEME + "://test"));
        when(mRegularTabMock.isInitialized()).thenReturn(true);
        when(mRegularTabMock.isDestroyed()).thenReturn(false);
        mLocationBarModel.setTab(mRegularTabMock, mRegularProfileMock);

        @DrawableRes
        int drawableRes = mLocationBarModel.getSecurityIconResource(/* isTablet= */ false);
        Assert.assertEquals(R.drawable.ic_reader_mode_24dp, drawableRes);
    }
}
