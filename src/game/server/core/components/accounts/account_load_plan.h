#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_LOAD_PLAN_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_LOAD_PLAN_H

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Game-thread coordinator for one account's asynchronous data. Stages without
// prerequisites start together; completion callbacks must return on the game thread.
class CAccountLoadPlan
{
public:
	using CCompletion = std::function<void(bool)>;
	using CStage = std::function<void(CCompletion)>;

	void Add(std::string Name, std::vector<std::string> Dependencies, CStage Start);
	void Start(CCompletion OnComplete);
	void Cancel();

private:
	struct SState;
	std::shared_ptr<SState> m_pState;
	static void Pump(const std::shared_ptr<SState>& pState);
	static void Complete(const std::shared_ptr<SState>& pState, size_t Index, bool Success);
};

#endif
