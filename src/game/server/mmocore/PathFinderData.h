#ifndef GAME_SERVER_MMOCORE_PATHFINDERHANDLER_H
#define GAME_SERVER_MMOCORE_PATHFINDERHANDLER_H

class CPathFinderPrepared
{
	friend class CPathFinder;

public:
	enum class TYPE : int
	{
		DEFAULT,
		RANDOM
	};

	class CData
	{
	public:
		int m_Size {};
		TYPE m_Type {};
		std::map<int, vec2> m_Points {};

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

		bool Empty() const { return m_Size <= 0; }
	};

	CData& Get() { return m_Data; }

	/*
	 * returns true if the data requires updating from prepared data or if the data is empty
	 */
	bool IsRequiredUpdatePreparedData() const
	{
		return m_Data.Empty() || m_FutureData.valid();
	}

private:
	CData m_Data;
	std::future<CData> m_FutureData;
};

#endif
