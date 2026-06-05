#include "debug_agent.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

#ifndef APP_DEBUG_AGENT
#define APP_DEBUG_AGENT 0
#endif

#if APP_DEBUG_AGENT

void dbg_log(const char *hypothesisId, const char *location, const char *message,
             int32_t a, int32_t b, int32_t c)
{
    Serial.printf(
        "DBG871466:{\"sessionId\":\"871466\",\"hypothesisId\":\"%s\",\"location\":\"%s\","
        "\"message\":\"%s\",\"data\":{\"a\":%ld,\"b\":%ld,\"c\":%ld},\"timestamp\":%lu}\n",
        hypothesisId ? hypothesisId : "?",
        location ? location : "?",
        message ? message : "?",
        (long)a, (long)b, (long)c,
        (unsigned long)millis());
}

void dbg_log_heap(const char *hypothesisId, const char *location, const char *tag)
{
    dbg_log(hypothesisId, location, tag,
            (int32_t)ESP.getFreeHeap(),
            (int32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            (int32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

#else

void dbg_log(const char *hypothesisId, const char *location, const char *message,
             int32_t a, int32_t b, int32_t c)
{
    (void)hypothesisId;
    (void)location;
    (void)message;
    (void)a;
    (void)b;
    (void)c;
}

void dbg_log_heap(const char *hypothesisId, const char *location, const char *tag)
{
    (void)hypothesisId;
    (void)location;
    (void)tag;
}

#endif
