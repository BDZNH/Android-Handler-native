#include "AHandler.h"
#include "AMessage.h"
namespace android {

    void AHandler::deliverMessage(const std::shared_ptr<AMessage>& msg) {
        onMessageReceived(msg);
        mMessageCounter++;

        if (mVerboseStats) {
            uint32_t what = msg->what();
            auto it = mMessages.find(what);
            if (it == mMessages.end())
            {
                mMessages.emplace(what, 1);
            }
            else
            {
                it->second += 1;
            }
        }
    }

}  // namespace android