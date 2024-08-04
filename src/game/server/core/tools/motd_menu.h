#ifndef GAME_SERVER_CORE_TOOLS_MOTD_MENU_H
#define GAME_SERVER_CORE_TOOLS_MOTD_MENU_H

class MotdMenu
{
public:
	using CommandHandler = std::function<void(int)>;

	void Init(int ClientID)
	{
		m_ClientID = ClientID;
	}

	void AddPoint(const std::string& command, const std::string& description, CommandHandler handler)
	{
		m_Points.push_back({ command, description });
		m_CommandHandlers[command] = std::move(handler);
	}

	void AddDescription(const std::string& description)
	{
		m_Description = description;
	}

	void Handle();

	int GetClientID() const { return m_ClientID; }

private:
	struct Point
	{
		std::string command;
		std::string description;
	};

	int m_ClientID{};
	std::string m_LastBuffer;
	std::string m_Description;
	std::vector<Point> m_Points;
	std::unordered_map<std::string, CommandHandler> m_CommandHandlers;
};


#endif
