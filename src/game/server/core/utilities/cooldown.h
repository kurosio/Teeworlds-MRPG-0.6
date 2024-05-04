/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_UTILITIES_COOLDOWN_H
#define GAME_SERVER_CORE_UTILITIES_COOLDOWN_H

class CCooldown
{
	using CCooldownCallback = std::function<void()>;

	std::string m_Name {};
	std::string m_Action {};
	int m_ClientID {};
	vec2 m_StartPos {};
	int m_Timer {};
	int m_StartTimer {};
	CCooldownCallback m_Callback {};
	bool m_IsCooldownActive {};
	bool m_Interrupted {};

public:
	CCooldown() = default;

	void Initilize(int ClientID) { m_ClientID = ClientID; }
	void Start(int Time, std::string Action, std::string Name, CCooldownCallback Callback);
	void Reset();
	bool IsCooldownActive() const { return m_IsCooldownActive; }
	void Handler();
};

#endif
