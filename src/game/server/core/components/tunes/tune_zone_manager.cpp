#include "tune_zone_manager.h"
#include "generated/server_data.h"

#include <base/format.h>
#include <engine/shared/datafile.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

namespace
{
	static char* SerializeLines(const std::vector<std::string>& vLines, size_t& FinalSize)
	{
		FinalSize = 0;
		if(vLines.empty())
			return nullptr;

		FinalSize = std::accumulate(
			vLines.begin(), vLines.end(), size_t { 0 },
			[](size_t acc, const std::string& s) { return acc + s.size() + 1; });

		char* pBuffer = (char*)malloc(FinalSize);
		if(!pBuffer)
			return nullptr;

		size_t offset = 0;
		for(const auto& line : vLines)
		{
			const size_t len = line.size() + 1;
			mem_copy(pBuffer + offset, line.c_str(), len);
			offset += len;
		}
		return pBuffer;
	}

	static std::string MakePreparedPath(const char* pMapName)
	{
		std::string s = pMapName ? pMapName : "";
		const size_t slash = s.find_last_of("/\\");
		size_t dot = s.find_last_of('.');
		if(dot == std::string::npos || (slash != std::string::npos && dot < slash))
			dot = s.size();

		const std::string dir = (slash == std::string::npos) ? "" : s.substr(0, slash + 1);
		const std::string base = (slash == std::string::npos) ? s.substr(0, dot) : s.substr(slash + 1, dot - (slash + 1));
		const std::string ext = (dot < s.size()) ? s.substr(dot) : ".map";
		return dir + base + "_prepared" + ext;
	}

