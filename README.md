����һ����AOSP������������ C++ ʵ�ֵ� Handler �⡣
������ʹ��ʾ��(p.s: logger ʹ�õ��� spdlog �����е���ʱ��Ҫ�޸� logger)
```cpp
#include "Logger.h"
#include "ALooper.h"
#include "AHandler.h"
#include "AMessage.h"
using namespace android;

struct SampleClass :public AHandler {
    virtual void onMessageReceived(const std::shared_ptr<AMessage>& msg) {
        LOGI("received msg what = {} msg = {}", msg->what() , fmt::ptr(msg));
        switch (msg->what())
        {
        case 1:
            LOGI("received msg what = 1 msg = {}", fmt::ptr(msg));
            break;
        default:
            LOGE("unknown msg what={}", msg->what());
            break;
        }
    }
};

struct SampleClass2 :public AHandler {
    virtual void onMessageReceived(const std::shared_ptr<AMessage>& msg) {
        LOGI("received msg what = {} msg = {}", msg->what(), fmt::ptr(msg));
        switch (msg->what())
        {
        case 2:
        {
            int32_t arg;
            if (msg->findInt32("arg1", &arg))
            {
                LOGI("received arg1 = {} from msg {}", arg, fmt::ptr(msg));
            }
            if (msg->findInt32("arg1", &arg))
            {
                LOGI("received arg2 = {} from msg {}", arg, fmt::ptr(msg));
            }
            std::string stringmsg;
            if (msg->findString("str1", stringmsg))
            {
                LOGI("received str1 = {} from msg {}", stringmsg, fmt::ptr(msg));
            }
            if (msg->findString("str2", stringmsg))
            {
                LOGI("received str1 = {} from msg {}", stringmsg, fmt::ptr(msg));
            }
            std::shared_ptr<AReplyToken> replyID;
            if (!msg->senderAwaitsResponse(&replyID))
            {
                LOGE("can't find replyToken");
                return;
            }
            std::shared_ptr<AMessage> response = std::make_shared<AMessage>();
            response->setWhat(22);
            response->postReply(replyID);
            LOGD("post reply in receive thread");
        }
        break;
        default:
            LOGE("unknown msg what={}", msg->what());
            break;
        }
    }
};

int main()
{
    printf("start init logger\n");
    initLogger();
    LOGI("started");
    std::shared_ptr<ALooper> Looper = std::make_shared<ALooper>();
    Looper->setName("LocalThread");

    if (Looper->start() != OK) {
        LOGE("start looper failed");
        return 0;
    }

    std::shared_ptr<SampleClass> sample = std::make_shared<SampleClass>();
    std::shared_ptr<SampleClass2> sample2 = std::make_shared<SampleClass2>();
    
    Looper->registerHandler(sample);
    Looper->registerHandler(sample2);
    std::shared_ptr<AMessage> msg = std::make_shared<AMessage>();
    msg->setWhat(1);
    msg->setTarget(std::static_pointer_cast<AHandler>(sample));
    msg->post(1000*1000); //delay 1s

    std::shared_ptr<AMessage> msg2 = std::make_shared<AMessage>(2, std::static_pointer_cast<AHandler>(sample2));
    msg2->setInt32("arg1", 123);
    msg2->setInt32("arg2", 4567);
    msg2->setString("str1", "str1 value");
    msg2->setString("str2", "str2 value");
    std::shared_ptr<AMessage> response;
    status_t ret = msg2->postAndAwaitResponse(&response);
    if ( ret == OK && response != nullptr)
    {
        LOGI("post response success what = {} msg = {}", response->what(), fmt::ptr(response));

    }
    else
    {
        LOGE("post response failed. ret = {}", ret);
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(5));

    Looper->unregisterHandler(sample->id());
    Looper->unregisterHandler(sample2->id());
    return 0;
}
```