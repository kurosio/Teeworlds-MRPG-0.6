#include "PathFinderHandler.h"
#include "PathFinder.h"

inline std::mutex m_MutexPathFinderFunction;

CPathFinderData CHandlerPathFinder::FindThreadPath(const std::shared_ptr<HandleArgsPack>& pHandleData)
{
	HandleArgsPack* pHandle = pHandleData.get();

	if(pHandle && pHandle->IsValid() && length(pHandle->m_StartFrom) > 0 && length(pHandle->m_Search) > 0)
	{
		// guard element, path finder working only with one item TODO: rework
		std::lock_guard QueueLock { m_MutexPathFinderFunction };

		const vec2 StartPos = pHandle->m_StartFrom;
		const vec2 SearchPos = pHandle->m_Search;
		CPathfinder* pPathFinder = pHandle->m_PathFinder;

		// path finder working
		pPathFinder->Init();
		pPathFinder->SetStart(StartPos);
		pPathFinder->SetEnd(SearchPos);
		pPathFinder->FindPath();

		// initilize for future data
		CPathFinderData Data;
		Data.m_Type = CPathFinderData::TYPE::CLASIC;
		Data.m_Size = pPathFinder->m_FinalSize;
		for(int i = Data.m_Size - 1, j = 0; i >= 0; i--, j++)
		{
			Data.m_Points[j] = vec2(pPathFinder->m_lFinalPath[i].m_Pos.x * 32 + 16, pPathFinder->m_lFinalPath[i].m_Pos.y * 32 + 16);
		}

		return Data;
	}

	return {};
}

CPathFinderData CHandlerPathFinder::GetThreadRandomRadiusWaypointTarget(const std::shared_ptr<HandleArgsPack>& pHandleData)
{
	HandleArgsPack* pHandle = pHandleData.get();

	if(pHandle && pHandle->IsValid() && length(pHandle->m_StartFrom) > 0)
	{
		// guard element, path finder working only with one item TODO: rework
		std::lock_guard QueueLock { m_MutexPathFinderFunction };

		const vec2 StartPos = pHandle->m_StartFrom;

		// path finder working
		const vec2 TargetPos = pHandle->m_PathFinder->GetRandomWaypointRadius(StartPos, 800.f);

		// initilize for future data
		CPathFinderData Data;
		Data.m_Type = CPathFinderData::TYPE::RANDOM;
		Data.m_Size = 1;
		Data.m_Points[0] = vec2(TargetPos.x * 32, TargetPos.y * 32);
		return Data;
	}

	return{};
}

bool CHandlerPathFinder::TryGetUpdateData(std::future<CPathFinderData>& pft, CPathFinderData& pData, vec2* pTarget, vec2* pOldTarget)
{
	// check future status
	if(pft.valid() && pft.wait_for(std::chrono::microseconds(0)) == std::future_status::ready)
	{
		pData.Clear();
		pData = pft.get();
		pData.Prepare(pTarget, pOldTarget);
		return true;
	}

	return false;
}
