#include "default.h"

#include <game/server/core/scenarios/base/component_registry.h>

// universal components
template struct ComponentRegistrar<ScenarioWaitComponent>;
template struct ComponentRegistrar<ScenarioBranchRandomComponent>;
template struct ComponentRegistrar<ScenarioMessageComponent>;
template struct ComponentRegistrar<ScenarioFollowCameraComponent>;
template struct ComponentRegistrar<ScenarioMovementConditionComponent>;
template struct ComponentRegistrar<ScenarioTeleportComponent>;
template struct ComponentRegistrar<ScenarioMovingDisableComponent>;
template struct ComponentRegistrar<ScenarioQuestActionComponent>;
template struct ComponentRegistrar<ScenarioQuestConditionComponent>;
template struct ComponentRegistrar<ScenarioEmoteComponent>;
template struct ComponentRegistrar<ScenarioDefeatMobsComponent>;
template struct ComponentRegistrar<ScenarioGroupFlagsComponent>;