	static std::vector<std::string> BuildTuneCommands(const CTuneZoneManager& Mgr)
	{
		std::vector<std::string> v;
		for(const auto& [zoneType, zoneParams] : Mgr.m_Zones)
		{
			for(const auto& [tuneIdx, tuneValue] : zoneParams.GetDiff())
				v.emplace_back(fmt_default("tune_zone {} {} {}", Mgr.GetZoneID(zoneType), CTuningParams::Name(tuneIdx), tuneValue));
		}
		return v;
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
	// delta settings
	const auto vNewCommands = BuildTuneCommands(*this);

	size_t deltaLen = 0;
	std::unique_ptr<char, void(*)(void*)> pDelta(nullptr, free);
	if(!vNewCommands.empty())
	{
		std::vector<std::string> vLines;
		vLines.emplace_back("");
		vLines.emplace_back("# Tune zones generated");
		vLines.insert(vLines.end(), vNewCommands.begin(), vNewCommands.end());
		pDelta.reset(SerializeLines(vLines, deltaLen));
	}
	const bool hasDelta = pDelta && deltaLen > 0;

	// collect new sounds by manifest order
	struct TNewSoundRef
	{
		std::string m_Name;
		const std::vector<char>* m_pData {};
	};
	std::vector<TNewSoundRef> vNewSounds;
	vNewSounds.reserve(SOUND_CUSTOM_COUNT);

	for(int i = 0; i < SOUND_CUSTOM_COUNT; ++i)
	{
		const std::string fileName = g_apSpecialSoundFiles[i];
		const auto it = m_Sounds.find(fileName);
		if(it != m_Sounds.end())
		{
			vNewSounds.push_back({ fileName, &it->second.m_vData });
		}
		else
		{
			dbg_msg("sound_baker", "Manifest sound missing in loaded set: '%s' (skip)", fileName.c_str());
		}
	}
	const int newCount = (int)vNewSounds.size();

	// open base map
	CDataFileReader reader;
	if(!reader.Open(pStorage, pMapName, IStorageEngine::TYPE_ALL))
	{
		dbg_msg("tune_baker", "Failed to open source map '%s'", pMapName);
		return std::nullopt;
	}
	for(int i = 0; i < reader.NumData(); ++i)
		reader.GetData(i);

	// settings: source + delta
	bool hadSettingsOriginally = false;
	int settingsIndex = -1;
	std::vector<char> settingsCombined;

	for(int i = 0; i < reader.NumItems(); ++i)
	{
		int typeId, itemId;
		void* pItem = reader.GetItem(i, &typeId, &itemId);
		if(typeId == MAPITEMTYPE_INFO && itemId == 0)
		{
			const int size = reader.GetItemSize(i);
			if(size >= (int)sizeof(CMapItemInfoSettings))
			{
				auto* pInfo = (CMapItemInfoSettings*)pItem;
				if(pInfo->m_Settings > -1)
				{
					hadSettingsOriginally = true;
					settingsIndex = pInfo->m_Settings;

					const char* pSet = (const char*)reader.GetData(settingsIndex);
					const int setSize = reader.GetDataSize(settingsIndex);
					if(hasDelta)
					{
						settingsCombined.resize(setSize + (int)deltaLen);
						if(setSize > 0)
							mem_copy(settingsCombined.data(), pSet, setSize);
						mem_copy(settingsCombined.data() + setSize, pDelta.get(), deltaLen);
					}
					else
					{
						if(setSize > 0)
						{
							settingsCombined.resize(setSize);
							mem_copy(settingsCombined.data(), pSet, setSize);
						}
					}
				}
			}
			break;
		}
	}

	if(!hadSettingsOriginally && hasDelta)
	{
		settingsIndex = reader.NumData();
		settingsCombined.assign(pDelta.get(), pDelta.get() + deltaLen);
	}

	// snapshot source sounds
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
	int soundItemVersionForNew = 1;

	for(int i = 0; i < reader.NumItems(); ++i)
	{
		int typeId, itemId;
		void* pItem = reader.GetItem(i, &typeId, &itemId);
		if(typeId != MAPITEMTYPE_SOUND)
			continue;

		const int itemSize = reader.GetItemSize(i);
		if(itemSize < (int)sizeof(CMapItemSound))
			continue;

		const auto* pSnd = (const CMapItemSound*)pItem;
		soundItemVersionForNew = pSnd->m_Version;

		TOrigSoundSnap snap;
		snap.m_OldId = itemId;
		snap.m_pItem = pSnd;
		snap.m_ItemSize = itemSize;
		snap.m_External = pSnd->m_External;

		if(const auto* pName = (const char*)reader.GetData(pSnd->m_SoundName))
		{
			const int nameSize = reader.GetDataSize(pSnd->m_SoundName);
			if(nameSize > 0)
				snap.m_Name = (pName[nameSize - 1] == '\0') ? std::string(pName) : std::string(pName, nameSize);
		}

		if(snap.m_External == 0)
		{
			if(const void* pBuf = reader.GetData(pSnd->m_SoundData))
			{
				const int bufSize = reader.GetDataSize(pSnd->m_SoundData);
				if(bufSize > 0)
				{
					snap.m_Data.resize(bufSize);
					mem_copy(snap.m_Data.data(), pBuf, bufSize);
				}
			}
		}

		vOrigSnaps.push_back(std::move(snap));
	}

	std::stable_sort(vOrigSnaps.begin(), vOrigSnaps.end(),
		[](const TOrigSoundSnap& a, const TOrigSoundSnap& b) { return a.m_OldId < b.m_OldId; });

	// checking prepared to actual
	const std::string preparedPath = MakePreparedPath(pMapName);
	auto PreparedIsUpToDate = [&](const std::string& path) -> bool
	{
		CDataFileReader prep;
		if(!prep.Open(pStorage, path.c_str(), IStorageEngine::TYPE_ALL))
			return false;
		for(int i = 0; i < prep.NumData(); ++i)
			prep.GetData(i);

		// settings - bit a bit
		int prepSettingsIndex = -1;
		for(int i = 0; i < prep.NumItems(); ++i)
		{
			int typeId, itemId;
			void* pItem = prep.GetItem(i, &typeId, &itemId);
			if(typeId == MAPITEMTYPE_INFO && itemId == 0)
			{
				const int size = prep.GetItemSize(i);
				if(size >= (int)sizeof(CMapItemInfoSettings))
					prepSettingsIndex = ((CMapItemInfoSettings*)pItem)->m_Settings;
				break;
			}
		}

		if(!settingsCombined.empty())
		{
			if(prepSettingsIndex < 0)
			{
				prep.Close();
				return false;
			}

			const char* pS = (const char*)prep.GetData(prepSettingsIndex);
			const int sSize = prep.GetDataSize(prepSettingsIndex);

			if(sSize != (int)settingsCombined.size())
			{
				prep.Close();
				return false;
			}

			if(sSize > 0 && mem_comp(pS, settingsCombined.data(), sSize) != 0)
			{
				prep.Close();
				return false;
			}
		}
		else
		{
			if(prepSettingsIndex >= 0 && prep.GetDataSize(prepSettingsIndex) != 0)
			{
				prep.Close();
				return false;
			}
		}

		// check sounds
		std::vector<std::pair<int, const CMapItemSound*>> vPrepSounds;
		for(int i = 0; i < prep.NumItems(); ++i)
		{
			int typeId, itemId;
			void* pItem = prep.GetItem(i, &typeId, &itemId);
			if(typeId == MAPITEMTYPE_SOUND)
			{
				if(prep.GetItemSize(i) < (int)sizeof(CMapItemSound))
				{
					prep.Close();
					return false;
				}

				vPrepSounds.emplace_back(itemId, (const CMapItemSound*)pItem);
			}
		}
		std::sort(vPrepSounds.begin(), vPrepSounds.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });

		if((int)vPrepSounds.size() != newCount + (int)vOrigSnaps.size())
		{
			prep.Close();
			return false;
		}

		// manifest sounds
		for(int i = 0; i < newCount; ++i)
		{
			const auto* pS = vPrepSounds[i].second;

			const char* pName = (const char*)prep.GetData(pS->m_SoundName);
			const int nameSize = prep.GetDataSize(pS->m_SoundName);
			if(!pName)
			{
				prep.Close();
				return false;
			}

			std::string name = (nameSize > 0 && pName[nameSize - 1] == '\0') ? std::string(pName) : std::string(pName, nameSize);
			if(name != vNewSounds[i].m_Name)
			{
				prep.Close();
				return false;
			}

			const void* pBuf = prep.GetData(pS->m_SoundData);
			const int bufSize = prep.GetDataSize(pS->m_SoundData);
			const auto& need = *vNewSounds[i].m_pData;
			if(bufSize != (int)need.size())
			{
				prep.Close();
				return false;
			}

			if(bufSize > 0 && mem_comp(pBuf, need.data(), bufSize) != 0)
			{
				prep.Close();
				return false;
			}

			if(pS->m_External != 0)
			{
				prep.Close();
				return false;
			}
		}

		// map sounds
		for(size_t j = 0; j < vOrigSnaps.size(); ++j)
		{
			const auto* pS = vPrepSounds[newCount + (int)j].second;

			const char* pName = (const char*)prep.GetData(pS->m_SoundName);
			const int nameSize = prep.GetDataSize(pS->m_SoundName);
			if(!pName)
			{
				prep.Close();
				return false;
			}

			std::string name = (nameSize > 0 && pName[nameSize - 1] == '\0') ? std::string(pName) : std::string(pName, nameSize);
			if(name != vOrigSnaps[j].m_Name)
			{
				prep.Close();
				return false;
			}

			if(pS->m_External != vOrigSnaps[j].m_External)
			{
				prep.Close();
				return false;
			}

			if(vOrigSnaps[j].m_External == 0)
			{
				const void* pBuf = prep.GetData(pS->m_SoundData);
				const int bufSize = prep.GetDataSize(pS->m_SoundData);
				const auto& need = vOrigSnaps[j].m_Data;
				if(bufSize != (int)need.size())
				{
					prep.Close();
					return false;
				}

				if(bufSize > 0 && mem_comp(pBuf, need.data(), bufSize) != 0)
				{
					prep.Close();
					return false;
				}
			}
		}

		prep.Close();
		return true;
	};

