/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "EidolonCore.h"

#include <game/server/gamecontext.h>

#include "EidolonInfoData.h"

using namespace sqlstr;

bool CEidolonCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "EIDOLON_SELECT") == 0)
	{
		m_EidolonItemSelected[ClientID] = VoteID;

		pPlayer->m_TempMenuValue = MENU_EIDOLON_COLLECTION_SELECTED;
		GS()->UpdateVotes(ClientID, MENU_EIDOLON_COLLECTION_SELECTED);
		return true;
	}

	if(PPSTR(CMD, "DELETE_MAIL") == 0)
	{
		return true;
	}

	return false;
}

bool CEidolonCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
		return false;

	if(Menulist == MENU_EIDOLON_COLLECTION)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		GS()->AVH(ClientID, TAB_EIDOLONS, "My eidolons (own {INT} out of {INT}).", GetPlayerCollectedEidolonsSize(pPlayer), CEidolonInfoData::Data().size());
		for(auto& pEidolon : CEidolonInfoData::Data())
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pEidolon.GetItemID());
			const char* pCollectedInfo = (pPlayerItem->HasItem() ? "✔" : "\0");
			const char* pUsedAtMoment = pPlayerItem->IsEquipped() ? Server()->Localization()->Localize(pPlayer->GetLanguage(), "[summoned by you]") : "\0";
			GS()->AVM(ClientID, "EIDOLON_SELECT", pEidolon.GetItemID(), TAB_EIDOLONS, "{STR} {STR} {STR}", pEidolon.GetDataBot()->m_aNameBot, pCollectedInfo, pUsedAtMoment);
		}

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_EIDOLON_COLLECTION_SELECTED)
	{
		pPlayer->m_LastVoteMenu = MENU_EIDOLON_COLLECTION;

		if(CEidolonInfoData* pEidolonInfo = GS()->GetEidolonByItemID(m_EidolonItemSelected[ClientID]))
		{
			GS()->AVH(ClientID, TAB_EIDOLON_DESCRIPTION, "Descriptions of eidolon ({STR})", pEidolonInfo->GetDataBot()->m_aNameBot);
			for(auto& Line : pEidolonInfo->GetLinesDescription())
				GS()->AVM(ClientID, "null", NOPE, TAB_EIDOLON_DESCRIPTION, Line.c_str());
			char aAttributeBonus[128];
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pEidolonInfo->GetItemID());
			pPlayerItem->StrFormatAttributes(pPlayer, aAttributeBonus, sizeof(aAttributeBonus));
			GS()->AVM(ClientID, "null", NOPE, TAB_EIDOLON_DESCRIPTION, aAttributeBonus);
			GS()->AV(ClientID, "null");

			GS()->AVH(ClientID, TAB_EIDOLON_UNLOCKING_ENHANCEMENTS, "Unlocking Enhancements.");
			GS()->AVM(ClientID, "null", NOPE, TAB_EIDOLON_UNLOCKING_ENHANCEMENTS, "Available soon.");
			GS()->AV(ClientID, "null");

			if(pPlayerItem->HasItem())
			{
				const char* pStateSummon = Server()->Localization()->Localize(pPlayer->GetLanguage(), pPlayerItem->IsEquipped() ? "Call off the summoned" : "Summon");
				GS()->AVM(ClientID, "ISETTINGS", pEidolonInfo->GetItemID(), NOPE, "{STR} {STR}", pStateSummon, pEidolonInfo->GetDataBot()->m_aNameBot);
			}
			else
				GS()->AVL(ClientID, "null", "To summon it, you must first get it");
		}
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	return false;
}

int CEidolonCore::GetPlayerCollectedEidolonsSize(CPlayer* pPlayer) const
{
	if(!pPlayer)
		return 0;

	int Collected = 0;
	for(auto& p : CPlayerItem::Data()[pPlayer->GetCID()])
	{
		if(p.second.HasItem() && p.second.Info()->IsType(ItemType::TYPE_EQUIP) && p.second.Info()->IsFunctional(
			EQUIP_EIDOLON))
			Collected++;
	}
	return Collected;
}
