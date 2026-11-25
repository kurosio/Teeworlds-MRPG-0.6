/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_profile.h"
#include "account_auth.h"
#include "account_db.h"

#include <base/system.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/inventory/item_data.h>
#include <game/server/core/tools/db_async_context.h>

namespace
{
	CGS* GetGameServer(int ClientID)
	{
		return static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
	}

	// The shared body of /set_pin and /change_pin. IsChangingPin selects what
	// confirms the change: the old PIN, or the account password for a first PIN.
	void SavePin(int ClientID, const char* pCurrentPasswordOrPin, const char* pNewPin, bool IsChangingPin)
	{
		// check valid player
		auto* pGS = GetGameServer(ClientID);
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, true) : nullptr;
		if(!pPlayer || !pPlayer->IsAuthed())
		{
			if(pGS)
				pGS->Chat(ClientID, "You must be logged in to set or change a PIN code.");
			return;
		}

		// check valid pin length and digits
		const std::string NewPin = pNewPin;
		if(!AccountAuth::IsValidPinLength(NewPin))
		{
			pGS->Chat(ClientID, "PIN code must be {} to {} digits long.", AccountAuth::LENGTH_PIN_MIN, AccountAuth::LENGTH_PIN_MAX);
			return;
		}
		if(!AccountAuth::IsDigitsOnly(NewPin))
		{
			pGS->Chat(ClientID, "PIN code must consist of digits only.");
			return;
		}

		// One statement answers everything the checks below need: whether a PIN is
		// set (and which one) and whether the entered password is the account one.
		const int AccountID = pPlayer->Account()->GetID();
		const std::string Current = pCurrentPasswordOrPin ? pCurrentPasswordOrPin : "";
		auto pContext = DbAsync::MakeContext(ClientID);
		AccountDb::ReadPinCodeAndPassword(AccountID)
			.Then([pContext, AccountID, IsChangingPin, NewPin, Current](Db::CResultSet&& Account)
		{
			if(!pContext->GetPlayer())
				return;

			auto* pGS = pContext->GS();
			const int ClientID = pContext->GetClientID();
			if(!Account.HasRows())
			{
				pGS->Chat(ClientID, "Error retrieving account data.");
				return;
			}

			const std::string PinInDb = Account.GetString("PinCode");
			const bool PinIsSet = !PinInDb.empty();

			if(IsChangingPin)
			{
				// is not set need set pin
				if(!PinIsSet)
				{
					pGS->Chat(ClientID, "You don't have a PIN code set.");
					pGS->Chat(ClientID, "Use '/set_pin' to set your PIN.");
					return;
				}

				// check valid pin
				if(PinInDb != Current)
				{
					pGS->Chat(ClientID, "The current PIN code you entered is incorrect.");
					return;
				}
				if(Current == NewPin)
				{
					pGS->Chat(ClientID, "The new PIN cannot be the same as the old PIN.");
					return;
				}
			}
			else
			{
				// is set need change pin
				if(PinIsSet)
				{
					pGS->Chat(ClientID, "You already have a PIN code set.");
					pGS->Chat(ClientID, "Use '/change_pin' to update your PIN.");
					return;
				}

				// check valid password
				if(Account.GetString("Password") != AccountAuth::HashPassword(Current.c_str(), Account.GetString("PasswordSalt")))
				{
					pGS->Chat(ClientID, "The account password you entered is incorrect.");
					return;
				}
			}

			// save new pin-code
			AccountDb::SavePinCode(AccountID, NewPin);
			pGS->Chat(ClientID, "Your PIN code has been successfully {}!", Instance::Localize(ClientID, IsChangingPin ? "changed" : "set"));
		});
	}
}

namespace AccountProfile
{
	void SetPin(int ClientID, const char* pPassword, const char* pNewPin)
	{
		SavePin(ClientID, pPassword, pNewPin, false);
	}

	void ChangePin(int ClientID, const char* pOldPin, const char* pNewPin)
	{
		SavePin(ClientID, pOldPin, pNewPin, true);
	}

