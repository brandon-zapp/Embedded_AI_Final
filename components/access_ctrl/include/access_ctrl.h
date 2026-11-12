// Door access policy: decides when the lock opens, based on recognition results.
// Pure C, no ESP-IDF or hardware calls, so it can be unit tested on a PC.
// All times are milliseconds from any free-running clock (wraparound safe).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACCESS_LOCKED = 0,
    ACCESS_UNLOCKED,
    ACCESS_LOCKOUT,       // too many failed attempts, all faces refused for a while
    ACCESS_CAMERA_ERROR,  // camera fault, fail secure
} access_state_t;

typedef enum {
    ACCESS_EVT_NONE = 0,
    ACCESS_EVT_GRANTED,
    ACCESS_EVT_DENIED_UNKNOWN,    // no enrolled face matched
    ACCESS_EVT_DENIED_LOW_SCORE,  // closest match was below the threshold
    ACCESS_EVT_DENIED_LOCKOUT,    // refused because a lockout is active
    ACCESS_EVT_DENIED_CAMERA,     // refused because the camera is faulted
    ACCESS_EVT_LOCKOUT_STARTED,
    ACCESS_EVT_LOCKOUT_ENDED,
    ACCESS_EVT_RELOCKED,          // unlock period expired
    ACCESS_EVT_CAMERA_FAULT,
    ACCESS_EVT_CAMERA_RECOVERED,
} access_event_t;

typedef struct {
    float match_threshold;       // minimum similarity score to grant access
    uint32_t unlock_ms;          // how long the lock stays open after a grant
    uint8_t max_failures;        // failed attempts that trigger a lockout
    uint32_t failure_window_ms;  // failures must fall within this window to count together
    uint32_t lockout_ms;         // lockout duration
} access_config_t;

typedef struct {
    access_config_t cfg;
    access_state_t state;
    uint32_t unlock_until;
    bool lockout_active;
    uint32_t lockout_until;
    uint8_t failures;
    uint32_t first_failure_ms;
} access_ctrl_t;

// Defaults match prototype/doorlock_demo.py: 0.70 threshold, 5 s unlock.
void access_default_config(access_config_t *cfg);

// Returns false (and leaves ctrl unusable) if the config is invalid.
bool access_init(access_ctrl_t *ctrl, const access_config_t *cfg);

// A face was seen. user_id < 0 means no enrolled face matched.
access_event_t access_on_face(access_ctrl_t *ctrl, int user_id, float score, uint32_t now_ms);

// Call regularly (e.g. every loop) to expire unlock and lockout timers.
access_event_t access_tick(access_ctrl_t *ctrl, uint32_t now_ms);

access_event_t access_on_camera_fault(access_ctrl_t *ctrl);
access_event_t access_on_camera_ok(access_ctrl_t *ctrl, uint32_t now_ms);

// True only when the lock output should be energized/opened.
bool access_should_unlock(const access_ctrl_t *ctrl);

const char *access_state_name(access_state_t state);
const char *access_event_name(access_event_t evt);

#ifdef __cplusplus
}
#endif
