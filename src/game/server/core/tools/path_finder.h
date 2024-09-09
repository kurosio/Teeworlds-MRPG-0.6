#ifndef GAME_SERVER_CORE_TOOLS_PATH_FINDER_H
#define GAME_SERVER_CORE_TOOLS_PATH_FINDER_H

#include "path_finder_result.h"

class BitCollideMap
{
public:
	BitCollideMap() = default;
	BitCollideMap(int width, int height) : m_Width(width), m_Height(height), m_Bits((width* height + 7) / 8, 0) {}
	bool IsCollide(int x, int y) const
	{
		if(x < 0 || x >= m_Width || y < 0 || y >= m_Height)
			return true;

		const size_t index = y * m_Width + x;
		return m_Bits[index / 8] & (1 << (index % 8));
	}
	void Set(int x, int y, bool value)
	{
		if(x < 0 || x >= m_Width || y < 0 || y >= m_Height)
			return;

		const size_t index = y * m_Width + x;
		if(value)
			m_Bits[index / 8] |= (1 << (index % 8));
		else
			m_Bits[index / 8] &= ~(1 << (index % 8));
	}
	const uint8_t* Data() const { return m_Bits.data(); }

private:
	int m_Width {};
	int m_Height {};
	std::vector<uint8_t> m_Bits {};
};

class CPathFinder
{
public:
	CPathFinder(class CCollision* pCollision);
	~CPathFinder();

	void Initialize();
	void RequestPath(PathRequestHandle& Handle, const vec2& Start, const vec2& End);
	void RequestRandomPath(PathRequestHandle& Handle, const vec2& Start, float Radius);

private:
	void PathfindingThread();
	std::vector<vec2> FindPath(const ivec2& Start, const ivec2& End);
	vec2 GetRandomWaypointRadius(const vec2& Pos, float Radius) const;

	int m_Width{};
	int m_Height{};
	BitCollideMap m_vMap{};
	std::vector<int> m_vCostSoFar{};
	std::vector<ivec2> m_vCameFrom{};

	std::queue<PathRequest> m_vRequestQueue{};
	std::condition_variable m_Condition{};
	std::atomic<bool> m_Running{};
	std::thread m_WorkerThread{};
	std::mutex m_QueueMutex{};

	CLayers* m_pLayers{};
	CCollision* m_pCollision{};
};

#endif