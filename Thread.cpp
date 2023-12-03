#include "Thread.h"
#include "Logger.h"

#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <pthread.h>
#endif

namespace android{

thread_id_t getThreadId(bool valid)
{
    if (valid)
    {
#ifdef _WIN32
        return GetCurrentThreadId();
#else
        return pthread_self();
#endif // _WIN32
    }
    else
    {
        return -1;
    }
}
Thread::Thread(bool canCallJava)
    : mCanCallJava(canCallJava),
    mThread(getThreadId(false)),
    mLock(),
    mStatus(OK),
    mExitPending(false),
    mRunning(false)
{
}

Thread::~Thread()
{
}

status_t Thread::readyToRun()
{
    return OK;
}

status_t Thread::run(const char* name, int32_t priority, size_t stack)
{
    //LOG_ALWAYS_FATAL_IF(name == nullptr, "thread name not provided to Thread::run");
    if (name == nullptr) {
        LOGE("thread name not provided to Thread::run");
    }

    MutexAutoLock _l(mLock);

    if (mRunning) {
        // thread already started
        return INVALID_OPERATION;
    }

    // reset status and exitPending to their default value, so we can
    // try again after an error happened (either below, or in readyToRun())
    mStatus = OK;
    mExitPending = false;
    mThread = getThreadId(false);

    // hold a strong reference on ourself
    mHoldSelf = shared_from_this();

    mRunning = true;

    std::thread* thread = new std::thread(&Thread::_threadLoop, this);
    bool res = thread != nullptr;
    if (res == false) {
        mStatus = UNKNOWN_ERROR;   // something happened!
        mRunning = false;
        mThread = getThreadId(false);
        mHoldSelf.reset();  // "this" may have gone away after this.

        return UNKNOWN_ERROR;
    }

    // Do not refer to mStatus here: The thread is already running (may, in fact
    // already have exited with a valid mStatus result). The OK indication
    // here merely indicates successfully starting the thread and does not
    // imply successful termination/execution.
    return OK;

    // Exiting scope of mLock is a memory barrier and allows new thread to run
}

int Thread::_threadLoop(void* user)
{
    Thread* const self = static_cast<Thread*>(user);

    std::shared_ptr<Thread> strong(self->mHoldSelf);
    std::weak_ptr<Thread> weak(strong);
    self->mHoldSelf.reset();

    bool first = true;

    do {
        bool result;
        if (first) {
            first = false;
            self->mStatus = self->readyToRun();
            result = (self->mStatus == OK);

            if (result && !self->exitPending()) {
                // Binder threads (and maybe others) rely on threadLoop
                // running at least once after a successful ::readyToRun()
                // (unless, of course, the thread has already been asked to exit
                // at that point).
                // This is because threads are essentially used like this:
                //   (new ThreadSubclass())->run();
                // The caller therefore does not retain a strong reference to
                // the thread and the thread would simply disappear after the
                // successful ::readyToRun() call instead of entering the
                // threadLoop at least once.
                result = self->threadLoop();
            }
        }
        else {
            result = self->threadLoop();
        }

        // establish a scope for mLock
        {
            MutexAutoLock _l(self->mLock);
            if (result == false || self->mExitPending) {
                self->mExitPending = true;
                self->mRunning = false;
                // clear thread ID so that requestExitAndWait() does not exit if
                // called by a new thread using the same thread ID as this one.
                self->mThread = getThreadId(false);
                // note that interested observers blocked in requestExitAndWait are
                // awoken by broadcast, but blocked on mLock until break exits scope
                self->mThreadExitedCondition.notify_all();
                break;
            }
        }

        // Release our strong reference, to let a chance to the thread
        // to die a peaceful death.
        strong.reset();
        // And immediately, re-acquire a strong reference for the next loop
        strong = weak.lock();
    } while (strong != nullptr);

    return 0;
}

void Thread::requestExit()
{
    MutexAutoLock _l(mLock);
    mExitPending = true;
}

status_t Thread::requestExitAndWait()
{
    MutexAutoLock _l(mLock);
    if (mThread == getThreadId()) {
        //ALOGW(
        //    "Thread (this=%p): don't call waitForExit() from this "
        //    "Thread object's thread. It's a guaranteed deadlock!",
        //    this);
        LOGE("Thread (this={}): don't call waitForExit() from this "
            "Thread object's thread. It's a guaranteed deadlock!",
            fmt::ptr(this));
        return WOULD_BLOCK;
    }

    mExitPending = true;

    while (mRunning == true) {
        mThreadExitedCondition.wait(_l);
    }
    // This next line is probably not needed any more, but is being left for
    // historical reference. Note that each interested party will clear flag.
    mExitPending = false;

    return mStatus;
}

status_t Thread::join()
{
    MutexAutoLock _l(mLock);
    if (mThread == getThreadId()) {
        LOGW("Thread (this={}): don't call join() from this Thread object's thread. It's a guaranteed deadlock!",
            fmt::ptr(this));
        return WOULD_BLOCK;
    }

    while (mRunning == true) {
        mThreadExitedCondition.wait(_l);
    }

    return mStatus;
}

bool Thread::isRunning() const {
    MutexAutoLock _l(mLock);
    return mRunning;
}

bool Thread::exitPending() const
{
    MutexAutoLock _l(mLock);
    return mExitPending;
}

};  // namespace android