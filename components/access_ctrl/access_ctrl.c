#include "access_ctrl.h"

#include <stddef.h>

// True once now has reached deadline, correct across uint32 wraparound
// (as long as intervals are under ~24 days).
static bool reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

void access_default_config(access_config_t *cfg)
{
    cfg->match_threshold = 0.70f;
    cfg->unlock_ms = 5000;
    cfg->max_failures = 3;
    cfg->failure_window_ms = 60000;
    cfg->lockout_ms = 30000;
}

bool access_init(access_ctrl_t *ctrl, const access_config_t *cfg)
{
    if (ctrl == NULL || cfg == NULL) return false;
    if (!(cfg->match_threshold >= -1.0f && cfg->match_threshold <= 1.0f)) return false;
    if (cfg->unlock_ms == 0 || cfg->max_failures == 0 || cfg->lockout_ms == 0) return false;

    ctrl->cfg = *cfg;
    ctrl->state = ACCESS_LOCKED;
    ctrl->unlock_until = 0;
    ctrl->lockout_active = false;
    ctrl->lockout_until = 0;
    ctrl->failures = 0;
    ctrl->first_failure_ms = 0;
    return true;
}

// Records a failed attempt; starts a lockout once enough pile up in the window.
static access_event_t record_failure(access_ctrl_t *ctrl, access_event_t denial, uint32_t now_ms)
{
    if (ctrl->failures == 0 || reached(now_ms, ctrl->first_failure_ms + ctrl->cfg.failure_window_ms)) {
        ctrl->failures = 0;
        ctrl->first_failure_ms = now_ms;
    }
    ctrl->failures++;

    if (ctrl->failures >= ctrl->cfg.max_failures) {
        ctrl->failures = 0;
        ctrl->lockout_active = true;
        ctrl->lockout_until = now_ms + ctrl->cfg.lockout_ms;
        ctrl->state = ACCESS_LOCKOUT;
        return ACCESS_EVT_LOCKOUT_STARTED;
    }
    ctrl->state = ACCESS_LOCKED;  // a denial while open relocks immediately (fail secure)
    return denial;
}

access_event_t access_on_face(access_ctrl_t *ctrl, int user_id, float score, uint32_t now_ms)
{
    // Expire timers first so a stale lockout or unlock can't affect this decision
    access_tick(ctrl, now_ms);

    if (ctrl->state == ACCESS_CAMERA_ERROR) return ACCESS_EVT_DENIED_CAMERA;
    if (ctrl->state == ACCESS_LOCKOUT) return ACCESS_EVT_DENIED_LOCKOUT;

    if (user_id < 0) return record_failure(ctrl, ACCESS_EVT_DENIED_UNKNOWN, now_ms);
    if (!(score >= ctrl->cfg.match_threshold))  // also rejects NaN
        return record_failure(ctrl, ACCESS_EVT_DENIED_LOW_SCORE, now_ms);

    ctrl->failures = 0;
    ctrl->state = ACCESS_UNLOCKED;
    ctrl->unlock_until = now_ms + ctrl->cfg.unlock_ms;  // a repeat grant extends the open time
    return ACCESS_EVT_GRANTED;
}

access_event_t access_tick(access_ctrl_t *ctrl, uint32_t now_ms)
{
    if (ctrl->state == ACCESS_UNLOCKED && reached(now_ms, ctrl->unlock_until)) {
        ctrl->state = ACCESS_LOCKED;
        return ACCESS_EVT_RELOCKED;
    }
    if (ctrl->lockout_active && reached(now_ms, ctrl->lockout_until)) {
        ctrl->lockout_active = false;
        if (ctrl->state == ACCESS_LOCKOUT) {
            ctrl->state = ACCESS_LOCKED;
            return ACCESS_EVT_LOCKOUT_ENDED;
        }
    }
    return ACCESS_EVT_NONE;
}

access_event_t access_on_camera_fault(access_ctrl_t *ctrl)
{
    if (ctrl->state == ACCESS_CAMERA_ERROR) return ACCESS_EVT_NONE;
    ctrl->state = ACCESS_CAMERA_ERROR;  // never leave the door open without a working camera
    return ACCESS_EVT_CAMERA_FAULT;
}

access_event_t access_on_camera_ok(access_ctrl_t *ctrl, uint32_t now_ms)
{
    if (ctrl->state != ACCESS_CAMERA_ERROR) return ACCESS_EVT_NONE;
    // A lockout that was running when the camera failed still applies
    if (ctrl->lockout_active && !reached(now_ms, ctrl->lockout_until)) {
        ctrl->state = ACCESS_LOCKOUT;
    } else {
        ctrl->lockout_active = false;
        ctrl->state = ACCESS_LOCKED;
    }
    return ACCESS_EVT_CAMERA_RECOVERED;
}

bool access_should_unlock(const access_ctrl_t *ctrl)
{
    return ctrl->state == ACCESS_UNLOCKED;
}

const char *access_state_name(access_state_t state)
{
    switch (state) {
    case ACCESS_LOCKED: return "LOCKED";
    case ACCESS_UNLOCKED: return "UNLOCKED";
    case ACCESS_LOCKOUT: return "LOCKOUT";
    case ACCESS_CAMERA_ERROR: return "CAMERA_ERROR";
    }
    return "?";
}

const char *access_event_name(access_event_t evt)
{
    switch (evt) {
    case ACCESS_EVT_NONE: return "NONE";
    case ACCESS_EVT_GRANTED: return "GRANTED";
    case ACCESS_EVT_DENIED_UNKNOWN: return "DENIED_UNKNOWN";
    case ACCESS_EVT_DENIED_LOW_SCORE: return "DENIED_LOW_SCORE";
    case ACCESS_EVT_DENIED_LOCKOUT: return "DENIED_LOCKOUT";
    case ACCESS_EVT_DENIED_CAMERA: return "DENIED_CAMERA";
    case ACCESS_EVT_LOCKOUT_STARTED: return "LOCKOUT_STARTED";
    case ACCESS_EVT_LOCKOUT_ENDED: return "LOCKOUT_ENDED";
    case ACCESS_EVT_RELOCKED: return "RELOCKED";
    case ACCESS_EVT_CAMERA_FAULT: return "CAMERA_FAULT";
    case ACCESS_EVT_CAMERA_RECOVERED: return "CAMERA_RECOVERED";
    }
    return "?";
}
