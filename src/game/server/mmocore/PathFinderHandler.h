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

class CHandlerPathFinder
{
	ThreadPool Pool { 4 };
	struct HandleArgsPack
	{
		class CPathfinder* m_PathFinder;
		vec2 m_StartFrom;
		vec2 m_Search;

		[[nodiscard]] bool IsValid() const { return m_PathFinder; }
	};

	static CPathFinderData FindThreadPath(const ::std::shared_ptr<HandleArgsPack>& pHandleData);
	static CPathFinderData GetThreadRandomRadiusWaypointTarget(const ::std::shared_ptr<HandleArgsPack>& pHandleData);

public:
	template<CPathFinderData::TYPE type>
	std::future<CPathFinderData> Add(CPathfinder* pPathFinder, vec2 StartPos, vec2 SearchPos)
	{
		auto Handle = std::make_shared<HandleArgsPack>(HandleArgsPack({ pPathFinder, StartPos, SearchPos }));

		if constexpr (type == CPathFinderData::TYPE::RANDOM)
			return Pool.enqueue(&GetThreadRandomRadiusWaypointTarget, Handle);
		else
			return Pool.enqueue(&FindThreadPath, Handle);
	}

	static bool TryGetUpdateData(std::future<CPathFinderData>& pft, CPathFinderData& pData, vec2* pTarget, vec2* pOldTarget);
};
#endif
