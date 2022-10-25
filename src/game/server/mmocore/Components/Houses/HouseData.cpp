/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseData.h"

#include "Entities/HouseDoor.h"
#include "game/server/gamecontext.h"
#include "game/server/mmocore/GameEntities/jobitems.h"

std::map <int, CHouseData> CHouseData::ms_aHouse;

CPlayer* CHouseBankData::GetPlayer() const { return m_pGS->GetPlayerFromUserID(*m_pAccountID); }

void CHouseBankData::Add(int Value)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	int ClientID = pPlayer->GetCID();

	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("ID, HouseBank", "tw_houses", "WHERE UserID = '%d'", pPlayer);
	if(pRes->next())
	{
		HouseIdentifier HouseID = pRes->getInt("ID");

		if(!pPlayer->SpendCurrency(Value))
			return;

		m_Bank = pRes->getInt("HouseBank") + Value;
		m_pGS->Chat(ClientID, "You put {VAL} gold in the safe {VAL}!", Value, m_Bank);
		Sqlpool.Execute<DB::UPDATE>("tw_houses", "HouseBank = '%d' WHERE ID = '%d'", m_Bank, HouseID);
	}
}

void CHouseBankData::Take(int Value)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	int ClientID = pPlayer->GetCID();

	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("ID, HouseBank", "tw_houses", "WHERE UserID = '%d'", *m_pAccountID);
	if(pRes->next())
	{
		HouseIdentifier HouseID = pRes->getInt("ID");
		int Bank = pRes->getInt("HouseBank");

		// checked
		if(Bank < Value)
		{
			m_pGS->Chat(ClientID, "Acceptable for take {VAL}gold", Bank);
			return;
		}

		// update data
		pPlayer->AddMoney(Value);
		m_Bank = Bank - Value;
		Sqlpool.Execute<DB::UPDATE>("tw_houses", "HouseBank = '%d' WHERE ID = '%d'", m_Bank, HouseID);

		// send information
		m_pGS->Chat(ClientID, "You take {VAL} gold in the safe {VAL}!", Value, m_Bank);
	}
}


CHouseDoorData::~CHouseDoorData()
{
	delete m_pDoor;
	m_pDoor = nullptr;
}

// house door data
void CHouseDoorData::Open()
{
	if(m_pDoor)
	{
		delete m_pDoor;
		m_pDoor = nullptr;
	}
}

void CHouseDoorData::Close()
{
	if(!m_pDoor)
		m_pDoor = new HouseDoor(&m_pGS->m_World, m_Pos);
}

void CHouseDoorData::Reverse()
{
	if(m_pDoor)
		Close();
	else
		Open();
}

CHouseDecoration::~CHouseDecoration()
{
	delete m_pDecoration;
	m_pDecoration = nullptr;
}

// house decoration data
void CHouseDecoration::Init(CGS* pGS, HouseDecorationIdentifier ID, vec2 Pos, int AccountID, ItemIdentifier DecorationItemID)
{
	m_ID = ID;
	m_DecorationItemID = DecorationItemID;
	m_pDecoration = new CDecorationHouses(&pGS->m_World, Pos, AccountID, DecorationItemID);
}

void CHouseDecoration::Reset()
{
	m_ID = -1;
	delete m_pDecoration;
	m_pDecoration = nullptr;
}

CGS* CHouseData::GS() const { return static_cast<CGS*>(Server()->GameServer(m_WorldID)); }
CPlayer* CHouseData::GetPlayer() const { return GS()->GetPlayerFromUserID(m_AccountID); }

bool CHouseData::AddDecoration(ItemIdentifier DecoItemID, vec2 DecorationPos)
{
	// checked
	CHouseDecoration* pDecoration = nullptr;
	for(auto& pElem : m_aDecorations)
	{
		if(!pElem.HasUsed())
		{
			pDecoration = &pElem;
			break;
		}
	}

	if(!pDecoration || distance(m_Pos, DecorationPos) > 400.0f)
	{
		return false;
	}

	// get housedecorationidentifier and insert
	ResultPtr pRes2 = Sqlpool.Execute<DB::SELECT>("ID", "tw_houses_decorations", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pRes2->next() ? pRes2->getInt("ID") + 1 : 1;
	Sqlpool.Execute<DB::INSERT>("tw_houses_decorations", "(ID, DecoID, HouseID, X, Y, WorldID) VALUES ('%d', '%d', '%d', '%d', '%d', '%d')", InitID, DecoItemID, m_ID, (int)DecorationPos.x, (int)DecorationPos.y, GS()->GetWorldID());

	// init slot
	pDecoration->Init(GS(), InitID, DecorationPos, m_AccountID, DecoItemID);

	return true;
}

bool CHouseData::RemoveDecoration(HouseDecorationIdentifier ID)
{
	// checked
	CHouseDecoration* pDecoration = nullptr;
	for (auto& pElem : m_aDecorations)
	{
		if(pElem.GetID() == ID)
		{
			pDecoration = &pElem;
			break;
		}
	}

	if(!pDecoration)
	{
		return false;
	}

	// reset slot
	pDecoration->Reset();
	Sqlpool.Execute<DB::REMOVE>("tw_houses_decorations", "WHERE ID = '%d'", ID);

	return true;
}

void CHouseData::Buy(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();

	// checked
	if(pPlayer->Acc().HasHouse())
	{
		GS()->Chat(ClientID, "You already have a home.");
		return;
	}

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
		m_DoorData.Close();
		m_Bank.Reset();
		Sqlpool.Execute<DB::UPDATE>("tw_houses", "UserID = '%d', HouseBank = '0' WHERE ID = '%d'", m_AccountID, m_ID);

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
	{
		return;
	}

	// returned fully value gold
	const int Price = m_Price + m_Bank.Get();
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
	m_DoorData.Open();
	m_Bank.Reset();
	Sqlpool.Execute<DB::UPDATE>("tw_houses", "UserID = NULL, HouseBank = '0' WHERE ID = '%d'", m_ID);

	// account used for GetPlayer() reset last moment
	m_AccountID = -1;
}

void CHouseData::UpdatePlantItemID(ItemIdentifier PlantItemID)
{
	// checked
	if(PlantItemID == m_PlantItemID)
	{
		return;
	}

	// check for update
	bool Updates = false;
	for(CJobItems* pPlant = (CJobItems*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_JOBITEMS); pPlant; pPlant = (CJobItems*)pPlant->TypeNext())
	{
		if(pPlant->m_HouseID == m_ID)
		{
			pPlant->m_ItemID = PlantItemID;
			Updates = true;
		}
	}

	// update data
	if(Updates)
	{
		m_PlantItemID = PlantItemID;
		Sqlpool.Execute<DB::UPDATE>("tw_houses", "PlantID = '%d' WHERE ID = '%d'", PlantItemID, m_ID);
	}
}

void CHouseData::ShowDecorations()
{
	if(CPlayer* pPlayer = GetPlayer())
	{
		int ClientID = pPlayer->GetCID();

		for(auto& p : m_aDecorations)
		{
			if(p.HasUsed())
			{
				GS()->AVD(ClientID, "DECODELETE", p.GetID(), p.GetDecorationItemID(), 1, "{STR}:{INT} back to the inventory",
					GS()->GetItemInfo(p.GetDecorationItemID())->GetName(), p.GetID());
			}
		}
	}
}