	if(PreparedIsUpToDate(preparedPath))
	{
		dbg_msg("tune_baker", "Prepared is up-to-date: '%s'", preparedPath.c_str());
		reader.Close();
		return preparedPath;
	}

	// index data for new sound
	const int oldNumData = reader.NumData();
	int nextDataIndex = oldNumData + ((hadSettingsOriginally || settingsCombined.empty()) ? 0 : 1);

	struct TNewIdx { int m_Name; int m_Data; };
	std::vector<TNewIdx> vNewIdx;
	vNewIdx.reserve(newCount);
	for(int i = 0; i < newCount; ++i)
	{
		vNewIdx.push_back({ nextDataIndex, nextDataIndex + 1 });
		nextDataIndex += 2;
	}

	// buffer names
	std::vector<std::vector<char>> vNewNameBuffers;
	vNewNameBuffers.reserve(newCount);
	for(const auto& ns : vNewSounds)
	{
		auto& buf = vNewNameBuffers.emplace_back(ns.m_Name.begin(), ns.m_Name.end());
		buf.push_back('\0');
	}

	// start writing prepared map
	CDataFileWriter writer;
	if(!writer.Open(pStorage, preparedPath.c_str(), IStorageEngine::TYPE_SAVE))
	{
		dbg_msg("tune_baker", "Failed to open prepared for write: '%s'", preparedPath.c_str());
		reader.Close();
		return std::nullopt;
	}

