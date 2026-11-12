// Door lock firmware entry point.
//
// The camera, display, and lock hardware aren't connected yet, so this runs a
// scripted scenario through the real access-control, face-database, and
// logging components using synthetic face embeddings. Once the camera and
// recognition model are in, the synthetic embeddings get replaced by real ones.
#include <stdlib.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "access_ctrl.h"
#include "access_log.h"
#include "face_db.h"

static const char *TAG = "doorlock";

static face_db_t s_db;  // ~8 KB, kept off the task stack
static access_ctrl_t s_ctrl;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// Deterministic pseudo-random numbers so every run prints the same scenario
static uint32_t s_seed = 12345;
static float frand(void)
{
    s_seed = s_seed * 1103515245u + 12345u;
    return (float)((s_seed >> 8) & 0xFFFF) / 32768.0f - 1.0f;
}

static void random_vec(float *v)
{
    for (int i = 0; i < FACE_EMB_DIM; i++) v[i] = frand();
}

static void noisy_copy(float *dst, const float *base, float noise)
{
    for (int i = 0; i < FACE_EMB_DIM; i++) dst[i] = base[i] + noise * frand();
}

static void log_event(access_event_t evt, int user_id, float score)
{
    if (evt == ACCESS_EVT_NONE) return;
    char line[96];
    if (access_log_format(line, sizeof(line), now_ms(), evt, s_ctrl.state, user_id,
                          face_db_name(&s_db, user_id), score) > 0) {
        // Later: also append this line to the SD card log
        printf("LOG %s", line);
    }
    // Later: drive the lock GPIO from access_should_unlock() here
    ESP_LOGI(TAG, "lock output: %s", access_should_unlock(&s_ctrl) ? "OPEN" : "closed");
}

// Simulates the camera seeing a face: match it, then apply the access policy
static void present_face(const char *label, const float *emb)
{
    float score = 0;
    int id = face_db_match(&s_db, emb, &score);
    ESP_LOGI(TAG, "face: %s -> best match %s (score %.3f)", label,
             id >= 0 ? face_db_name(&s_db, id) : "none", score);
    // The policy applies the threshold; the log keeps the best id and score
    // either way, so the threshold can be tuned from real logs later
    log_event(access_on_face(&s_ctrl, id, score, now_ms()), id, score);
}

static void wait_ms(uint32_t ms)
{
    uint32_t end = now_ms() + ms;
    while ((int32_t)(now_ms() - end) < 0) {
        log_event(access_tick(&s_ctrl, now_ms()), -1, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 door lock firmware started (simulation mode)");

    access_config_t cfg;
    access_default_config(&cfg);
    cfg.lockout_ms = 10000;  // shorter than default so the demo finishes quickly
    access_init(&s_ctrl, &cfg);
    face_db_init(&s_db);
    printf("LOG %s", ACCESS_LOG_HEADER);

    // Enroll two users from three noisy "photos" each
    static float alex[FACE_EMB_DIM], sam[FACE_EMB_DIM], q[FACE_EMB_DIM];
    static float samples[3 * FACE_EMB_DIM];
    random_vec(alex);
    random_vec(sam);
    for (int k = 0; k < 3; k++) noisy_copy(samples + k * FACE_EMB_DIM, alex, 0.3f);
    face_db_enroll(&s_db, "Alex", samples, 3);
    for (int k = 0; k < 3; k++) noisy_copy(samples + k * FACE_EMB_DIM, sam, 0.3f);
    face_db_enroll(&s_db, "Sam", samples, 3);
    ESP_LOGI(TAG, "enrolled %d users", face_db_count(&s_db));

    while (1) {
        ESP_LOGI(TAG, "--- authorized user ---");
        noisy_copy(q, alex, 0.3f);
        present_face("Alex", q);
        wait_ms(6000);  // unlock expires after 5 s

        ESP_LOGI(TAG, "--- stranger x3 -> lockout ---");
        for (int i = 0; i < 3; i++) {
            random_vec(q);
            present_face("stranger", q);
            wait_ms(500);
        }
        noisy_copy(q, sam, 0.3f);
        present_face("Sam during lockout", q);
        wait_ms(11000);

        ESP_LOGI(TAG, "--- camera fault ---");
        log_event(access_on_camera_fault(&s_ctrl), -1, 0);
        noisy_copy(q, alex, 0.3f);
        present_face("Alex with camera down", q);
        log_event(access_on_camera_ok(&s_ctrl, now_ms()), -1, 0);

        ESP_LOGI(TAG, "scenario complete, repeating in 10 s");
        wait_ms(10000);
    }
}
