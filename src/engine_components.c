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
  cs->data = calloc(MAX_ENTITIES, elementSize);
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

  uint8_t *addr = (uint8_t *)cs->data + (idx * cs->elementSize);
  memcpy(addr, elementValue, cs->elementSize);

  if (!cs->occupied[idx])
    cs->count++;
  cs->occupied[idx] = true;

  em->masks[idx] |= (1u << componentId);
}

void *getComponent(ActorComponents_t *actors, entity_t entity,
                   int componentId) {
  int idx = GetEntityIndex(entity);
  ComponentStorage_t *cs = &actors->componentStore[componentId];
  if (!cs->occupied[idx])
    return NULL;
  return (uint8_t *)cs->data + (idx * cs->elementSize);
}

void removeComponentFromEntity(EntityManager_t *em, ActorComponents_t *actors,
                               entity_t entity, ComponentID id) {
  int idx = GetEntityIndex(entity);
  ComponentStorage_t *cs = &actors->componentStore[id];

  if (cs->occupied[idx]) {
    cs->occupied[idx] = false;
    cs->count--;
  }

  memset((uint8_t *)cs->data + idx * cs->elementSize, 0, cs->elementSize);
  em->masks[idx] &= ~(1u << id);
}

// return an entire array of a component
void *GetComponentArray(ActorComponents_t *actors, ComponentID cid) {
  return actors->componentStore[cid].data;
}