	// all items, edit info and layertype sounds
	for(int i = 0; i < reader.NumItems(); ++i)
	{
		int typeId, itemId;
		void* pItem = reader.GetItem(i, &typeId, &itemId);
		const int size = reader.GetItemSize(i);

		if(typeId == MAPITEMTYPE_SOUND)
			continue;

		if(typeId == MAPITEMTYPE_INFO && itemId == 0)
		{
			if(size >= (int)sizeof(CMapItemInfoSettings))
			{
				CMapItemInfoSettings info = *(CMapItemInfoSettings*)pItem;
				if(!settingsCombined.empty())
					info.m_Settings = settingsIndex;
				writer.AddItem(typeId, itemId, sizeof(info), &info);
			}
			else
			{
				CMapItemInfoSettings info;
				*(CMapItemInfo*)&info = *(CMapItemInfo*)pItem;
				info.m_Settings = settingsCombined.empty() ? -1 : settingsIndex;
				writer.AddItem(MAPITEMTYPE_INFO, 0, sizeof(info), &info);
			}
			continue;
		}

		if(typeId == MAPITEMTYPE_LAYER && size >= (int)sizeof(CMapItemLayer))
		{
			auto* pBase = (CMapItemLayer*)pItem;
			if(pBase->m_Type == LAYERTYPE_SOUNDS && size >= (int)sizeof(CMapItemLayerSounds))
			{
				CMapItemLayerSounds L = *(CMapItemLayerSounds*)pItem;
				if(L.m_Sound >= 0)
					L.m_Sound += newCount;
				writer.AddItem(typeId, itemId, sizeof(L), &L);
				continue;
			}
		}

		writer.AddItem(typeId, itemId, size, pItem);
	}

	// new sound items (by manifest order)
	for(int i = 0; i < newCount; ++i)
	{
		CMapItemSound snd;
		snd.m_Version = soundItemVersionForNew;
		snd.m_External = 0;
		snd.m_SoundName = vNewIdx[i].m_Name;
		snd.m_SoundData = vNewIdx[i].m_Data;
		writer.AddItem(MAPITEMTYPE_SOUND, i, sizeof(CMapItemSound), &snd);
	}

	// source sound with shift by id
	for(const auto& s : vOrigSnaps)
		writer.AddItem(MAPITEMTYPE_SOUND, s.m_OldId + newCount, s.m_ItemSize, (void*)s.m_pItem);

	// copy data from source, replacing/inserting settings
	for(int di = 0; di < reader.NumData(); ++di)
	{
		if(!settingsCombined.empty() && hadSettingsOriginally && di == settingsIndex)
		{
			writer.AddData((int)settingsCombined.size(), settingsCombined.data());
			continue;
		}
		int compressed = 0;
		const void* pRaw = reader.GetRawData(di, &compressed);
		if(pRaw)
		{
			const int uncompressed = reader.GetDataSize(di);
			writer.AddDataRaw(pRaw, compressed, uncompressed);
			free((void*)pRaw);
		}
	}
	if(!settingsCombined.empty() && !hadSettingsOriginally)
		writer.AddData((int)settingsCombined.size(), settingsCombined.data());

	// add data new sounds
	for(int i = 0; i < newCount; ++i)
	{
		const auto& nameBuf = vNewNameBuffers[i];
		writer.AddData((int)nameBuf.size(), nameBuf.data());

		const auto& buf = *vNewSounds[i].m_pData;
		writer.AddData((int)buf.size(), buf.empty() ? nullptr : buf.data());
	}

	writer.Finish();
	reader.Close();

	dbg_msg("tune_baker", "Prepared map written: '%s' (sounds: manifest first, settings appended)", preparedPath.c_str());
	return preparedPath;
}