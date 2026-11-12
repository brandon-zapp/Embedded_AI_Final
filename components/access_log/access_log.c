#include "access_log.h"

#include <stdio.h>

int access_log_format(char *buf, size_t len, uint32_t uptime_ms, access_event_t evt,
                      access_state_t state, int user_id, const char *name, float score)
{
    if (buf == NULL || len == 0) return -1;

    char safe[32];
    size_t i = 0;
    if (name != NULL) {
        for (; name[i] != '\0' && i < sizeof(safe) - 1; i++) {
            char c = name[i];
            safe[i] = (c == ',' || c == '"' || c == '\n' || c == '\r') ? '_' : c;
        }
    }
    safe[i] = '\0';

    int n = snprintf(buf, len, "%lu.%03lu,%s,%s,%d,%s,%.3f\n",
                     (unsigned long)(uptime_ms / 1000), (unsigned long)(uptime_ms % 1000),
                     access_event_name(evt), access_state_name(state), user_id, safe,
                     (double)score);
    if (n < 0 || (size_t)n >= len) {
        buf[0] = '\0';
        return -1;
    }
    return n;
}
