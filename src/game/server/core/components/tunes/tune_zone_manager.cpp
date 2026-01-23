#include "tune_zone_manager.h"

#include <base/format.h>
#include <base/hash.h>
#include <engine/shared/datafile.h>
#include <generated/server_data.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

namespace
{
	static std::vector<char> SerializeZeroSeparated(const std::vector<std::string_view>& Header,
		const std::vector<std::string>& Body)
	{
		size_t Total = 0;
		for(auto s : Header)
			Total += s.size() + 1;
		for(const auto& s : Body)
			Total += s.size() + 1;

		std::vector<char> Out;
		Out.resize(Total);

		size_t off = 0;
		auto Append = [&](std::string_view s)
		{
			if(!s.empty())
				mem_copy(Out.data() + off, s.data(), s.size());
			off += s.size();
			Out[off++] = '\0';
		};

		for(auto s : Header)
			Append(s);
		for(const auto& s : Body)
			Append(s);
		return Out;
	}

	static std::string MakePreparedPath(const char* pMapName)
	{
		std::string_view s(pMapName ? pMapName : "");
		const auto slash = s.find_last_of("/\\");
		const auto nameStart = (slash == std::string::npos) ? 0 : slash + 1;
		const auto dotPos = s.find_last_of('.');
		const auto nameEnd = (dotPos == std::string::npos || dotPos < nameStart) ? s.size() : dotPos;

		auto base = std::string(s.substr(nameStart, nameEnd - nameStart));
		auto ext = (nameEnd < s.size()) ? std::string(s.substr(nameEnd)) : ".map";
		return "maps/" + base + "_prepared" + ext;
	}

	static std::vector<std::string> BuildTuneCommands(const CTuneZoneManager& Mgr)
	{
		std::vector<std::string> v;
		v.reserve(16);

		for(const auto& [zoneType, zoneParams] : Mgr.m_Zones)
		{
			const auto& diff = zoneParams.GetDiff();
			v.reserve(v.size() + diff.size());
			for(const auto& [tuneIdx, tuneValue] : diff)
				v.emplace_back(fmt_default("tune_zone {} {} {}", Mgr.GetZoneID(zoneType), CTuningParams::Name(tuneIdx), tuneValue));
		}

		return v;
	}

	static std::string DataToString(CDataFileReader& R, int DataIndex)
	{
		if(DataIndex < 0)
			return {};

		const auto* p = (const char*)R.GetData(DataIndex);
		if(!p)
			return {};

		const int sz = R.GetDataSize(DataIndex);
		if(sz <= 0)
			return {};

		if(p[sz - 1] == '\0')
			return std::string(p);

		return std::string(p, sz);
	}

	static std::optional<std::string> ExtractSourceSha256(const char* pData, int DataSize)
	{
		if(!pData || DataSize <= 0)
			return std::nullopt;

		const std::string_view Prefix = "# source_sha256 ";
		const char* pCur = pData;
		const char* pEnd = pData + DataSize;
		while(pCur < pEnd)
		{
			const char* pStart = pCur;
			while(pCur < pEnd && *pCur)
				++pCur;

			std::string_view Entry(pStart, pCur - pStart);
			if(Entry.size() >= Prefix.size() && Entry.compare(0, Prefix.size(), Prefix) == 0)
				return std::string(Entry.substr(Prefix.size()));

			++pCur;
		}

		return std::nullopt;
	}
}

CTuneZoneManager& CTuneZoneManager::GetInstance()
{
	static CTuneZoneManager instance;
	return instance;
}

CTuneZoneManager::CTuneZoneManager()
{
	dbg_msg("TuneZoneManager", "Initializing Tune Zones...");

	CTuningParams params;
	m_Zones[ETuneZone::DEFAULT] = params;

	params = CTuningParams();
	params.m_Gravity = 0.25f;
	params.m_GroundJumpImpulse = 8.0f;
	params.m_AirFriction = 0.75f;
	params.m_AirControlAccel = 1.0f;
	params.m_AirControlSpeed = 3.75f;
	params.m_AirJumpImpulse = 8.0f;
	params.m_HookFireSpeed = 30.0f;
	params.m_HookDragAccel = 1.5f;
	params.m_HookDragSpeed = 8.0f;
	params.m_PlayerHooking = 0;
	m_Zones[ETuneZone::SLOW] = params;

	params = CTuningParams();
	params.m_GroundControlSpeed = 5.0f;
	params.m_GroundControlAccel = 1.0f;
	m_Zones[ETuneZone::WALKING] = params;

	if(m_Zones.size() > NUM_TUNEZONES)
		dbg_msg("TuneZoneManager", "Too much tune zones defined");
}

