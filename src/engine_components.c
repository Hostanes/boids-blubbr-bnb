#include "engine_components.h"
#include "engine.h"
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

int registerComponent(ActorComponents_t *actors, size_t elementSize) {
  if (actors->componentCount >= MAX_COMPONENTS)
    return -1;

  int componentId = actors->componentCount;
  ComponentStorage_t *cs = &actors->componentStore[componentId];

  cs->id = componentId;
  cs->elementSize = elementSize;

  // Pointer-per-entity storage
  cs->ptrs = calloc(MAX_ENTITIES, sizeof(void *));
  cs->occupied = calloc(MAX_ENTITIES, sizeof(bool));
  cs->count = 0;

  actors->componentCount++;
  return componentId;
}

void addComponentToElement(EntityManager_t *em, ActorComponents_t *actors,
                           entity_t entity, int componentId,
                           void *elementValue) {
  int idx = GetEntityIndex(entity);
  ComponentStorage_t *cs = &actors->componentStore[componentId];

  if (idx < 0 || idx >= MAX_ENTITIES)
    return;

  // Allocate per-entity component storage on first add
  if (cs->ptrs[idx] == NULL) {
    cs->ptrs[idx] = calloc(1, cs->elementSize);
  }

  memcpy(cs->ptrs[idx], elementValue, cs->elementSize);

  if (!cs->occupied[idx])
    cs->count++;
  cs->occupied[idx] = true;

  em->masks[idx] |= (1u << componentId);
}

void *getComponent(ActorComponents_t *actors, entity_t entity,
                   int componentId) {
  int idx = GetEntityIndex(entity);
  ComponentStorage_t *cs = &actors->componentStore[componentId];

  if (idx < 0 || idx >= MAX_ENTITIES)
    return NULL;
  if (!cs->occupied[idx])
    return NULL;

  return cs->ptrs[idx];
}

void removeComponentFromEntity(EntityManager_t *em, ActorComponents_t *actors,
                               entity_t entity, ComponentID id) {
  int idx = GetEntityIndex(entity);
  ComponentStorage_t *cs = &actors->componentStore[id];

  if (idx < 0 || idx >= MAX_ENTITIES)
    return;

  if (cs->occupied[idx]) {
    cs->occupied[idx] = false;
    cs->count--;
  }

  if (cs->ptrs[idx]) {
    free(cs->ptrs[idx]);
    cs->ptrs[idx] = NULL;
  }

  em->masks[idx] &= ~(1u << id);
}

