#ifndef GAME_SERVER_CORE_TOOLS_PATH_FINDER_RESULT_H
#define GAME_SERVER_CORE_TOOLS_PATH_FINDER_RESULT_H

struct PathResult
{
	std::vector<vec2> Path;
	bool Success;
};

struct PathRequest
{
	ivec2 Start;
	ivec2 End;
	std::promise<PathResult*> Promise;
};

struct PathRequestHandle
{
	std::future<PathResult*> Future;
	std::vector<vec2> vPath;

	bool IsReady() const
	{
		if(Future.valid() && Future.wait_for(std::chrono::microseconds(0)) == std::future_status::ready)
			return true;
		return false;
	}

	bool TryGetPath()
	{
		if(IsReady())
		{
			bool Success = false;
			try
			{
				if(const PathResult* pathResult = Future.get())
				{
					if(pathResult->Success)
					{
						vPath = pathResult->Path;
						Future = std::future<PathResult*>();
						Success = true;
					}
					delete pathResult;
				}
			}
			catch(const std::future_error& e)
			{
				dbg_msg("PathFinder", "future error: %s", e.what());
			}
			return Success;
		}
		return false;
	}

	void Reset()
	{
		Future = std::future<PathResult*>();
		vPath.clear();
	}
};

#endif