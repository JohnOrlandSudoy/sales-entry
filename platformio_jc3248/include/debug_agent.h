#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Serial NDJSON line prefixed DBG871466: for debug session 871466 */
void dbg_log(const char *hypothesisId, const char *location, const char *message,
             int32_t a, int32_t b, int32_t c);
void dbg_log_heap(const char *hypothesisId, const char *location, const char *tag);

#ifdef __cplusplus
}
#endif
