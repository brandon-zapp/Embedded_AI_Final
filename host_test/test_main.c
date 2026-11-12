// Host (PC) unit tests for the hardware-free firmware components.
// Build and run: see host_test/README.md
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "access_ctrl.h"
#include "access_log.h"
#include "face_db.h"

static int g_run, g_fail;

#define CHECK(cond)                                                            \
    do {                                                                       \
        g_run++;                                                               \
        if (!(cond)) {                                                         \
            g_fail++;                                                          \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                      \
    } while (0)

#define CHECK_EVT(got, want)                                                   \
    do {                                                                       \
        access_event_t g_ = (got), w_ = (want);                                \
        g_run++;                                                               \
        if (g_ != w_) {                                                        \
            g_fail++;                                                          \
            printf("  FAIL %s:%d: got %s, want %s\n", __FILE__, __LINE__,      \
                   access_event_name(g_), access_event_name(w_));              \
        }                                                                      \
    } while (0)

#define RUN(test)                                                              \
    do {                                                                       \
        int before_ = g_fail;                                                  \
        test();                                                                \
        printf("%s %s\n", g_fail == before_ ? "ok  " : "FAIL", #test);         \
    } while (0)

/* ---------- access_ctrl ---------- */

static access_ctrl_t make_ctrl(void)
{
    access_config_t cfg;
    access_default_config(&cfg);  // 0.70, 5 s unlock, 3 failures / 60 s, 30 s lockout
    access_ctrl_t c;
    access_init(&c, &cfg);
    return c;
}

static void test_starts_locked(void)
{
    access_ctrl_t c = make_ctrl();
    CHECK(c.state == ACCESS_LOCKED);
    CHECK(!access_should_unlock(&c));
}

static void test_rejects_bad_config(void)
{
    access_config_t cfg;
    access_ctrl_t c;
    access_default_config(&cfg);
    cfg.unlock_ms = 0;
    CHECK(!access_init(&c, &cfg));
    access_default_config(&cfg);
    cfg.match_threshold = 1.5f;
    CHECK(!access_init(&c, &cfg));
    access_default_config(&cfg);
    cfg.max_failures = 0;
    CHECK(!access_init(&c, &cfg));
}

static void test_grant_at_threshold(void)
{
    access_ctrl_t c = make_ctrl();
    CHECK_EVT(access_on_face(&c, 0, 0.70f, 1000), ACCESS_EVT_GRANTED);
    CHECK(access_should_unlock(&c));
}

static void test_low_score_denied(void)
{
    access_ctrl_t c = make_ctrl();
    CHECK_EVT(access_on_face(&c, 0, 0.69f, 1000), ACCESS_EVT_DENIED_LOW_SCORE);
    CHECK(!access_should_unlock(&c));
}

static void test_nan_score_denied(void)
{
    access_ctrl_t c = make_ctrl();
    CHECK_EVT(access_on_face(&c, 0, NAN, 1000), ACCESS_EVT_DENIED_LOW_SCORE);
}

static void test_unknown_denied(void)
{
    access_ctrl_t c = make_ctrl();
    CHECK_EVT(access_on_face(&c, -1, 0.99f, 1000), ACCESS_EVT_DENIED_UNKNOWN);
    CHECK(c.state == ACCESS_LOCKED);
}

static void test_relocks_after_unlock_time(void)
{
    access_ctrl_t c = make_ctrl();
    access_on_face(&c, 0, 0.9f, 1000);
    CHECK_EVT(access_tick(&c, 5999), ACCESS_EVT_NONE);
    CHECK(access_should_unlock(&c));
    CHECK_EVT(access_tick(&c, 6000), ACCESS_EVT_RELOCKED);
    CHECK(!access_should_unlock(&c));
}

static void test_repeat_grant_extends_unlock(void)
{
    access_ctrl_t c = make_ctrl();
    access_on_face(&c, 0, 0.9f, 1000);
    access_on_face(&c, 0, 0.9f, 4000);
    CHECK_EVT(access_tick(&c, 8999), ACCESS_EVT_NONE);
    CHECK_EVT(access_tick(&c, 9000), ACCESS_EVT_RELOCKED);
}

static void test_denial_while_open_relocks(void)
{
    access_ctrl_t c = make_ctrl();
    access_on_face(&c, 0, 0.9f, 1000);
    CHECK_EVT(access_on_face(&c, -1, 0.0f, 2000), ACCESS_EVT_DENIED_UNKNOWN);
    CHECK(!access_should_unlock(&c));
}

static void test_lockout_after_three_failures(void)
{
    access_ctrl_t c = make_ctrl();
    CHECK_EVT(access_on_face(&c, -1, 0, 1000), ACCESS_EVT_DENIED_UNKNOWN);
    CHECK_EVT(access_on_face(&c, -1, 0, 2000), ACCESS_EVT_DENIED_UNKNOWN);
    CHECK_EVT(access_on_face(&c, -1, 0, 3000), ACCESS_EVT_LOCKOUT_STARTED);
    CHECK(c.state == ACCESS_LOCKOUT);
    // Even an enrolled, confident face is refused during lockout
    CHECK_EVT(access_on_face(&c, 0, 0.99f, 4000), ACCESS_EVT_DENIED_LOCKOUT);
    CHECK(!access_should_unlock(&c));
    CHECK_EVT(access_tick(&c, 32999), ACCESS_EVT_NONE);
    CHECK_EVT(access_tick(&c, 33000), ACCESS_EVT_LOCKOUT_ENDED);
    CHECK_EVT(access_on_face(&c, 0, 0.99f, 33100), ACCESS_EVT_GRANTED);
}

static void test_failures_outside_window_dont_lock_out(void)
{
    access_ctrl_t c = make_ctrl();
    access_on_face(&c, -1, 0, 0);
    access_on_face(&c, -1, 0, 1000);
    // Third failure comes after the 60 s window, so the count restarts
    CHECK_EVT(access_on_face(&c, -1, 0, 61000), ACCESS_EVT_DENIED_UNKNOWN);
    CHECK(c.state == ACCESS_LOCKED);
}

static void test_grant_resets_failure_count(void)
{
    access_ctrl_t c = make_ctrl();
    access_on_face(&c, -1, 0, 1000);
    access_on_face(&c, -1, 0, 2000);
    access_on_face(&c, 0, 0.9f, 3000);
    CHECK_EVT(access_on_face(&c, -1, 0, 4000), ACCESS_EVT_DENIED_UNKNOWN);
    CHECK(c.state == ACCESS_LOCKED);
}

static void test_camera_fault_fails_secure(void)
{
    access_ctrl_t c = make_ctrl();
    access_on_face(&c, 0, 0.9f, 1000);
    CHECK_EVT(access_on_camera_fault(&c), ACCESS_EVT_CAMERA_FAULT);
    CHECK(!access_should_unlock(&c));
    CHECK_EVT(access_on_face(&c, 0, 0.99f, 1500), ACCESS_EVT_DENIED_CAMERA);
    CHECK_EVT(access_on_camera_ok(&c, 2000), ACCESS_EVT_CAMERA_RECOVERED);
    CHECK(c.state == ACCESS_LOCKED);
}

static void test_camera_recovery_keeps_active_lockout(void)
{
    access_ctrl_t c = make_ctrl();
    access_on_face(&c, -1, 0, 1000);
    access_on_face(&c, -1, 0, 2000);
    access_on_face(&c, -1, 0, 3000);  // lockout until 33000
    access_on_camera_fault(&c);
    access_on_camera_ok(&c, 10000);
    CHECK(c.state == ACCESS_LOCKOUT);
    access_on_camera_fault(&c);
    access_on_camera_ok(&c, 40000);
    CHECK(c.state == ACCESS_LOCKED);
}

static void test_timer_wraparound(void)
{
    access_ctrl_t c = make_ctrl();
    uint32_t t = 0xFFFFF000u;  // ~4 s before the 32-bit ms counter wraps
    access_on_face(&c, 0, 0.9f, t);
    CHECK_EVT(access_tick(&c, t + 4999), ACCESS_EVT_NONE);
    CHECK_EVT(access_tick(&c, t + 5000), ACCESS_EVT_RELOCKED);
}

/* ---------- face_db ---------- */

static float frand(void) { return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f; }

static void random_vec(float *v)
{
    for (int i = 0; i < FACE_EMB_DIM; i++) v[i] = frand();
}

// base + small noise: a second photo of the same person
static void noisy_copy(float *dst, const float *base, float noise)
{
    for (int i = 0; i < FACE_EMB_DIM; i++) dst[i] = base[i] + noise * frand();
}

static void test_db_enroll_and_match(void)
{
    static face_db_t db;
    float alex[FACE_EMB_DIM], sam[FACE_EMB_DIM], q[FACE_EMB_DIM], score;
    srand(1);
    face_db_init(&db);
    random_vec(alex);
    random_vec(sam);
    int a = face_db_enroll(&db, "Alex", alex, 1);
    int s = face_db_enroll(&db, "Sam", sam, 1);
    CHECK(a >= 0 && s >= 0 && a != s);
    CHECK(face_db_count(&db) == 2);
    CHECK(strcmp(face_db_name(&db, a), "Alex") == 0);

    noisy_copy(q, alex, 0.2f);
    CHECK(face_db_match(&db, q, &score) == a);
    CHECK(score > 0.9f);

    noisy_copy(q, sam, 0.2f);
    CHECK(face_db_match(&db, q, &score) == s);

    // A random stranger should score low against everyone
    random_vec(q);
    face_db_match(&db, q, &score);
    CHECK(score < 0.5f);
}

static void test_db_scale_invariant(void)
{
    static face_db_t db;
    float v[FACE_EMB_DIM], big[FACE_EMB_DIM], score;
    face_db_init(&db);
    random_vec(v);
    for (int i = 0; i < FACE_EMB_DIM; i++) big[i] = v[i] * 37.0f;
    int id = face_db_enroll(&db, "Alex", v, 1);
    CHECK(face_db_match(&db, big, &score) == id);
    CHECK(fabsf(score - 1.0f) < 1e-4f);
}

static void test_db_multi_sample_enroll(void)
{
    static face_db_t db;
    float base[FACE_EMB_DIM], samples[3 * FACE_EMB_DIM], score;
    face_db_init(&db);
    random_vec(base);
    for (int k = 0; k < 3; k++) noisy_copy(samples + k * FACE_EMB_DIM, base, 0.3f);
    int id = face_db_enroll(&db, "Alex", samples, 3);
    CHECK(id >= 0);
    // The averaged template should sit closer to the true face than any one sample
    CHECK(face_db_match(&db, base, &score) == id);
    CHECK(score > face_cosine(samples, base));
}

static void test_db_errors(void)
{
    static face_db_t db;
    float v[FACE_EMB_DIM], zero[FACE_EMB_DIM] = {0}, score = 123.0f;
    face_db_init(&db);
    random_vec(v);
    CHECK(face_db_match(&db, v, &score) == -1);  // empty
    CHECK(face_db_enroll(&db, "", v, 1) == FACE_DB_ERR_ARG);
    CHECK(face_db_enroll(&db, NULL, v, 1) == FACE_DB_ERR_ARG);
    CHECK(face_db_enroll(&db, "Alex", v, 0) == FACE_DB_ERR_ARG);
    CHECK(face_db_enroll(&db, "Zero", zero, 1) == FACE_DB_ERR_ZERO);
    for (int i = 0; i < FACE_DB_MAX_USERS; i++) CHECK(face_db_enroll(&db, "u", v, 1) >= 0);
    CHECK(face_db_enroll(&db, "extra", v, 1) == FACE_DB_ERR_FULL);
    CHECK(face_db_match(&db, zero, &score) == -1);
}

static void test_db_remove_and_reuse_slot(void)
{
    static face_db_t db;
    float a[FACE_EMB_DIM], b[FACE_EMB_DIM], score;
    face_db_init(&db);
    random_vec(a);
    random_vec(b);
    int ia = face_db_enroll(&db, "Alex", a, 1);
    CHECK(face_db_remove(&db, ia));
    CHECK(!face_db_remove(&db, ia));
    CHECK(face_db_name(&db, ia) == NULL);
    CHECK(face_db_count(&db) == 0);
    int ib = face_db_enroll(&db, "Sam", b, 1);
    CHECK(ib != ia);  // ids are never reused, so old log entries stay unambiguous
    CHECK(face_db_match(&db, a, &score) == ib);  // only Sam is left
}

static void test_db_long_name_truncated(void)
{
    static face_db_t db;
    float v[FACE_EMB_DIM];
    face_db_init(&db);
    random_vec(v);
    int id = face_db_enroll(&db, "A_really_long_name_that_will_not_fit", v, 1);
    CHECK(strlen(face_db_name(&db, id)) == FACE_NAME_LEN - 1);
}

static void test_db_serialize_roundtrip(void)
{
    static face_db_t db, copy;
    static uint8_t buf[sizeof(face_db_t) + 64];
    float a[FACE_EMB_DIM], score;
    face_db_init(&db);
    random_vec(a);
    int ia = face_db_enroll(&db, "Alex", a, 1);
    size_t n = face_db_serialize(&db, buf, sizeof(buf));
    CHECK(n == face_db_serialized_size());
    CHECK(face_db_serialize(&db, buf, n - 1) == 0);

    face_db_init(&copy);
    CHECK(face_db_deserialize(&copy, buf, n));
    CHECK(face_db_count(&copy) == 1);
    CHECK(face_db_match(&copy, a, &score) == ia);
    CHECK(face_db_enroll(&copy, "Sam", a, 1) == ia + 1);  // next_id restored

    buf[0] ^= 0xFF;  // corrupt the magic number
    CHECK(!face_db_deserialize(&copy, buf, n));
    CHECK(!face_db_deserialize(&copy, buf, n - 1));
}

/* ---------- access_log ---------- */

static void test_log_format(void)
{
    char line[96];
    int n = access_log_format(line, sizeof(line), 12345, ACCESS_EVT_GRANTED, ACCESS_UNLOCKED, 0,
                              "Alex", 0.9123f);
    CHECK(n > 0);
    CHECK(strcmp(line, "12.345,GRANTED,UNLOCKED,0,Alex,0.912\n") == 0);
}

static void test_log_sanitizes_name(void)
{
    char line[96];
    access_log_format(line, sizeof(line), 0, ACCESS_EVT_DENIED_UNKNOWN, ACCESS_LOCKED, -1,
                      "a,b\"c\n", 0.1f);
    CHECK(strcmp(line, "0.000,DENIED_UNKNOWN,LOCKED,-1,a_b_c_,0.100\n") == 0);
    access_log_format(line, sizeof(line), 0, ACCESS_EVT_RELOCKED, ACCESS_LOCKED, -1, NULL, 0);
    CHECK(strcmp(line, "0.000,RELOCKED,LOCKED,-1,,0.000\n") == 0);
}

static void test_log_small_buffer(void)
{
    char line[8];
    CHECK(access_log_format(line, sizeof(line), 0, ACCESS_EVT_GRANTED, ACCESS_UNLOCKED, 0, "Alex",
                            1.0f) == -1);
    CHECK(line[0] == '\0');
}

int main(void)
{
    puts("access_ctrl");
    RUN(test_starts_locked);
    RUN(test_rejects_bad_config);
    RUN(test_grant_at_threshold);
    RUN(test_low_score_denied);
    RUN(test_nan_score_denied);
    RUN(test_unknown_denied);
    RUN(test_relocks_after_unlock_time);
    RUN(test_repeat_grant_extends_unlock);
    RUN(test_denial_while_open_relocks);
    RUN(test_lockout_after_three_failures);
    RUN(test_failures_outside_window_dont_lock_out);
    RUN(test_grant_resets_failure_count);
    RUN(test_camera_fault_fails_secure);
    RUN(test_camera_recovery_keeps_active_lockout);
    RUN(test_timer_wraparound);

    puts("face_db");
    RUN(test_db_enroll_and_match);
    RUN(test_db_scale_invariant);
    RUN(test_db_multi_sample_enroll);
    RUN(test_db_errors);
    RUN(test_db_remove_and_reuse_slot);
    RUN(test_db_long_name_truncated);
    RUN(test_db_serialize_roundtrip);

    puts("access_log");
    RUN(test_log_format);
    RUN(test_log_sanitizes_name);
    RUN(test_log_small_buffer);

    printf("\n%d checks, %d failed\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
