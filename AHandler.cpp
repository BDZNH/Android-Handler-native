#include "AHandler.h"
#include "AMessage.h"
namespace android {
    std::shared_ptr<AMessage> AHandler::obtainMessage()
    {
        return std::make_shared<AMessage>(shared_from_this());
    }
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