/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "class_data.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>

void ClassData::SetProfessionID(ProfessionIdentifier ProfID)
{
	m_ProfessionID = ProfID;
	UpdateProfessionSkin();
}

float ClassData::GetExtraHP() const
{
	if(m_ProfessionID == ProfessionIdentifier::Tank)
		return 30.f;
	if(m_ProfessionID == ProfessionIdentifier::Healer)
		return 15.f;
	if(m_ProfessionID == ProfessionIdentifier::Dps)
		return 5.f;
	return 0.f;
}

float ClassData::GetExtraMP() const
{
	if(m_ProfessionID == ProfessionIdentifier::Tank)
		return 5.f;
	if(m_ProfessionID == ProfessionIdentifier::Healer)
		return 30.f;
	if(m_ProfessionID == ProfessionIdentifier::Dps)
		return 15.f;
	return 0;
}

float ClassData::GetExtraDMG() const
{
	if(m_ProfessionID == ProfessionIdentifier::Tank)
		return 10.f;
	if(m_ProfessionID == ProfessionIdentifier::Healer)
		return 5.f;
	if(m_ProfessionID == ProfessionIdentifier::Dps)
		return 30.f;
	return 0;
}

void ClassData::UpdateProfessionSkin() const
{
	const auto* pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);

	// check valid player
	auto* pPlayer = pGS->GetPlayer(m_ClientID);
	if(!pPlayer)
		return;

	// check customizer
	if(pPlayer->GetItem(itCustomizer)->IsEquipped())
		return;

	// update tee info
	if(m_ProfessionID == ProfessionIdentifier::Tank)
	{
		str_copy(pPlayer->Account()->m_TeeInfos.m_aSkinName, "red_panda", sizeof(pPlayer->Account()->m_TeeInfos.m_aSkinName));
	}
	else if(m_ProfessionID == ProfessionIdentifier::Healer)
	{
		str_copy(pPlayer->Account()->m_TeeInfos.m_aSkinName, "Empieza", sizeof(pPlayer->Account()->m_TeeInfos.m_aSkinName));
	}
	else if(m_ProfessionID == ProfessionIdentifier::Dps)
	{
		str_copy(pPlayer->Account()->m_TeeInfos.m_aSkinName, "flokes", sizeof(pPlayer->Account()->m_TeeInfos.m_aSkinName));
	}
	pPlayer->Account()->m_TeeInfos.m_UseCustomColor = 0;
}