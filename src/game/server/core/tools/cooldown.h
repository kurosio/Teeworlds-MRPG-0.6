#ifndef GAME_SERVER_CORE_UTILITIES_COOLDOWN_H
#define GAME_SERVER_CORE_UTILITIES_COOLDOWN_H

class CGS;
class IServer;
class CCharacter;
using CCooldownCallback = std::function<void()>;

class CCooldown
{
	int m_ClientID { NOPE };
	std::string m_Name {};
	vec2 m_Pos {};
	int m_Tick {};
	int m_StartedTick {};
	CCooldownCallback m_Callback {};
	bool m_Active {};
	bool m_Interrupted {};

public:
	CCooldown() = default;

	void Init(int ClientID);
	void Start(int Time, std::string_view Name, CCooldownCallback fnCallback);
	void Reset();
	void Tick();

	[[nodiscard]] constexpr bool IsActive() const { return m_Active; }

private:
	[[nodiscard]] bool HasPlayerMoved(CCharacter* pChar) const;
	void BroadcastCooldownInfo(const char* pMessage = nullptr) const;
	void BroadcastCooldownProgress(IServer* pServer) const;
};

#endif // GAME_SERVER_CORE_UTILITIES_COOLDOWN_H