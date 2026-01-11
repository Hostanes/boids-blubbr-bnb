// systems.c
// Implements player input, physics, and rendering

#ifdef _OPENMP
#include <omp.h>
#endif
#include "../engine.h"
#include "../game.h"
#include "raylib.h"
#include "rlgl.h"
#include "systems.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>

static inline float vlen(Vector3 v) {
  return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline Vector3 vclamp_mag(Vector3 v, float maxMag) {
  float m = vlen(v);
  if (m <= maxMag || m <= 0.00001f)
    return v;
  float s = maxMag / m;
  return (Vector3){v.x * s, v.y * s, v.z * s};
}

static inline Vector3 vadd(Vector3 a, Vector3 b) {
  return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}
static inline Vector3 vsub(Vector3 a, Vector3 b) {
  return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}
static inline Vector3 vscale(Vector3 a, float s) {
  return (Vector3){a.x * s, a.y * s, a.z * s};
}

static inline int clamp_int(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

// Convert world coordinate -> cell coordinate along one axis
static inline int cellCoord(float p, float mn, float invCell, int dim) {
  int c = (int)floorf((p - mn) * invCell);
  return clamp_int(c, 0, dim - 1);
}

// Convert (cx,cy,cz) -> flattened 1D cell index
static inline int cellIndex(int cx, int cy, int cz, int dimX, int dimY) {
  return cx + cy * dimX + cz * (dimX * dimY);
}

void SysBoidsUpdate(GameState_t *gs, Engine_t *eng, float dt) {
  ActorComponents_t *ac = eng->actors;

  ComponentStorage_t *boidS = &ac->componentStore[gs->reg.cid_Boid];

  // -----------------------------
  // Grid setup (linked-list buckets)
  // -----------------------------
  const float cellSize =
      (gs->neighborRadius > 0.001f) ? gs->neighborRadius : 1.0f;

  Vector3 bmin = gs->boundsMin;
  Vector3 bmax = gs->boundsMax;

  float sx = bmax.x - bmin.x;
  float sy = bmax.y - bmin.y;
  float sz = bmax.z - bmin.z;

  int dimX = (int)ceilf(sx / cellSize);
  int dimY = (int)ceilf(sy / cellSize);
  int dimZ = (int)ceilf(sz / cellSize);

  if (dimX < 1)
    dimX = 1;
  if (dimX > 64)
    dimX = 64;
  if (dimY < 1)
    dimY = 1;
  if (dimY > 64)
    dimY = 64;
  if (dimZ < 1)
    dimZ = 1;
  if (dimZ > 64)
    dimZ = 64;

  const int cellCount = dimX * dimY * dimZ;

  static int head[64 * 64 * 64];
  static int nextIdx[MAX_ENTITIES];
  static int boidCell[MAX_ENTITIES];
  static Vector3 nextVel[MAX_ENTITIES];

  for (int c = 0; c < cellCount; c++)
    head[c] = -1;

  const float invCell = 1.0f / cellSize;

  // Build grid + seed nextVel (single-threaded)
  for (int i = 0; i < MAX_ENTITIES; i++) {
    nextIdx[i] = -1;
    boidCell[i] = -1;

    if (!eng->em.alive[i])
      continue;
    if (!boidS->occupied[i])
      continue;

    entity_t ei = MakeEntityID(ET_ACTOR, i);
    Boid_t *b = (Boid_t *)getComponent(ac, ei, gs->reg.cid_Boid);
    if (!b || !b->pos || !b->vel)
      continue;

    Vector3 p = *b->pos;

    int cx = cellCoord(p.x, bmin.x, invCell, dimX);
    int cy = cellCoord(p.y, bmin.y, invCell, dimY);
    int cz = cellCoord(p.z, bmin.z, invCell, dimZ);

    int ci = cellIndex(cx, cy, cz, dimX, dimY);
    boidCell[i] = ci;

    nextIdx[i] = head[ci];
    head[ci] = i;

    nextVel[i] = *b->vel;
  }

  const float neighborR2 = gs->neighborRadius * gs->neighborRadius;
  const float sepR2 = gs->separationRadius * gs->separationRadius;

  // -----------------------------
  // Compute nextVel (parallel safe)
  // -----------------------------
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!eng->em.alive[i])
      continue;
    if (!boidS->occupied[i])
      continue;

    entity_t ei = MakeEntityID(ET_ACTOR, i);
    Boid_t *bi = (Boid_t *)getComponent(ac, ei, gs->reg.cid_Boid);
    if (!bi || !bi->pos || !bi->vel)
      continue;

    Vector3 p = *bi->pos;
    Vector3 v = *bi->vel;

    int cx = cellCoord(p.x, bmin.x, invCell, dimX);
    int cy = cellCoord(p.y, bmin.y, invCell, dimY);
    int cz = cellCoord(p.z, bmin.z, invCell, dimZ);

    Vector3 sumVel = (Vector3){0};
    Vector3 sumPos = (Vector3){0};
    Vector3 sumSep = (Vector3){0};
    int neighborCount = 0;
    int sepCount = 0;

    for (int dz = -1; dz <= 1; dz++) {
      int z2 = cz + dz;
      if ((unsigned)z2 >= (unsigned)dimZ)
        continue;

      for (int dy = -1; dy <= 1; dy++) {
        int y2 = cy + dy;
        if ((unsigned)y2 >= (unsigned)dimY)
          continue;

        for (int dx = -1; dx <= 1; dx++) {
          int x2 = cx + dx;
          if ((unsigned)x2 >= (unsigned)dimX)
            continue;

          int ci = cellIndex(x2, y2, z2, dimX, dimY);

          for (int j = head[ci]; j != -1; j = nextIdx[j]) {
            if (j == i)
              continue;

            entity_t ej = MakeEntityID(ET_ACTOR, j);
            Boid_t *bj = (Boid_t *)getComponent(ac, ej, gs->reg.cid_Boid);
            if (!bj || !bj->pos || !bj->vel)
              continue;

            Vector3 d = vsub(*bj->pos, p);
            float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
            if (dist2 <= 0.0000001f)
              continue;

            if (dist2 < neighborR2) {
              sumVel = vadd(sumVel, *bj->vel);
              sumPos = vadd(sumPos, *bj->pos);
              neighborCount++;
            }

            if (dist2 < sepR2) {
              float invDist = 1.0f / sqrtf(dist2);
              sumSep = vadd(sumSep, vscale(d, -invDist));
              sepCount++;
            }
          }
        }
      }
    }

    Vector3 accel = (Vector3){0};

    if (neighborCount > 0) {
      float invN = 1.0f / (float)neighborCount;

      // Alignment
      Vector3 avgVel = vscale(sumVel, invN);
      Vector3 desiredA = (Vector3){0};
      float avm = vlen(avgVel);
      if (avm > 0.0001f)
        desiredA = vscale(avgVel, gs->maxSpeed / avm);
      Vector3 steerA = vsub(desiredA, v);
      steerA = vclamp_mag(steerA, gs->maxForce);

      // Cohesion
      Vector3 center = vscale(sumPos, invN);
      Vector3 toCenter = vsub(center, p);
      Vector3 desiredC = (Vector3){0};
      float tcm = vlen(toCenter);
      if (tcm > 0.0001f)
        desiredC = vscale(toCenter, gs->maxSpeed / tcm);
      Vector3 steerC = vsub(desiredC, v);
      steerC = vclamp_mag(steerC, gs->maxForce);

      // Separation
      if (sepCount > 0)
        sumSep = vscale(sumSep, 1.0f / (float)sepCount);
      Vector3 desiredS = (Vector3){0};
      float sm = vlen(sumSep);
      if (sm > 0.0001f)
        desiredS = vscale(sumSep, gs->maxSpeed / sm);
      Vector3 steerS = vsub(desiredS, v);
      steerS = vclamp_mag(steerS, gs->maxForce);

      accel = vadd(accel, vscale(steerA, gs->alignWeight));
      accel = vadd(accel, vscale(steerC, gs->cohesionWeight));
      accel = vadd(accel, vscale(steerS, gs->separationWeight));
    }

    v = vadd(v, vscale(accel, dt));
    v = vclamp_mag(v, gs->maxSpeed);

    float sp = vlen(v);
    if (sp > 0.0001f && sp < gs->minSpeed) {
      v = vscale(v, gs->minSpeed / sp);
    }

    nextVel[i] = v;
  }

  // -----------------------------
  // Commit + integrate positions (parallel safe)
  // -----------------------------
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!eng->em.alive[i])
      continue;
    if (!boidS->occupied[i])
      continue;

    entity_t ei = MakeEntityID(ET_ACTOR, i);
    Boid_t *b = (Boid_t *)getComponent(ac, ei, gs->reg.cid_Boid);
    if (!b || !b->pos || !b->vel)
      continue;

    *b->vel = nextVel[i];
    *b->pos = vadd(*b->pos, vscale(*b->vel, dt));

    // wrap
    if (b->pos->x < bmin.x)
      b->pos->x = bmax.x;
    if (b->pos->x > bmax.x)
      b->pos->x = bmin.x;

    if (b->pos->y < bmin.y)
      b->pos->y = bmax.y;
    if (b->pos->y > bmax.y)
      b->pos->y = bmin.y;

    if (b->pos->z < bmin.z)
      b->pos->z = bmax.z;
    if (b->pos->z > bmax.z)
      b->pos->z = bmin.z;
  }
}

