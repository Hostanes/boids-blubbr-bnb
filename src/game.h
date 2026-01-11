#pragma once
#include "engine.h"
#include "raylib.h"
#include <stdint.h>

typedef struct {
  int cid_Boid;
} BoidComponentRegistry_t;

typedef struct Boid {
  Vector3 pos;
  Vector3 vel;
} Boid_t;

typedef struct {
  BoidComponentRegistry_t reg;

  int boidCount;
  entity_t boids[MAX_ENTITIES];

  float neighborRadius;
  float separationRadius;
  float alignWeight;
  float cohesionWeight;
  float separationWeight;

  float maxSpeed;
  float minSpeed;
  float maxForce;

  Vector3 boundsMin;
  Vector3 boundsMax;

  Camera3D cam;
} GameState_t;

void GameInitBoids(Engine_t *eng);
void GameUpdate(Engine_t *eng, float dt);
void GameDraw(Engine_t *eng);
void GameShutdown(Engine_t *eng);
