// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Holder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

import java.util.List;

/** Unit tests for {@link TabListMergeAnimationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class TabListMergeAnimationManagerUnitTest {
    private static final int FULL_HEIGHT = 100;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListRecyclerView mRecyclerView;
    @Mock private Runnable mOnAnimationEndRunnable;
    @Mock private View mTargetView;
    @Mock private View mOtherView;

    private ViewHolder mTargetViewHolder;
    private ViewHolder mOtherViewHolder;
    private TabListMergeAnimationManager mAnimationManager;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        when(mRecyclerView.getContext()).thenReturn(context);

        mTargetViewHolder = new ViewHolder(mTargetView, (a, b, c) -> {});
        mOtherViewHolder = new ViewHolder(mOtherView, (a, b, c) -> {});

        doCallback(
                        0,
                        runnable -> {
                            assert runnable instanceof Runnable;
                            ((Runnable) runnable).run();
                        })
                .when(mRecyclerView)
                .post(any());
        mAnimationManager = new TabListMergeAnimationManager(mRecyclerView);
    }

    private void mockTargetVisibility(boolean isVisible) {
        when(mRecyclerView.findViewHolderForAdapterPosition(0)).thenReturn(mTargetViewHolder);
        when(mTargetView.isShown()).thenReturn(isVisible);
        when(mTargetView.getMeasuredHeight()).thenReturn(FULL_HEIGHT);
        doCallback(
                        0,
                        item -> {
                            assert item instanceof Rect;
                            Rect rect = (Rect) item;
                            if (isVisible) {
                                rect.set(0, 0, 50, FULL_HEIGHT);
                            } else {
                                rect.set(0, 0, 50, FULL_HEIGHT - 1);
                            }
                        })
                .when(mTargetView)
                .getGlobalVisibleRect(any());
    }

    @Test
    public void testPlayAnimation_whenTargetIsVisible() {
        mockTargetVisibility(true);
        when(mRecyclerView.findViewHolderForAdapterPosition(1)).thenReturn(mOtherViewHolder);

        mAnimationManager.playAnimation(0, List.of(0, 1), mOnAnimationEndRunnable);
        ShadowLooper.runUiThreadTasks();

        verify(mRecyclerView).setBlockTouchInput(true);
        verify(mRecyclerView).setSmoothScrolling(true);
        verify(mRecyclerView, never()).smoothScrollToPosition(anyInt());

        verify(mRecyclerView).setBlockTouchInput(false);
        verify(mRecyclerView).setSmoothScrolling(false);
        verify(mOnAnimationEndRunnable).run();
    }

    @Test
    public void testPlayAnimation_whenTargetIsNotVisible() {
        mockTargetVisibility(false);

        Holder<@Nullable OnScrollListener> listener = new Holder<>(null);
        doCallback(0, listener).when(mRecyclerView).addOnScrollListener(any());

        mAnimationManager.playAnimation(0, List.of(0, 1), mOnAnimationEndRunnable);
        ShadowLooper.runUiThreadTasks();

        verify(mRecyclerView).setBlockTouchInput(true);
        verify(mRecyclerView).setSmoothScrolling(true);
        verify(mRecyclerView).smoothScrollToPosition(0);

        mockTargetVisibility(true);
        listener.get().onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_IDLE);
        ShadowLooper.runUiThreadTasks();

        verify(mRecyclerView).removeOnScrollListener(listener.get());
        verify(mRecyclerView).setBlockTouchInput(false);
        verify(mRecyclerView).setSmoothScrolling(false);
        verify(mOnAnimationEndRunnable).run();
    }

    @Test
    public void testPlayAnimation_whenAlreadyAnimating() {
        mockTargetVisibility(true);
        mAnimationManager.playAnimation(0, List.of(0, 1), mOnAnimationEndRunnable);
        mAnimationManager.playAnimation(0, List.of(0, 1), mock());
        ShadowLooper.runUiThreadTasks();

        verify(mRecyclerView, times(1)).setBlockTouchInput(true);
        verify(mOnAnimationEndRunnable, times(1)).run();
    }

    @Test
    public void testAnimationCleanup() {
        mockTargetVisibility(true);
        when(mRecyclerView.findViewHolderForAdapterPosition(1)).thenReturn(mOtherViewHolder);

        PropertyModel otherModel = new PropertyModel(TabProperties.IS_HIGHLIGHTED);
        otherModel.set(TabProperties.IS_HIGHLIGHTED, true);
        mOtherViewHolder.model = otherModel;

        mAnimationManager.playAnimation(0, List.of(1), mOnAnimationEndRunnable);
        ShadowLooper.runUiThreadTasks();

        verify(mOtherView, atLeast(1)).setTranslationX(0f);
        verify(mOtherView, atLeast(1)).setTranslationY(0f);
        assertFalse(otherModel.get(TabProperties.IS_HIGHLIGHTED));

        verify(mRecyclerView).setBlockTouchInput(false);
        verify(mRecyclerView).setSmoothScrolling(false);
        verify(mOnAnimationEndRunnable).run();
    }

    @Test
    public void testPlayAnimation_nullTargetViewHolder() {
        mockTargetVisibility(true);

        when(mRecyclerView.findViewHolderForAdapterPosition(0)).thenReturn(null);
        Holder<@Nullable OnScrollListener> listener = new Holder<>(null);
        doCallback(0, listener).when(mRecyclerView).addOnScrollListener(any());

        mAnimationManager.playAnimation(0, List.of(0, 1), mOnAnimationEndRunnable);
        listener.get().onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_IDLE);
        ShadowLooper.runUiThreadTasks();

        verify(mRecyclerView).setBlockTouchInput(false);
        verify(mRecyclerView).setSmoothScrolling(false);
        verify(mOnAnimationEndRunnable).run();
        verify(mTargetView, never()).getX();
    }
}
