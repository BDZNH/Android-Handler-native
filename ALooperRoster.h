#pragma once

#include "ALooper.h"
#include "Base.h"

namespace android {

    struct ALooperRoster {
        ALooperRoster();

        ALooper::handler_id registerHandler(
            const std::shared_ptr<ALooper>& looper, const std::shared_ptr<AHandler>& handler);

        void unregisterHandler(ALooper::handler_id handlerID);

        void unregisterStaleHandlers();

        void dump(int fd, const std::vector<std::string>& args);

    private:
        struct HandlerInfo {
            std::weak_ptr<ALooper> mLooper;
            std::weak_ptr<AHandler> mHandler;
        };

        std::mutex mLock;
        std::unordered_map<ALooper::handler_id, HandlerInfo> mHandlers;
        ALooper::handler_id mNextHandlerID;

        DISALLOW_EVIL_CONSTRUCTORS(ALooperRoster);
    };

}  // namespace android

