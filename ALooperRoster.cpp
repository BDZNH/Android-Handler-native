#include "ALooperRoster.h"
#include "AHandler.h"
#include "AMessage.h"
#include "Logger.h"
namespace android {

    static bool verboseStats = false;

    ALooperRoster::ALooperRoster()
        : mNextHandlerID(1) {
    }

    ALooper::handler_id ALooperRoster::registerHandler(
        const std::shared_ptr<ALooper>& looper, const std::shared_ptr<AHandler>& handler) {
        MutexAutoLock autoLock(mLock);

        if (handler->id() != 0) {
            //CHECK(!"A handler must only be registered once.");
            LOGE("A handler must only be registered once.");
            return INVALID_OPERATION;
        }

        HandlerInfo info;
        info.mLooper = looper;
        info.mHandler = handler;
        ALooper::handler_id handlerID = mNextHandlerID++;
        mHandlers.emplace(handlerID, info);

        handler->setID(handlerID, looper);

        return handlerID;
    }

    void ALooperRoster::unregisterHandler(ALooper::handler_id handlerID) {
        MutexAutoLock autoLock(mLock);
        auto it = mHandlers.find(handlerID);
        if (it == mHandlers.end()) {
            return;
        }

        const HandlerInfo& info = it->second;
        std::shared_ptr<AHandler> handler = info.mHandler.lock();
        if (handler != nullptr) {
            handler->clear();
        }
        mHandlers.erase(it);
    }

    void ALooperRoster::unregisterStaleHandlers() {

        std::vector<std::shared_ptr<ALooper> > activeLoopers;
        {
            MutexAutoLock autoLock(mLock);
            auto it = mHandlers.begin();
            while (it != mHandlers.end()) {
                const HandlerInfo& info = it->second;
                std::shared_ptr<ALooper> looper = info.mLooper.lock();
                if (looper == nullptr) {
                    it = mHandlers.erase(it);
                }
                else 
                {
                    activeLoopers.emplace_back(looper);
                    it++;
                }
            }
        }
    }

    void ALooperRoster::dump(int fd, const std::vector<std::string>& args) {

    }

}  // namespace android