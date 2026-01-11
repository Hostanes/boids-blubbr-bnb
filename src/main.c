#include "engine.h"
#include "game.h"
#include "raylib.h"
#include "systems/systems.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
  printf("raylib version: %s\n", RAYLIB_VERSION);

  SetConfigFlags(FLAG_VSYNC_HINT);

  EngineConfig_t cfg = {
      .window_width = (int)(1280 * 1.2f),
      .window_height = (int)(720 * 1.2f),

      .fov_deg = 90.0f,
      .near_plane = 0.6f,
      .far_plane = 5000.0f,

      .max_entities = 2048,
      .max_projectiles = 256,
      .max_actors = 256,
      .max_particles = 4096,
      .max_statics = 1024,
  };

  srand((unsigned)time(0));

  Engine_t eng;
  engine_init(&eng, &cfg);

  SetTargetFPS(60);

  // ----- Init boids "game"
  GameInitBoids(&eng);

  // Free camera mode uses mouse look; lock cursor by default
  DisableCursor();

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    // Toggle cursor lock/capture (Right Mouse)
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
      if (IsCursorHidden())
        EnableCursor();
      else
        DisableCursor();
    }

    // Update simulation
    GameUpdate(&eng, dt);

    GameDraw(&eng);
  }

  GameShutdown(&eng);
  engine_shutdown();

  return 0;
}
