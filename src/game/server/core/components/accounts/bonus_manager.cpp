#include "bonus_manager.h"

#include <engine/shared/linereader.h>
#include <game/server/gamecontext.h>

static std::string GetFileName(int AccountID)
{
	return fmt_default("server_data/account_bonuses/{}.txt", AccountID);
}

void BonusManager::SendInfoAboutActiveBonuses() const
{
	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
	if (!pGS) return;

	const auto Count = std::ranges::count_if(m_vTemporaryBonuses, [](const TemporaryBonus& B) {
		return B.IsActive();
		});
	if (Count == 0)
		pGS->Chat(m_ClientID, "You have no active bonuses.");
	else
		pGS->Chat(m_ClientID, "You have '{} active bonus{}'.", Count, Count > 1 ? "es" : "");
}

void BonusManager::AddBonus(const TemporaryBonus& Bonus)
{
	if (!CMultiplierManager::IsValidType(Bonus.Type) || !CMultiplierManager::IsValidSource(Bonus.Source)
		|| Bonus.Source == MultiplierSource::World || Bonus.Source == MultiplierSource::RandomEvent
		|| !std::isfinite(Bonus.Amount) || Bonus.Amount <= 0.0f || Bonus.Duration <= 0)
		return;

	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
	if (!pGS) return;

	const time_t Now = time(nullptr);
	for (auto& Existing : m_vTemporaryBonuses)
	{
		if (Existing.Type != Bonus.Type || Existing.Amount != Bonus.Amount
			|| Existing.Source != Bonus.Source || !Existing.IsActive())
			continue;

		const int Remaining = Existing.RemainingTime();
		Existing.Duration = (Bonus.Duration > std::numeric_limits<int>::max() - Remaining)
			? std::numeric_limits<int>::max()
			: Remaining + Bonus.Duration;
		Existing.StartTime = Now;
		pGS->Chat(m_ClientID, "'{}' extended by '{} minutes'. Total: '{} minutes'.",
			CMultiplierManager::GetTypeName(Bonus.Type), Bonus.Duration / 60, Existing.Duration / 60);
		RebuildTimedMultipliers();
		Save();
		return;
	}

	TemporaryBonus NewBonus = Bonus;
	NewBonus.StartTime = Now;
	m_vTemporaryBonuses.push_back(NewBonus);
	pGS->Chat(m_ClientID, "You received '{} +{~.2}% from {}.'",
		CMultiplierManager::GetTypeName(Bonus.Type), Bonus.Amount, CMultiplierManager::GetSourceName(Bonus.Source));
	RebuildTimedMultipliers();
	Save();
}

void BonusManager::Load()
{
	m_vTemporaryBonuses.clear();
	m_TimedMultipliers.Clear();

	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
	const auto* pPlayer = pGS ? pGS->GetPlayer(m_ClientID) : nullptr;
	if (!pPlayer) return;

	CLineReader Reader;
	if (!Reader.OpenFile(pGS->Storage()->OpenFile(GetFileName(pPlayer->Account()->GetID()).c_str(), IOFLAG_READ, IStorageEngine::TYPE_ABSOLUTE)))
		return;

	const time_t Now = time(nullptr);
	while (const char* pLine = Reader.Get())
	{
		TemporaryBonus Bonus;
		int SavedType{}, SavedSource{};
#if defined(__GNUC__) && __WORDSIZE == 64
		const int Fields = sscanf(pLine, "%d %f %ld %d %d", &SavedType, &Bonus.Amount, &Bonus.StartTime, &Bonus.Duration, &SavedSource);
#else
		const int Fields = sscanf(pLine, "%d %f %lld %d %d", &SavedType, &Bonus.Amount, &Bonus.StartTime, &Bonus.Duration, &SavedSource);
#endif
		const auto Type = CMultiplierManager::FromId(SavedType);
		if (Fields < 4 || !Type || !std::isfinite(Bonus.Amount) || Bonus.Amount <= 0.0f || Bonus.Duration <= 0)
			continue;
		Bonus.Type = *Type;

		if (Fields >= 5)
		{
			const auto Source = CMultiplierManager::SourceFromId(SavedSource);
			if (!Source || *Source == MultiplierSource::World || *Source == MultiplierSource::RandomEvent)
				continue;
			Bonus.Source = *Source;
		}

		const double Elapsed = difftime(Now, Bonus.StartTime);
		if (!std::isfinite(Elapsed) || Elapsed >= Bonus.Duration)
			continue;

		Bonus.StartTime = Now - std::max(0, static_cast<int>(Elapsed));
		m_vTemporaryBonuses.push_back(Bonus);
	}
	RebuildTimedMultipliers();
}

