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
  Vector3 *pos = (Vector3 *)GetComponentArray(eng->actors, gs->reg.cid_pos);
  Vector3 *vel = (Vector3 *)GetComponentArray(eng->actors, gs->reg.cid_vel);

  ComponentStorage_t *posS = &eng->actors->componentStore[gs->reg.cid_pos];
  ComponentStorage_t *velS = &eng->actors->componentStore[gs->reg.cid_vel];

  // temp arrays so updates are “simultaneous”
  static Vector3 nextVel[MAX_ENTITIES];

  // Copy current velocities
  for (int i = 0; i < MAX_ENTITIES; i++) {
    nextVel[i] = vel[i];
  }

  const float neighborR = gs->neighborRadius;
  const float sepR = gs->separationRadius;

  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!posS->occupied[i] || !velS->occupied[i])
      continue;
    if (!eng->em.alive[i])
      continue;

    Vector3 p = pos[i];
    Vector3 v = vel[i];

    Vector3 sumVel = (Vector3){0};
    Vector3 sumPos = (Vector3){0};
    Vector3 sumSep = (Vector3){0};

    int neighborCount = 0;
    int sepCount = 0;

    // Neighborhood scan (O(n^2))
    for (int j = 0; j < MAX_ENTITIES; j++) {
      if (j == i)
        continue;
      if (!posS->occupied[j] || !velS->occupied[j])
        continue;
      if (!eng->em.alive[j])
        continue;

      Vector3 d = vsub(pos[j], p);
      float dist = vlen(d);
      if (dist <= 0.00001f)
        continue;

      if (dist < neighborR) {
        sumVel = vadd(sumVel, vel[j]);
        sumPos = vadd(sumPos, pos[j]);
        neighborCount++;
      }

      if (dist < sepR) {
        // Separation: push away (1/dist is smoother than 1/dist^2)
        // Direction is away from neighbor: -d/dist
        sumSep = vadd(sumSep, vscale(d, -1.0f / dist));
        sepCount++;
      }
    }

    Vector3 accel = (Vector3){0};

    if (neighborCount > 0) {
      float invN = 1.0f / (float)neighborCount;

      // --- Alignment ---
      Vector3 avgVel = vscale(sumVel, invN);
      Vector3 desiredA = (Vector3){0};
      float avm = vlen(avgVel);
      if (avm > 0.0001f)
        desiredA = vscale(avgVel, gs->maxSpeed / avm);

      Vector3 steerA = vsub(desiredA, v);
      steerA = vclamp_mag(steerA, gs->maxForce);

      // --- Cohesion ---
      Vector3 center = vscale(sumPos, invN);
      Vector3 toCenter = vsub(center, p);
      Vector3 desiredC = (Vector3){0};
      float tcm = vlen(toCenter);
      if (tcm > 0.0001f)
        desiredC = vscale(toCenter, gs->maxSpeed / tcm);

      Vector3 steerC = vsub(desiredC, v);
      steerC = vclamp_mag(steerC, gs->maxForce);

      // --- Separation ---
      if (sepCount > 0) {
        sumSep = vscale(sumSep, 1.0f / (float)sepCount);
      }

      Vector3 desiredS = (Vector3){0};
      float sm = vlen(sumSep);
      if (sm > 0.0001f)
        desiredS = vscale(sumSep, gs->maxSpeed / sm);

      Vector3 steerS = vsub(desiredS, v);
      steerS = vclamp_mag(steerS, gs->maxForce);

      // Weighted sum
      accel = vadd(accel, vscale(steerA, gs->alignWeight));
      accel = vadd(accel, vscale(steerC, gs->cohesionWeight));
      accel = vadd(accel, vscale(steerS, gs->separationWeight));
    }

    // Integrate velocity
    v = vadd(v, vscale(accel, dt));

    // Clamp to max speed
    v = vclamp_mag(v, gs->maxSpeed);

    // Enforce min speed (keep direction, but ensure it doesn't stall)
    float sp = vlen(v);
    if (sp > 0.0001f && sp < gs->minSpeed) {
      v = vscale(v, gs->minSpeed / sp);
    }

    nextVel[i] = v;
  }

  // Commit + integrate positions + bounds
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!posS->occupied[i] || !velS->occupied[i])
      continue;
    if (!eng->em.alive[i])
      continue;

    vel[i] = nextVel[i];
    pos[i] = vadd(pos[i], vscale(vel[i], dt));

    // simple wrap
    if (pos[i].x < gs->boundsMin.x)
      pos[i].x = gs->boundsMax.x;
    if (pos[i].x > gs->boundsMax.x)
      pos[i].x = gs->boundsMin.x;

    if (pos[i].y < gs->boundsMin.y)
      pos[i].y = gs->boundsMax.y;
    if (pos[i].y > gs->boundsMax.y)
      pos[i].y = gs->boundsMin.y;

    if (pos[i].z < gs->boundsMin.z)
      pos[i].z = gs->boundsMax.z;
    if (pos[i].z > gs->boundsMax.z)
      pos[i].z = gs->boundsMin.z;
  }
}

void SysBoidsDraw(GameState_t *gs, Engine_t *eng) {
  Vector3 *pos = (Vector3 *)GetComponentArray(eng->actors, gs->reg.cid_pos);
  Vector3 *vel = (Vector3 *)GetComponentArray(eng->actors, gs->reg.cid_vel);
  ComponentStorage_t *posS = &eng->actors->componentStore[gs->reg.cid_pos];

  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!posS->occupied[i] || !eng->em.alive[i])
      continue;

    Vector3 p = pos[i];
    Vector3 v = vel[i];

    float sp = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (sp < 0.001f)
      continue;

    Vector3 dir = (Vector3){v.x / sp, v.y / sp, v.z / sp};

    // Map dir.x (-1..1) -> hue (0..360)
    float hue = (dir.x * 0.5f + 0.5f) * 360.0f;

    // Map dir.y (-1..1) -> saturation (0.15..1.0) so it never goes fully gray
    float sat = 0.15f + (dir.y * 0.5f + 0.5f) * 0.85f;

    // Keep value/brightness high and stable
    float val = 0.95f;

    Color c = ColorFromHSV(hue, sat, val);

    DrawSphere(p, 0.3f, c);

    // optional: direction line in same color but slightly dimmer
    Color line = Fade(c, 0.7f);
    DrawLine3D(
        p,
        (Vector3){p.x + dir.x * 2.0f, p.y + dir.y * 2.0f, p.z + dir.z * 2.0f},
        line);
  }
}
