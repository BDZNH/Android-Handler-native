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
    LOGV("constructed {}", fmt::ptr(this));
}

AMessage::AMessage(uint32_t what, const std::shared_ptr<AHandler>& handler)
    : mWhat(what) {
    setTarget(handler);
    LOGV("constructed {}", fmt::ptr(this));
}

AMessage::AMessage(const std::shared_ptr<AHandler>& handler)
{
    setTarget(handler);
    LOGV("constructed {}", fmt::ptr(this));
}

AMessage::~AMessage() {
    clear();
    LOGV("destructed {}", fmt::ptr(this));
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
    setReplyToken("replyID", token);

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
    bool found = findReplyToken("replyID", &tmp);

    if (!found) {
        return false;
    }

    *replyToken = mToken;
    tmp.reset();
    mToken.reset();
    mTokenName.clear();
    return *replyToken != nullptr;
}

void AMessage::clear() {
     //Item needs to be handled delicately
    for (Item& item : mItems) {
        delete[] item.mName;
        item.mName = NULL;
        freeItemValue(&item);
    }
    mItems.clear();
}

void AMessage::freeItemValue(Item* item) {
    switch (item->mType) {
    case kTypeString:
    {
        delete item->u.stringValue;
        break;
    }
    default:
        break;
    }
    item->mType = kTypeInt32; // clear type
}

#define BASIC_TYPE(NAME,FIELDNAME,TYPENAME)                             \
void AMessage::set##NAME(const char *name, TYPENAME value) {            \
Item *item = allocateItem(name);                                    \
if (item) {                                                         \
    item->mType = kType##NAME;                                      \
    item->u.FIELDNAME = value;                                      \
}                                                                   \
}                                                                       \
                                                                    \
/* NOLINT added to avoid incorrect warning/fix from clang.tidy */       \
bool AMessage::find##NAME(const char *name, TYPENAME *value) const {  /* NOLINT */ \
const Item *item = findItem(name, kType##NAME);                     \
if (item) {                                                         \
    *value = item->u.FIELDNAME;                                     \
    return true;                                                    \
}                                                                   \
return false;                                                       \
}

BASIC_TYPE(Int32, int32Value, int32_t)
BASIC_TYPE(Int64, int64Value, int64_t)
BASIC_TYPE(Size, sizeValue, size_t)
BASIC_TYPE(Float, floatValue, float)
BASIC_TYPE(Double, doubleValue, double)
BASIC_TYPE(Pointer, ptrValue, void*)

#undef BASIC_TYPE

void AMessage::setString(
        const char* name, const char* s, ssize_t len) {
    Item* item = allocateItem(name);
    if (item) {
        item->mType = kTypeString;
        item->u.stringValue = new std::string(s, len < 0 ? strlen(s) : len);
    }
}

void AMessage::setString(
    const char* name, const std::string& s) {
    setString(name, s.c_str(), s.size());
}

bool AMessage::contains(const char* name) const {
    size_t i = findItemIndex(name, strlen(name));
    return i < mItems.size();
}

bool AMessage::findString(const char* name, std::string& value) const {
    const Item* item = findItem(name, kTypeString);
    if (item) {
        value = *item->u.stringValue;
        return true;
    }
    return false;
}

bool AMessage::findAsFloat(const char* name, float* value) const {
    size_t i = findItemIndex(name, strlen(name));
    if (i < mItems.size()) {
        const Item* item = &mItems[i];
        switch (item->mType) {
        case kTypeFloat:
            *value = item->u.floatValue;
            return true;
        case kTypeDouble:
            *value = (float)item->u.doubleValue;
            return true;
        case kTypeInt64:
            *value = (float)item->u.int64Value;
            return true;
        case kTypeInt32:
            *value = (float)item->u.int32Value;
            return true;
        case kTypeSize:
            *value = (float)item->u.sizeValue;
            return true;
        default:
            return false;
        }
    }
    return false;
}

bool AMessage::findAsInt64(const char* name, int64_t* value) const {
    size_t i = findItemIndex(name, strlen(name));
    if (i < mItems.size()) {
        const Item* item = &mItems[i];
        switch (item->mType) {
        case kTypeInt64:
            *value = item->u.int64Value;
            return true;
        case kTypeInt32:
            *value = item->u.int32Value;
            return true;
        default:
            return false;
        }
    }
    return false;
}

bool AMessage::findRect(
    const char* name,
    int32_t* left, int32_t* top, int32_t* right, int32_t* bottom) const {
    const Item* item = findItem(name, kTypeRect);
    if (item == NULL) {
        return false;
    }

    *left = item->u.rectValue.mLeft;
    *top = item->u.rectValue.mTop;
    *right = item->u.rectValue.mRight;
    *bottom = item->u.rectValue.mBottom;

    return true;
}

// assumes item's name was uninitialized or NULL
void AMessage::Item::setName(const char* name, size_t len) {
    mNameLength = len;
    mName = new char[len + 1];
    memcpy((void*)mName, name, len + 1);
}

AMessage::Item::Item(const char* name, size_t len)
    : mType(kTypeInt32) {
    // mName and mNameLength are initialized by setName
    setName(name, len);
}

AMessage::Item* AMessage::allocateItem(const char* name) {
    size_t len = strlen(name);
    size_t i = findItemIndex(name, len);
    Item* item;

    if (i < mItems.size()) {
        item = &mItems[i];
        freeItemValue(item);
    }
    else {
        //CHECK(mItems.size() < kMaxNumItems);
        if (mItems.size() > kMaxNumItems) {
            //too big items
            abort();
        }
        i = mItems.size();
        // place a 'blank' item at the end - this is of type kTypeInt32
        mItems.emplace_back(name, len);
        item = &mItems[i];
    }

    return item;
}

const AMessage::Item* AMessage::findItem(
    const char* name, Type type) const {
    size_t i = findItemIndex(name, strlen(name));
    if (i < mItems.size()) {
        const Item* item = &mItems[i];
        return item->mType == type ? item : NULL;

    }
    return NULL;
}

inline size_t AMessage::findItemIndex(const char* name, size_t len) const {
    size_t i = 0;
    for (; i < mItems.size(); i++) {
        if (len != mItems[i].mNameLength) {
            continue;
        }
        if (!memcmp(mItems[i].mName, name, len)) {
            break;
        }
    }
    return i;
}

void AMessage::setReplyToken(const char* name, const std::shared_ptr<AReplyToken>& token)
{
    mTokenName = std::string(name);
    mToken = token;
    }

bool AMessage::findReplyToken(const char* name, std::shared_ptr<AReplyToken>* obj)
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

}