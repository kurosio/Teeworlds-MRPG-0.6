#ifndef GAME_SERVER_CORE_TOOLS_EFFECT_MANAGER_H
#define GAME_SERVER_CORE_TOOLS_EFFECT_MANAGER_H

class CEffectManager
{
	std::unordered_map<std::string, int> m_vmEffects;

public:
	bool Add(const char* pEffect, int Ticks, float Chance = 100.f)
	{
		if(Chance < 100.0f && random_float(100.0f) >= Chance)
			return false;

		m_vmEffects[pEffect] = Ticks;
		return false;
	}

	bool Remove(const char* pEffect)
	{
		return m_vmEffects.erase(pEffect) > 0;
	}

	bool RemoveAll()
	{
		if(m_vmEffects.empty())
			return false;

		m_vmEffects.clear();
		return true;
	}

	bool IsActive(const char* pEffect) const
	{
		return m_vmEffects.contains(pEffect);
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