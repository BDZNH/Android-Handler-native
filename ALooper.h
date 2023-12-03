#pragma once
#include <stdint.h>

#include "Errors.h"
#include "Base.h"
namespace android {

    struct AHandler;
    struct AMessage;
    struct AReplyToken;

    struct ALooper : public std::enable_shared_from_this<ALooper> {
        typedef int32_t event_id;
        typedef int32_t handler_id;

        ALooper();

        // Takes effect in a subsequent call to start().
        void setName(const char* name);

        handler_id registerHandler(const std::shared_ptr<AHandler>& handler);
        void unregisterHandler(handler_id handlerID);

        status_t start(
            bool runOnCallingThread = false,
            bool canCallJava = false,
            int32_t priority = 0
        );

        status_t stop();

        static int64_t GetNowUs();

        const char* getName() const {
            return mName.c_str();
        }

        virtual ~ALooper();

    private:
        friend struct AMessage;       // post()

        struct Event {
            int64_t mWhenUs;
            std::shared_ptr<AMessage> mMessage;
        };

        std::mutex mLock;
        std::condition_variable mQueueChangedCondition;

        std::string mName;

        std::vector<Event> mEventQueue;

        struct LooperThread;
        std::shared_ptr<LooperThread> mThread;
        bool mRunningLocally;

        // use a separate lock for reply handling, as it is always on another thread
        // use a central lock, however, to avoid creating a mutex for each reply
        std::mutex mRepliesLock;
        std::condition_variable mRepliesCondition;

        // START --- methods used only by AMessage

        // posts a message on this looper with the given timeout
        void post(const std::shared_ptr<AMessage>& msg, int64_t delayUs);

        // creates a reply token to be used with this looper
        std::shared_ptr<AReplyToken> createReplyToken();
        // waits for a response for the reply token.  If status is OK, the response
        // is stored into the supplied variable.  Otherwise, it is unchanged.
        status_t awaitResponse(const std::shared_ptr<AReplyToken>& replyToken, std::shared_ptr<AMessage>* response);
        // posts a reply for a reply token.  If the reply could be successfully posted,
        // it returns OK. Otherwise, it returns an error value.
        status_t postReply(const std::shared_ptr<AReplyToken>& replyToken, const std::shared_ptr<AMessage>& msg);

        // END --- methods used only by AMessage

        bool loop();

        DISALLOW_EVIL_CONSTRUCTORS(ALooper);
    };

} // namespace android