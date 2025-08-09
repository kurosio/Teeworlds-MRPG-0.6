#include "tune_zone_manager.h"

#include <base/format.h>
#include <engine/shared/datafile.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

char* SerializeLines(const std::vector<std::string>& vLines, size_t& FinalSize)
{
	FinalSize = 0;
	if(vLines.empty())
	{
		return nullptr;
	}

	for(const auto& Line : vLines)
	{
		FinalSize += Line.length() + 1;
	}

	char* pBuffer = (char*)malloc(FinalSize);
	if(!pBuffer)
	{
		return nullptr;
	}

	size_t Offset = 0;
	for(const auto& Line : vLines)
	{
		const size_t Length = Line.length() + 1;
		mem_copy(pBuffer + Offset, Line.c_str(), Length);
		Offset += Length;
	}
	return pBuffer;
}

CTuneZoneManager& CTuneZoneManager::GetInstance()
{
	static CTuneZoneManager instance;
	return instance;
}

CTuneZoneManager::CTuneZoneManager()
{
	dbg_msg("TuneZoneManager", "Initializing Tune Zones...");

	CTuningParams Params;
	m_Zones[ETuneZone::DEFAULT] = Params;

	Params = CTuningParams();
	Params.m_Gravity = 0.25f;
	Params.m_GroundJumpImpulse = 8.0f;
	Params.m_AirFriction = 0.75f;
	Params.m_AirControlAccel = 1.0f;
	Params.m_AirControlSpeed = 3.75f;
	Params.m_AirJumpImpulse = 8.0f;
	Params.m_HookFireSpeed = 30.0f;
	Params.m_HookDragAccel = 1.5f;
	Params.m_HookDragSpeed = 8.0f;
	Params.m_PlayerHooking = 0;
	m_Zones[ETuneZone::SLOW] = Params;

	Params = CTuningParams();
	Params.m_GroundControlSpeed = 5.0f;
	Params.m_GroundControlAccel = 1.0f;
	m_Zones[ETuneZone::WALKING] = Params;

	if(m_Zones.size() > NUM_TUNEZONES)
	{
		dbg_msg("TuneZoneManager", "Too much tune zones defined");
	}
}

int CTuneZoneManager::GetZoneID(ETuneZone Zone) const
{
	return static_cast<int>(Zone);
}

const CTuningParams* CTuneZoneManager::GetParams(const ETuneZone Zone) const
{
	auto it = m_Zones.find(Zone);
	if(it != m_Zones.end())
	{
		return &it->second;
	}
	return nullptr;
}

const CTuningParams* CTuneZoneManager::GetParams(int ZoneID) const
{
	if (ZoneID < 0 || ZoneID >= static_cast<int>(ETuneZone::NUM_TUNE_ZONES))
	{
		return nullptr;
	}
	return GetParams(static_cast<ETuneZone>(ZoneID));
}

