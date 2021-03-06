// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ProgrammaticScrollAnimator.h"

#include <memory>
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"

namespace blink {

ProgrammaticScrollAnimator::ProgrammaticScrollAnimator(
    ScrollableArea* scrollable_area)
    : scrollable_area_(scrollable_area), start_time_(0.0) {}

ProgrammaticScrollAnimator::~ProgrammaticScrollAnimator() {}

void ProgrammaticScrollAnimator::ResetAnimationState() {
  ScrollAnimatorCompositorCoordinator::ResetAnimationState();
  animation_curve_.reset();
  start_time_ = 0.0;
}

void ProgrammaticScrollAnimator::NotifyOffsetChanged(
    const ScrollOffset& offset) {
  ScrollOffsetChanged(offset, kProgrammaticScroll);
}

void ProgrammaticScrollAnimator::ScrollToOffsetWithoutAnimation(
    const ScrollOffset& offset) {
  CancelAnimation();
  NotifyOffsetChanged(offset);
}

void ProgrammaticScrollAnimator::AnimateToOffset(const ScrollOffset& offset) {
  if (run_state_ == RunState::kPostAnimationCleanup)
    ResetAnimationState();

  start_time_ = 0.0;
  target_offset_ = offset;
  animation_curve_ = CompositorScrollOffsetAnimationCurve::Create(
      CompositorOffsetFromBlinkOffset(target_offset_),
      CompositorScrollOffsetAnimationCurve::kScrollDurationDeltaBased);

  scrollable_area_->RegisterForAnimation();
  if (!scrollable_area_->ScheduleAnimation()) {
    ResetAnimationState();
    NotifyOffsetChanged(offset);
  }
  run_state_ = RunState::kWaitingToSendToCompositor;
}

void ProgrammaticScrollAnimator::CancelAnimation() {
  ASSERT(run_state_ != RunState::kRunningOnCompositorButNeedsUpdate);
  ScrollAnimatorCompositorCoordinator::CancelAnimation();
}

void ProgrammaticScrollAnimator::TickAnimation(double monotonic_time) {
  if (run_state_ != RunState::kRunningOnMainThread)
    return;

  if (!start_time_)
    start_time_ = monotonic_time;
  double elapsed_time = monotonic_time - start_time_;
  bool is_finished = (elapsed_time > animation_curve_->Duration());
  ScrollOffset offset =
      BlinkOffsetFromCompositorOffset(animation_curve_->GetValue(elapsed_time));
  NotifyOffsetChanged(offset);

  if (is_finished) {
    run_state_ = RunState::kPostAnimationCleanup;
  } else if (!scrollable_area_->ScheduleAnimation()) {
    NotifyOffsetChanged(offset);
    ResetAnimationState();
  }
}

void ProgrammaticScrollAnimator::UpdateCompositorAnimations() {
  if (run_state_ == RunState::kPostAnimationCleanup) {
    // No special cleanup, simply reset animation state. We have this state
    // here because the state machine is shared with ScrollAnimator which
    // has to do some cleanup that requires the compositing state to be clean.
    return ResetAnimationState();
  }

  if (compositor_animation_id_ &&
      run_state_ != RunState::kRunningOnCompositor) {
    // If the current run state is WaitingToSendToCompositor but we have a
    // non-zero compositor animation id, there's a currently running
    // compositor animation that needs to be removed here before the new
    // animation is added below.
    ASSERT(run_state_ == RunState::kWaitingToCancelOnCompositor ||
           run_state_ == RunState::kWaitingToSendToCompositor);

    RemoveAnimation();

    compositor_animation_id_ = 0;
    compositor_animation_group_id_ = 0;
    if (run_state_ == RunState::kWaitingToCancelOnCompositor) {
      ResetAnimationState();
      return;
    }
  }

  if (run_state_ == RunState::kWaitingToSendToCompositor) {
    if (!compositor_animation_attached_to_element_id_)
      ReattachCompositorPlayerIfNeeded(
          GetScrollableArea()->GetCompositorAnimationTimeline());

    bool sent_to_compositor = false;

    if (!scrollable_area_->ShouldScrollOnMainThread()) {
      std::unique_ptr<CompositorAnimation> animation =
          CompositorAnimation::Create(
              *animation_curve_, CompositorTargetProperty::SCROLL_OFFSET, 0, 0);

      int animation_id = animation->Id();
      int animation_group_id = animation->Group();

      if (AddAnimation(std::move(animation))) {
        sent_to_compositor = true;
        run_state_ = RunState::kRunningOnCompositor;
        compositor_animation_id_ = animation_id;
        compositor_animation_group_id_ = animation_group_id;
      }
    }

    if (!sent_to_compositor) {
      run_state_ = RunState::kRunningOnMainThread;
      animation_curve_->SetInitialValue(
          CompositorOffsetFromBlinkOffset(scrollable_area_->GetScrollOffset()));
      if (!scrollable_area_->ScheduleAnimation()) {
        NotifyOffsetChanged(target_offset_);
        ResetAnimationState();
      }
    }
  }
}

void ProgrammaticScrollAnimator::LayerForCompositedScrollingDidChange(
    CompositorAnimationTimeline* timeline) {
  ReattachCompositorPlayerIfNeeded(timeline);

  // If the composited scrolling layer is lost during a composited animation,
  // continue the animation on the main thread.
  if (run_state_ == RunState::kRunningOnCompositor &&
      !scrollable_area_->LayerForScrolling()) {
    run_state_ = RunState::kRunningOnMainThread;
    compositor_animation_id_ = 0;
    compositor_animation_group_id_ = 0;
    animation_curve_->SetInitialValue(
        CompositorOffsetFromBlinkOffset(scrollable_area_->GetScrollOffset()));
    scrollable_area_->RegisterForAnimation();
    if (!scrollable_area_->ScheduleAnimation()) {
      ResetAnimationState();
      NotifyOffsetChanged(target_offset_);
    }
  }
}

void ProgrammaticScrollAnimator::NotifyCompositorAnimationFinished(
    int group_id) {
  ASSERT(run_state_ != RunState::kRunningOnCompositorButNeedsUpdate);
  ScrollAnimatorCompositorCoordinator::CompositorAnimationFinished(group_id);
}

DEFINE_TRACE(ProgrammaticScrollAnimator) {
  visitor->Trace(scrollable_area_);
  ScrollAnimatorCompositorCoordinator::Trace(visitor);
}

}  // namespace blink
