#pragma once
#include "Base.h"
#include "ALooper.h"
#include "Logger.h"
namespace android {

    struct AMessage;

    struct AHandler : public std::enable_shared_from_this<AHandler> {
        AHandler()
            : mID(0),
            mVerboseStats(false),
            mMessageCounter(0) {
            LOGV("constructed {}",fmt::ptr(this));
        }

        ~AHandler() {
            LOGV("destructed {}",fmt::ptr(this));
        }

        ALooper::handler_id id() const {
            return mID;
        }

        std::shared_ptr<ALooper> looper() {
            return mLooper.lock();
        }

        std::weak_ptr<ALooper> getLooper() const {
            return mLooper;
        }

        std::weak_ptr<AHandler> getHandler() {
            // allow getting a weak reference to a const handler
            return shared_from_this();
        }

        std::shared_ptr<AMessage> obtainMessage();

    protected:
        virtual void onMessageReceived(const std::shared_ptr<AMessage>& msg) = 0;

    private:
        friend struct AMessage;      // deliverMessage()
        friend struct ALooperRoster; // setID()

        ALooper::handler_id mID;
        std::weak_ptr<ALooper> mLooper;

        inline void setID(ALooper::handler_id id, const std::weak_ptr<ALooper>& looper) {
            mID = id;
            mLooper = looper;
        }

        inline void clear() {
            mID = 0;
            mLooper.reset();
        }

        bool mVerboseStats;
        uint64_t mMessageCounter;
        KeyedI32Vector mMessages;

        void deliverMessage(const std::shared_ptr<AMessage>& msg);

        DISALLOW_EVIL_CONSTRUCTORS(AHandler);
    };

}  // namespace android