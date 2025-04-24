#ifndef GAME_SERVER_CORE_TOOLS_SCENARIO_BASE_H
#define GAME_SERVER_CORE_TOOLS_SCENARIO_BASE_H

class CGS;
class IServer;
class CPlayer;
class CCharacter;
class CEventListenerManager;
class CScenarioPlayerManager;
class CScenarioGroupManager;

using ScenarioAction = std::function<void()>;
using ScenarioCondition = std::function<bool()>;

enum class ConditionPriority
{
	CONDITION_AND_TIMER,
	CONDITION_OR_TIMER,
};

// ========================================================================
// ScenarioBase: Core non-player-specific scenario logic
// ========================================================================
class ScenarioBase
{
	friend class CScenarioPlayerManager;
	friend class CScenarioGroupManager;
	CGS* m_pGS {};

protected:
	CGS* GS() const;
	IServer* Server() const;

	struct Step
	{
		ScenarioAction FuncActive {};
		ScenarioAction FuncOnStart {};
		ScenarioAction FuncOnEnd {};
		ScenarioCondition FuncCheckCondition {};
		ConditionPriority Priority { ConditionPriority::CONDITION_AND_TIMER };
		int DelayTick {};

		explicit Step(int delayTick) : DelayTick(delayTick) { }
	};

	int m_ScenarioID {};
	std::vector<Step> m_vSteps {};
	size_t m_CurrentStepIndex {};
	int m_LastStepTimeTick {};
	bool m_Running {};
	int m_Flags {};

	virtual void OnSetupScenario() { }
	virtual void OnScenarioStart() { }
	virtual void OnScenarioEnd() { }
	virtual void OnStepAdded(Step& step) { }
	virtual void OnStepStart(size_t stepIndex) { }
	virtual void OnStepTick(size_t stepIndex) { }
	virtual void OnStepEnd(size_t stepIndex) { }
	virtual bool OnPauseConditions() { return false; };
	virtual bool OnStopConditions() { return false; }

	bool CanConcludeCurrentStep() const;
	void ExecuteStepStartActions();
	void ExecuteStepActiveActions();
	void ExecuteStepEndActions();
	void AdvanceStep();


private:
	void Start();
	void Tick();
	void Stop();
	bool IsFinished() const { return m_CurrentStepIndex >= m_vSteps.size(); }

public:
	enum Flags
	{
		FLAG_NONE = 0,
		FLAG_REPEATABLE = 1 << 0,
	};

	explicit ScenarioBase(int Flags = FLAG_NONE) : m_Flags(Flags) { }
	virtual ~ScenarioBase();

	ScenarioBase& AddStep(int delayTick = -1);
	ScenarioBase& WhenStarted(const ScenarioAction& pfnOnStart);
	ScenarioBase& WhenActive(const ScenarioAction& pfnActive);
	ScenarioBase& WhenFinished(const ScenarioAction& pfnOnEnd);
	ScenarioBase& CheckCondition(ConditionPriority priority, const ScenarioCondition& pfnCheckCondition);

	int GetScenarioID() const { return m_ScenarioID; }
	bool IsRunning() const { return m_Running; }
	bool IsRepeatable() const { return m_Flags & FLAG_REPEATABLE; }
	size_t GetCurrentStepIndex() const { return m_CurrentStepIndex; }
	size_t GetNumSteps() const { return m_vSteps.size(); }
};


// ========================================================================
// PlayerScenarioBase: Base for scenarios tied to a single player
// ========================================================================
class PlayerScenarioBase : public ScenarioBase
{
	friend class CScenarioPlayerManager;

protected:
	CPlayer* GetPlayer() const;
	CCharacter* GetCharacter() const;

	int m_ClientID {};

	bool OnPauseConditions() override;
	bool OnStopConditions() override;

public:
	explicit PlayerScenarioBase(int Flags = FLAG_NONE)
		: ScenarioBase(Flags) {}

	int GetClientID() const { return m_ClientID; }
};


// ========================================================================
// GroupScenarioBase: Base for scenarios involving multiple participants
// ========================================================================
class GroupScenarioBase : public ScenarioBase
{
	friend class CScenarioGroupManager;

protected:
	std::vector<CPlayer*> GetPlayers() const;
	std::vector<CCharacter*> GetCharacters() const;

	std::set<int> m_vParticipantIDs {};

	bool OnPauseConditions() override;
	bool OnStopConditions() override;
	void OnScenarioEnd() override;

	virtual void OnPlayerJoin(int ClientID) {}
	virtual void OnPlayerLeave(int ClientID, bool scenarioEnding) {}

public:
	explicit GroupScenarioBase(int Flags = FLAG_NONE)
		: ScenarioBase(Flags)
	{
	}

	virtual bool AddParticipant(int ClientID);
	virtual bool RemoveParticipant(int ClientID);
	bool IsParticipant(int ClientID) const;
	int GetParticipantCount() const;
	const std::set<int>& GetParticipants() const { return m_vParticipantIDs; }
};

#endif // GAME_SERVER_CORE_TOOLS_SCENARIO_BASE_H