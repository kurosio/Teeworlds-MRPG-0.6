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

	m_MapData = MapData(m_Width, m_Height);
	m_vCostSoFar.resize(m_Width * m_Height, std::numeric_limits<int>::max());
	m_vCameFrom.resize(m_Width * m_Height, ivec2 { -1, -1 });
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
	if(Handle.Future.valid())
		return;

	const ivec2 istart((int)Start.x / 32, (int)Start.y / 32);
	const ivec2 iend((int)End.x / 32, (int)End.y / 32);

	std::lock_guard lock(m_QueueMutex);
	PathRequest request;
	request.Start = istart;
	request.End = iend;

	std::future<PathResult*> temp_future = request.Promise.get_future();
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
		std::vector<PathRequest> requests;

		{
			std::unique_lock lock(m_QueueMutex);
			m_Condition.wait(lock, [this] { return !m_vRequestQueue.empty() || !m_Running; });

			if(!m_Running)
				return;

			// get the next request from the queue
			while(!m_vRequestQueue.empty())
			{
				requests.push_back(std::move(m_vRequestQueue.front()));
				m_vRequestQueue.pop();
			}
		}

		for(auto& request : requests)
		{
			std::vector<vec2> vPath = FindPath(request.Start, request.End);
			const bool Success = !vPath.empty();

			auto result = new PathResult({ vPath, Success });
			request.Promise.set_value(result);
		}
	}
}

int ManhattanDistance(const ivec2& a, const ivec2& b)
{
	return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

std::vector<vec2> CPathFinder::FindPath(const ivec2& Start, const ivec2& End)
{
	std::vector<vec2> vPath;
	if(m_MapData.IsCollide(Start.x, Start.y) || m_MapData.IsCollide(End.x, End.y))
		return vPath;

	// initialize variables
	std::ranges::fill(m_vCostSoFar, std::numeric_limits<int>::max());
	auto ToIndex = [this](const ivec2& pos)
	{
		return pos.y * m_Width + pos.x;
	};

	using Node = std::pair<int, ivec2>;
	auto NodeComparator = [](const Node& a, const Node& b) { return a.first > b.first; };
	std::priority_queue<Node, std::vector<Node>, decltype(NodeComparator)> vFrontier(NodeComparator);

	vFrontier.emplace(0, Start);
	m_vCostSoFar[ToIndex(Start)] = 0;
	m_vCameFrom[ToIndex(Start)] = Start;
	m_vCameFrom[ToIndex(End)] = { -1, -1 };

	// search path
	constexpr std::array<ivec2, 4> directions = { {{-1, 0}, {1, 0}, {0, -1}, {0, 1}} };
	while(!vFrontier.empty())
	{
		ivec2 current = vFrontier.top().second;
		vFrontier.pop();

		if(current == End)
			break;

		const int currentIndex = ToIndex(current);

		// check if the current tile is a teleport
		if(m_MapData.IsTeleport(current.x, current.y))
		{
			// get the destination of the teleport
			ivec2 teleportDest = m_MapData.GetTeleportDestination(current.x, current.y);
			const int teleportIndex = ToIndex(teleportDest);
			const int teleportCost = m_vCostSoFar[currentIndex] + 1;

			// add teleport destination to the frontier if it's more optimal
			if(teleportCost < m_vCostSoFar[teleportIndex])
			{
				m_vCostSoFar[teleportIndex] = teleportCost;
				int priority = teleportCost + ManhattanDistance(teleportDest, End);
				vFrontier.emplace(priority, teleportDest);
				m_vCameFrom[teleportIndex] = current;
			}
		}

		// check neighboring tiles
		for(const auto& dir : directions)
		{
			ivec2 next = { current.x + dir.x, current.y + dir.y };
			if(next.x < 0 || next.x >= m_Width || next.y < 0 || next.y >= m_Height)
				continue;

			if(m_MapData.IsCollide(next.x, next.y))
				continue;

			const int nextIndex = ToIndex(next);
			const int newCost = m_vCostSoFar[currentIndex] + 1;
			if(newCost < m_vCostSoFar[nextIndex])
			{
				m_vCostSoFar[nextIndex] = newCost;
				int priority = newCost + ManhattanDistance(next, End);
				vFrontier.emplace(priority, next);
				m_vCameFrom[nextIndex] = current;
			}
		}
	}

	if(m_vCameFrom[ToIndex(End)] != ivec2 { -1, -1 })
	{
		vPath.reserve(ManhattanDistance(Start, End));
		for(ivec2 current = End; current != Start; current = m_vCameFrom[ToIndex(current)])
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