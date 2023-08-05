/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseData.h"

#include "game/server/gamecontext.h"
#include "game/server/mmocore/GameEntities/jobitems.h"
#include "game/server/mmocore/GameEntities/decoration_houses.h"

CGS* CHouseData::GS() const { return static_cast<CGS*>(Server()->GameServer(m_WorldID)); }
CPlayer* CHouseData::GetPlayer() const { return GS()->GetPlayerFromUserID(m_AccountID); }

CHouseData::~CHouseData()
{
	delete m_pDoorData;
	m_pDoorData = nullptr;
	delete m_pBank;
	m_pBank = nullptr;

	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		delete m_apDecorations[i];
		m_apDecorations[i] = nullptr;
	}
}

void CHouseData::InitDecorations()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_houses_decorations", "WHERE WorldID = '%d' AND HouseID = '%d'", GS()->GetWorldID(), m_ID);
	while(pRes->next())
	{
		for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
		{
			if(!m_apDecorations[i])
			{
				int DecorationID = pRes->getInt("ID");
				int ItemID = pRes->getInt("ItemID");
				vec2 DecorationPos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
				m_apDecorations[i] = new CDecorationHouses(&GS()->m_World, DecorationPos, m_ID, DecorationID, ItemID);
				break;
			}
		}
	}
}

bool CHouseData::AddDecoration(ItemIdentifier ItemID, vec2 DecorationPos)
{
	// check distance
	if(distance(m_Pos, DecorationPos) > 400.0f)
	{
		GS()->Chat(GetPlayer()->GetCID(), "Too much distance from home!");
		return false;
	}

	// add decoration
	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		if(!m_apDecorations[i])
		{
			// insert to last identifier and got it
			ResultPtr pRes2 = Database->Execute<DB::SELECT>("ID", "tw_houses_decorations", "ORDER BY ID DESC LIMIT 1");
			const int InitID = pRes2->next() ? pRes2->getInt("ID") + 1 : 1;
			Database->Execute<DB::INSERT>("tw_houses_decorations", "(ID, ItemID, HouseID, PosX, PosY, WorldID) VALUES ('%d', '%d', '%d', '%d', '%d', '%d')", InitID, ItemID, m_ID, (int)DecorationPos.x, (int)DecorationPos.y, GS()->GetWorldID());

			// create new decoration on gameworld
			m_apDecorations[i] = new CDecorationHouses(&GS()->m_World, DecorationPos, m_AccountID, InitID, ItemID);
			return true;
		}
	}

	GS()->Chat(GetPlayer()->GetCID(), "All decoration slots are occupied");
	return false;
}

bool CHouseData::RemoveDecoration(HouseDecorationIdentifier DecoID)
{
	// remove decoration
	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		if(m_apDecorations[i] && m_apDecorations[i]->GetDecorationID() == DecoID)
		{
			// delete from gameworld
			delete m_apDecorations[i];
			m_apDecorations[i] = nullptr;

			// remove from database
			Database->Execute<DB::REMOVE>("tw_houses_decorations", "WHERE ID = '%d'", DecoID);
			return true;
		}
	}

	return false;
}

void CHouseData::Buy(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();

	// check player house
	if(pPlayer->Acc().HasHouse())
	{
		GS()->Chat(ClientID, "You already have a home.");
		return;
	}

	// check house owner
	if(HasOwner())
	{
		GS()->Chat(ClientID, "House has already been purchased!");
		return;
	}

	// buy house
	if(pPlayer->SpendCurrency(m_Price))
	{
		// update data
		m_AccountID = pPlayer->Acc().m_UserID;
		m_pDoorData->Close();
		m_pBank->Reset();
		Database->Execute<DB::UPDATE>("tw_houses", "UserID = '%d', HouseBank = '0', AccessData = NULL WHERE ID = '%d'", m_AccountID, m_ID);

		// send information
		GS()->Chat(-1, "{STR} becomes the owner of the house class {STR}", Server()->ClientName(ClientID), GetClassName());
		GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**{STR} becomes the owner of the house class {STR}**", Server()->ClientName(ClientID), GetClassName());
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
	}
}

void CHouseData::Sell()
{
	// check house
	if(!HasOwner())
		return;

	// returned fully value gold
	const int Price = m_Price + m_pBank->Get();
	GS()->SendInbox("System", m_AccountID, "House is sold", "Your house is sold !", itGold, Price, 0);

	// send information
	if(CPlayer* pPlayer = GetPlayer())
	{
		GS()->Chat(pPlayer->GetCID(), "Your House is sold!");
		GS()->UpdateVotes(pPlayer->GetCID(), MENU_MAIN);
	}
	GS()->Chat(-1, "House: {INT} have been is released!", m_ID);
	GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**[House: {INT}] have been sold!**", m_ID);

	// update data
	m_pDoorData->Open();
	m_pBank->Reset();
	Database->Execute<DB::UPDATE>("tw_houses", "UserID = NULL, HouseBank = '0', AccessData = NULL WHERE ID = '%d'", m_ID);

	// account used for GetPlayer() reset last moment
	m_AccountID = -1;
}

void CHouseData::SetPlantItemID(ItemIdentifier ItemID)
{
	// checked
	if(ItemID == m_PlantItemID)
		return;

	// check for update and set new plant itemid
	bool Updates = false;
	for(CJobItems* pPlant = (CJobItems*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_JOBITEMS); pPlant; pPlant = (CJobItems*)pPlant->TypeNext())
	{
		if(pPlant->m_HouseID == m_ID)
		{
			pPlant->m_ItemID = ItemID;
			Updates = true;
		}
	}

	// update data
	if(Updates)
	{
		m_PlantItemID = ItemID;
		Database->Execute<DB::UPDATE>("tw_houses", "PlantID = '%d' WHERE ID = '%d'", ItemID, m_ID);
	}
}

void CHouseData::ShowDecorations() const
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// show all house decoration slots
	bool Found = false;
	int ClientID = pPlayer->GetCID();
	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		if(m_apDecorations[i])
		{
			int DecoID = m_apDecorations[i]->GetDecorationID();
			CItemDescription* pItemDecoration = GS()->GetItemInfo(m_apDecorations[i]->GetItemID());
			GS()->AVD(ClientID, "DECORATION_HOUSE_DELETE", DecoID, pItemDecoration->GetID(), 1, "[Slot {INT}]: {STR} back to the inventory", i + 1, pItemDecoration->GetName());
			Found = true;
		}
	}

	if(!Found)
	{
		GS()->AVL(ClientID, "null", "There are no decorations in your house");
	}
}