	void ChangePassword(int ClientID, const char* pOldPassword, const char* pNewPassword, const char* pPinCode)
	{
		// check valid account
		auto* pGS = GetGameServer(ClientID);
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, true) : nullptr;
		if(!pPlayer || !pPlayer->IsAuthed())
		{
			if(pGS)
				pGS->Chat(ClientID, "You must be logged in to change your password.");
			return;
		}

		// check valid length and password same
		const int LengthOldPass = str_length(pOldPassword);
		const int LengthNewPass = str_length(pNewPassword);
		if(LengthOldPass < AccountAuth::LENGTH_LOGPASS_MIN || LengthOldPass > AccountAuth::LENGTH_LOGPASS_MAX
			|| LengthNewPass < AccountAuth::LENGTH_LOGPASS_MIN || LengthNewPass > AccountAuth::LENGTH_LOGPASS_MAX)
		{
			pGS->Chat(ClientID, "The old and new passwords must each contain '{} - {} characters'.", AccountAuth::LENGTH_LOGPASS_MIN, AccountAuth::LENGTH_LOGPASS_MAX);
			return;
		}
		if(str_comp(pOldPassword, pNewPassword) == 0)
		{
			pGS->Chat(ClientID, "The new password cannot be the same as the old password.");
			return;
		}

		// require pin code
		if(pPinCode == nullptr || *pPinCode == '\0')
		{
			pGS->Chat(ClientID, "PIN code is required to change your password.");
			return;
		}

		// check valid and length pin code
		if(!AccountAuth::IsValidPinLength(pPinCode))
		{
			pGS->Chat(ClientID, "Invalid PIN code format.");
			return;
		}
		if(!AccountAuth::IsDigitsOnly(pPinCode))
		{
			pGS->Chat(ClientID, "PIN code must consist of digits only.");
			return;
		}