std::optional<std::string> CTuneZoneManager::BakeZonesIntoMap(const char* pMapName, IStorageEngine* pStorage)
{
	CDataFileReader Reader;
	if(!Reader.Open(pStorage, pMapName, IStorageEngine::TYPE_ALL))
	{
		dbg_msg("tune_baker", "Failed to bake tune zones: failed to open map '%s' for reading", pMapName);
		return std::nullopt;
	}

	// Cache whole file at once instead of lazy-loading it
	for(int i = 0; i < Reader.NumData(); ++i)
		Reader.GetData(i);

	std::vector<std::string> vNewCommands;
	for(auto const& [ZoneType, Zone] : m_Zones)
	{
		for(auto const& [TuneIndex, TuneValue] : Zone.GetDiff())
			vNewCommands.emplace_back(fmt_default("tune_zone {} {} {}", GetZoneID(ZoneType), CTuningParams::Name(TuneIndex), TuneValue));
	}

	if(vNewCommands.empty())
		return std::nullopt;

	std::vector<std::string> vLinesToWrite;
	vLinesToWrite.emplace_back("");
	vLinesToWrite.emplace_back("# Tune zones generated");
	vLinesToWrite.insert(vLinesToWrite.end(), vNewCommands.begin(), vNewCommands.end());

	size_t TotalLength = 0;
	for(const auto& Line : vLinesToWrite)
		TotalLength += Line.length() + 1;

	char* pSettings = (char*)malloc(TotalLength);
	if(!pSettings)
	{
		dbg_msg("tune_baker", "Failed to allocate memory for settings");
		return std::nullopt;
	}

	int Offset = 0;
	for(const auto& Line : vLinesToWrite)
	{
		int Length = Line.length() + 1;
		mem_copy(pSettings + Offset, Line.c_str(), Length);
		Offset += Length;
	}

	CDataFileWriter Writer;

	int SettingsIndex = Reader.NumData();
	bool FoundInfo = false;
	for(int i = 0; i < Reader.NumItems(); i++)
	{
		int TypeId;
		int ItemId;
		void* pData = Reader.GetItem(i, &TypeId, &ItemId);
		int Size = Reader.GetItemSize(i);
		CMapItemInfoSettings MapInfo;
		if(TypeId == MAPITEMTYPE_INFO && ItemId == 0)
		{
			FoundInfo = true;
			if(Size >= (int)sizeof(CMapItemInfoSettings))
			{
				CMapItemInfoSettings* pInfo = (CMapItemInfoSettings*)pData;
				if(pInfo->m_Settings > -1)
				{
					SettingsIndex = pInfo->m_Settings;
					const char* pMapSettings = (const char*)Reader.GetData(SettingsIndex);
					int OldDataSize = Reader.GetDataSize(SettingsIndex);
					if((size_t)OldDataSize >= TotalLength && mem_comp(pSettings, pMapSettings + (OldDataSize - TotalLength), TotalLength) == 0)
					{
						// Configs coincide, no need to update map.
						free(pSettings);
						return std::nullopt;
					}

					size_t NewTotalLength = OldDataSize + TotalLength;
					char* pCombinedSettings = (char*)malloc(NewTotalLength);
					if(!pCombinedSettings)
					{
						dbg_msg("tune_baker", "Failed to allocate memory for combined settings");
						free(pSettings);
						return std::nullopt;
					}
					mem_copy(pCombinedSettings, pMapSettings, OldDataSize);
					mem_copy(pCombinedSettings + OldDataSize, pSettings, TotalLength);
					free(pSettings);

					pSettings = pCombinedSettings;
					TotalLength = NewTotalLength;
					Reader.UnloadData(pInfo->m_Settings);
				}
				else
				{
					MapInfo = *pInfo;
					MapInfo.m_Settings = SettingsIndex;
					pData = &MapInfo;
					Size = sizeof(MapInfo);
				}
			}
			else
			{
				*(CMapItemInfo*)&MapInfo = *(CMapItemInfo*)pData;
				MapInfo.m_Settings = SettingsIndex;
				pData = &MapInfo;
				Size = sizeof(MapInfo);
			}
		}
		Writer.AddItem(TypeId, ItemId, Size, pData);
	}

	if(!FoundInfo)
	{
		CMapItemInfoSettings Info;
		Info.m_Version = 1;
		Info.m_Author = -1;
		Info.m_MapVersion = -1;
		Info.m_Credits = -1;
		Info.m_License = -1;
		Info.m_Settings = SettingsIndex;
		Writer.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Info), &Info);
	}

	for(int i = 0; i < Reader.NumData() || i == SettingsIndex; i++)
	{
		if(i == SettingsIndex)
		{
			Writer.AddData(TotalLength, pSettings);
			continue;
		}

		int CompressedSize;
		const void* pRawData = Reader.GetRawData(i, &CompressedSize);
		if(pRawData)
		{
			int UncompressedSize = Reader.GetDataSize(i);
			Writer.AddDataRaw(pRawData, CompressedSize, UncompressedSize);
			free((void*)pRawData);
		}
	}

	free(pSettings);
	Reader.Close();
	char aTemp[IO_MAX_PATH_LENGTH];
	if(!Writer.Open(pStorage, IStorageEngine::FormatTmpPath(aTemp, sizeof(aTemp), pMapName), IStorageEngine::TYPE_ABSOLUTE))
	{
		dbg_msg("tune_baker", "Failed to bake tune zones: failed to open map '%s' for writing", aTemp);
		return std::nullopt;
	}

	Writer.Finish();
	dbg_msg("tune_baker", "Baked tune zones into '%s'", aTemp);

	str_copy(m_aDeleteTempfile, aTemp);
	return aTemp;
}

void CTuneZoneManager::DeleteTempfile(IStorageEngine *pStorage)
{
	if(m_aDeleteTempfile[0] != 0)
	{
		dbg_msg("tune_baker", "Removing temp file: %s", m_aDeleteTempfile);
		pStorage->RemoveFile(m_aDeleteTempfile, IStorageEngine::TYPE_ABSOLUTE);
		m_aDeleteTempfile[0] = 0;
	}
}

void CTuneZoneManager::LoadSoundsFromDirectory(const char* pDirectory, IStorageEngine* pStorage)
{
	m_Sounds.clear();
	dbg_msg("sound_loader", "Loading sounds from directory '%s'...", pDirectory);

	std::vector<std::string> vSoundFiles;
	auto ListCallback = [](const char* pName, int IsDir, int StorageType, void* pUser) -> int
	{
		if(IsDir || pName[0] == '.')
			return 0;

		if(str_endswith(pName, ".opus"))
		{
			auto* pVec = static_cast<std::vector<std::string>*>(pUser);
			pVec->emplace_back(pName);
		}
		return 0;
	};

	pStorage->ListDirectory(IStorageEngine::TYPE_ALL, pDirectory, ListCallback, &vSoundFiles);
	if(vSoundFiles.empty())
	{
		dbg_msg("sound_loader", "No sounds found in '%s'.", pDirectory);
		return;
	}

	std::sort(vSoundFiles.begin(), vSoundFiles.end());
	for(const auto& FileName : vSoundFiles)
	{
		char aFullPath[IO_MAX_PATH_LENGTH];
		str_format(aFullPath, sizeof(aFullPath), "%s/%s", pDirectory, FileName.c_str());

		IOHANDLE File = pStorage->OpenFile(aFullPath, IOFLAG_READ, IStorageEngine::TYPE_ALL);
		if(!File)
		{
			dbg_msg("sound_loader", "Failed to open sound file '%s'", aFullPath);
			continue;
		}

		const long long FileSize = io_length(File);
		std::vector<char> vBuffer(FileSize);
		io_read(File, vBuffer.data(), FileSize);
		io_close(File);

		RegisterSound(FileName, vBuffer.data(), vBuffer.size());
	}

	dbg_msg("sound_loader", "Successfully loaded and registered %d sounds.", (int)vSoundFiles.size());
}

void CTuneZoneManager::RegisterSound(const std::string& Name, const void* pData, size_t DataSize)
{
	if(Name.empty() || !pData || DataSize == 0)
		return;

	const char* pCharData = static_cast<const char*>(pData);
	m_Sounds[Name].m_vData.assign(pCharData, pCharData + DataSize);
	dbg_msg("sound_baker", "Registered sound '%s' with size %zu", Name.c_str(), DataSize);
}
