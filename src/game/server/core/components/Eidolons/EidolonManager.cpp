/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "EidolonManager.h"

#include <game/server/gamecontext.h>

using namespace sqlstr;

#define TW_TABLE_EIDOLON_ENHANCEMENTS "tw_account_eidolon_enhancements"

bool CEidolonManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "EIDOLON_SELECT") == 0)
	{
		m_EidolonItemSelected[ClientID] = VoteID;

		//pPlayer->m_TempMenuValue = MENU_EIDOLON_COLLECTION_SELECTED;
		pPlayer->m_VotesData.UpdateVotes(MENU_EIDOLON_COLLECTION_SELECTED);
		return true;
	}

	return false;
}

bool CEidolonManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
		return false;

	if(Menulist == MENU_EIDOLON_COLLECTION)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		GS()->AVH(ClientID, TAB_INFO_EIDOLONS, "Eidolon Collection Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_EIDOLONS, "Here you can see your collection of eidolons.");
		GS()->AV(ClientID, "null");

		std::pair EidolonSize = GetEidolonsSize(ClientID);
		GS()->AVH(ClientID, TAB_EIDOLONS, "\u2727 My eidolons (own {INT} out of {INT}).", EidolonSize.first, EidolonSize.second);
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
		pPlayer->m_VotesData.SetLastMenuID(MENU_EIDOLON_COLLECTION);

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

std::pair<int, int> CEidolonManager::GetEidolonsSize(int ClientID) const
{
	int Collect = 0;
	int Max = static_cast<int>(CEidolonInfoData::Data().size());

	if(CPlayerItem::Data().find(ClientID) != CPlayerItem::Data().end())
	{
		for(auto& p : CPlayerItem::Data()[ClientID])
		{
			if(p.second.HasItem() && p.second.Info()->IsType(ItemType::TYPE_EQUIP) && p.second.Info()->IsFunctional(
				EQUIP_EIDOLON))
				Collect++;
		}
	}

	return std::make_pair(Collect, Max);
}
