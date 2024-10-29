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
	class ScrollManager
	{
		int m_ScrollPos {};
		int m_MaxScrollPos {};
		int m_MaxItemsVisible {};

	public:
		explicit ScrollManager(int visibleLines) : m_MaxItemsVisible(visibleLines) {}
		void SetMaxScrollPos(int itemCount) { m_MaxScrollPos = maximum(0, itemCount - m_MaxItemsVisible); }
		int GetScrollPos() const { return m_ScrollPos; }
		int GetEndScrollPos() const { return minimum(m_ScrollPos + m_MaxItemsVisible, m_MaxScrollPos + m_MaxItemsVisible); }
		int GetMaxVisibleItems() const { return m_MaxItemsVisible; }
		void ScrollUp() { m_ScrollPos = minimum(m_ScrollPos + 1, m_MaxScrollPos); }
		void ScrollDown() { m_ScrollPos = maximum(m_ScrollPos - 1, 0); }
		bool CanScrollUp() const { return m_ScrollPos > 0; }
		bool CanScrollDown() const { return m_ScrollPos < m_MaxScrollPos; }
	};

	struct Point
	{
		int m_Extra{};
		int m_Extra2 {};
		std::string m_Command{"NULL"};
		char m_aDesc[32]{};
	};

	std::optional<int> m_MenuExtra {};
	int m_LastMenulist{NOPE};
	int m_Menulist {NOPE};
	int m_Flags {};
	int m_ClientID {};
	int m_ResendMotdTick {};
	std::string m_LastBuffer{};
	std::string m_Description{};
	std::vector<Point> m_Points{};
	ScrollManager m_ScrollManager{13};

public:
	MotdMenu(int ClientID) : m_ClientID(ClientID) {}
	MotdMenu(int ClientID, int Flags) : m_Flags(Flags), m_ClientID(ClientID) {}

	template <typename... Ts>
	MotdMenu(int ClientID, const char* pDesc, const Ts&... args)
		: m_ClientID(ClientID), m_Description(fmt_localize(ClientID, pDesc, args...)) {}

	template <typename... Ts>
	MotdMenu(int ClientID, int Flags, const char* pDesc, const Ts&... args)
		: m_Flags(Flags), m_ClientID(ClientID), m_Description(fmt_localize(ClientID, pDesc, args...)) {}

	template <typename... Ts>
	void AddText(std::string_view description, const Ts&... args)
	{
		AddImpl(NOPE, NOPE, "NULL", fmt_localize(m_ClientID, description.data(), args...));
	}

	template <typename... Ts>
	void Add(std::string_view command, std::string_view description, const Ts&... args)
	{
		AddImpl(NOPE, NOPE, command, fmt_localize(m_ClientID, description.data(), args...));
	}

	template <typename... Ts>
	void Add(std::string_view command, int extra, std::string_view description, const Ts&... args)
	{
		AddImpl(extra, NOPE, command, fmt_localize(m_ClientID, description.data(), args...));
	}

	template <typename... Ts>
	void AddMenu(int MenuID, int Extra, std::string_view description, const Ts&... args)
	{
		AddImpl(MenuID, Extra, "MENU", fmt_localize(m_ClientID, description.data(), args...));
	}

	void AddLine()
	{
		AddImpl(NOPE, NOPE, "NULL", "");
	}

	void AddSeparateLine()
	{
		AddImpl(NOPE, NOPE, "NULL", "\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
			"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
			"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500");
	}

	void AddBackpage()
	{
		AddImpl(NOPE, NOPE,"BACKPAGE", "<<< Backpage");
	}

	void Tick();
	void Send(int Menulist);
	int GetLastMenulist() const
	{
		return m_LastMenulist;
	}

	int GetMenulist() const
	{
		return m_Menulist;
	}

	void SetLastMenulist(int Menulist)
	{
		m_LastMenulist = Menulist;
	}

	std::optional<int> GetMenuExtra() const
	{
		return m_MenuExtra;
	}

	void ClearMotd(CGS* pGS, CPlayer* pPlayer);

private:
	void UpdateMotd(IServer* pServer, CGS* pGS, CPlayer* pPlayer);
	void AddImpl(int extra, int extra2, std::string_view command, const std::string& description);
};

#endif
