#include "face_db.h"

#include <math.h>
#include <string.h>

#define FACE_DB_MAGIC   0x46444231u  // "FDB1"
#define FACE_DB_VERSION 1u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t dim;
    uint16_t max_users;
    uint16_t next_id;
} face_db_header_t;

static float dot(const float *a, const float *b)
{
    float s = 0.0f;
    for (int i = 0; i < FACE_EMB_DIM; i++) s += a[i] * b[i];
    return s;
}

// Scales v to unit length in place; returns false if it is all zeros.
static bool normalize(float *v)
{
    float n = sqrtf(dot(v, v));
    if (!(n > 1e-12f)) return false;
    for (int i = 0; i < FACE_EMB_DIM; i++) v[i] /= n;
    return true;
}

void face_db_init(face_db_t *db)
{
    memset(db, 0, sizeof(*db));
}

int face_db_enroll(face_db_t *db, const char *name, const float *samples, int n_samples)
{
    if (db == NULL || name == NULL || name[0] == '\0' || samples == NULL || n_samples <= 0)
        return FACE_DB_ERR_ARG;

    face_record_t *slot = NULL;
    for (int i = 0; i < FACE_DB_MAX_USERS; i++) {
        if (!db->rec[i].used) { slot = &db->rec[i]; break; }
    }
    if (slot == NULL) return FACE_DB_ERR_FULL;

    // Average of the normalized samples, then normalize again
    float acc[FACE_EMB_DIM] = {0};
    float tmp[FACE_EMB_DIM];
    int good = 0;
    for (int s = 0; s < n_samples; s++) {
        memcpy(tmp, samples + (size_t)s * FACE_EMB_DIM, sizeof(tmp));
        if (!normalize(tmp)) continue;  // skip a bad sample rather than failing
        for (int i = 0; i < FACE_EMB_DIM; i++) acc[i] += tmp[i];
        good++;
    }
    if (good == 0 || !normalize(acc)) return FACE_DB_ERR_ZERO;

    slot->used = true;
    slot->id = db->next_id++;
    strncpy(slot->name, name, FACE_NAME_LEN - 1);
    slot->name[FACE_NAME_LEN - 1] = '\0';
    memcpy(slot->emb, acc, sizeof(acc));
    return slot->id;
}

static face_record_t *find(face_db_t *db, int id)
{
    if (id < 0) return NULL;
    for (int i = 0; i < FACE_DB_MAX_USERS; i++) {
        if (db->rec[i].used && db->rec[i].id == (uint16_t)id) return &db->rec[i];
    }
    return NULL;
}

bool face_db_remove(face_db_t *db, int id)
{
    face_record_t *r = find(db, id);
    if (r == NULL) return false;
    memset(r, 0, sizeof(*r));  // also wipes the stored embedding
    return true;
}

int face_db_count(const face_db_t *db)
{
    int n = 0;
    for (int i = 0; i < FACE_DB_MAX_USERS; i++) n += db->rec[i].used;
    return n;
}

const char *face_db_name(const face_db_t *db, int id)
{
    face_record_t *r = find((face_db_t *)db, id);
    return r ? r->name : NULL;
}

float face_cosine(const float *a, const float *b)
{
    float na = dot(a, a), nb = dot(b, b);
    if (!(na > 1e-24f) || !(nb > 1e-24f)) return 0.0f;
    return dot(a, b) / (sqrtf(na) * sqrtf(nb));
}

int face_db_match(const face_db_t *db, const float *emb, float *score)
{
    float q[FACE_EMB_DIM];
    memcpy(q, emb, sizeof(q));
    if (!normalize(q)) return -1;

    int best_id = -1;
    float best = -2.0f;
    for (int i = 0; i < FACE_DB_MAX_USERS; i++) {
        if (!db->rec[i].used) continue;
        float s = dot(q, db->rec[i].emb);  // both unit length, so this is the cosine
        if (s > best) { best = s; best_id = db->rec[i].id; }
    }
    if (score) *score = best_id >= 0 ? best : 0.0f;
    return best_id;
}

size_t face_db_serialized_size(void)
{
    return sizeof(face_db_header_t) + sizeof(((face_db_t *)0)->rec);
}

size_t face_db_serialize(const face_db_t *db, uint8_t *buf, size_t len)
{
    size_t need = face_db_serialized_size();
    if (db == NULL || buf == NULL || len < need) return 0;
    face_db_header_t h = {FACE_DB_MAGIC, FACE_DB_VERSION, FACE_EMB_DIM, FACE_DB_MAX_USERS, db->next_id};
    memcpy(buf, &h, sizeof(h));
    memcpy(buf + sizeof(h), db->rec, sizeof(db->rec));
    return need;
}

bool face_db_deserialize(face_db_t *db, const uint8_t *buf, size_t len)
{
    if (db == NULL || buf == NULL || len < face_db_serialized_size()) return false;
    face_db_header_t h;
    memcpy(&h, buf, sizeof(h));
    if (h.magic != FACE_DB_MAGIC || h.version != FACE_DB_VERSION || h.dim != FACE_EMB_DIM ||
        h.max_users != FACE_DB_MAX_USERS)
        return false;
    memcpy(db->rec, buf + sizeof(h), sizeof(db->rec));
    db->next_id = h.next_id;
    for (int i = 0; i < FACE_DB_MAX_USERS; i++) db->rec[i].name[FACE_NAME_LEN - 1] = '\0';
    return true;
}
