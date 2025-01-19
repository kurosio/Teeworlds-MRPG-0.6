#ifndef GAME_SERVER_CORE_UTILITIES_COOLDOWN_H
#define GAME_SERVER_CORE_UTILITIES_COOLDOWN_H

class CGS;
class CPlayer;
using CCooldownCallback = std::function<void()>;

class CCooldown
{
	std::string m_Name {};
	int m_ClientID {};
	vec2 m_Pos {};
	int m_Tick {};
	int m_StartedTick {};
	CCooldownCallback m_Callback {};
	bool m_Active {};
	bool m_Interrupted {};

public:
	CCooldown() = default;
	void Init(int ClientID) { m_ClientID = ClientID; }
	void Start(int Time, std::string Name, CCooldownCallback fnCallback);
	void Reset();
	bool IsActive() const { return m_Active; }
	void Tick();

private:
	void EndCooldown(const char* pMessage = "\0");
	bool HasPlayerMoved(CPlayer* pPlayer) const;
	void BroadcastCooldown(IServer* pServer) const;

};

#endif // GAME_SERVER_CORE_UTILITIES_COOLDOWN_H