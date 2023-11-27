#ifndef GAME_PATHFIND_H
#define GAME_PATHFIND_H

#include <game/layers.h>
#include "BinaryHeap.h"

#include "PathFinderData.h"

#define MAX_WAY_CALC 50000

/*
 * Example:
 * The handler works in sync while not restricting the main thread
 * Handler has its own pool with the number of threads, threads are static regardless of the number of instances of the class
 * To use it, first set the task with Prepare, when ready TryGetPrepared will return the data that can be worked with.
 * There is no danger of working with unprepared data.
 */

 // player object
class CPathFinder
{
	// handler
	class CHandler
	{
		inline static ThreadPool m_Pool { 4 };
		CPathFinder* m_pPathFinder;

		struct HandleArgsPack
		{
			CPathFinder* m_PathFinder {};
			vec2 m_StartFrom {};
			vec2 m_Search {};
			float m_Radius {};

			[[nodiscard]] bool IsValid() const
			{
				return m_PathFinder;
			}
		};

		static CPathFinderPrepared::CData CallbackFindPath(const ::std::shared_ptr<HandleArgsPack>& pHandleData);
		static CPathFinderPrepared::CData CallbackRandomRadiusWaypoint(const ::std::shared_ptr<HandleArgsPack>& pHandleData);

	public:
		explicit CHandler(CPathFinder* pPathFinder) : m_pPathFinder(pPathFinder) {}

		// This function prepares the path finder for a search by creating a future task and storing it in the provided CPathFinderPrepared object.
		// The type template parameter determines the type of search to be performed.
		// The function returns true if the preparation is starting, meaning that a new future task is created.
		// Otherwise, it returns false, indicating that the preparation is already in progress.
		template<CPathFinderPrepared::Type type>
		bool Prepare(CPathFinderPrepared* pPrepare, vec2 StartPos, vec2 SearchPos, float Radius = 800.0f) const
		{
			bool StartingPrepare = !pPrepare->m_FutureData.valid(); 

			// Check if the preparation is starting
			if(StartingPrepare)
			{
				// Create a shared pointer to a HandleArgsPack object with the necessary arguments for the callback function
				auto Handle = std::make_shared<HandleArgsPack>(HandleArgsPack({ m_pPathFinder, StartPos, SearchPos, Radius }));

				// Enqueue a future task based on the type of search
				if constexpr(type == CPathFinderPrepared::Type::RANDOM)
				{
					pPrepare->m_FutureData = m_Pool.enqueue(&CallbackRandomRadiusWaypoint, Handle);
				}
				else
				{
					pPrepare->m_FutureData = m_Pool.enqueue(&CallbackFindPath, Handle);
				}
			}

			return StartingPrepare;
		}

		// Function: TryGetPreparedData
		// Returns: bool
		// Parameters:
		// - pPrepare: A pointer to a CPathFinderPrepared object that will be populated with prepared data if available.
		// - pTarget (optional): A pointer to a vec2 object that will be populated with the target position if available.
		// - pOldTarget (optional): A pointer to a vec2 object that will be populated with the old target position if available.
		// 
		// This function attempts to retrieve prepared data for pathfinding. If prepared data is available, it populates the pPrepare object with the data and returns true.
		// If pTarget is provided, it also populates the pTarget object with the target position and returns true.
		// If pOldTarget is provided, it also populates the pOldTarget object with the old target position and returns true.
		// If prepared data is not available, it returns false.
		bool TryGetPreparedData(CPathFinderPrepared* pPrepare, vec2* pTarget = nullptr, vec2* pOldTarget = nullptr);
	};

	friend class CHandler;
	CHandler* m_pHandler {};

public:
	CPathFinder(CLayers* Layers, class CCollision* Collision);
	~CPathFinder();

	struct CNode
	{
		vec2 m_Pos;
		int m_Parent;
		int m_ID;
		int m_G;
		int m_H;
		int m_F;
		bool m_IsCol;
		bool m_IsClosed;
		bool m_IsOpen;

		bool operator<(const CNode& Other) const { return (this->m_F < Other.m_F); }
		bool operator==(const CNode& Other) const { return (this->m_ID < Other.m_ID); }
	};

	void Init();
	void SetStart(vec2 Pos);
	void SetEnd(vec2 Pos);

	std::mutex m_mtxLimitedOnceUse;
	int GetIndex(int x, int y) const;
	CHandler* Handle() const { return m_pHandler; }

	vec2 GetRandomWaypoint();
	vec2 GetRandomWaypointRadius(vec2 Pos, float Radius);

	void FindPath();

	array<CNode> m_lMap;
	array<CNode> m_lFinalPath;

	int m_FinalSize;
private:
	enum
	{
		START = -3,
		END = -4,
	};

	CLayers* m_pLayers;
	CCollision* m_pCollision;

	array<CNode> m_lNodes;

	// binary heap for open nodes
	CBinaryHeap<CNode> m_Open;

	int m_StartIndex;
	int m_EndIndex;

	int m_LayerWidth;
	int m_LayerHeight;

	// fake array sizes
	int m_ClosedNodes;
};

#endif
