/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "eventhandler.h"
#include "gamecontext.h"

//////////////////////////////////////////////////
// Event handler
//////////////////////////////////////////////////
CEventHandler::CEventHandler()
{
	m_pGameServer = nullptr;
	Clear();
}

void CEventHandler::SetGameServer(CGS *pGameServer)
{
	m_pGameServer = pGameServer;
}

void *CEventHandler::Create(int Type, int Size, int64_t Mask)
{
	if(m_NumEvents == MAX_EVENTS)
		return nullptr;
	if(m_CurrentOffset+Size >= MAX_DATASIZE)
		return nullptr;

	void *p = &m_aData[m_CurrentOffset];
	m_aOffsets[m_NumEvents] = m_CurrentOffset;
	m_aTypes[m_NumEvents] = Type;
	m_aSizes[m_NumEvents] = Size;
	m_aClientMasks[m_NumEvents] = Mask;
	m_CurrentOffset += Size;
	m_NumEvents++;
	return p;
}

void CEventHandler::Clear()
{
	m_NumEvents = 0;
	m_CurrentOffset = 0;
}

void CEventHandler::Snap(int SnappingClient)
{
	for(int i = 0; i < m_NumEvents; i++)
	{
		if(SnappingClient == -1 || CmaskIsSet(m_aClientMasks[i], SnappingClient))
		{
			void* pEventData = &m_aData[m_aOffsets[i]];
			auto* pCommonEvent = static_cast<CNetEvent_Common*>(pEventData);
			auto* pSnapPlayer = GS()->GetPlayer(SnappingClient);
			if(distance(pSnapPlayer->m_ViewPos, vec2(pCommonEvent->m_X, pCommonEvent->m_Y)) > 1500.0)
				continue;

			if(m_aTypes[i] == NETEVENTTYPE_DEATH)
			{
				auto* pDeathEvent = static_cast<CNetEvent_Death*>(pEventData);
				int TranslatedID = pDeathEvent->m_ClientId;
				if(!GS()->Server()->Translate(TranslatedID, SnappingClient))
					TranslatedID = (int)VANILLA_MAX_CLIENTS - 1;
				pDeathEvent->m_ClientId = TranslatedID;
			}

			// create snapshot
			if(void* pSnapItem = GS()->Server()->SnapNewItem(m_aTypes[i], i, m_aSizes[i]))
			{
				mem_copy(pSnapItem, pEventData, m_aSizes[i]);
			}
		}
	}
}