void SysBoidsDraw(GameState_t *gs, Engine_t *eng) {
  ActorComponents_t *ac = eng->actors;
  ComponentStorage_t *boidS = &ac->componentStore[gs->reg.cid_Boid];

  rlBegin(RL_LINES);
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!eng->em.alive[i])
      continue;
    if (!boidS->occupied[i])
      continue;

    entity_t ei = MakeEntityID(ET_ACTOR, i);
    Boid_t *b = (Boid_t *)getComponent(ac, ei, gs->reg.cid_Boid);
    if (!b || !b->pos || !b->vel)
      continue;

    Vector3 p = *b->pos;
    Vector3 v = *b->vel;

    float sp2 = v.x * v.x + v.y * v.y + v.z * v.z;
    if (sp2 < 0.000001f)
      continue;
    float invSp = 1.0f / sqrtf(sp2);

    Vector3 dir = (Vector3){v.x * invSp, v.y * invSp, v.z * invSp};

    float hue = (dir.x * 0.5f + 0.5f) * 360.0f;
    float sat = 0.15f + (dir.y * 0.5f + 0.5f) * 0.85f;
    Color c = ColorFromHSV(hue, sat, 0.95f);

    Vector3 tip =
        (Vector3){p.x + dir.x * 1.6f, p.y + dir.y * 1.6f, p.z + dir.z * 1.6f};

    rlColor4ub(c.r, c.g, c.b, c.a);
    rlVertex3f(p.x, p.y, p.z);
    rlVertex3f(tip.x, tip.y, tip.z);
  }
  rlEnd();
}
