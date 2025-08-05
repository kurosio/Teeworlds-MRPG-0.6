#include "default.h"

#include <game/server/core/scenarios/base/component_registry.h>

// universal components
static ComponentRegistrar<ScenarioWaitComponent> g_cregistrar("wait");
static ComponentRegistrar<ScenarioMessageComponent> g_cmessage_registrar("message");
static ComponentRegistrar<ScenarioFollowCameraComponent> g_ccamera_registrar("follow_camera");
static ComponentRegistrar<ScenarioMovementConditionComponent> g_cmovement_registrar("condition_movement");
static ComponentRegistrar<ScenarioTeleportComponent> g_cteleport_registrar("teleport");
