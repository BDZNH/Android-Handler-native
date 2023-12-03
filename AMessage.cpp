#include "AMessage.h"
#include "Logger.h"
#include "AHandler.h"
#include "ALooperRoster.h"
namespace android {

    extern ALooperRoster gLooperRoster;

    status_t AReplyToken::setReply(const std::shared_ptr<AMessage>& reply) {
        if (mReplied) {
            LOGE("trying to post a duplicate reply");
            return -EBUSY;
        }
        //CHECK(mReply == NULL);
        if (reply == nullptr) {
            abort();
        }
        mReply = reply;
        mReplied = true;
        return OK;
    }

    AMessage::AMessage(void)
        : mWhat(0),
        mTarget(0) {
        LOGD("constructed {}", fmt::ptr(this));
    }

    AMessage::AMessage(uint32_t what, const std::shared_ptr<AHandler>& handler)
        : mWhat(what) {
        setTarget(handler);
        LOGD("constructed {}", fmt::ptr(this));
    }

    AMessage::~AMessage() {
        clear();
        LOGD("destructed {}", fmt::ptr(this));
    }

    void AMessage::setWhat(uint32_t what) {
        mWhat = what;
    }

    uint32_t AMessage::what() const {
        return mWhat;
    }

    void AMessage::setTarget(const std::shared_ptr<AHandler>& handler) {
        if (handler == nullptr) {
            mTarget = 0;
            mHandler.reset();
            mLooper.reset();
        }
        else {
            mTarget = handler->id();
            mHandler = handler->getHandler();
            mLooper = handler->getLooper();
        }
    }

    void AMessage::clear() {
        // todo: fix me
        // Item needs to be handled delicately
        //for (Item& item : mItems) {
        //    delete[] item.mName;
        //    item.mName = NULL;
        //    freeItemValue(&item);
        //}
        //mItems.clear();
    }

    void AMessage::setObject(const char* name, const std::shared_ptr<AReplyToken>& token)
    {
        mTokenName = std::string(name);
        mToken = token;
    }

    bool AMessage::findObject(const char* name, std::shared_ptr<AReplyToken>* obj)
    {
        if (mTokenName == name) {
            *obj = mToken;
            return true;
        }
        return false;
    }

    void AMessage::deliver() {
        std::shared_ptr<AHandler> handler = mHandler.lock();
        if (handler == nullptr) {
            LOGW("failed to deliver message as target handler {} is gone.", mTarget);
            return;
        }

        handler->deliverMessage(shared_from_this());
    }

    status_t AMessage::post(int64_t delayUs) {
        std::shared_ptr<ALooper> looper = mLooper.lock();
        if (looper == nullptr) {
            LOGW("failed to post message as target looper for handler {} is gone.", mTarget);
            return -ENOENT;
        }

        looper->post(shared_from_this(), delayUs);
        return OK;
    }

    status_t AMessage::postAndAwaitResponse(std::shared_ptr<AMessage>* response) {
        std::shared_ptr<ALooper> looper = mLooper.lock();
        if (looper == nullptr) {
            LOGW("failed to post message as target looper for handler {} is gone.", mTarget);
            return -ENOENT;
        }

        std::shared_ptr<AReplyToken> token = looper->createReplyToken();
        if (token == nullptr) {
            LOGE("failed to create reply token");
            return -ENOMEM;
        }
        setObject("replyID", token);

        looper->post(shared_from_this(), 0 /* delayUs */);
        return looper->awaitResponse(token, response);
    }

    status_t AMessage::postReply(const std::shared_ptr<AReplyToken>& replyToken) {
        if (replyToken == nullptr) {
            LOGW("failed to post reply to a NULL token");
            return -ENOENT;
        }
        std::shared_ptr<ALooper> looper = replyToken->getLooper();
        if (looper == nullptr) {
            LOGW("failed to post reply as target looper is gone.");
            return -ENOENT;
        }
        return looper->postReply(replyToken, shared_from_this());
    }

    bool AMessage::senderAwaitsResponse(std::shared_ptr<AReplyToken>* replyToken) {

        std::shared_ptr<AReplyToken> tmp;
        bool found = findObject("replyID", &tmp);

        if (!found) {
            return false;
        }

        *replyToken = mToken;
        tmp.reset();
        mToken.reset();
        mTokenName.clear();
        return *replyToken != nullptr;
    }
}