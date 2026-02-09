#ifndef GAME_SERVER_CORE_TOOLS_DB_ASYNC_CONTEXT_H
#define GAME_SERVER_CORE_TOOLS_DB_ASYNC_CONTEXT_H

#include <engine/server.h>

class CGS;

namespace DbAsync
{
	// context base
	class CContextBase
	{
		int m_ClientID { -1 };
		int m_WorldID { INITIALIZER_WORLD_ID };

	public:
		explicit CContextBase(int ClientID, int WorldID = INITIALIZER_WORLD_ID)
			: m_ClientID(ClientID), m_WorldID(WorldID == INITIALIZER_WORLD_ID ? ResolveClientWorldID(ClientID) : WorldID)
		{
		}

		int GetClientID() const { return m_ClientID; }
		int GetWorldID() const { return m_WorldID; }

		IServer* Server() const { return Instance::Server(); }
		CGS* GS() const { return static_cast<CGS*>(Instance::GameServer(m_WorldID)); }
		CGS* GS(int WorldID) const { return static_cast<CGS*>(Instance::GameServer(WorldID)); }
		CPlayer* GetPlayer(bool CheckAuth = false, bool CheckCharacter = false) const { return GS()->GetPlayer(GetClientID(), CheckAuth, CheckCharacter); }

	private:
		static int ResolveClientWorldID(int ClientID) { return Instance::Server()->GetClientWorldID(ClientID); }
	};

	template<typename TPayload>
	class CContext : public CContextBase
	{
		TPayload m_Payload;

	public:
		template<typename... TArgs>
		CContext(int ClientID, int WorldID, TArgs&&... Args)
			: CContextBase(ClientID, WorldID), m_Payload(std::forward<TArgs>(Args)...)
		{
		}

		TPayload& Data() { return m_Payload; }
		const TPayload& Data() const { return m_Payload; }

		TPayload* operator->() { return &m_Payload; }
		const TPayload* operator->() const { return &m_Payload; }
	};

	template<typename TPayload, typename... TArgs>
	std::shared_ptr<CContext<TPayload>> MakeContext(int ClientID, TArgs&&... Args)
	{
		return std::make_shared<CContext<TPayload>>(ClientID, INITIALIZER_WORLD_ID, std::forward<TArgs>(Args)...);
	}

	template<typename TPayload, typename... TArgs>
	std::shared_ptr<CContext<TPayload>> MakeContextInWorld(int ClientID, int WorldID, TArgs&&... Args)
	{
		return std::make_shared<CContext<TPayload>>(ClientID, WorldID, std::forward<TArgs>(Args)...);
	}
}

#endif
