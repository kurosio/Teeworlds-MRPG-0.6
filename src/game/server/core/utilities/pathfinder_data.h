#ifndef GAME_SERVER_CORE_UTILITIES_PATHFINDER_DATA_H
#define GAME_SERVER_CORE_UTILITIES_PATHFINDER_DATA_H

class CPathFinderPrepared
{
	friend class CPathFinder;

public:
	enum Type
	{
		DEFAULT,
		RANDOM
	};

	class CData
	{
	public:
		Type m_Type {};
		ska::unordered_map<int, vec2> m_Points {};

		/*
		ska::unordered_map<int, vec2> CEntityPathNavigator::getFinishedContainer(ska::unordered_map<int, vec2>& pathContainer)
		{
			ska::unordered_map<int, vec2> prepareContainer;
			prepareContainer.emplace(0, pathContainer[0]);

			vec2 Back = prepareContainer[0];
			for(int i = 0; i < (int)pathContainer.size(); i++)
			{
				if(GS()->Collision()->IntersectLineWithInvisible(Back, pathContainer[i], nullptr, nullptr))
				{
					prepareContainer.emplace(prepareContainer.size(), pathContainer[i - 1]);
					Back = pathContainer[i];
				}
			}

			prepareContainer.emplace(prepareContainer.size(), pathContainer.at((int)pathContainer.size() - 1));
			return prepareContainer;
		}
		 */

		// Prepare the data
		void Prepare(vec2* pTarget, vec2* pOldTarget) const
		{
			if(pTarget)
			{
				// If both pointers are valid
				if(pOldTarget)
				{
					*pOldTarget = *pTarget;
				}

				// If the target pointer is valid and the type is random
				if(m_Type == RANDOM && m_Points.count(0) > 0)
				{
					*pTarget = m_Points.at(0);
				}
			}
		}

		// This function returns the last position in a vector of points
		vec2 GetLastPos() const
		{
			// Get the index of the last point in the vector
			int LastIndex = maximum(0, (int)m_Points.size() - 1);

			// Check if the last index exists in the vector
			if(m_Points.find(LastIndex) != m_Points.end())
				return m_Points.at(LastIndex);

			// If the last index does not exist, return an empty vec2
			return {};
		}

		// Clear the data
		void Clear()
		{
			m_Points.clear();
		}

		// Check if the data is empty
		bool Empty() const { return m_Points.empty(); }
	};

	// Get() returns the data stored in the member variable m_Data
	CData& Get() { return m_Data; }

	// IsRequiredPrepare() checks if either m_Data is empty or m_FutureData is valid mark for update
	bool IsRequiredPrepare() const
	{
		// Return true if the data is empty and either the future data is not valid or the result is greater than timeout
		auto Result = m_FutureData.valid() ? m_FutureData.wait_for(std::chrono::microseconds(0)) : std::future_status::timeout;
		return m_Data.Empty() && (!m_FutureData.valid() || (m_FutureData.valid() && Result > std::future_status::timeout));
	}

	// This function checks if m_Data is empty or not
	bool IsPrepared() const
	{
		// If m_Data is empty, return true (not prepared)
		// If m_Data is not empty, return false (prepared)
		return !m_Data.Empty();
	}

private:
	CData m_Data {};
	std::future<CData> m_FutureData {};
};

#endif