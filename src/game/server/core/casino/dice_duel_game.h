#ifndef GAME_SERVER_CORE_CASINO_DICE_DUEL_GAME_H
#define GAME_SERVER_CORE_CASINO_DICE_DUEL_GAME_H

#include "casino_game.h"

enum class EDiceBetType { HIGH = 0, LOW = 1, EQUAL = 2 };
enum class EDiceGameState { LOBBY, ROLLING, SHOW_RESULT };

class CGS;
class CEntityDiceLaser;

class CDiceDuelGame : public CCasinoGame
{
public:
	using SlotBet = CCasinoGame::SlotBet;

	struct DiceResult
	{
		int m_Face1{};
		int m_Face2{};
		int m_Sum{};
		std::vector<SlotBet> m_vPlayers{};
	};

	using FOnFinished = std::function<void(const DiceResult&)>;

private:
	vec2 m_Pos{};
	CEntityDiceLaser* m_apDice[2]{};
	EDiceGameState m_State = EDiceGameState::LOBBY;
	int m_aFaces[2]{};
	int m_FinishedDice{};

	FOnFinished m_pfnOnFinished;

	void EnterLobby();
	void EnterRolling();
	void EnterShowResult();

public:
	CDiceDuelGame(CGS* pGS, vec2 Pos, int LobbyDurationTicks = 15, int ShowResultTicks = 4);
	~CDiceDuelGame() override;

	void Tick() override;
	vec2 GetPos() const override { return m_Pos; }
	bool Leave(int ClientID, int* pRefund = nullptr) override;

	bool Join(int ClientID, int Bet, EDiceBetType Type, int Threshold);
	void SetOnFinished(FOnFinished pfCallback) { m_pfnOnFinished = std::move(pfCallback); }

	EDiceGameState GetState() const { return m_State; }

	static float GetWinProbability(EDiceBetType Type, int Threshold);
	static float GetMultiplier(EDiceBetType Type, int Threshold);
	static int CalcPayout(int Bet, EDiceBetType Type, int Threshold);
	static bool IsValidThreshold(EDiceBetType Type, int Threshold);
	static const char* BetTypeName(EDiceBetType Type);
};

#endif // GAME_SERVER_CORE_CASINO_DICE_DUEL_GAME_H