int CTuneZoneManager::GetZoneID(ETuneZone Zone) const
{
	return static_cast<int>(Zone);
}

const CTuningParams* CTuneZoneManager::GetParams(const ETuneZone Zone) const
{
	if(const auto it = m_Zones.find(Zone); it != m_Zones.end())
		return &it->second;
	return nullptr;
}

const CTuningParams* CTuneZoneManager::GetParams(const int ZoneID) const
{
	if(ZoneID < 0 || ZoneID >= static_cast<int>(ETuneZone::NUM_TUNE_ZONES))
		return nullptr;
	return GetParams(static_cast<ETuneZone>(ZoneID));
}

void CTuneZoneManager::LoadSoundsFromDirectory(const char* pRootDir, IStorageEngine* pStorage)
{
	m_Sounds.clear();
	if(!pRootDir)
	{
		dbg_msg("sound_loader", "No root directory provided");
		return;
	}

	int loaded = 0, missing = 0;
	dbg_msg("sound_loader", "Loading manifest sounds from '%s'...", pRootDir);

	for(int i = 0; i < SOUND_CUSTOM_COUNT; ++i)
	{
		const char* pRel = g_apSpecialSoundFiles[i];
		if(!pRel || !*pRel)
			continue;

		char aFull[IO_MAX_PATH_LENGTH];
		str_format(aFull, sizeof(aFull), "%s/%s", pRootDir, pRel);

		IOHANDLE File = pStorage->OpenFile(aFull, IOFLAG_READ, IStorageEngine::TYPE_ALL);
		if(!File)
		{
			++missing;
			dbg_msg("sound_loader", "Missing manifest sound: '%s'", aFull);
			continue;
		}

		const long long FileSize = io_length(File);
		if(FileSize <= 0)
		{
			io_close(File);
			++missing;
			dbg_msg("sound_loader", "Empty sound: '%s'", aFull);
			continue;
		}

		std::vector<char> vBuffer((size_t)FileSize);
		io_read(File, vBuffer.data(), FileSize);
		io_close(File);

		RegisterSound(pRel, vBuffer.data(), vBuffer.size());
		++loaded;
	}

	dbg_msg("sound_loader", "Loaded %d/%d manifest sounds (%d missing).", loaded, (int)SOUND_CUSTOM_COUNT, missing);
}

void CTuneZoneManager::RegisterSound(const std::string& Name, const void* pData, size_t DataSize)
{
	if(Name.empty() || !pData || DataSize == 0)
		return;

	const char* pCharData = static_cast<const char*>(pData);
	m_Sounds[Name].m_vData.assign(pCharData, pCharData + DataSize);
	dbg_msg("sound_baker", "Registered sound '%s' with size %zu", Name.c_str(), DataSize);
}

