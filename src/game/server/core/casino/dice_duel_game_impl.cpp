#include "entities/dice_laser.h"
#include "dice_duel_game.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>

static int Combos(int s)
{
	return (s < 2 || s > 12) ? 0 : (s <= 7 ? s - 1 : 13 - s);
}

float CDiceDuelGame::GetWinProbability(EDiceBetType Type, int Threshold)
{
	int f = 0;
	for (int s = 2; s <= 12; s++)
	{
		const bool ok =
			(Type == EDiceBetType::HIGH && s > Threshold) ||
			(Type == EDiceBetType::LOW && s < Threshold) ||
			(Type == EDiceBetType::EQUAL && s == Threshold);
		if (ok)
			f += Combos(s);
	}

	return f / 36.0f;
}

float CDiceDuelGame::GetMultiplier(EDiceBetType Type, int Threshold)
{
	const float p = GetWinProbability(Type, Threshold);
	return p <= 0.0001f ? 0.0f : (1.0f / p) * 0.97f;
}

int CDiceDuelGame::CalcPayout(int Bet, EDiceBetType Type, int Threshold)
{
	const float m = GetMultiplier(Type, Threshold);
	if (m <= 0.0f)
		return 0;

	// calculate in 64-bit and clamp: a big bet times the multiplier can overflow int
	const auto Payout = (int64_t)std::ceil((double)Bet * (double)m);
	return (int)std::clamp<int64_t>(Payout, 0, std::numeric_limits<int>::max());
}

bool CDiceDuelGame::IsValidThreshold(EDiceBetType Type, int Threshold)
{
	switch (Type)
	{
		case EDiceBetType::HIGH: return Threshold >= 2 && Threshold <= 11;
		case EDiceBetType::LOW: return Threshold >= 3 && Threshold <= 12;
		case EDiceBetType::EQUAL: return Threshold >= 2 && Threshold <= 12;
	}
	return false;
}

const char* CDiceDuelGame::BetTypeName(EDiceBetType T)
{
	switch (T)
	{
		case EDiceBetType::HIGH: return "HIGH";
		case EDiceBetType::LOW: return "LOW";
		case EDiceBetType::EQUAL: return "EQUAL";
	}

	return "?";
}

CDiceDuelGame::CDiceDuelGame(CGS* pGS, vec2 Pos, int LobbyDurationTicks, int ShowResultTicks)
	: CCasinoGame(pGS, LobbyDurationTicks, ShowResultTicks), m_pGS(pGS), m_Pos(Pos)
{
	m_LobbyDurationTicks = LobbyDurationTicks;
	m_ShowResultTicks = ShowResultTicks;

	const vec2 Off(90.0f, 0.0f);
	m_apDice[0] = new CEntityDiceLaser(&m_pGS->m_World, -1, m_Pos - Off, 1, 2.0f);
	m_apDice[1] = new CEntityDiceLaser(&m_pGS->m_World, -1, m_Pos + Off, 1, 2.0f);

	EnterLobby();
}

CDiceDuelGame::~CDiceDuelGame()
{
	for (auto* pDice : m_apDice)
	{
		if (pDice)
			pDice->Destroy();
	}
}

int CDiceDuelGame::GetTicksLeft() const
{
	return maximum(0, m_StateEndTick - m_pGS->Server()->Tick());
}

void CDiceDuelGame::EnterLobby()
{
	m_State = EDiceGameState::LOBBY;
	ClearPlayers();
	m_aFaces[0] = 0;
	m_aFaces[1] = 0;
	m_FinishedDice = 0;
	m_StateEndTick = 0;
}

void CDiceDuelGame::EnterRolling()
{
	m_State = EDiceGameState::ROLLING;
	m_FinishedDice = 0;

	const int Dur = m_pGS->Server()->TickSpeed() * 3;
	for (int i = 0; i < 2; i++)
	{
		m_apDice[i]->SetOnRollFinished([this, i](int Face)
		{
			m_aFaces[i] = Face;
			if (++m_FinishedDice >= 2)
			{
				DiceResult Result;
				Result.m_Face1 = m_aFaces[0];
				Result.m_Face2 = m_aFaces[1];
				Result.m_Sum = Result.m_Face1 + Result.m_Face2;
				Result.m_vPlayers.reserve(GetPlayers().size());

				for (auto& slotBet : GetPlayers())
				{
					auto betType = static_cast<EDiceBetType>(slotBet.m_Type);
					slotBet.m_Win =
						(betType == EDiceBetType::HIGH && Result.m_Sum > slotBet.m_Threshold) ||
						(betType == EDiceBetType::LOW && Result.m_Sum < slotBet.m_Threshold) ||
						(betType == EDiceBetType::EQUAL && Result.m_Sum == slotBet.m_Threshold);
					slotBet.m_Payout = slotBet.m_Win ? CalcPayout(slotBet.m_Bet, betType, slotBet.m_Threshold) : 0;
					if (slotBet.m_Win)
						SettleBet(slotBet);
					Result.m_vPlayers.push_back(slotBet);
				}

				if (m_pfnOnFinished)
					m_pfnOnFinished(Result);
				EnterShowResult();
			}
		});
	}

	m_apDice[0]->Roll(Dur);
	m_apDice[1]->Roll(Dur + m_pGS->Server()->TickSpeed() / 4);

	NotifyStart();
}

void CDiceDuelGame::EnterShowResult()
{
	m_State = EDiceGameState::SHOW_RESULT;
	m_StateEndTick = m_pGS->Server()->Tick() + m_ShowResultTicks;
}

void CDiceDuelGame::PurgeOutOfRange()
{
	CCasinoGame::PurgeOutOfRange();
}

void CDiceDuelGame::Tick()
{
	switch (m_State)
	{
		case EDiceGameState::LOBBY:
		{
			PurgeOutOfRange();

			if (GetPlayers().empty())
			{
			m_StateEndTick = 0;
				return;
			}

			if (m_StateEndTick == 0)
				m_StateEndTick = m_pGS->Server()->Tick() + m_LobbyDurationTicks;

			if (m_pGS->Server()->Tick() >= m_StateEndTick)
				EnterRolling();

		} break;

		case EDiceGameState::ROLLING:
			break;

		case EDiceGameState::SHOW_RESULT:
			if (m_pGS->Server()->Tick() >= m_StateEndTick)
				EnterLobby();
			break;
	}
}

bool CDiceDuelGame::Join(int ClientID, int Bet, EDiceBetType Type, int Threshold)
{
	if (m_State != EDiceGameState::LOBBY || Bet <= 0 || !IsValidThreshold(Type, Threshold))
		return false;

	auto* pPlayer = m_pGS->GetPlayer(ClientID, true);
	if (!pPlayer || !pPlayer->GetCharacter())
		return false;

	if (IsPlayerJoined(ClientID))
		return false;

	const auto Currency = pPlayer->m_CurrencyCasinoItemID;
	return CCasinoGame::Join(ClientID, Bet, static_cast<int>(Type), Threshold, Currency);
}

bool CDiceDuelGame::Leave(int ClientID, int* pRefund)
{
	if (m_State != EDiceGameState::LOBBY)
		return false;

	return CCasinoGame::Leave(ClientID, pRefund);
}
