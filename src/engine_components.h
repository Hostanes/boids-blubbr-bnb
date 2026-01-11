
#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H
#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define HEIGHTMAP_RES_X 512
#define HEIGHTMAP_RES_Z 512

#define MAX_COMPONENTS 32

#define TERRAIN_SIZE 200
#define TERRAIN_SCALE 10.0f

#define ENTITY_TYPE_SHIFT 30
#define ENTITY_INDEX_MASK 0x3FFFFFFF

#define MAX_ENTITIES 4096

typedef enum {
  ET_ACTOR = 0,
} EntityCategory_t;
typedef enum { STATE_INLEVEL, STATE_MAINMENU } AllState_t;
//----------------------------------------
// ECS: Component Flags
//----------------------------------------
typedef uint32_t ComponentMask_t;

typedef enum {
  C_BOID = 0,
} ComponentFlag_t;

typedef int32_t entity_t;
//----------------------------------------
// Entity Manager + Components
//----------------------------------------
typedef int32_t entity_t;

typedef enum {
  ENTITY_BOID,
} EntityType_t;

typedef struct {
  uint8_t alive[MAX_ENTITIES];
  uint32_t masks[MAX_ENTITIES];
  int count;
} EntityManager_t;

typedef uint32_t ComponentID;

// TODO implement this instead of hardcoded components
// this is just for actors
// TODO later on implement dynamic archetypes
typedef struct {
  ComponentID id;
  size_t elementSize;
  void *data; // contiguous array: element_size * max_entities
  int count;
  bool *occupied; // per entity
} ComponentStorage_t;

typedef struct {

  ComponentStorage_t *componentStore; //  TODO define max comps
  int componentCount;

} ActorComponents_t;

typedef struct {
} ProjectilePool_t;

typedef struct {
} StaticPool_t;

typedef struct {
} ParticlePool_t;

// Inline category ID helpers
static inline entity_t MakeEntityID(EntityCategory_t cat, int index) {
  return ((uint32_t)cat << ENTITY_TYPE_SHIFT) | (index & ENTITY_INDEX_MASK);
}

static inline EntityCategory_t GetEntityCategory(entity_t id) {
  return (EntityCategory_t)(id >> ENTITY_TYPE_SHIFT);
}

static inline int GetEntityIndex(entity_t id) { return id & ENTITY_INDEX_MASK; }

#endif
