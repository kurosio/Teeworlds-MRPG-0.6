/* Pathfind class by Sushi */
#include "pathfinder.h"

#include <game/collision.h>
#include <game/layers.h>
#include <game/mapitems.h>

CPathFinder::CPathFinder(CLayers* Layers, CCollision* Collision) : m_pLayers(Layers), m_pCollision(Collision)
{
	m_ClosedNodes = 0;
	m_FinalSize = 0;
	m_StartIndex = -1;
	m_EndIndex = -1;

	m_LayerWidth = m_pLayers->GameLayer()->m_Width;
	m_LayerHeight = m_pLayers->GameLayer()->m_Height;

	// set size for array
	m_lMap.resize(m_LayerWidth * m_LayerHeight);
	m_lFinalPath.resize(MAX_WAY_CALC);

	// init size for heap
	m_Open.SetSize(4 * MAX_WAY_CALC);

	int ID = 0;
	for(int i = 0; i < m_LayerHeight; i++)
	{
		for(int j = 0; j < m_LayerWidth; j++)
		{
			CNode Node;
			Node.m_Pos = vec2(j, i);
			Node.m_Parent = -1;
			Node.m_ID = ID;
			Node.m_G = 0;
			Node.m_H = 0;
			Node.m_F = 0;
			Node.m_IsOpen = false;
			if(m_pCollision->CheckPoint((float)j * 32.f + 16.f, (float)i * 32.f + 16.f))
				Node.m_IsClosed = true;
			else
				Node.m_IsClosed = false;

			// add the node to the list
			m_lMap[i * m_LayerWidth + j] = Node;
			ID++;
		}
	}

	// create handler
	m_pHandler = new CHandler(this);
}

CPathFinder::~CPathFinder()
{
	m_lMap.clear();
	m_lFinalPath.clear();

	// delete handler
	delete m_pHandler;
	m_pHandler = nullptr;
}

void CPathFinder::Init()
{
	m_ClosedNodes = 0;
	m_FinalSize = 0;
	m_Open.MakeEmpty();
}

void CPathFinder::SetStart(vec2 Pos)
{
	int Index = GetIndex(Pos.x, Pos.y);
	m_ClosedNodes++;
	m_StartIndex = Index;
}

void CPathFinder::SetEnd(vec2 Pos)
{
	int Index = GetIndex(Pos.x, Pos.y);
	m_EndIndex = Index;
}


int CPathFinder::GetIndex(int x, int y) const
{
	int Nx = clamp(x / 32, 0, m_LayerWidth - 1);
	int Ny = clamp(y / 32, 0, m_LayerHeight - 1);
	return Ny * m_LayerWidth + Nx;
}

void CPathFinder::FindPath()
{
	if(m_StartIndex == -1 || m_EndIndex == -1)
		return;

	std::map<int, CNode> lNodes;
	lNodes[m_StartIndex].m_Parent = START;
	lNodes[m_StartIndex].m_IsClosed = true;
	lNodes[m_EndIndex].m_Parent = END;

	int CurrentIndex = m_StartIndex;
	int neighborOffsets[] = { 1, -1, m_LayerWidth, -m_LayerWidth };
	while(m_ClosedNodes < MAX_WAY_CALC && m_lMap[CurrentIndex].m_ID != m_EndIndex)
	{
		for(const int neighborOffset : neighborOffsets)
		{
			int WorkingIndex = CurrentIndex + neighborOffset;
			if(WorkingIndex >= 0 && WorkingIndex < (int)m_lMap.size())
			{
				if(!m_lMap[WorkingIndex].m_IsClosed)
				{
					CNode& WorkingNode = lNodes[WorkingIndex];
					WorkingNode = m_lMap[WorkingIndex];

					int tentativeG = lNodes[CurrentIndex].m_G + 1;
					if(!WorkingNode.m_IsOpen || tentativeG < WorkingNode.m_G)
					{
						WorkingNode.m_Parent = CurrentIndex;
						WorkingNode.m_G = tentativeG;
						WorkingNode.m_H = std::abs(WorkingNode.m_Pos.x - lNodes[m_EndIndex].m_Pos.x) + std::abs(WorkingNode.m_Pos.y - lNodes[m_EndIndex].m_Pos.y);
						WorkingNode.m_F = WorkingNode.m_G + WorkingNode.m_H;
						if(!WorkingNode.m_IsOpen)
						{
							WorkingNode.m_IsOpen = true;
							m_Open.Insert(WorkingNode);
						}
						else
						{
							m_Open.Replace(WorkingNode);
						}
					}
				}
			}
		}

		if(m_Open.GetSize() < 1)
			break;

		CurrentIndex = m_Open.GetMin()->m_ID;
		m_Open.RemoveMin();

		lNodes[CurrentIndex] = m_lMap[CurrentIndex];
		lNodes[CurrentIndex].m_IsClosed = true;
		m_ClosedNodes++;
	}

	while(lNodes[CurrentIndex].m_ID != m_StartIndex)
	{
		m_lFinalPath[m_FinalSize] = lNodes[CurrentIndex];
		m_FinalSize++;
		CurrentIndex = lNodes[CurrentIndex].m_Parent;
		if(CurrentIndex < 0)
			break;
	}
}

