#include "path_finder.h"

#include <game/collision.h>
#include <game/mapitems.h>
#include <game/layers.h>

template <>
struct std::hash<ivec2>
{
	std::size_t operator()(const ivec2& v) const noexcept { return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1); }
};

CPathFinder::CPathFinder(CCollision* pCollision)
	: m_pLayers(pCollision->GetLayers()), m_pCollision(pCollision)
{
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_Width = m_pLayers->GameLayer()->m_Width;

	m_SearchId = 0;
	m_MapData = MapData(m_Width, m_Height);
	m_vCostSoFar.resize(m_Width * m_Height, std::numeric_limits<int>::max());
	m_vCameFrom.resize(m_Width * m_Height, ivec2{ -1, -1 });
	m_vVisitedSearch.resize(m_Width * m_Height, 0);

	Initialize();
}

CPathFinder::~CPathFinder()
{
	{
		std::lock_guard lock(m_QueueMutex);
		m_Running = false;
	}
	m_Condition.notify_one();
	if(m_WorkerThread.joinable())
	{
		m_WorkerThread.join();
	}
}

void CPathFinder::Initialize()
{
	for(int y = 0; y < m_Height; y++)
	{
		for(int x = 0; x < m_Width; x++)
		{
			// initialize collides
			const vec2 Position(static_cast<float>(x) * 32.f + 16.f, static_cast<float>(y) * 32.f + 16.f);
			m_MapData.SetCollide(x, y, m_pCollision->CheckPoint(Position));

			// initialize teleports
			if(const auto& optTeleValue = m_pCollision->TryGetTeleportOut(Position))
			{
				const int Nx = clamp(round_to_int(optTeleValue->x) / 32, 0, m_Width - 1);
				const int Ny = clamp(round_to_int(optTeleValue->y) / 32, 0, m_Height - 1);
				m_MapData.SetTeleport(x, y, Nx, Ny);
			}
		}
	}

	m_Running = true;
	m_WorkerThread = std::thread(&CPathFinder::PathfindingThread, this);
}

void CPathFinder::RequestPath(PathRequestHandle& Handle, const vec2& Start, const vec2& End)
{
	if(Handle.IsValid())
		return;

	const ivec2 istart((int)Start.x / 32, (int)Start.y / 32);
	const ivec2 iend((int)End.x / 32, (int)End.y / 32);

	PathRequest request;
	request.Start = istart;
	request.End = iend;
	Handle.Future = request.Promise.get_future();

	{
		std::lock_guard lock(m_QueueMutex);
		m_vRequestQueue.push(std::move(request));
	}

	m_Condition.notify_one();
}

void CPathFinder::RequestRandomPath(PathRequestHandle& Handle, const vec2& Start, float Radius)
{
	RequestPath(Handle, Start, GetRandomWaypointRadius(Start, Radius));
}

void CPathFinder::PathfindingThread()
{
	while(m_Running)
	{
		std::vector<PathRequest> currentRequestsBatch;

		{
			std::unique_lock lock(m_QueueMutex);
			m_Condition.wait(lock, [this] { return !m_vRequestQueue.empty() || !m_Running.load(); });

			if(!m_Running.load() && m_vRequestQueue.empty())
				return;

			// get the next request from the queue
			currentRequestsBatch.reserve(m_vRequestQueue.size());
			while(!m_vRequestQueue.empty())
			{
				currentRequestsBatch.push_back(std::move(m_vRequestQueue.front()));
				m_vRequestQueue.pop();
			}
		}

		for(auto& request : currentRequestsBatch)
		{
			std::vector<vec2> vPath = FindPath(request.Start, request.End);
			const bool bSuccess = !vPath.empty();
			auto resultPtr = std::make_unique<PathResult>(PathResult{ std::move(vPath), bSuccess });

			try
			{
				request.Promise.set_value(std::move(resultPtr));
			}
			catch(const std::future_error& e)
			{
				dbg_msg("path_finder", "Future error: %s", e.what());
			}
		}
	}
}

