#include "engine_components.h"
#include "engine.h"
#include <stdint.h>
#include <stdlib.h> // calloc, free
#include <string.h>
#include <sys/types.h>

int registerComponent(ActorComponents_t *actors, size_t elementSize) {
  if (!actors)
    return -1;
  if (actors->componentCount >= MAX_COMPONENTS)
    return -1;

  int componentId = actors->componentCount;
  ComponentStorage_t *cs = &actors->componentStore[componentId];

  cs->id = (ComponentID)componentId;
  cs->elementSize = elementSize;

  cs->data = calloc(MAX_ENTITIES, elementSize);
  cs->occupied = calloc(MAX_ENTITIES, sizeof(bool));
  cs->count = 0;

  if (!cs->data || !cs->occupied) {
    free(cs->data);
    free(cs->occupied);
    cs->data = NULL;
    cs->occupied = NULL;
    return -1;
  }

  actors->componentCount++;
  return componentId;
}

void addComponentToElement(EntityManager_t *em, ActorComponents_t *actors,
                           entity_t entity, int componentId,
                           void *elementValue) {
  if (!em || !actors || !elementValue)
    return;

  int idx = GetEntityIndex(entity);
  if (idx < 0 || idx >= MAX_ENTITIES)
    return;

  if (componentId < 0 || componentId >= actors->componentCount)
    return;

  ComponentStorage_t *cs = &actors->componentStore[componentId];
  if (!cs->data || !cs->occupied)
    return;

  uint8_t *dst = (uint8_t *)cs->data + ((size_t)idx * cs->elementSize);
  memcpy(dst, elementValue, cs->elementSize);

  if (!cs->occupied[idx])
    cs->count++;
  cs->occupied[idx] = true;

  em->masks[idx] |= (1u << (uint32_t)componentId);
}

void *getComponent(ActorComponents_t *actors, entity_t entity,
                   int componentId) {
  if (!actors)
    return NULL;

  int idx = GetEntityIndex(entity);
  if (idx < 0 || idx >= MAX_ENTITIES)
    return NULL;

  if (componentId < 0 || componentId >= actors->componentCount)
    return NULL;

  ComponentStorage_t *cs = &actors->componentStore[componentId];
  if (!cs->data || !cs->occupied)
    return NULL;

  if (!cs->occupied[idx])
    return NULL;

  return (uint8_t *)cs->data + ((size_t)idx * cs->elementSize);
}

void removeComponentFromEntity(EntityManager_t *em, ActorComponents_t *actors,
                               entity_t entity, ComponentID id) {
  if (!em || !actors)
    return;

  int idx = GetEntityIndex(entity);
  if (idx < 0 || idx >= MAX_ENTITIES)
    return;

  if ((int)id < 0 || (int)id >= actors->componentCount)
    return;

  ComponentStorage_t *cs = &actors->componentStore[id];
  if (!cs->data || !cs->occupied)
    return;

  if (cs->occupied[idx]) {
    cs->occupied[idx] = false;
    cs->count--;
  }

  // zero memory for safety (optional but nice)
  memset((uint8_t *)cs->data + ((size_t)idx * cs->elementSize), 0,
         cs->elementSize);

  em->masks[idx] &= ~(1u << (uint32_t)id);
}

// return an entire array of a component
void *GetComponentArray(ActorComponents_t *actors, ComponentID cid) {
  if (!actors)
    return NULL;
  if ((int)cid < 0 || (int)cid >= actors->componentCount)
    return NULL;
  return actors->componentStore[cid].data;
}