vec2 CPathFinder::GetRandomWaypoint()
{
	array<vec2> lPossibleWaypoints;
	for(int i = 0; i < m_LayerHeight; i++)
	{
		for(int j = 0; j < m_LayerWidth; j++)
		{
			if(m_lMap[i * m_LayerWidth + j].m_IsClosed)
				continue;

			lPossibleWaypoints.add(m_lMap[i * m_LayerWidth + j].m_Pos);
		}
	}

	if(lPossibleWaypoints.size())
	{
		int Rand = secure_rand() % lPossibleWaypoints.size();
		return lPossibleWaypoints[Rand];
	}
	return vec2(0, 0);
}

vec2 CPathFinder::GetRandomWaypointRadius(vec2 Pos, float Radius)
{
	array<vec2> lPossibleWaypoints;
	float Range = (Radius / 2.0f);
	int StartX = clamp((int)((Pos.x - Range) / 32.0f), 0, m_LayerWidth - 1);
	int StartY = clamp((int)((Pos.y - Range) / 32.0f), 0, m_LayerHeight - 1);
	int EndX = clamp((int)((Pos.x + Range) / 32.0f), 0, m_LayerWidth - 1);
	int EndY = clamp((int)((Pos.y + Range) / 32.0f), 0, m_LayerHeight - 1);

	for(int i = StartY; i < EndY; i++)
	{
		for(int j = StartX; j < EndX; j++)
		{
			if(m_lMap[i * m_LayerWidth + j].m_IsClosed)
				continue;

			lPossibleWaypoints.add(m_lMap[i * m_LayerWidth + j].m_Pos);
		}
	}

	if(lPossibleWaypoints.size())
	{
		int Rand = secure_rand() % lPossibleWaypoints.size();
		return lPossibleWaypoints[Rand];
	}
	return vec2(0, 0);
}

/*
 * CHandler
 * - Synchronized path search in a separate thread pool
 */

CPathFinderPrepare::CData CPathFinder::CHandler::CallbackFindPath(const std::shared_ptr<HandleArgsPack>& pHandleData)
{
	HandleArgsPack* pHandle = pHandleData.get();

	if(pHandle && pHandle->IsValid() && !is_negative_vec(pHandle->m_StartFrom) && !is_negative_vec(pHandle->m_Search))
	{
		// guard element, path finder working only with one item TODO: rework
		std::lock_guard QueueLock { pHandle->m_PathFinder->m_mtxLimitedOnceUse };

		const vec2 StartPos = pHandle->m_StartFrom;
		const vec2 SearchPos = pHandle->m_Search;
		CPathFinder* pPathFinder = pHandle->m_PathFinder;

		// path finder working
		pPathFinder->Init();
		pPathFinder->SetStart(StartPos);
		pPathFinder->SetEnd(SearchPos);
		pPathFinder->FindPath();

		// initilize for future data
		CPathFinderPrepare::CData Data;
		Data.m_Type = CPathFinderPrepare::DEFAULT;
		Data.m_Points.reserve(pPathFinder->m_FinalSize);
		for(int i = pPathFinder->m_FinalSize - 1, j = 0; i >= 0; i--, j++)
		{
			Data.m_Points[j] = vec2(pPathFinder->m_lFinalPath[i].m_Pos.x * 32 + 16, pPathFinder->m_lFinalPath[i].m_Pos.y * 32 + 16);
		}

		return Data;
	}

	return {};
}

CPathFinderPrepare::CData CPathFinder::CHandler::CallbackRandomRadiusWaypoint(const std::shared_ptr<HandleArgsPack>& pHandleData)
{
	HandleArgsPack* pHandle = pHandleData.get();

	if(pHandle && pHandle->IsValid() && !is_negative_vec(pHandle->m_StartFrom))
	{
		// guard element, path finder working only with one item TODO: rework
		std::lock_guard QueueLock { pHandle->m_PathFinder->m_mtxLimitedOnceUse };

		const vec2 StartPos = pHandle->m_StartFrom;

		// path finder working
		const vec2 TargetPos = pHandle->m_PathFinder->GetRandomWaypointRadius(StartPos, pHandle->m_Radius);

		// initilize for future data
		CPathFinderPrepare::CData Data;
		Data.m_Type = CPathFinderPrepare::RANDOM;
		Data.m_Points[0] = vec2(TargetPos.x * 32, TargetPos.y * 32);
		return Data;
	}

	return{};
}

bool CPathFinder::CHandler::TryGetPreparedData(CPathFinderPrepare* pPrepare, vec2* pTarget, vec2* pOldTarget)
{
	// check future status
	if(pPrepare && pPrepare->m_FutureData.valid() && pPrepare->m_FutureData.wait_for(std::chrono::microseconds(0)) == std::future_status::ready)
	{
		pPrepare->m_Data.Clear();
		pPrepare->m_Data = pPrepare->m_FutureData.get();
		pPrepare->m_Data.Prepare(pTarget, pOldTarget);
		pPrepare->m_FutureData = {};
	}

	return pPrepare && !pPrepare->m_Data.Empty();
}
