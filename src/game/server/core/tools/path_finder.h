#ifndef GAME_SERVER_CORE_TOOLS_PATH_FINDER_H
#define GAME_SERVER_CORE_TOOLS_PATH_FINDER_H

#include "path_finder_result.h"

class CPathFinder
{
public:
    CPathFinder(CLayers* Layers, class CCollision* Collision);
    ~CPathFinder();

    void Initialize();
    void RequestPath(PathRequestHandle& Handle, const vec2& Start, const vec2& End);
    void RequestRandomPath(PathRequestHandle& Handle, const vec2& Start, float Radius);

private:
    void PathfindingThread();
    std::vector<vec2> FindPath(const ivec2& Start, const ivec2& End) const;
    bool IsWalkable(const ivec2& Pos) const;
    vec2 GetRandomWaypointRadius(const vec2& Pos, float Radius) const;

    int m_Width;
    int m_Height;
    std::vector<std::vector<bool>> m_vMap;
    std::vector<std::vector<int>> m_vHeuristicMap;

    std::queue<PathRequest> m_vRequestQueue;
    std::condition_variable m_Condition;
    std::atomic<bool> m_Running;
    std::thread m_WorkerThread;
    std::mutex m_QueueMutex;

    CLayers* m_pLayers;
    CCollision* m_pCollision;
};

#endif
