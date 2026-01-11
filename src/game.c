// game.c
// Boids demo "game layer":
// - owns GameState_t
// - calls boids systems (SysBoidsUpdate/SysBoidsDraw)
// - contains ALL draw calls inside GameDraw() as requested

#include "game.h"
#include "engine.h"
#include "engine_components.h"
#include "raylib.h"
#include "systems/systems.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// ------------------------------------------------------------
// Internal game state (single instance)
// ------------------------------------------------------------
static GameState_t g_gs = {0};
static bool g_inited = false;

// Small helper
static float frand01(void) {
  return (float)GetRandomValue(0, 10000) / 10000.0f;
}

static Vector3 rand_in_box(Vector3 mn, Vector3 mx) {
  return (Vector3){
      mn.x + frand01() * (mx.x - mn.x),
      mn.y + frand01() * (mx.y - mn.y),
      mn.z + frand01() * (mx.z - mn.z),
  };
}

static Vector3 rand_vel(float scale) {
  return (Vector3){
      ((float)GetRandomValue(-10000, 10000) / 10000.0f) * scale,
      ((float)GetRandomValue(-10000, 10000) / 10000.0f) * scale,
      ((float)GetRandomValue(-10000, 10000) / 10000.0f) * scale,
  };
}

// ------------------------------------------------------------
// One-time init
// ------------------------------------------------------------
void GameInitBoids(Engine_t *eng) {
  memset(&g_gs, 0, sizeof(g_gs));

  // ---- Register components (contiguous arrays)
  // NOTE: assumes eng->actors->componentStore was allocated in engine_init
  g_gs.reg.cid_pos = registerComponent(eng->actors, sizeof(Vector3));
  g_gs.reg.cid_vel = registerComponent(eng->actors, sizeof(Vector3));

  // ---- Simulation params
  g_gs.boidCount = 5000;
  if (g_gs.boidCount > MAX_ENTITIES)
    g_gs.boidCount = MAX_ENTITIES;

  g_gs.neighborRadius = 8.0f;
  g_gs.separationRadius = 3.0f;

  g_gs.alignWeight = 1.0f;
  g_gs.cohesionWeight = 0.8f;
  g_gs.separationWeight = 1.4f;

  g_gs.maxSpeed = 15.0f;
  g_gs.minSpeed = 5.0f;
  g_gs.maxForce = 6.0f;

  g_gs.boundsMin = (Vector3){-50, -50, -50};
  g_gs.boundsMax = (Vector3){50, 50, 50};

  // ---- Camera (raylib standard free camera)
  g_gs.cam.position = (Vector3){0, 40, 120};
  g_gs.cam.target = (Vector3){0, 0, 0};
  g_gs.cam.up = (Vector3){0, 1, 0};
  g_gs.cam.fovy = eng->config.fov_deg > 0 ? eng->config.fov_deg : 60.0f;
  g_gs.cam.projection = CAMERA_PERSPECTIVE;

  UpdateCamera(&g_gs.cam, CAMERA_FREE);
  DisableCursor(); // lock mouse for fly cam by default

  // ---- Spawn boids
  // IMPORTANT: your ECS must index arrays by entity INDEX (0..MAX_ENTITIES-1),
  // not the full entity id with category bits.
  // This spawn assumes engine_components uses GetEntityIndex(entity)
  // internally.
  for (int i = 0; i < g_gs.boidCount; i++) {
    if (eng->em.count >= MAX_ENTITIES)
      break;

    int idx = eng->em.count++;
    entity_t e = MakeEntityID(ET_ACTOR, idx);
    g_gs.boids[i] = e;

    eng->em.alive[idx] = 1;
    eng->em.masks[idx] = 0;

    Vector3 p = rand_in_box(g_gs.boundsMin, g_gs.boundsMax);
    Vector3 v = rand_vel(5.0f);

    addComponentToElement(&eng->em, eng->actors, e, g_gs.reg.cid_pos, &p);
    addComponentToElement(&eng->em, eng->actors, e, g_gs.reg.cid_vel, &v);
  }

  g_inited = true;
}

// ------------------------------------------------------------
// Public API expected by your main.c
// ------------------------------------------------------------
void GameUpdate(Engine_t *eng, float dt) {
  if (!g_inited)
    GameInitBoids(eng);

  // Toggle mouse capture/cursor with RMB
  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    if (IsCursorHidden())
      EnableCursor();
    else
      DisableCursor();
  }

  // Update fly camera
  UpdateCamera(&g_gs.cam, CAMERA_FREE);

  // Update boids
  SysBoidsUpdate(&g_gs, eng, dt);
}

void GameDraw(Engine_t *eng) {
  if (!g_inited)
    GameInitBoids(eng);

  BeginDrawing();
  ClearBackground(BLACK);

  BeginMode3D(g_gs.cam);

  // Optional: bounds + grid for reference
  DrawBoundingBox((BoundingBox){g_gs.boundsMin, g_gs.boundsMax}, DARKGRAY);
  DrawGrid(20, 10.0f);

  // Draw boids
  SysBoidsDraw(&g_gs, eng);

  EndMode3D();

  DrawFPS(10, 10);
  DrawText(
      "RMB: toggle mouse capture | WASD: move | Mouse: look | Q/E: down/up", 10,
      32, 16, RAYWHITE);

  EndDrawing();
}

void GameShutdown(Engine_t *eng) {
  (void)eng;
  // If you later allocate game resources (models, textures, etc.), unload them
  // here.
  g_inited = false;
  memset(&g_gs, 0, sizeof(g_gs));
}