void BonusManager::Save() const
{
	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
	const auto* pPlayer = pGS ? pGS->GetPlayer(m_ClientID) : nullptr;
	if (!pPlayer) return;

	auto* pStorage = pGS->Storage();
	if (!pStorage->FolderExists("server_data/account_bonuses", IStorageEngine::TYPE_ABSOLUTE))
		pStorage->CreateFolder("server_data/account_bonuses", IStorageEngine::TYPE_ABSOLUTE);

	if (const auto File = pStorage->OpenFile(GetFileName(pPlayer->Account()->GetID()).c_str(), IOFLAG_WRITE, IStorageEngine::TYPE_ABSOLUTE))
	{
		for (const auto& B : m_vTemporaryBonuses)
		{
			char Buf[128];
#if defined(__GNUC__) && __WORDSIZE == 64
			str_format(Buf, sizeof(Buf), "%d %.9g %ld %d %d\n", static_cast<int>(B.Type), B.Amount, B.StartTime, B.Duration, static_cast<int>(B.Source));
#else
			str_format(Buf, sizeof(Buf), "%d %.9g %lld %d %d\n", static_cast<int>(B.Type), B.Amount, B.StartTime, B.Duration, static_cast<int>(B.Source));
#endif
			io_write(File, Buf, str_length(Buf));
		}
		io_close(File);
	}
}

void BonusManager::PostTick()
{
	bool Changed = false;
	for (auto It = m_vTemporaryBonuses.begin(); It != m_vTemporaryBonuses.end();)
	{
		if (!It->IsActive())
		{
			if (auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID)))
				pGS->Chat(m_ClientID, "Your '{}' of '{~.2}%' has expired.", CMultiplierManager::GetTypeName(It->Type), It->Amount);
			It = m_vTemporaryBonuses.erase(It);
			Changed = true;
		}
		else
			++It;
	}
	if (Changed)
	{
		RebuildTimedMultipliers();
		Save();
	}
}

void BonusManager::RebuildTimedMultipliers()
{
	m_TimedMultipliers.Clear();
	for (const auto& B : m_vTemporaryBonuses)
	{
		if (!CMultiplierManager::IsValidType(B.Type) || !B.IsActive())
			continue;
		m_TimedMultipliers.AddMultiplier(B.Type, B.Source, B.Amount,
			fmt_default("{}: {}", CMultiplierManager::GetSourceName(B.Source), CMultiplierManager::GetTypeName(B.Type)));
	}
}

float BonusManager::GetTotalBonusPercentage(MultiplierType Type) const
{
	if (!CMultiplierManager::IsValidType(Type))
		return 0.0f;
	long double Total = m_TimedMultipliers.GetTotalPercent(Type);
	if (const auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID)))
		Total += pGS->m_Multipliers.GetTotalPercent(Type);
	return CMultiplierManager::ClampPercent(Total);
}

std::vector<MultiplierType> BonusManager::GetActiveMultiplierTypes() const
{
	std::vector<MultiplierType> Types;
	const auto Add = [&Types](const auto& Contributions) {
		for (const auto& C : Contributions)
			if (CMultiplierManager::IsValidType(C.m_Type) && std::ranges::find(Types, C.m_Type) == Types.end())
				Types.push_back(C.m_Type);
		};
	Add(m_TimedMultipliers.GetContributions());
	if (const auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID)))
		Add(pGS->m_Multipliers.GetContributions());
	std::ranges::sort(Types);
	return Types;
}

std::string BonusManager::GetBonusActivitiesString() const
{
	std::string Result;
	int InLine = 0;
	for (const auto Type : GetActiveMultiplierTypes())
	{
		const int Pct = round_to_int(GetTotalBonusPercentage(Type));
		if (Pct <= 0) continue;
		if (!Result.empty())
			Result += (InLine >= 2) ? "\n" : ", ";
		Result += fmt_default("{} +{}%", CMultiplierManager::GetTypeName(Type), Pct);
		InLine = (InLine >= 2) ? 1 : InLine + 1;
	}
	return Result;
}