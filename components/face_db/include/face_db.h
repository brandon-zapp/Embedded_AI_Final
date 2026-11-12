// Enrolled-face database: stores one embedding per user and finds the closest
// match by cosine similarity. Pure C, so it can be unit tested on a PC.
//
// Embeddings come from the face recognition model (esp-dl / esp-who on the
// board). FACE_EMB_DIM must match that model's output length; override it with
// a compile definition once the model is chosen.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FACE_EMB_DIM
#define FACE_EMB_DIM 128
#endif
#ifndef FACE_DB_MAX_USERS
#define FACE_DB_MAX_USERS 16
#endif
#define FACE_NAME_LEN 24  // including the terminating '\0'

enum {
    FACE_DB_ERR_ARG = -1,   // bad pointer, empty name, or no samples
    FACE_DB_ERR_FULL = -2,  // no free slots
    FACE_DB_ERR_ZERO = -3,  // embedding has zero length, can't be normalized
};

typedef struct {
    bool used;
    uint16_t id;
    char name[FACE_NAME_LEN];
    float emb[FACE_EMB_DIM];  // stored unit length
} face_record_t;

typedef struct {
    face_record_t rec[FACE_DB_MAX_USERS];
    uint16_t next_id;
} face_db_t;

void face_db_init(face_db_t *db);

// Enrolls a user from n_samples embeddings (each FACE_EMB_DIM floats, back to
// back). Samples are normalized and averaged, which is more robust than one shot.
// Returns the new user id (>= 0) or a FACE_DB_ERR_* code.
int face_db_enroll(face_db_t *db, const char *name, const float *samples, int n_samples);

bool face_db_remove(face_db_t *db, int id);
int face_db_count(const face_db_t *db);
const char *face_db_name(const face_db_t *db, int id);  // NULL if not found

// Finds the most similar enrolled face. Returns its id and writes the cosine
// similarity (-1..1) to *score, or returns -1 if the database is empty or the
// query embedding is zero.
int face_db_match(const face_db_t *db, const float *emb, float *score);

// Cosine similarity of two FACE_EMB_DIM vectors (0 if either is zero).
float face_cosine(const float *a, const float *b);

// Flat binary image of the database, for saving to NVS or the SD card.
size_t face_db_serialized_size(void);
size_t face_db_serialize(const face_db_t *db, uint8_t *buf, size_t len);    // returns bytes written, 0 on error
bool face_db_deserialize(face_db_t *db, const uint8_t *buf, size_t len);   // rejects bad magic/version/dimension

#ifdef __cplusplus
}
#endif
