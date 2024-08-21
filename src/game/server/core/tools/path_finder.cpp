#include "path_finder.h"

#include <game/collision.h>
#include <game/mapitems.h>
#include <game/layers.h>

template <>
struct std::hash<ivec2>
{
	std::size_t operator()(const ivec2& v) const noexcept { return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1); }
};

CPathFinder::CPathFinder(CLayers* Layers, CCollision* Collision)
	: m_pLayers(Layers), m_pCollision(Collision)
{
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_vMap.resize(m_Height, std::vector(m_Width, true));
	m_vHeuristicMap.resize(m_Height, std::vector<int>(m_Width, 0));

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
			m_vMap[y][x] = !m_pCollision->CheckPoint(static_cast<float>(x) * 32.f + 16.f, static_cast<float>(y) * 32.f + 16.f);
			m_vHeuristicMap[y][x] = std::abs(x - m_Width / 2) + std::abs(y - m_Height / 2);
		}
	}

	m_Running = true;
	m_WorkerThread = std::thread(&CPathFinder::PathfindingThread, this);
}

void CPathFinder::RequestPath(PathRequestHandle& Handle, const vec2& Start, const vec2& End)
{
	if(Handle.Future.valid())
		return;

	const ivec2 istart((int)Start.x / 32, (int)Start.y / 32);
	const ivec2 iend((int)End.x / 32, (int)End.y / 32);

	std::lock_guard lock(m_QueueMutex);
	PathRequest request;
	request.Start = istart;
	request.End = iend;

	std::future<PathResult> temp_future = request.Promise.get_future();
	m_vRequestQueue.push(std::move(request));
	m_Condition.notify_one();

	Handle.Future = std::move(temp_future);
}

void CPathFinder::RequestRandomPath(PathRequestHandle& Handle, const vec2& Start, float Radius)
{
	RequestPath(Handle, Start, GetRandomWaypointRadius(Start, Radius));
}

void CPathFinder::PathfindingThread()
{
	while(m_Running)
	{
		PathRequest request;
		{
			std::unique_lock lock(m_QueueMutex);
			m_Condition.wait(lock, [this] { return !m_vRequestQueue.empty() || !m_Running; });

			if(!m_Running)
				return;

			// get the next request from the queue
			request = std::move(m_vRequestQueue.front());
			m_vRequestQueue.pop();
		}

		std::vector<vec2> vPath = FindPath(request.Start, request.End);
		const bool Success = !vPath.empty();
		request.Promise.set_value({ vPath, Success });
	}
}

bool CPathFinder::IsWalkable(const ivec2& Pos) const
{
	if(Pos.x < 0 || Pos.x >= m_Width || Pos.y < 0 || Pos.y >= m_Height)
		return false;

	return m_vMap[Pos.y][Pos.x];
}

std::vector<vec2> CPathFinder::FindPath(const ivec2& Start, const ivec2& End) const
{
	std::vector<vec2> vPath;
	if(!IsWalkable(Start) || !IsWalkable(End))
		return vPath;

	// initialize variables
	using Node = std::pair<int, ivec2>;
	auto NodeComparator = [](const Node& a, const Node& b) { return a.first > b.first; };
	std::priority_queue<Node, std::vector<Node>, decltype(NodeComparator)> vFrontier(NodeComparator);
	std::unordered_map<ivec2, ivec2> vCameFrom;
	std::unordered_map<ivec2, int> vCostSoFar;

	vFrontier.emplace(0, Start);
	vCameFrom[Start] = Start;
	vCostSoFar[Start] = 0;

	// search path
	while(!vFrontier.empty())
	{
		ivec2 current = vFrontier.top().second;
		vFrontier.pop();

		if(current == End)
			break;

		ivec2 neighbors[4] = { {current.x - 1, current.y}, {current.x + 1, current.y}, {current.x, current.y - 1}, {current.x, current.y + 1} };
		for(const auto& next : neighbors)
		{
			if(!IsWalkable(next))
				continue;

			const int newCost = vCostSoFar[current] + 1;
			if(!vCostSoFar.contains(next) || newCost < vCostSoFar[next])
			{
				vCostSoFar[next] = newCost;
				int priority = newCost + m_vHeuristicMap[next.y][next.x];
				vFrontier.emplace(priority, next);
				vCameFrom[next] = current;
			}
		}
	}

	// construct path
	if(vCameFrom.contains(End))
	{
		for(ivec2 current = End; current != Start; current = vCameFrom[current])
			vPath.emplace_back(static_cast<float>(current.x) * 32.f + 16.f, static_cast<float>(current.y) * 32.f + 16.f);

		vPath.emplace_back(static_cast<float>(Start.x) * 32.f + 16.f, static_cast<float>(Start.y) * 32.f + 16.f);
		std::ranges::reverse(vPath);
	}

	return vPath;
}

vec2 CPathFinder::GetRandomWaypointRadius(const vec2& Pos, float Radius) const
{
	std::vector<vec2> vPossibleWaypoints;
	const float Range = Radius / 2.0f;
	const int StartX = clamp(static_cast<int>((Pos.x - Range) / 32.0f), 0, m_Width - 1);
	const int StartY = clamp(static_cast<int>((Pos.y - Range) / 32.0f), 0, m_Height - 1);
	const int EndX = clamp(static_cast<int>((Pos.x + Range) / 32.0f), 0, m_Width - 1);
	const int EndY = clamp(static_cast<int>((Pos.y + Range) / 32.0f), 0, m_Height - 1);

	for(int y = StartY; y <= EndY; y++)
	{
		for(int x = StartX; x <= EndX; x++)
		{
			if(IsWalkable({ x, y }))
				vPossibleWaypoints.emplace_back(static_cast<float>(x) * 32.0f + 16.0f, static_cast<float>(y) * 32.0f + 16.0f);
		}
	}

	if(!vPossibleWaypoints.empty())
	{
		const int Rand = secure_rand() % vPossibleWaypoints.size();
		return vPossibleWaypoints[Rand];
	}
	return {0.0f, 0.0f};
}