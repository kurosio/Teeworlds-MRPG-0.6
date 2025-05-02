#ifndef GAME_SERVER_ENTITIES_AI_CORE_GET_VALID_AI_UTIL_H
#define GAME_SERVER_ENTITIES_AI_CORE_GET_VALID_AI_UTIL_H

class CBaseAI;
class CPlayerBot;

template <typename TargetAI = CBaseAI>
TargetAI* GetValidAI(CPlayerBot* pStartObject);

#endif
