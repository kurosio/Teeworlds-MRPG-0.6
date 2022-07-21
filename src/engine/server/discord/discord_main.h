/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_DISCORD_MAIN_SERVER_H
#define ENGINE_DISCORD_MAIN_SERVER_H

#ifdef CONF_DISCORD

#include <sleepy_discord/websocketpp_websocket.h>

typedef std::function<void()> DiscordTask;
class DiscordJob final : public SleepyDiscord::DiscordClient
{
public:
	using SleepyDiscord::DiscordClient::DiscordClient;
	DiscordJob(class IServer* pServer);

private:
	/************************************************************************/
	/* Discord main events functions                                        */
	/************************************************************************/
	void onAddMember(SleepyDiscord::Snowflake<SleepyDiscord::Server> serverID, SleepyDiscord::ServerMember member) override;
	void onMessage(SleepyDiscord::Message message) override;
	void onInteraction(SleepyDiscord::Interaction interaction) override;
	void onReady(SleepyDiscord::Ready readyData) override;

	/************************************************************************/
	/* Discord main functions                                               */
	/************************************************************************/
	void SendWarningMessage(SleepyDiscord::Snowflake<SleepyDiscord::Channel> channelID, std::string Message);
	void SendSuccesfulMessage(SleepyDiscord::Snowflake<SleepyDiscord::Channel> channelID, std::string Message);
	bool SendGenerateMessage(SleepyDiscord::User UserRequestFrom, std::string Channel, std::string Title, std::string SearchNickname, int Color = 0, bool MultipleSearch = true);
	bool SendGenerateMessageAccountID(SleepyDiscord::User UserRequestFrom, std::string Channel, std::string Title, int AccountID, int Color = 0);

	/************************************************************************/
	/* Discord main buttons                                                 */
	/************************************************************************/
	typedef std::function<void(DiscordJob* pDiscord, SleepyDiscord::Interaction* pInteraction)> ButtonCallback;
	struct StructButtonCallback
	{
		char m_aButtonID[128];
		ButtonCallback m_BtnCallback;
	};
	std::list < StructButtonCallback > m_aBtnCallbacks;
	void CreateButton(std::shared_ptr<SleepyDiscord::ActionRow> pActionRow, const char* pInteractive, const char* pLabel, SleepyDiscord::ButtonStyle ButtonStyle, bool Disabled = false, ButtonCallback pBtnCallback = nullptr);

	/************************************************************************/
	/* Discord teeworlds server side                                        */
	/************************************************************************/
	// Allow access only from the CServer
	friend class CServer;
	friend class DiscordCommands;
	struct DiscordHandle
	{
		DiscordTask m_pEvent;
		DiscordHandle(DiscordTask Event) : m_pEvent(Event) {};
	};
	std::list<DiscordHandle*> m_pThreadHandler;

	class IServer* m_pServer;
	IServer* Server() const { return m_pServer; }

	void HandlerThreadTasks();
	void AddThreadTask(DiscordTask Task);
};

#endif
#endif