/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "house_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/entities/items/gathering_node.h>
#include <game/server/core/entities/tools/draw_board.h>
#include <game/server/core/components/mails/mail_wrapper.h>

CGS* CHouse::GS() const { return static_cast<CGS*>(Server()->GameServer(m_WorldID)); }
CPlayer* CHouse::GetPlayer() const { return GS()->GetPlayerByUserID(m_AccountID); }

CHouse::~CHouse()
{
	delete m_pFarmzonesManager;
	delete m_pDecorationManager;
	delete m_pDoorManager;
	delete m_pBankManager;
}

void CHouse::InitComponents(BigInt Bank, const std::string& DoorsData, const std::string& FarmzonesData, const std::string& PropertiesData)
{
	// assert main properties string
	dbg_assert(PropertiesData.length() > 0, "The properties string is empty");


	// parse properties
	mystd::json::parse(PropertiesData, [this](nlohmann::json& pJson)
	{
		dbg_assert(pJson.find("position") != pJson.end(), "The importal properties value is empty");
		m_Position = pJson.value("position", vec2());
		m_TextPosition = pJson.value("text_position", vec2());
	});


	// initialize components
	m_pBankManager = new CBankManager(this, Bank);
	m_pDoorManager = new CDoorManager(this, DoorsData);
	m_pDecorationManager = new CDecorationManager(this, TW_GUILD_HOUSES_DECORATION_TABLE);
	m_pFarmzonesManager = new CFarmzonesManager(this, FarmzonesData);

	// Asserts
	dbg_assert(m_pBankManager != nullptr, "The house bank is null");
	dbg_assert(m_pFarmzonesManager != nullptr, "The house farmzones manager is null");
	dbg_assert(m_pDecorationManager != nullptr, "The house decorations manager is null");
	dbg_assert(m_pDoorManager != nullptr, "The house doors manager is null");
}

void CHouse::Buy(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();

	// check is player already have a house
	if(pPlayer->Account()->HasHouse())
	{
		GS()->Chat(ClientID, "You already have a home.");
		return;
	}

	// check is house has owner
	if(HasOwner())
	{
		GS()->Chat(ClientID, "House has already been purchased!");
		return;
	}

	// try spend currency
	if(pPlayer->Account()->SpendCurrency(m_Price))
	{
		// update data
		m_AccountID = pPlayer->Account()->GetID();
		m_pDoorManager->CloseAll();
		m_pBankManager->Reset();
		pPlayer->Account()->ReinitializeHouse();
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = '{}', Bank = '0', AccessList = NULL WHERE ID = '{}'", m_AccountID, m_ID);

		// send information
		GS()->Chat(-1, "'{}' becomes the owner of the house class '{}'.", Server()->ClientName(ClientID), GetClassName());
		pPlayer->m_VotesData.UpdateCurrentVotes();
	}
}

void CHouse::Sell()
{
	// check is has owner
	if(!HasOwner())
		return;

	// send mail
	BigInt ReturnsGold = std::max((BigInt)1, m_pBankManager->Get());
	MailWrapper Mail("System", m_AccountID, "House is sold.");
	Mail.AddDescLine("Your house is sold.");
	mystd::process_bigint_in_chunks<int>(ReturnsGold, [&Mail](int chunk)
	{
		Mail.AttachItem(CItem(itGold, chunk));
	});
	Mail.Send();

	// Update the house data
	if(CPlayer* pPlayer = GetPlayer())
	{
		pPlayer->Account()->ReinitializeHouse(true);
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
	}
	m_pDoorManager->CloseAll();
	m_pBankManager->Reset();
	m_AccountID = -1;
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = NULL, Bank = '0', AccessList = NULL WHERE ID = '{}'", m_ID);

	// send information
	GS()->ChatAccount(m_AccountID, "Your House is sold!");
	GS()->Chat(-1, "House: '{}' have been is released!", m_ID);
}

void CHouse::UpdateText(int Lifetime) const
{
	// check valid vector and now time
	if(is_negative_vec(m_TextPosition))
		return;

	// initialize variable with name
	const char* pName = HasOwner() ? Server()->GetAccountNickname(m_AccountID) : "FREE HOUSE";
	GS()->EntityManager()->Text(m_TextPosition, Lifetime - 5, pName);
}

void CHouse::HandleTimePeriod(ETimePeriod Period)
{
	// day event rent paid
	if(Period == DAILY_STAMP && HasOwner())
	{
		// try spend to rent paid
		if(!m_pBankManager->Spend(GetRentPrice()))
		{
			Sell();
			return;
		}

		// send message
		GS()->ChatAccount(m_AccountID, "Your house rent has been paid.");
	}
}

int CHouse::GetRentPrice() const
{
	int DoorCount = (int)GetDoorManager()->GetContainer().size();
	int FarmzoneCount = (int)GetFarmzonesManager()->GetContainer().size();
	return (DoorCount * 400) + (FarmzoneCount * 400);
}