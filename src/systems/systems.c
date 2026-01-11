// systems.c
// Implements player input, physics, and rendering
#include "systems.h"
#include "../engine.h"
#include "../game.h"
#include "raylib.h"
#include "rlgl.h"
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

void SysBoidsUpdate(GameState_t *gs, Engine_t *eng, float dt) {
  ActorComponents_t *ac = eng->actors;

  // temp arrays so updates are “simultaneous”
  static Vector3 nextVel[MAX_ENTITIES];

  // Initialize nextVel from current velocity (via getComponent)
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!eng->em.alive[i]) {
      nextVel[i] = (Vector3){0};
      continue;
    }

    entity_t ei = MakeEntityID(ET_ACTOR, i);
    Vector3 *vi = (Vector3 *)getComponent(ac, ei, gs->reg.cid_vel);
    if (!vi) {
      nextVel[i] = (Vector3){0};
      continue;
    }

    nextVel[i] = *vi;
  }

  const float neighborR = gs->neighborRadius;
  const float sepR = gs->separationRadius;

  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!eng->em.alive[i])
      continue;

    entity_t ei = MakeEntityID(ET_ACTOR, i);

    Vector3 *piPtr = (Vector3 *)getComponent(ac, ei, gs->reg.cid_pos);
    Vector3 *viPtr = (Vector3 *)getComponent(ac, ei, gs->reg.cid_vel);
    if (!piPtr || !viPtr)
      continue;

    Vector3 p = *piPtr;
    Vector3 v = *viPtr;

    Vector3 sumVel = (Vector3){0};
    Vector3 sumPos = (Vector3){0};
    Vector3 sumSep = (Vector3){0};

    int neighborCount = 0;
    int sepCount = 0;

    // Neighborhood scan (O(n^2))
    for (int j = 0; j < MAX_ENTITIES; j++) {
      if (j == i)
        continue;
      if (!eng->em.alive[j])
        continue;

      entity_t ej = MakeEntityID(ET_ACTOR, j);

      Vector3 *pjPtr = (Vector3 *)getComponent(ac, ej, gs->reg.cid_pos);
      Vector3 *vjPtr = (Vector3 *)getComponent(ac, ej, gs->reg.cid_vel);
      if (!pjPtr || !vjPtr)
        continue;

      Vector3 d = vsub(*pjPtr, p);
      float dist = vlen(d);
      if (dist <= 0.00001f)
        continue;

      if (dist < neighborR) {
        sumVel = vadd(sumVel, *vjPtr);
        sumPos = vadd(sumPos, *pjPtr);
        neighborCount++;
      }

      if (dist < sepR) {
        sumSep = vadd(sumSep, vscale(d, -1.0f / dist));
        sepCount++;
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

    // Integrate velocity
    v = vadd(v, vscale(accel, dt));
    v = vclamp_mag(v, gs->maxSpeed);

    // Enforce min speed
    float sp = vlen(v);
    if (sp > 0.0001f && sp < gs->minSpeed) {
      v = vscale(v, gs->minSpeed / sp);
    }

    nextVel[i] = v;
  }

  // Commit + integrate positions + bounds (via getComponent)
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!eng->em.alive[i])
      continue;

    entity_t ei = MakeEntityID(ET_ACTOR, i);

    Vector3 *piPtr = (Vector3 *)getComponent(ac, ei, gs->reg.cid_pos);
    Vector3 *viPtr = (Vector3 *)getComponent(ac, ei, gs->reg.cid_vel);
    if (!piPtr || !viPtr)
      continue;

    *viPtr = nextVel[i];
    *piPtr = vadd(*piPtr, vscale(*viPtr, dt));

    // wrap
    if (piPtr->x < gs->boundsMin.x)
      piPtr->x = gs->boundsMax.x;
    if (piPtr->x > gs->boundsMax.x)
      piPtr->x = gs->boundsMin.x;

    if (piPtr->y < gs->boundsMin.y)
      piPtr->y = gs->boundsMax.y;
    if (piPtr->y > gs->boundsMax.y)
      piPtr->y = gs->boundsMin.y;

    if (piPtr->z < gs->boundsMin.z)
      piPtr->z = gs->boundsMax.z;
    if (piPtr->z > gs->boundsMax.z)
      piPtr->z = gs->boundsMin.z;
  }
}

void SysBoidsDraw(GameState_t *gs, Engine_t *eng) {
  ActorComponents_t *ac = eng->actors;

  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!eng->em.alive[i])
      continue;

    entity_t ei = MakeEntityID(ET_ACTOR, i);

    Vector3 *pPtr = (Vector3 *)getComponent(ac, ei, gs->reg.cid_pos);
    Vector3 *vPtr = (Vector3 *)getComponent(ac, ei, gs->reg.cid_vel);
    if (!pPtr || !vPtr)
      continue;

    Vector3 p = *pPtr;
    Vector3 v = *vPtr;

    float sp = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (sp < 0.001f)
      continue;

    Vector3 dir = (Vector3){v.x / sp, v.y / sp, v.z / sp};

    float hue = (dir.x * 0.5f + 0.5f) * 360.0f;
    float sat = 0.15f + (dir.y * 0.5f + 0.5f) * 0.85f;
    float val = 0.95f;

    Color c = ColorFromHSV(hue, sat, val);

    DrawSphere(p, 0.3f, c);
    DrawLine3D(
        p,
        (Vector3){p.x + dir.x * 2.0f, p.y + dir.y * 2.0f, p.z + dir.z * 2.0f},
        Fade(c, 0.7f));
  }
}
