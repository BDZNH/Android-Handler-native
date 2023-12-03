#pragma once
#include "Base.h"
#include "ALooper.h"
#include "Logger.h"
namespace android {

    struct AHandler;

    struct AReplyToken : public std::enable_shared_from_this<AReplyToken> {
        explicit AReplyToken(const std::shared_ptr<ALooper>& looper)
            : mLooper(looper),
            mReplied(false) {
            LOGD("constructed {}", fmt::ptr(this));
        }
        ~AReplyToken() {
            LOGD("destructed {}", fmt::ptr(this));
        }

    private:
        friend struct AMessage;
        friend struct ALooper;
        std::weak_ptr<ALooper> mLooper;
        std::shared_ptr<AMessage> mReply;
        bool mReplied;

        std::shared_ptr<ALooper> getLooper() const {
            return mLooper.lock();
        }
        // if reply is not set, returns false; otherwise, it retrieves the reply and returns true
        bool retrieveReply(std::shared_ptr<AMessage>* reply) {
            if (reply == nullptr) {
                LOGE("reply is null pointer");
                abort();
            }
            if (mReplied) {
                *reply = mReply;
                mReply = nullptr;
            }
            return mReplied;
        }
        // sets the reply for this token. returns OK or error
        status_t setReply(const std::shared_ptr<AMessage>& reply);
    };

    struct AMessage : std::enable_shared_from_this<AMessage> {
    public:
        AMessage();
        AMessage(uint32_t what, const std::shared_ptr<AHandler>& handler);
        void setWhat(uint32_t what);
        uint32_t what() const;

        void setTarget(const std::shared_ptr<AHandler>& handler);

        status_t post(int64_t delayUs = 0);

        // Posts the message to its target and waits for a response (or error)
        // before returning.
        status_t postAndAwaitResponse(std::shared_ptr<AMessage>* response);

        // If this returns true, the sender of this message is synchronously
        // awaiting a response and the reply token is consumed from the message
        // and stored into replyID. The reply token must be used to send the response
        // using "postReply" below.
        bool senderAwaitsResponse(std::shared_ptr<AReplyToken>* replyID);

        // Posts the message as a response to a reply token.  A reply token can
        // only be used once. Returns OK if the response could be posted; otherwise,
        // an error.
        status_t postReply(const std::shared_ptr<AReplyToken>& replyID);

        // Performs a deep-copy of "this", contained messages are in turn "dup'ed".
        // Warning: RefBase items, i.e. "objects" are _not_ copied but only have
        // their refcount incremented.
        std::shared_ptr<AMessage> dup() const;

        // removes all items
        void clear();

        virtual ~AMessage();

    private:
        friend struct ALooper; // deliver()

        uint32_t mWhat;

        // used only for debugging
        ALooper::handler_id mTarget;

        std::weak_ptr<AHandler> mHandler;
        std::weak_ptr<ALooper> mLooper;

        std::shared_ptr<AReplyToken> mToken = nullptr;
        std::string mTokenName;

        void setObject(const char* name, const std::shared_ptr<AReplyToken>& token);
        bool findObject(const char* name, std::shared_ptr<AReplyToken>* obj);

        void deliver();

        DISALLOW_EVIL_CONSTRUCTORS(AMessage);
    };
}


