#include "private.h"

#include <library/cpp/yt/cpu_clock/clock.h>

namespace NYT::NJobProxy {

////////////////////////////////////////////////////////////////////////////////

TFirstBatchTimeTrackingBase::TFirstBatchTimeTrackingBase(TInstant startTime)
    : StartTime_(startTime)
{ }

std::optional<TDuration> TFirstBatchTimeTrackingBase::GetTimeToFirstBatch() const
{
    if (auto firstBatchTime = FirstBatchTime_.load()) {
        YT_VERIFY(firstBatchTime >= StartTime_);
        return firstBatchTime - StartTime_;
    }

    return std::nullopt;
}

void TFirstBatchTimeTrackingBase::TryUpdateFirstBatchTime()
{
    // Fast path.
    if (FirstBatchTime_.load(std::memory_order::relaxed)) {
        return;
    }

    // Slow path.
    TInstant zero = TInstant::Zero();
    auto now = GetInstant();
    // Try to set FirstBatchTime_.
    // If FirstBatchTime_ appeared not to be equal to zero then
    // we are not the first batch and should do nothing.
    FirstBatchTime_.compare_exchange_strong(zero, now);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NJobProxy
