#pragma once

#include "public.h"

#include <yt/yt/core/actions/future.h>

#include <library/cpp/yt/memory/ref.h>

namespace NYT::NNbd {

////////////////////////////////////////////////////////////////////////////////

struct TReadOptions
{
    ui64 Cookie = 0;
};

////////////////////////////////////////////////////////////////////////////////

struct TWriteOptions
{
    //! The 'FUA (Force Unit Access) flag'.
    bool Flush = false;
    ui64 Cookie = 0;
};

//! Represents a block device that can be exposed via the NBD protocol.
struct IBlockDevice
    : public virtual TRefCounted
{
    virtual i64 GetTotalSize() const = 0;
    virtual bool IsReadOnly() const = 0;
    virtual TString DebugString() const = 0;
    virtual TString GetProfileSensorTag() const = 0;

    virtual TFuture<void> Initialize() = 0;
    virtual TFuture<void> Finalize() = 0;

    virtual TFuture<TSharedRef> Read(
        i64 offset,
        i64 length,
        const TReadOptions& options = {}) = 0;

    virtual TFuture<void> Write(
        i64 offset,
        const TSharedRef& data,
        const TWriteOptions& options = {}) = 0;

    virtual TFuture<void> Flush() = 0;

    virtual void SetError(TError error) = 0;
    virtual const TError& GetError() const = 0;
};

DEFINE_REFCOUNTED_TYPE(IBlockDevice)

////////////////////////////////////////////////////////////////////////////////

class TBaseBlockDevice
    : public IBlockDevice
{
public:
    void SetError(TError error) override
    {
        Error_ = std::move(error);
    }

    const TError& GetError() const override
    {
        return Error_;
    }

private:
    TError Error_;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NNbd
