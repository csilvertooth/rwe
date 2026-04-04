#pragma once

#include <cassert>
#include <memory>
#include <rwe/util/UniqueHandle.h>

namespace rwe
{
    template <typename Value, typename Deleter>
    class SharedHandle
    {
    public:
        using Type = SharedHandle<Value, Deleter>;

    private:
        Value handle;
        std::shared_ptr<unsigned int> referenceCount;

    public:
        SharedHandle() : handle(), referenceCount(nullptr) {}

        explicit SharedHandle(Value handle) : handle(handle), referenceCount(std::make_shared<unsigned int>(1)) {}

        SharedHandle(const Type& other) = default;
        SharedHandle& operator=(const Type& other)
        {
            if (this != &other)
            {
                destroy();
                handle = other.handle;
                referenceCount = other.referenceCount;
            }
            return *this;
        }

        SharedHandle(Type&& other) noexcept : handle(other.handle), referenceCount(std::move(other.referenceCount))
        {
        }

        SharedHandle& operator=(Type&& other) noexcept
        {
            if (this != &other)
            {
                destroy();
                handle = other.handle;
                referenceCount = std::move(other.referenceCount);
            }
            return *this;
        }

        ~SharedHandle()
        {
            destroy();
        }

        explicit SharedHandle(UniqueHandle<Value, Deleter>&& other) noexcept : SharedHandle(other.release())
        {
        }

        bool operator==(const Type& rhs) const
        {
            return referenceCount == rhs.referenceCount;
        }

        bool operator!=(const Type& rhs) const
        {
            return !(rhs == *this);
        }

        /**
         * Equivalent to isValid()
         */
        explicit operator bool() const
        {
            return isValid();
        }

        /** Returns the underlying texture handle. */
        Value get() const
        {
            assert(isValid());
            return handle;
        }

        /**
         * Returns true if the handle contains a valid resource, otherwise false.
         */
        bool isValid() const
        {
            return referenceCount != nullptr;
        }

        unsigned int useCount() const
        {
            return referenceCount ? referenceCount.use_count() : 0;
        }

        /** Replaces the contents of the handle with the given value. */
        void reset(Value newValue)
        {
            destroy();
            handle = newValue;
            referenceCount = std::make_shared<unsigned int>(1);
        }

        /** Resets the handle to the empty state. */
        void reset()
        {
            destroy();
            referenceCount = nullptr;
        }

    private:
        void destroy()
        {
            if (referenceCount && referenceCount.use_count() == 1)
            {
                Deleter()(handle);
            }
            referenceCount.reset();
        }
    };
}
