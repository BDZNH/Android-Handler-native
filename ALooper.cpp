#include "ALooper.h"
#include "ALooperRoster.h"
#include "AMessage.h"
#include "Logger.h"
#include "Thread.h"


namespace android {

ALooperRoster gLooperRoster;

struct ALooper::LooperThread : public Thread {
    LooperThread(ALooper* looper, bool canCallJava)
        : Thread(canCallJava),
        mLooper(looper),
        mThreadId(getThreadId()) {
    }

    virtual status_t readyToRun() {
        mThreadId = getThreadId();

        return Thread::readyToRun();
    }

    virtual bool threadLoop() {
        bool ret= mLooper->loop();
        return ret;
    }

    bool isCurrentThread() const {
        return mThreadId == getThreadId();
    }

    virtual ~LooperThread() {}

private:
    ALooper* mLooper;
    thread_id_t mThreadId;

    DISALLOW_EVIL_CONSTRUCTORS(LooperThread);
};

// static
int64_t ALooper::GetNowUs() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

ALooper::ALooper()
    : mRunningLocally(false) {
    LOGV("constructed {}", fmt::ptr(this));
    // clean up stale AHandlers. Doing it here instead of in the destructor avoids
    // the side effect of objects being deleted from the unregister function recursively.
    gLooperRoster.unregisterStaleHandlers();
}

ALooper::~ALooper() {
    LOGV("destructed {}", fmt::ptr(this));
    
    stop();
    if (mEventQueue.size() > 0) {
        LOGV("destructed with droped {} message", mEventQueue.size());
    }
    // stale AHandlers are now cleaned up in the constructor of the next ALooper to come along
}

void ALooper::setName(const char* name) {
    mName = name;
}

ALooper::handler_id ALooper::registerHandler(const std::shared_ptr<AHandler>& handler) {
    return gLooperRoster.registerHandler(shared_from_this(), handler);
}

void ALooper::unregisterHandler(handler_id handlerID) {
    gLooperRoster.unregisterHandler(handlerID);
}

status_t ALooper::start(
    bool runOnCallingThread, bool canCallJava, int32_t priority) {
    if (runOnCallingThread) {
        {
            MutexAutoLock autoLock(mLock);

            if (mThread != NULL || mRunningLocally) {
                return INVALID_OPERATION;
            }

            mRunningLocally = true;
        }

        do {
        } while (loop());

        return OK;
    }

    MutexAutoLock autoLock(mLock);

    if (mThread != NULL || mRunningLocally) {
        return INVALID_OPERATION;
    }

    mThread = std::make_shared<LooperThread>(this, canCallJava);

    status_t err = mThread->run(
        mName.empty() ? "ALooper" : mName.c_str(), priority);
    if (err != OK) {
        mThread.reset();
    }

    return err;
}

status_t ALooper::stop() {
    std::shared_ptr<LooperThread> thread;
    bool runningLocally;

    {
        MutexAutoLock autoLock(mLock);

        thread = mThread;
        runningLocally = mRunningLocally;
        mThread.reset();
        mRunningLocally = false;
    }

    if (thread == NULL && !runningLocally) {
        return INVALID_OPERATION;
    }

    if (thread != NULL) {
        thread->requestExit();
    }

    mQueueChangedCondition.notify_one();
    {
        MutexAutoLock autoLock(mRepliesLock);
        mRepliesCondition.notify_all();
    }

    if (!runningLocally && !thread->isCurrentThread()) {
        // If not running locally and this thread _is_ the looper thread,
        // the loop() function will return and never be called again.
        thread->requestExitAndWait();
    }

    return OK;
}

void ALooper::post(const std::shared_ptr<AMessage>& msg, int64_t delayUs) {
    MutexAutoLock autoLock(mLock);

    int64_t whenUs;
    if (delayUs > 0) {
        int64_t nowUs = GetNowUs();
        whenUs = (delayUs > INT64_MAX - nowUs ? INT64_MAX : nowUs + delayUs);

    }
    else {
        whenUs = GetNowUs();
    }

    std::vector<Event>::iterator it = mEventQueue.begin();
    while (it != mEventQueue.end() && (*it).mWhenUs <= whenUs) {
        ++it;
    }

    Event event;
    event.mWhenUs = whenUs;
    event.mMessage = msg;

    if (it == mEventQueue.begin()) {
        mQueueChangedCondition.notify_one();
    }

    mEventQueue.insert(it, event);
}

bool ALooper::loop() {
    Event event;
    
    {
        MutexAutoLock autoLock(mLock);
        if (mThread == NULL && !mRunningLocally) {
            return false;
        }
        if (mEventQueue.empty()) {
            mQueueChangedCondition.wait(autoLock);
            return true;
        }
        int64_t whenUs = (*mEventQueue.begin()).mWhenUs;
        int64_t nowUs = GetNowUs();

        if (whenUs > nowUs) {
            int64_t delayUs = whenUs - nowUs;
            if (delayUs > INT64_MAX / 1000) {
                delayUs = INT64_MAX / 1000;
            }
            mQueueChangedCondition.wait_for(autoLock, std::chrono::nanoseconds(delayUs * 1000));

            return true;
        }

        event = *mEventQueue.begin();
        mEventQueue.erase(mEventQueue.begin());
    }

    event.mMessage->deliver();

    // NOTE: It's important to note that at this point our "ALooper" object
    // may no longer exist (its final reference may have gone away while
    // delivering the message). We have made sure, however, that loop()
    // won't be called again.

    return true;
}

// to be called by AMessage::postAndAwaitResponse only
std::shared_ptr<AReplyToken> ALooper::createReplyToken() {
    return std::make_shared<AReplyToken>(shared_from_this());
}

// to be called by AMessage::postAndAwaitResponse only
status_t ALooper::awaitResponse(const std::shared_ptr<AReplyToken>& replyToken, std::shared_ptr<AMessage>* response) {
    // return status in case we want to handle an interrupted wait
    MutexAutoLock autoLock(mRepliesLock);
    if (replyToken == nullptr) {
        LOGE("null replyToken");
        abort();
    }
    while (!replyToken->retrieveReply(response)) {
        {
            MutexAutoLock _l(mLock);
            if (mThread == NULL) {
                return -ENOENT;
            }
        }
        mRepliesCondition.wait(autoLock);
    }
    return OK;
}

status_t ALooper::postReply(const std::shared_ptr<AReplyToken>& replyToken, const std::shared_ptr<AMessage>& reply) {
    MutexAutoLock autoLock(mRepliesLock);
    status_t err = replyToken->setReply(reply);
    if (err == OK) {
        mRepliesCondition.notify_all();
    }
    return err;
}

}  // namespace android