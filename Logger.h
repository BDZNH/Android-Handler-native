#pragma once
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include<spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#define LOGV(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOGD(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOGI(...) SPDLOG_INFO(__VA_ARGS__)
#define LOGW(...) SPDLOG_WARN(__VA_ARGS__)
#define LOGE(...) SPDLOG_ERROR(__VA_ARGS__)

//#define ALOGV(fmt,...) printf("V %s:%d " fmt "\n",__FUNCTION__,__LINE__,##__VA_ARGS__)
//#define ALOGD(fmt,...) printf("D %s:%d " fmt "\n",__FUNCTION__,__LINE__,##__VA_ARGS__)
//#define ALOGI(fmt,...) printf("I %s:%d " fmt "\n",__FUNCTION__,__LINE__,##__VA_ARGS__)
//#define ALOGW(fmt,...) printf("W %s:%d " fmt "\n",__FUNCTION__,__LINE__,##__VA_ARGS__)
//#define ALOGE(fmt,...) printf("E %s:%d " fmt "\n",__FUNCTION__,__LINE__,##__VA_ARGS__)

inline void initLogger()
{
    std::shared_ptr<spdlog::logger> stdout_logger = spdlog::stdout_color_mt("console");
    stdout_logger->set_level(spdlog::level::trace);
    stdout_logger->set_pattern("%Y-%m-%d %T.%e %P %-6t %^%L %n: %s %!:%# %v%$");
    spdlog::set_default_logger(stdout_logger);
}