static inline int ManhattanDistance(const ivec2& a, const ivec2& b)
{
	return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

std::vector<vec2> CPathFinder::FindPath(const ivec2& Start, const ivec2& End)
{
	std::vector<vec2> vPath;

	if(Start.x < 0 || Start.x >= m_Width || Start.y < 0 || Start.y >= m_Height ||
		End.x < 0 || End.x >= m_Width || End.y < 0 || End.y >= m_Height)
	{
		//dbg_msg("path_finder", "invalid start/end coordinates.");
		return vPath;
	}

	if(m_MapData.IsCollide(Start.x, Start.y) || m_MapData.IsCollide(End.x, End.y))
		return vPath;

	// skip same
	if(Start == End)
	{
		vPath.emplace_back(static_cast<float>(Start.x) * 32.f + 16.f, static_cast<float>(Start.y) * 32.f + 16.f);
		return vPath;
	}

	// O(1) reset
	++m_SearchId;
	if (m_SearchId == 0)
	{
		std::fill(m_vVisitedSearch.begin(), m_vVisitedSearch.end(), 0u);
		m_SearchId = 1;
	}
	
	const uint32_t SearchId = m_SearchId;
	auto WasVisited = [&](int Index) -> bool
	{
		return m_vVisitedSearch[Index] == SearchId;
	};

	auto GetCost = [&](int Index) -> int
	{
		return WasVisited(Index) ? m_vCostSoFar[Index] : std::numeric_limits<int>::max();
	};

	auto SetState = [&](int Index, int Cost, const ivec2& From)
	{
		m_vVisitedSearch[Index] = SearchId;
		m_vCostSoFar[Index] = Cost;
		m_vCameFrom[Index] = From;
	};

	using Node = std::pair<int, ivec2>;
	auto NodeComparator = [](const Node& a, const Node& b) { return a.first > b.first; };
	std::priority_queue<Node, std::vector<Node>, decltype(NodeComparator)> vFrontier(NodeComparator);

	const int StartIndex = ToIndex(Start);
	const int EndIndex = ToIndex(End);

	SetState(StartIndex, 0, Start);
	vFrontier.emplace(ManhattanDistance(Start, End), Start);

	// search path
	bool Found = false;
	constexpr std::array<ivec2, 4> directions = { {{-1, 0}, {1, 0}, {0, -1}, {0, 1}} };
	while(!vFrontier.empty())
	{
		const Node Top = vFrontier.top();
		vFrontier.pop();

		const ivec2 current = Top.second;
		const int currentIndex = ToIndex(current);
		const int currentCost = GetCost(currentIndex);

		// skip old
		if(Top.first != currentCost + ManhattanDistance(current, End))
			continue;

		if(current == End)
		{
			Found = true;
			break;
		}

		// check if the current tile is a teleport
		if(m_MapData.IsTeleport(current.x, current.y))
		{
			// get the destination of the teleport
			const ivec2 teleportDest = m_MapData.GetTeleportDestination(current.x, current.y);
			const int teleportIndex = ToIndex(teleportDest);
			const int teleportCost = currentCost + 1;

			// add teleport destination to the frontier if it's more optimal
			if (teleportCost < GetCost(teleportIndex))
			{
				SetState(teleportIndex, teleportCost, current);
				const int priority = teleportCost + ManhattanDistance(teleportDest, End);
				vFrontier.emplace(priority, teleportDest);
			}
		}

		// check neighboring tiles
		for (const auto& dir : directions)
		{
			const ivec2 next = { current.x + dir.x, current.y + dir.y };
			if (static_cast<unsigned>(next.x) >= static_cast<unsigned>(m_Width) ||
				static_cast<unsigned>(next.y) >= static_cast<unsigned>(m_Height))
				continue;

			if(m_MapData.IsCollide(next.x, next.y))
				continue;

			const int nextIndex = ToIndex(next);
			const int newCost = currentCost + 1;
			if(newCost < GetCost(nextIndex))
			{
				SetState(nextIndex, newCost, current);
				const int priority = newCost + ManhattanDistance(next, End);
				vFrontier.emplace(priority, next);
			}
		}
	}

	if (Found)
	{
		vPath.reserve(m_vCostSoFar[EndIndex] + 1);
		for (ivec2 current = End; current != Start; current = m_vCameFrom[ToIndex(current)])
		{
			vPath.emplace_back(static_cast<float>(current.x) * 32.f + 16.f, static_cast<float>(current.y) * 32.f + 16.f);
		}

		vPath.emplace_back(static_cast<float>(Start.x) * 32.f + 16.f, static_cast<float>(Start.y) * 32.f + 16.f);
		std::ranges::reverse(vPath);
	}

	return vPath;
}

vec2 CPathFinder::GetRandomWaypointRadius(const vec2& Pos, float Radius) const
{
	const float RadiusSquared = Radius * Radius;
	const int StartX = clamp(static_cast<int>((Pos.x - Radius) / 32.0f), 0, m_Width - 1);
	const int StartY = clamp(static_cast<int>((Pos.y - Radius) / 32.0f), 0, m_Height - 1);
	const int EndX = clamp(static_cast<int>((Pos.x + Radius) / 32.0f), 0, m_Width - 1);
	const int EndY = clamp(static_cast<int>((Pos.y + Radius) / 32.0f), 0, m_Height - 1);

	vec2 selectedPoint = { -1.0f, -1.0f };
	int count = 0;

	for(int y = StartY; y <= EndY; ++y)
	{
		const float yCenter = y * 32.0f + 16.0f;
		const float deltaY = Pos.y - yCenter;
		const float deltaYSquared = deltaY * deltaY;

		for(int x = StartX; x <= EndX; ++x)
		{
			if(!m_MapData.IsCollide(x, y))
			{
				const float xCenter = x * 32.0f + 16.0f;
				const float deltaX = Pos.x - xCenter;
				if(deltaX * deltaX + deltaYSquared <= RadiusSquared)
				{
					++count;
					if(secure_rand() % count == 0)
					{
						selectedPoint = vec2(xCenter, yCenter);
					}
				}
			}
		}
	}

	return selectedPoint;
}