std::optional<std::string> CTuneZoneManager::BakePreparedMap(const char* pMapName, IStorageEngine* pStorage)
{
	const auto vNewCommands = BuildTuneCommands(*this);

	// 1. collect new sounds by manifest order
	struct TNewSoundRef
	{
		std::string m_Name;
		const std::vector<char>* m_pData {};
	};
	std::vector<TNewSoundRef> vNewSounds;
	vNewSounds.reserve(SOUND_CUSTOM_COUNT);
	for(int i = 0; i < SOUND_CUSTOM_COUNT; ++i)
	{
		const char* pRel = g_apSpecialSoundFiles[i];
		if(!pRel || !*pRel)
			continue;
		auto it = m_Sounds.find(pRel);
		if(it != m_Sounds.end())
			vNewSounds.push_back({ pRel, &it->second.m_vData });
		else
			dbg_msg("sound_baker", "Manifest sound missing in loaded set: '%s' (skip)", pRel);
	}
	const int NewCount = (int)vNewSounds.size();


	// 2. open base map getting settings and sounds
	CDataFileReader Reader;
	if(!Reader.Open(pStorage, pMapName, IStorageEngine::TYPE_ALL))
	{
		dbg_msg("tune_baker", "Failed to open source map '%s'", pMapName);
		return std::nullopt;
	}

	const SHA256_DIGEST SourceSha256 = Reader.Sha256();
	char aSourceSha256[SHA256_MAXSTRSIZE];
	sha256_str(SourceSha256, aSourceSha256, sizeof(aSourceSha256));
	const std::string SourceSha256Str = aSourceSha256;
	const std::string SourceHashLine = fmt_default("# source_sha256 {}", SourceSha256Str);

	std::vector<std::string_view> DeltaHeader;
	DeltaHeader.reserve(3);
	DeltaHeader.emplace_back("");
	if(!vNewCommands.empty())
		DeltaHeader.emplace_back("# Tune zones generated");
	DeltaHeader.emplace_back(SourceHashLine);

	const std::vector<char> Delta = SerializeZeroSeparated(DeltaHeader, vNewCommands);
	const bool HasDelta = !Delta.empty();

	bool HadSettingsOriginally = false;
	int SettingsIndex = -1;
	std::vector<char> SettingsCombined;
	int SoundItemVersionForNew = 1;

	struct TOrigSoundSnap
	{
		int m_OldId {};
		const CMapItemSound* m_pItem {};
		int m_ItemSize {};
		std::string m_Name;
		int m_External {};
		std::vector<char> m_Data;
	};
	std::vector<TOrigSoundSnap> vOrigSnaps;

	for(int i = 0; i < Reader.NumItems(); ++i)
	{
		int TypeId, ItemId;
		void* pItem = Reader.GetItem(i, &TypeId, &ItemId);
		const int ItemSize = Reader.GetItemSize(i);

		if(TypeId == MAPITEMTYPE_INFO && ItemId == 0)
		{
			if(ItemSize >= (int)sizeof(CMapItemInfoSettings))
			{
				auto* pInfo = (CMapItemInfoSettings*)pItem;
				if(pInfo->m_Settings > -1)
				{
					HadSettingsOriginally = true;
					SettingsIndex = pInfo->m_Settings;

					const char* pSet = (const char*)Reader.GetData(SettingsIndex);
					const int SetSize = Reader.GetDataSize(SettingsIndex);

					if(HasDelta)
					{
						SettingsCombined.resize(SetSize + (int)Delta.size());
						if(SetSize > 0)
							mem_copy(SettingsCombined.data(), pSet, SetSize);
						mem_copy(SettingsCombined.data() + SetSize, Delta.data(), Delta.size());
					}
					else
					{
						if(SetSize > 0)
						{
							SettingsCombined.resize(SetSize);
							mem_copy(SettingsCombined.data(), pSet, SetSize);
						}
					}
				}
			}
			continue;
		}

		if(TypeId == MAPITEMTYPE_SOUND)
		{
			if(ItemSize < (int)sizeof(CMapItemSound))
				continue;

			const auto* pSnd = (const CMapItemSound*)pItem;
			SoundItemVersionForNew = pSnd->m_Version;

			TOrigSoundSnap Snap;
			Snap.m_OldId = ItemId;
			Snap.m_pItem = pSnd;
			Snap.m_ItemSize = ItemSize;
			Snap.m_External = pSnd->m_External;
			Snap.m_Name = DataToString(Reader, pSnd->m_SoundName);

			if(Snap.m_External == 0)
			{
				if(const void* pBuf = Reader.GetData(pSnd->m_SoundData))
				{
					const int BufSize = Reader.GetDataSize(pSnd->m_SoundData);
					if(BufSize > 0)
					{
						Snap.m_Data.resize(BufSize);
						mem_copy(Snap.m_Data.data(), pBuf, BufSize);
					}
				}
			}

			vOrigSnaps.push_back(std::move(Snap));
			continue;
		}
	}

	std::sort(vOrigSnaps.begin(), vOrigSnaps.end(),
		[](const TOrigSoundSnap& a, const TOrigSoundSnap& b) { return a.m_OldId < b.m_OldId; });

	// if base map does not hass settings but we has delta creating combined settings.
	if(!HadSettingsOriginally && HasDelta)
	{
		SettingsIndex = Reader.NumData();
		SettingsCombined = Delta;
	}

	// 4. check is prepared map is updated.
	const std::string PreparedPath = MakePreparedPath(pMapName);
	auto PreparedIsUpToDate = [&](const std::string& Path) -> bool
	{
		CDataFileReader Prep;
		if(!Prep.Open(pStorage, Path.c_str(), IStorageEngine::TYPE_ALL))
			return false;

		// check settings
		int PrepSettingsIndex = -1;
		for(int i = 0; i < Prep.NumItems(); ++i)
		{
			int TypeId, ItemId;
			void* pItem = Prep.GetItem(i, &TypeId, &ItemId);
			if(TypeId == MAPITEMTYPE_INFO && ItemId == 0)
			{
				if(Prep.GetItemSize(i) >= (int)sizeof(CMapItemInfoSettings))
					PrepSettingsIndex = ((CMapItemInfoSettings*)pItem)->m_Settings;
				break;
			}
		}

		if(PrepSettingsIndex < 0)
		{
			Prep.Close();
			return false;
		}

		const char* pPrepSettings = (const char*)Prep.GetData(PrepSettingsIndex);
		const int PrepSettingsSize = Prep.GetDataSize(PrepSettingsIndex);
		const auto PrepSha256 = ExtractSourceSha256(pPrepSettings, PrepSettingsSize);
		if(!PrepSha256 || *PrepSha256 != SourceSha256Str)
		{
			Prep.Close();
			return false;
		}

		if(!SettingsCombined.empty())
		{
			if(PrepSettingsSize != (int)SettingsCombined.size() ||
				(PrepSettingsSize > 0 && mem_comp(pPrepSettings, SettingsCombined.data(), PrepSettingsSize) != 0))
			{
				Prep.Close();
				return false;
			}
		}
		else
		{
			if(PrepSettingsSize != 0)
			{
				Prep.Close();
				return false;
			}
		}

		// check sounds
		std::vector<std::pair<int, const CMapItemSound*>> vPrepSounds;
		vPrepSounds.reserve(NewCount + (int)vOrigSnaps.size());
		for(int i = 0; i < Prep.NumItems(); ++i)
		{
			int TypeId, ItemId;
			void* pItem = Prep.GetItem(i, &TypeId, &ItemId);
			if(TypeId == MAPITEMTYPE_SOUND)
			{
				if(Prep.GetItemSize(i) < (int)sizeof(CMapItemSound))
				{
					Prep.Close();
					return false;
				}
				vPrepSounds.emplace_back(ItemId, (const CMapItemSound*)pItem);
			}
		}
		std::sort(vPrepSounds.begin(), vPrepSounds.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });

		if((int)vPrepSounds.size() != NewCount + (int)vOrigSnaps.size())
		{
			Prep.Close();
			return false;
		}

		// first sounds from manifest.
		for(int i = 0; i < NewCount; ++i)
		{
			const auto* pS = vPrepSounds[i].second;

			const std::string Name = DataToString(Prep, pS->m_SoundName);
			if(Name != vNewSounds[i].m_Name)
			{
				Prep.Close();
				return false;
			}

			const void* pBuf = Prep.GetData(pS->m_SoundData);
			const int BufSize = Prep.GetDataSize(pS->m_SoundData);
			const auto& Need = *vNewSounds[i].m_pData;

			if(BufSize != (int)Need.size() ||
				(BufSize > 0 && mem_comp(pBuf, Need.data(), BufSize) != 0) ||
				pS->m_External != 0)
			{
				Prep.Close();
				return false;
			}
		}

		// other map sounds.
		for(size_t j = 0; j < vOrigSnaps.size(); ++j)
		{
			const auto* pS = vPrepSounds[NewCount + (int)j].second;

			if(DataToString(Prep, pS->m_SoundName) != vOrigSnaps[j].m_Name ||
				pS->m_External != vOrigSnaps[j].m_External)
			{
				Prep.Close();
				return false;
			}

			if(vOrigSnaps[j].m_External == 0)
			{
				const void* pBuf = Prep.GetData(pS->m_SoundData);
				const int BufSize = Prep.GetDataSize(pS->m_SoundData);
				const auto& Need = vOrigSnaps[j].m_Data;

				if(BufSize != (int)Need.size() ||
					(BufSize > 0 && mem_comp(pBuf, Need.data(), BufSize) != 0))
				{
					Prep.Close();
					return false;
				}
			}
		}

		Prep.Close();
		return true;
	};

	if(PreparedIsUpToDate(PreparedPath))
	{
		dbg_msg("tune_baker", "Prepared is up-to-date: '%s'", PreparedPath.c_str());
		Reader.Close();
		return PreparedPath;
	}

	// 5. write new map
	CDataFileWriter Writer;
	if(!Writer.Open(pStorage, PreparedPath.c_str(), IStorageEngine::TYPE_SAVE))
	{
		dbg_msg("tune_baker", "Failed to open prepared for write: '%s'", PreparedPath.c_str());
		Reader.Close();
		return std::nullopt;
	}

	// through all items and add them, replacing INFO and LAYERTYPE_SOUNDS if necessary.
	const int OldNumData = Reader.NumData();
	const bool InsertNewSettings = !SettingsCombined.empty() && !HadSettingsOriginally;
	const int BaseNewDataIndex = OldNumData + (InsertNewSettings ? 1 : 0);
	for(int i = 0; i < Reader.NumItems(); ++i)
	{
		int TypeId, ItemId;
		void* pItem = Reader.GetItem(i, &TypeId, &ItemId);
		const int Size = Reader.GetItemSize(i);

		if(TypeId == MAPITEMTYPE_SOUND)
			continue;

		if(TypeId == MAPITEMTYPE_INFO && ItemId == 0)
		{
			if(Size >= (int)sizeof(CMapItemInfoSettings))
			{
				CMapItemInfoSettings Info = *(CMapItemInfoSettings*)pItem;
				if(!SettingsCombined.empty())
					Info.m_Settings = HadSettingsOriginally ? SettingsIndex : OldNumData;
				Writer.AddItem(TypeId, ItemId, sizeof(Info), &Info);
			}
			else
			{
				CMapItemInfoSettings Info;
				*(CMapItemInfo*)&Info = *(CMapItemInfo*)pItem;
				Info.m_Settings = SettingsCombined.empty() ? -1 : OldNumData;
				Writer.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Info), &Info);
			}
			continue;
		}

		if(TypeId == MAPITEMTYPE_LAYER && Size >= (int)sizeof(CMapItemLayer))
		{
			auto* pBase = (CMapItemLayer*)pItem;
			if(pBase->m_Type == LAYERTYPE_SOUNDS && Size >= (int)sizeof(CMapItemLayerSounds))
			{
				CMapItemLayerSounds L = *(CMapItemLayerSounds*)pItem;
				if(L.m_Sound >= 0)
					L.m_Sound += NewCount;
				Writer.AddItem(TypeId, ItemId, sizeof(L), &L);
				continue;
			}
		}

		Writer.AddItem(TypeId, ItemId, Size, pItem);
	}

	// add sounds first from manifest
	for(int i = 0; i < NewCount; ++i)
	{
		CMapItemSound Snd;
		Snd.m_Version = SoundItemVersionForNew;
		Snd.m_External = 0;
		Snd.m_SoundName = BaseNewDataIndex + i * 2;
		Snd.m_SoundData = BaseNewDataIndex + i * 2 + 1;
		Writer.AddItem(MAPITEMTYPE_SOUND, i, sizeof(CMapItemSound), &Snd);
	}

	// add map sounds
	for(const auto& S : vOrigSnaps)
		Writer.AddItem(MAPITEMTYPE_SOUND, S.m_OldId + NewCount, S.m_ItemSize, (void*)S.m_pItem);

	// copy all data chunks from the source map, replacing/inserting settings as necessary
	for(int di = 0; di < Reader.NumData(); ++di)
	{
		if(!SettingsCombined.empty() && HadSettingsOriginally && di == SettingsIndex)
		{
			Writer.AddData((int)SettingsCombined.size(), SettingsCombined.data());
			continue;
		}
		int Compressed = 0;
		const void* pRaw = Reader.GetRawData(di, &Compressed);
		if(pRaw)
		{
			const int Uncompressed = Reader.GetDataSize(di);
			Writer.AddDataRaw(pRaw, Compressed, Uncompressed);
			free((void*)pRaw);
		}
	}
	if(InsertNewSettings)
		Writer.AddData((int)SettingsCombined.size(), SettingsCombined.data());

	// add new sound data : binary file
	for(int i = 0; i < NewCount; ++i)
	{
		const auto& Name = vNewSounds[i].m_Name;
		Writer.AddData((int)Name.size() + 1, Name.c_str());

		const auto& Buf = *vNewSounds[i].m_pData;
		Writer.AddData((int)Buf.size(), Buf.empty() ? nullptr : Buf.data());
	}

	Writer.Finish();
	Reader.Close();

	dbg_msg("tune_baker", "Prepared map written: '%s' (sounds: manifest first, settings appended)", PreparedPath.c_str());
	return PreparedPath;
}
