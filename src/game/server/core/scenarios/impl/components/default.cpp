#include "default.h"

#include <game/server/core/scenarios/base/component_registry.h>

// universal components
template struct ComponentRegistrar<ScenarioMessageComponent>;
template struct ComponentRegistrar<ScenarioFollowCameraComponent>;
template struct ComponentRegistrar<ScenarioMovementConditionComponent>;
template struct ComponentRegistrar<ScenarioTeleportComponent>;
