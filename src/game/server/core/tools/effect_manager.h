#ifndef GAME_SERVER_CORE_TOOLS_EFFECT_MANAGER_H
#define GAME_SERVER_CORE_TOOLS_EFFECT_MANAGER_H

enum class ECharacterEffect {
	INVALID,
	SLOWNESS,
	STUN,
	POISON,
	LAST_STAND,
	FIRE,
};

static const char *EffectName(const ECharacterEffect Effect) {
	switch (Effect) {
		case ECharacterEffect::SLOWNESS:
			return "Slowness";
		case ECharacterEffect::STUN:
			return "Stun";
		case ECharacterEffect::POISON:
			return "Poison";
		case ECharacterEffect::LAST_STAND:
			return "LastStand";
		case ECharacterEffect::FIRE:
			return "Fire";
		default:
			return "Invalid";
	}
	return "Invalid";
}

static ECharacterEffect EffectFromName(const char *pName) {
	if(str_comp_nocase(pName, "Slowness") == 0)
		return ECharacterEffect::SLOWNESS;
	if(str_comp_nocase(pName, "Stun") == 0)
		return ECharacterEffect::STUN;
	if(str_comp_nocase(pName, "Poison") == 0)
		return ECharacterEffect::POISON;
	if(str_comp_nocase(pName, "LastStand") == 0)
		return ECharacterEffect::LAST_STAND;
	if(str_comp_nocase(pName, "Fire") == 0)
		return ECharacterEffect::FIRE;
	return ECharacterEffect::INVALID;
}

static ECharacterEffect EffectFromName(const std::string& Name) {
	return EffectFromName(Name.c_str());
}

class CEffectManager
{
	std::unordered_map<ECharacterEffect, int> m_vmEffects;

public:
	bool Add(const ECharacterEffect Effect, const int Ticks, const float Chance = 100.f)
	{
		if(Chance < 100.0f && random_float(100.0f) >= Chance)
			return false;

		m_vmEffects[Effect] = Ticks;
		return true;
	}

	bool Remove(const ECharacterEffect Effect)
	{
		return m_vmEffects.erase(Effect) > 0;
	}

	bool RemoveAll()
	{
		if(m_vmEffects.empty())
			return false;

		m_vmEffects.clear();
		return true;
	}

	bool IsActive(const ECharacterEffect Effect) const
	{
		return m_vmEffects.contains(Effect);
	}

	void PostTick()
	{
		for(auto it = m_vmEffects.begin(); it != m_vmEffects.end();)
		{
			if(--it->second <= 0)
			{
				it = m_vmEffects.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
};

#endif