#ifndef GAME_SERVER_CORE_TOOLS_PATH_FINDER_VERTEX_H
#define GAME_SERVER_CORE_TOOLS_PATH_FINDER_VERTEX_H

// vertex graph
class PathFinderVertex
{
	int m_numVertices{};
	bool m_initilized{};
	std::list<int>* m_adjLists{};

	// Breadth-First Search Algorithm
	bool algoritmBFS(int startVertex, int endVertex, std::vector<int>& paPath) const
	{
		// check if start and end vertices are valid
		if(startVertex < 0 || startVertex >= m_numVertices || endVertex < 0 || endVertex >= m_numVertices)
		{
			dbg_msg("BFS", "invalid start (%d) or end (%d) vertex", startVertex, endVertex);
			return false;
		}

		std::vector visited(m_numVertices, false);
		visited[startVertex] = true;

		std::queue<int> queue;
		queue.push(startVertex);

		std::vector parent(m_numVertices, -1);
		while(!queue.empty())
		{
			const int currVertex = queue.front();
			queue.pop();

			if(currVertex == endVertex)
			{
				int node = currVertex;

				while(node != -1)
				{
					paPath.push_back(node);
					node = parent[node];
				}

				std::ranges::reverse(paPath);
				return true;
			}

			for(int adjVertex : m_adjLists[currVertex])
			{
				if(!visited[adjVertex])
				{
					visited[adjVertex] = true;
					queue.push(adjVertex);
					parent[adjVertex] = currVertex;
				}
			}
		}

		return false;
	}

public:
	PathFinderVertex() = default;
	~PathFinderVertex()
	{
		clear();
	}

	// initialize the graph with the given number of vertices
	void init(int vertices)
	{
		m_initilized = true;
		m_numVertices = vertices;
		m_adjLists = new std::list<int>[m_numVertices];
	}

	// clear all data and change state to uninitilized
	void clear()
	{
		m_initilized = false;
		m_numVertices = 0;

		if(m_adjLists)
		{
			delete[] m_adjLists;
			m_adjLists = nullptr;
		}
	}

	// add an edge between two vertices (undirected)
	void addEdge(int from, int to) const
	{
		m_adjLists[from].push_back(to);
		m_adjLists[to].push_back(from);
	}

	// find a path from startVertex to endVertex using BFS algorithm
	std::vector<int> findPath(int startVertex, int endVertex) const
	{
		std::vector<int> path;
		algoritmBFS(startVertex, endVertex, path);
		return path;
	}

	// check if the graph is initialized
	bool isInitilized() const { return m_initilized; }
};

#endif //GAME_SERVER_MMO_UTILS_TIME_PERIOD_DATA_H
