#ifndef GAME_SERVER_CORE_TOOLS_MOTD_MENU_H
#define GAME_SERVER_CORE_TOOLS_MOTD_MENU_H

class CGS;
class CPlayer;
enum MotdMenuFlags
{
	MTFLAG_CLOSE_BUTTON = 1 << 0,
	MTFLAG_CLOSE_LAST_MENU_ON_SELECT = 1 << 2,
	MTFLAG_CLOSE_ON_SELECT = 1 << 2,
};

class MotdMenu
{
	struct Point
	{
		int m_Extra;
		std::string m_Command;
		char m_aDesc[32];
	};

	int m_Flags {};
	int m_ClientID {};
	int m_ResendMotdTick {};
	int m_ScrollPos {};
	std::string m_LastBuffer;
	std::string m_Description;
	std::vector<Point> m_Points;

public:
	MotdMenu(int ClientID) : m_ClientID(ClientID) {}
	MotdMenu(int ClientID, int Flags) : m_Flags(Flags), m_ClientID(ClientID) {}
	template <typename ... Ts> MotdMenu(int ClientID, const char* pDesc, const Ts&... args)
		: m_ClientID(ClientID) {
		m_Description = fmt(pDesc, args...);
	}
	template <typename ... Ts> MotdMenu(int ClientID, int Flags, const char* pDesc, const Ts&... args)
		: m_Flags(Flags), m_ClientID(ClientID) {
		m_Description = fmt(pDesc, args...);
	}
	template <typename ... Ts>
	void Add(const std::string& command, const std::string& description, const Ts&... args)
	{
		AddImpl(NOPE, command, fmt(description.c_str(), args...));
	}
	template <typename ... Ts>
	void Add(const std::string& command, int extra, const std::string& description, const Ts&... args)
	{
		AddImpl(extra, command, fmt(description.c_str(), args...));
	}
	void Handle();
	void Send();

private:
	void AddImpl(int extra, const std::string& command, const std::string& description);
	void ClearMotd(CGS* pGS, CPlayer* pPlayer);
};

#endif
