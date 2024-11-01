#ifndef GAME_SERVER_CORE_TOOLS_EFFECT_MANAGER_H
#define GAME_SERVER_CORE_TOOLS_EFFECT_MANAGER_H

class CEffectManager
{
	ska::unordered_map<std::string, int> m_vmEffects {};

public:
	bool Add(const char* pEffect, int Ticks, float Chance = 100.f)
	{
		const float RandomChance = random_float(100.0f);
		if(RandomChance < Chance)
		{
			m_vmEffects[pEffect] = Ticks;
			return true;
		}

		return false;
	}

	bool Remove(const char* pEffect)
	{
		if(m_vmEffects.erase(pEffect) > 0)
			return true;

		return false;
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
		return m_vmEffects.find(pEffect) != m_vmEffects.end();
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