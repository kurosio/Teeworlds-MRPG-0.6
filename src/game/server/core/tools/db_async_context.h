#ifndef GAME_SERVER_CORE_TOOLS_DB_ASYNC_CONTEXT_H
#define GAME_SERVER_CORE_TOOLS_DB_ASYNC_CONTEXT_H

#include <engine/server.h>

class CGS;
class CPlayer;

namespace DbAsync
{
	// context base
	class CContextBase
	{
		int m_ClientID { -1 };
		std::weak_ptr<void> m_pConnectionLifetime;

	public:
		explicit CContextBase(int ClientID);

		int GetClientID() const { return m_ClientID; }

		IServer* Server() const { return Instance::Server(); }
		CGS* GS() const;
		CPlayer* GetPlayer(bool CheckAuth = false, bool CheckCharacter = false) const;
	};

	template<typename TPayload>
	class CContext : public CContextBase
	{
		TPayload m_Payload;

	public:
		template<typename... TArgs>
		CContext(int ClientID, TArgs&&... Args)
			: CContextBase(ClientID), m_Payload(std::forward<TArgs>(Args)...)
		{
		}

		TPayload& Data() { return m_Payload; }
		const TPayload& Data() const { return m_Payload; }
	};

	inline std::shared_ptr<CContextBase> MakeContext(int ClientID)
	{
		return std::make_shared<CContextBase>(ClientID);
	}

	template<typename TPayload, typename... TArgs>
	std::shared_ptr<CContext<TPayload>> MakeContext(int ClientID, TArgs&&... Args)
	{
		return std::make_shared<CContext<TPayload>>(ClientID, std::forward<TArgs>(Args)...);
	}
}

#endif
