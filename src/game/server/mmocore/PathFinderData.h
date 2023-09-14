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

		// Prepare the data
		void Prepare(vec2* pTarget, vec2* pOldTarget)
		{
			// If both pointers are valid
			if(pTarget && pOldTarget)
			{
				*pOldTarget = *pTarget;

			}

			// If the target pointer is valid and the type is random
			if(pTarget && m_Type == TYPE::RANDOM && m_Points.find(0) != m_Points.end())
			{
				*pTarget = m_Points[0];
			}
		}

		// Clear the data
		void Clear()
		{
			m_Size = -1;
			m_Points.clear();
		}

		// Check if the data is empty
		bool Empty() const { return m_Size <= 0; }
	};

	// Get() returns the data stored in the member variable m_Data
	CData& Get() { return m_Data; }

	// IsRequiredUpdatePreparedData() checks if either m_Data is empty or m_FutureData is valid mark for update
	bool IsRequiredUpdatePreparedData() const
	{
		return m_Data.Empty() || m_FutureData.valid();
	}

private:
	CData m_Data {};
	std::future<CData> m_FutureData {};
};

#endif