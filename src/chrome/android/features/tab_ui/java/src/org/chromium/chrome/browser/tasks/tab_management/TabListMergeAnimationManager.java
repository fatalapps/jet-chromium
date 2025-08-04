// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.animation.AnimationListeners.onAnimationEnd;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.LinearSmoothScroller;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.List;

@NullMarked
public class TabListMergeAnimationManager {
    private static final long MERGE_ANIMATION_DURATION_MS = 300L;
    private final AnimationHandler mMergeAnimationHandler = new AnimationHandler();
    private final TabListRecyclerView mRecyclerView;
    private boolean mIsAnimating;

    /**
     * Plays an animation where all visible tab cards fade out and move towards a target tab card.
     *
     * @param recyclerView The {@link TabListRecyclerView} to run the animations on.
     */
    public TabListMergeAnimationManager(TabListRecyclerView recyclerView) {
        mRecyclerView = recyclerView;
    }

    /**
     * Plays an animation where all visible tab cards fade out and move towards a target tab card.
     *
     * @param targetIndex The model index of the tab card to focus on.
     * @param visibleTabIndexes A list of model indexes for all currently visible tab cards.
     * @param onAnimationEnd A {@link Runnable} to execute when the animation is complete.
     */
    public void playAnimation(
            int targetIndex, List<Integer> visibleTabIndexes, Runnable onAnimationEnd) {
        if (mIsAnimating) return;
        mIsAnimating = true;
        mRecyclerView.setBlockTouchInput(true);
        mRecyclerView.setSmoothScrolling(true);

        boolean isTargetVisible = isTargetFullyVisible(targetIndex);

        // If the target is already visible, don't scroll.
        if (isTargetVisible) {
            postAnimationToRecyclerView(targetIndex, visibleTabIndexes, onAnimationEnd);
            return;
        }

        // If the target is not visible, scroll to it before running the animation.
        smoothScrollAndAnimate(targetIndex, visibleTabIndexes, onAnimationEnd);
    }

    private void smoothScrollAndAnimate(
            int targetIndex, List<Integer> visibleTabIndexes, Runnable onAnimationEnd) {
        LinearSmoothScroller smoothScroller =
                new LinearSmoothScroller(mRecyclerView.getContext()) {
                    @Override
                    protected int getVerticalSnapPreference() {
                        return LinearSmoothScroller.SNAP_TO_START;
                    }
                };
        smoothScroller.setTargetPosition(targetIndex);
        mRecyclerView.smoothScrollToPosition(targetIndex);
        mRecyclerView.addOnScrollListener(
                new OnScrollListener() {
                    @Override
                    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                        super.onScrollStateChanged(recyclerView, newState);
                        onStateChanged(
                                this, newState, targetIndex, visibleTabIndexes, onAnimationEnd);
                    }
                });
    }

    private boolean isTargetFullyVisible(int targetIndex) {
        RecyclerView.ViewHolder viewHolder =
                mRecyclerView.findViewHolderForAdapterPosition(targetIndex);
        if (viewHolder == null) return false;

        View view = viewHolder.itemView;
        if (!view.isShown()) return false;

        Rect actualPosition = new Rect();
        view.getGlobalVisibleRect(actualPosition);

        return view.getMeasuredHeight() == actualPosition.height();
    }

    private void onStateChanged(
            OnScrollListener listener,
            int newState,
            int targetIndex,
            List<Integer> visibleTabIndexes,
            Runnable onAnimationEnd) {
        if (newState == RecyclerView.SCROLL_STATE_IDLE) {
            mRecyclerView.removeOnScrollListener(listener);
            postAnimationToRecyclerView(targetIndex, visibleTabIndexes, onAnimationEnd);
        }
    }

    private void postAnimationToRecyclerView(
            int targetIndex, List<Integer> visibleTabIndexes, Runnable onAnimationEnd) {
        mRecyclerView.post(
                () ->
                        animateMergeWith(
                                targetIndex, visibleTabIndexes, mRecyclerView, onAnimationEnd));
    }

    private void animateMergeWith(
            int targetIndex,
            List<Integer> visibleTabIndexes,
            TabListRecyclerView recyclerView,
            Runnable onAnimationEnd) {
        RecyclerView.ViewHolder targetViewHolder =
                recyclerView.findViewHolderForAdapterPosition(targetIndex);
        if (targetViewHolder == null) {
            cleanUp(onAnimationEnd);
            return;
        }
        Animator animation =
                buildMergeAnimation(
                        visibleTabIndexes, recyclerView, onAnimationEnd, targetViewHolder);
        mMergeAnimationHandler.startAnimation(animation);
    }

    private Animator buildMergeAnimation(
            List<Integer> visibleTabIndexes,
            TabListRecyclerView recyclerView,
            Runnable onAnimationEnd,
            RecyclerView.ViewHolder targetViewHolder) {
        View targetView = targetViewHolder.itemView;
        AnimatorSet animatorSet = new AnimatorSet();
        List<Animator> animators = new ArrayList<>();

        for (int index : visibleTabIndexes) {
            RecyclerView.ViewHolder viewHolder =
                    recyclerView.findViewHolderForAdapterPosition(index);
            if (viewHolder == null) continue;
            View view = viewHolder.itemView;

            animators.add(ObjectAnimator.ofFloat(view, View.ALPHA, 1f, 0f));

            float targetTranslationX = targetView.getX() - view.getX();
            float targetTranslationY = targetView.getY() - view.getY();
            animators.add(ObjectAnimator.ofFloat(view, View.TRANSLATION_X, 0f, targetTranslationX));
            animators.add(ObjectAnimator.ofFloat(view, View.TRANSLATION_Y, 0f, targetTranslationY));
        }

        animatorSet.playTogether(animators);
        animatorSet.setDuration(MERGE_ANIMATION_DURATION_MS);
        animatorSet.setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR);

        animatorSet.addListener(
                onAnimationEnd(
                        animator -> {
                            for (int index : visibleTabIndexes) {
                                cleanCardState(index);
                            }
                            cleanUp(onAnimationEnd);
                        }));
        return animatorSet;
    }

    private void cleanCardState(int index) {
        SimpleRecyclerViewAdapter.ViewHolder viewHolder =
                (SimpleRecyclerViewAdapter.ViewHolder)
                        mRecyclerView.findViewHolderForAdapterPosition(index);
        if (viewHolder == null) return;
        View view = viewHolder.itemView;

        view.setTranslationX(0f);
        view.setTranslationY(0f);

        PropertyModel model = viewHolder.model;
        if (model != null && model.containsKeyEqualTo(IS_HIGHLIGHTED, true)) {
            model.set(IS_HIGHLIGHTED, false);
        }
    }

    private void cleanUp(Runnable onAnimationEnd) {
        mRecyclerView.setBlockTouchInput(false);
        mRecyclerView.setSmoothScrolling(false);
        onAnimationEnd.run();
        mIsAnimating = false;
    }
}
