// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_MODEL_DOWNLOAD_PROGRESS_MANAGER_H_
#define CHROME_BROWSER_AI_AI_MODEL_DOWNLOAD_PROGRESS_MANAGER_H_

#include <memory>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/id_type.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"

namespace on_device_ai {

// Manages a set of `ModelDownloadProgressObserver`s and sends them download
// progress updates for their respective components.
class AIModelDownloadProgressManager {
 public:
  // A component can be implemented to report progress for any resource or
  // operation. When added to `AIModelDownloadProgressManager` via
  // `AddObserver`, it will report its progress updates to the respective
  // `ModelDownloadProgressObserver`.
  class Component {
   public:
    Component();
    virtual ~Component();

    // Move only.
    Component(Component&&);
    Component& operator=(Component&&) = default;

   protected:
    // The implementer calls these when downloaded bytes is changed. Downloaded
    // bytes should only ever monotonically increase.
    void SetDownloadedBytes(int64_t downloaded_bytes);

    // The implementer calls this when total bytes has been determined. Total
    // bytes should never change after its been determined.
    void SetTotalBytes(int64_t total_bytes);

   private:
    friend AIModelDownloadProgressManager;

    using EventCallback = base::RepeatingCallback<void(Component&)>;

    // Only call if `determined_bytes()` is true.
    int64_t downloaded_bytes() const {
      CHECK(determined_bytes());
      return downloaded_bytes_.value();
    }

    int64_t total_bytes() const {
      CHECK(determined_bytes());
      return total_bytes_.value();
    }

    // True if both total and downloaded bytes are determined and they equal
    // each other.
    bool is_complete() const {
      return determined_bytes() &&
             (total_bytes_.value() == downloaded_bytes_.value());
    }

    // Returns true if both total and downloaded bytes are determined.
    bool determined_bytes() const {
      return downloaded_bytes_.has_value() && total_bytes_.has_value();
    }

    // `AIModelDownloadProgressManager` sets the event callback.
    void SetEventCallback(EventCallback event_callback);
    void MaybeRunEventCallback();

    std::optional<int64_t> downloaded_bytes_;
    std::optional<int64_t> total_bytes_;

    // Called anytime `SetDownloadedBytes()` or `SetTotalBytes()` is called.
    EventCallback event_callback_;
  };

  AIModelDownloadProgressManager();
  ~AIModelDownloadProgressManager();

  // Not copyable or movable.
  AIModelDownloadProgressManager(const AIModelDownloadProgressManager&) =
      delete;
  AIModelDownloadProgressManager& operator=(
      const AIModelDownloadProgressManager&) = delete;

  // Adds a `ModelDownloadProgressObserver` to send progress updates for
  // `components`.
  void AddObserver(
      mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
          observer_remote,
      base::flat_set<std::unique_ptr<Component>> components);

  int GetNumberOfReporters();

 private:
  // Observes progress updates from `components`, filters and processes them,
  // and reports the result to `observer_remote`.
  class Reporter {
   public:
    Reporter(AIModelDownloadProgressManager& manager,
             mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
                 observer_remote,
             base::flat_set<std::unique_ptr<Component>> components);
    ~Reporter();

    // Not copyable or movable.
    Reporter(const Reporter&) = delete;
    Reporter& operator=(const Reporter&) = delete;

    void OnEvent(Component& component);

   private:
    void OnRemoteDisconnect();
    void ProcessEvent(const Component& component);
    int64_t GetDownloadedBytes();

    // `manager_` owns `this`.
    base::raw_ref<AIModelDownloadProgressManager> manager_;

    mojo::Remote<blink::mojom::ModelDownloadProgressObserver> observer_remote_;

    // The components we're reporting the progress for.
    base::flat_set<std::unique_ptr<Component>> components_;

    // Map of the components to their observed downloaded bytes. Also serves as
    // a way to keep track of what components we've observed the total bytes of.
    //
    // `raw_ptr` safe since `this` owns the `Component` in `components_` and
    // `components_` and all its members outlive `observed_downloaded_bytes_`.
    std::map<raw_ptr<const Component>, int64_t> observed_downloaded_bytes_;

    // Sum of all observed components' total_bytes.
    int64_t components_total_bytes_ = 0;

    // The bytes already downloaded before we determined the `total_bytes_`.
    int64_t already_downloaded_bytes_ = 0;

    // True if we know the total bytes of the components we'll be watching.
    // Meaning we can start reporting.
    bool ready_to_report_ = false;

    int last_reported_progress_ = 0;
    base::TimeTicks last_progress_time_;

    base::WeakPtrFactory<Reporter> weak_ptr_factory_{this};
  };

  void RemoveReporter(Reporter* reporter);

  base::flat_set<std::unique_ptr<Reporter>, base::UniquePtrComparator>
      reporters_;
};
}  // namespace on_device_ai

#endif  // CHROME_BROWSER_AI_AI_MODEL_DOWNLOAD_PROGRESS_MANAGER_H_
