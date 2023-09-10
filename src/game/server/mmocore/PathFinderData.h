#ifndef GAME_SERVER_MMOCORE_PATHFINDERHANDLER_H
#define GAME_SERVER_MMOCORE_PATHFINDERHANDLER_H

class CPathFinderData
{
public:
	enum class TYPE : int
	{
		CLASIC,
		RANDOM
	};

	int m_Size{};
	TYPE m_Type{};
	std::map<int, vec2> m_Points{};

	void Prepare(vec2* pTarget, vec2* pOldTarget)
	{
		if(pTarget && pOldTarget)
		{
			*pOldTarget = *pTarget;

		}

		if(pTarget && m_Type == TYPE::RANDOM && m_Points.find(0) != m_Points.end())
		{
			*pTarget = m_Points[0];
		}
	}

	void Clear()
	{
		m_Size = -1;
		m_Points.clear();
	}

	[[nodiscard]] bool IsValid() const { return m_Size > 0; }
};


#endif
