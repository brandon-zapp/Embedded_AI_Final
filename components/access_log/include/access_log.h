// Formats access events as CSV lines for the SD card log. Pure C.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "access_ctrl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACCESS_LOG_HEADER "uptime_s,event,state,user_id,name,score\n"

// Writes one line like "12.345,GRANTED,UNLOCKED,0,Alex,0.912\n".
// name may be NULL; commas, quotes and newlines in it are replaced with '_'.
// Returns the line length, or -1 if buf is too small (buf is then left as "").
int access_log_format(char *buf, size_t len, uint32_t uptime_ms, access_event_t evt,
                      access_state_t state, int user_id, const char *name, float score);

#ifdef __cplusplus
}
#endif