		// One statement answers both questions: the PIN of the account and its
		// password hash. The checks and the hash of the new password follow in the
		// answer, on the game thread.
		const int AccountID = pPlayer->Account()->GetID();
		const std::string OldPassword = pOldPassword;
		const std::string NewPassword = pNewPassword;
		const std::string Pin = pPinCode;
		auto pContext = DbAsync::MakeContext(ClientID);
		AccountDb::ReadPinCodeAndPassword(AccountID)
			.Then([pContext, AccountID, OldPassword, NewPassword, Pin](Db::CResultSet&& Account)
		{
			if(!pContext->GetPlayer())
				return;

			auto* pGS = pContext->GS();
			const int ClientID = pContext->GetClientID();
			if(!Account.HasRows())
			{
				pGS->Chat(ClientID, "Error retrieving account data.");
				return;
			}

			// check is pincode set
			const std::string PinFromDb = Account.GetString("PinCode");
			if(PinFromDb.empty())
			{
				pGS->Chat(ClientID, "For security, you must first set up a PIN code.");
				pGS->Chat(ClientID, "Use '/set_pin' to set your PIN.");
				return;
			}

			// check valid pincode
			if(PinFromDb != Pin)
			{
				pGS->Chat(ClientID, "The PIN code you entered is incorrect.");
				return;
			}

			// check valid password
			if(Account.GetString("Password") != AccountAuth::HashPassword(OldPassword.c_str(), Account.GetString("PasswordSalt")))
			{
				pGS->Chat(ClientID, "The old password you entered is incorrect.");
				return;
			}

			// update new password
			char aNewSalt[32] = { 0 };
			secure_random_password(aNewSalt, sizeof(aNewSalt), 24);
			const std::string HashedNewPassword = AccountAuth::HashPassword(NewPassword.c_str(), aNewSalt);
			AccountDb::SavePassword(AccountID, HashedNewPassword, aNewSalt);

			// information
			pGS->Chat(ClientID, "Your password has been successfully changed!");
			pGS->Chat(ClientID, "Please remember your new login details.");
			pGS->Chat(ClientID, "New password: '{}'.", NewPassword.c_str());
		});
	}

	void ChangeNickname(int ClientID, const std::string& NewNickname)
	{
		// The name is only changed when the database says it is free; the player is
		// told in the answer, so nothing waits for the lookup.
		auto* pGS = GetGameServer(ClientID);
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, true) : nullptr;
		if(!pPlayer)
			return;
		const int AccountID = pPlayer->Account()->GetID();

		auto pContext = DbAsync::MakeContext(ClientID);
		AccountDb::FindNicknameOwner(NewNickname)
			.Then([pContext, AccountID, NewNickname](Db::CResultSet&& Existing)
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer)
				return;

			auto* pGS = pContext->GS();
			const int ClientID = pContext->GetClientID();
			if(Existing.HasRows())
			{
				pGS->Chat(ClientID, "This nickname is already in use.");
				return;
			}

			AccountDb::SaveNickname(AccountID, NewNickname);
			pGS->Server()->SetClientName(ClientID, NewNickname.c_str());
			pGS->Chat(ClientID, "Your nickname has been successfully updated.");
		});
	}

	void UseVoucher(int ClientID, const char* pVoucher)
	{
		auto* pGS = GetGameServer(ClientID);
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID) : nullptr;
		if(!pPlayer || !pPlayer->IsAuthed())
			return;

		// The voucher decides what the player gets, so it is read, checked and
		// redeemed in the answer; nothing waits for it here.
		const std::string VoucherCode = pVoucher;
		auto pContext = DbAsync::MakeContext(ClientID);
		AccountDb::FindVoucher(pPlayer->Account()->GetID(), VoucherCode)
			.Then([pContext, VoucherCode](Db::CResultSet&& Voucher)
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer)
				return;

			auto* pGS = pContext->GS();
			const int ClientID = pContext->GetClientID();
			if(!Voucher.HasRows())
			{
				pGS->Chat(ClientID, "The voucher code '{}' does not exist.", VoucherCode.c_str());
				return;
			}

			const int VoucherID = Voucher.GetInt("ID");
			const int ValidUntil = Voucher.GetInt("ValidUntil");

			// The voucher decides what the player gets, so a document that cannot be
			// read gives nothing: the parsing helper reports it and hands back an
			// empty object instead of throwing inside the answer.
			nlohmann::json JsonData = nlohmann::json::object();
			mystd::json::parse(Voucher.GetString("Data"), [&JsonData](nlohmann::json& Json) { JsonData = Json; });

			if(ValidUntil > 0 && ValidUntil < time(0))
			{
				pGS->Chat(ClientID, "The voucher code '{}' has expired.", VoucherCode.c_str());
				return;
			}

			if(Voucher.GetBoolean("used"))
			{
				pGS->Chat(ClientID, "This voucher has already been redeemed.");
				return;
			}

			const int Exp = JsonData.value("exp", 0);
			const int Money = JsonData.value("money", 0);

			if(Exp > 0)
				pPlayer->Account()->AddExperience(Exp);
			if(Money > 0)
				pPlayer->Account()->AddGold(Money);

			if(JsonData.find("items") != JsonData.end() && JsonData["items"].is_array())
			{
				for(const nlohmann::json& Item : JsonData["items"])
				{
					const int ItemID = Item.value("id", -1);
					const int Value = Item.value("value", 0);
					if(Value > 0 && CItemDescription::Data().contains(ItemID))
						pPlayer->GetItem(ItemID)->Add(Value);
				}
			}

			pGS->Core()->SaveAccount(pPlayer, SAVE_STATS);
			pGS->Core()->SaveAccount(pPlayer, SAVE_UPGRADES);

			AccountDb::RedeemVoucher(VoucherID, pPlayer->Account()->GetID(), (int64_t)time(0));
			pGS->Chat(ClientID, "You have successfully redeemed the voucher '{}'.", VoucherCode.c_str());
		});
	}
}
