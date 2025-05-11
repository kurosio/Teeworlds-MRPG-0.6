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
	std::promise<std::unique_ptr<PathResult>> Promise;
};

struct PathRequestHandle
{
	std::future<std::unique_ptr<PathResult>> Future{};
	std::vector<vec2> vPath {};

	bool IsValid() const
	{
		return Future.valid();
	}

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
			try
			{
				std::unique_ptr<PathResult> resultPtr = Future.get();
				if(resultPtr && resultPtr->Success)
				{
					vPath = std::move(resultPtr->Path);
					return true;
				}
			}
			catch(const std::future_error& e)
			{
				dbg_msg("path_finder", "future error: %s", e.what());
			}

			return false;
		}

		return false;
	}

	void Reset()
	{
		Future = std::future<std::unique_ptr<PathResult>>();
		vPath.clear();
	}
};

#endif