#include "warehouse_data.h"

#include <game/server/gamecontext.h>
#include <game/server/entity_manager.h>
#include <game/server/core/components/inventory/inventory_manager.h>
#include <sstream>

const std::string CWarehouse::s_DefaultSubgroupKey = "Uncategorized";

void CWarehouse::Init(const std::string& Name, const DBSet& Type, const std::string& TradesStr,
    const nlohmann::json& StorageData, vec2 Pos, int Currency, int WorldID)
{
    m_Flags = WF_NONE;
    m_Name = Name;
    m_Pos = Pos;
    m_Currency = Currency;
    m_WorldID = WorldID;
    m_Storage.Init(this);
    InitData(Type, TradesStr, StorageData);
}


void CWarehouse::InitializeFlags(const DBSet& Type)
{
    m_Flags = WF_NONE;
    if(Type.hasSet("buying"))
        m_Flags |= WF_BUY;
    if(Type.hasSet("selling"))
        m_Flags |= WF_SELL;
    if(Type.hasSet("storage"))
        m_Flags |= WF_STORAGE;
}


void CWarehouse::ParseCollectionBlock(const std::string& block_content, const std::string& currentGroup, const std::string& currentSubgroup)
{
    // check parent
    const auto openParen = block_content.find('(');
    const auto closeParen = block_content.rfind(')');
    if(openParen == std::string::npos || closeParen == std::string::npos || closeParen <= openParen)
    {
        dbg_msg("warehouse_parser", "Warehouse(%d): Malformed COLLECTION block: missing '()' or invalid format. Content: '%s'", m_ID, block_content.c_str());
        return;
    }

    // sub params
    const auto paramsStr = mystd::string::trim(block_content.substr(openParen + 1, closeParen - openParen - 1));
    const auto colonPos = paramsStr.find(':');
    if(colonPos == std::string::npos)
    {
        dbg_msg("warehouse_parser", "Warehouse(%d): Malformed COLLECTION parameters: missing ':'. Expected GroupName:TypeName. Content: '%s'", m_ID, paramsStr.c_str());
        return;
    }

    // initialize by group type collection
    const auto groupNameStr = mystd::string::trim(paramsStr.substr(0, colonPos));
    const auto groupPrep = GetItemGroupFromDBSet(DBSet(groupNameStr));
    const auto groupOpt = (groupPrep != ItemGroup::Unknown ? std::make_optional<ItemGroup>(groupPrep) : std::nullopt);
    const auto typeNameStr = mystd::string::trim(paramsStr.substr(colonPos + 1));
    const auto typePrep = GetItemTypeFromDBSet(DBSet(typeNameStr));
    const auto typeOpt = (typePrep != ItemType::Unknown ? std::make_optional<ItemType>(typePrep) : std::nullopt);
    auto vItems = CInventoryManager::GetItemsCollection(groupOpt, typeOpt);
    for(const int ItemID : vItems)
    {
        CItem Item(ItemID, 1, 0);
        if(Item.IsValid() && Item.Info() && Item.Info()->GetInitialPrice() > 0)
        {
            CTrade& newTrade = m_vTradingList.emplace_back(m_vTradingList.size(), std::move(Item), Item.Info()->GetInitialPrice(), -1);
            m_mGroupedTradeIDs[currentGroup][currentSubgroup].push_back(&newTrade);
        }
    }
}


void CWarehouse::ParseItemBlock(const std::string& ItemEntryString, const std::string& currentGroup, const std::string& currentSubgroup)
{
    const auto slashPos = ItemEntryString.find('/');
    const auto openParenPos = ItemEntryString.find('(');
    const auto closeParenPos = ItemEntryString.rfind(')');

    if(slashPos == std::string::npos || openParenPos == std::string::npos || closeParenPos == std::string::npos ||
        openParenPos <= slashPos || closeParenPos <= openParenPos)
    {
        dbg_msg("warehouse_parser", "Warehouse(%d): Malformed ITEM block. Content: '%s'. Expected ItemID/Value(Price[/Products])", m_ID, ItemEntryString.c_str());
        return;
    }

    try
    {
        const int itemId = std::stoi(mystd::string::trim(ItemEntryString.substr(0, slashPos)));
        const int itemValue = std::stoi(mystd::string::trim(ItemEntryString.substr(slashPos + 1, openParenPos - (slashPos + 1))));
        const auto priceProdStr = mystd::string::trim(ItemEntryString.substr(openParenPos + 1, closeParenPos - (openParenPos + 1)));
        const auto priceSlashPos = priceProdStr.find('/');

        int priceVal;
        int productsVal = -1;
        if(priceSlashPos != std::string::npos)
        {
            priceVal = std::stoi(mystd::string::trim(priceProdStr.substr(0, priceSlashPos)));
            productsVal = std::stoi(mystd::string::trim(priceProdStr.substr(priceSlashPos + 1)));
        }
        else
        {
            priceVal = std::stoi(priceProdStr);
        }

        CItem Item(itemId, itemValue, 0);
        if(Item.IsValid() && priceVal > 0)
        {
            CTrade& newTrade = m_vTradingList.emplace_back(m_vTradingList.size(), std::move(Item), priceVal, productsVal);
            m_mGroupedTradeIDs[currentGroup][currentSubgroup].push_back(&newTrade);
        }
    }
    catch(const std::invalid_argument& ia)
    {
        dbg_msg("warehouse_parser", "Warehouse(%d): Invalid number in ITEM parameters: %s. Content: '%s'", m_ID, ia.what(), ItemEntryString.c_str());
    }
    catch(const std::out_of_range& oor)
    {
        dbg_msg("warehouse_parser", "Warehouse(%d): Number out of range in ITEM parameters: %s. Content: '%s'", m_ID, oor.what(), ItemEntryString.c_str());
    }
}


void CWarehouse::InitData(const DBSet& Type, const std::string& ItemsString, const nlohmann::json& StorageData)
{
    InitializeFlags(Type);
    m_vTradingList.clear();
    m_mGroupedTradeIDs.clear();

    if(!(m_Flags & (WF_BUY | WF_SELL)) || ItemsString.empty())
    {
        InitializeStorage(StorageData);
        dbg_msg("warehouse", "'%s' (ID: %d) initialized. Flags: %lld. No trade items defined or flags not set for trading.",
            m_Name.c_str(), m_ID, m_Flags);
        return;
    }

    // initialize variables
    std::string currentParsingGroup = "Uncategorized";
    std::string currentParsingSubgroup = s_DefaultSubgroupKey;
    int lineNumber = 0;
    std::string line;

    // parsing string lines
    std::istringstream iss(ItemsString);
    while(std::getline(iss, line))
    {
        lineNumber++;
       const auto trimmedLine = mystd::string::trim(line);

       // skip empty or comment line
        if(trimmedLine.empty() || trimmedLine[0] == '#')
            continue;

        // group setting line
        if(trimmedLine[0] == '*')
        {
            const auto groupLineContent = mystd::string::trim(trimmedLine.substr(1));
            const auto colonPos = groupLineContent.find(':');
            if(colonPos != std::string::npos)
            {
                currentParsingGroup = mystd::string::trim(groupLineContent.substr(0, colonPos));
                currentParsingSubgroup = mystd::string::trim(groupLineContent.substr(colonPos + 1));
                if(currentParsingSubgroup.empty())
                    currentParsingSubgroup = s_DefaultSubgroupKey;
            }
            else
            {
                currentParsingGroup = mystd::string::trim(groupLineContent);
                currentParsingSubgroup = s_DefaultSubgroupKey;
            }
        }
        // collect rule
        else if(trimmedLine[0] == '[' && trimmedLine.back() == ']')
        {
            const auto blockContent = mystd::string::trim(trimmedLine.substr(1, trimmedLine.length() - 2));
            if(blockContent.empty())
            {
                dbg_msg("warehouse_parser", "L%d: Warehouse(%d): Empty block '[]' in context group '%s':'%s'. Skipping.", lineNumber, m_ID, currentParsingGroup.c_str(), currentParsingSubgroup.c_str());
                continue;
            }

            if(blockContent.rfind("COLLECTION(", 0) == 0)
                ParseCollectionBlock(blockContent, currentParsingGroup, currentParsingSubgroup);
            else
                ParseItemBlock(blockContent, currentParsingGroup, currentParsingSubgroup);
        }
        else
        {
            dbg_msg("warehouse_parser", "L%d: Warehouse(%d): Unrecognized line format in context group '%s':'%s': %s", lineNumber, m_ID, currentParsingGroup.c_str(), currentParsingSubgroup.c_str(), trimmedLine.c_str());
        }
    }

    InitializeStorage(StorageData);

    // sorting items
    for(auto& [groupName, vSubgroupedTrades] : m_mGroupedTradeIDs)
    {
        for(auto& [subgroupname, vTrades] : vSubgroupedTrades)
        {
            std::ranges::sort(vTrades, [](const CTrade* a, const CTrade* b)
            {
                const auto aPrice = a->GetItem()->Info()->GetInitialPrice();
                const auto bPrice = b->GetItem()->Info()->GetInitialPrice();

                if(aPrice != bPrice)
                {
                    return aPrice < bPrice;
                }

                const auto aGroup = a->GetItem()->Info()->GetGroup();
                const auto bGroup = b->GetItem()->Info()->GetGroup();
                if(aGroup != bGroup)
                {
                    return aGroup < bGroup;
                }

                const auto aType = a->GetItem()->Info()->GetType();
                const auto bType = b->GetItem()->Info()->GetType();
                return aType < bType;
            });
        }
    }

    dbg_msg("warehouse", "'%s' (ID: %d) initialized. Flags: %lld, Total Items: %lu, Groups: %lu, Currency: %d, WorldID: %d",
        m_Name.c_str(), m_ID, m_Flags,
        m_vTradingList.size(), m_mGroupedTradeIDs.size(), m_Currency, m_WorldID);
}


void CWarehouse::InitializeStorage(const nlohmann::json& StorageData)
{
    if(!(m_Flags & WF_STORAGE) || StorageData.is_null())
        return;

    m_Storage.m_Value = StorageData.value("value", BigInt(0));
    m_Storage.m_TextPos = StorageData.value("position", vec2());
}


void CWarehouse::SaveData()
{
    if(IsHasFlag(WF_STORAGE))
    {
        nlohmann::json storageJson;
        storageJson["value"] = m_Storage.GetValue().to_string();
        storageJson["position"] = m_Storage.GetTextPos();
        Database->Execute<DB::UPDATE>(TW_WAREHOUSE_TABLE, "StorageData = '{}' WHERE ID = {}", storageJson.dump(), m_ID);
    }
}


CTrade* CWarehouse::GetTrade(int TradeID)
{
    const auto iter = std::ranges::find_if(m_vTradingList, [TradeID](const CTrade& Trade)
    {
        return Trade.GetID() == TradeID;
    });
    return iter != m_vTradingList.end() ? &(*iter) : nullptr;
}


void CWarehouse::CStorage::UpdateText(int LifeTime) const
{
    const auto* pGS = (CGS*)Instance::GameServer(m_pWarehouse->GetWorldID());
    if(!pGS) return;

    if(m_TextPos.x != 0 || m_TextPos.y != 0)
        pGS->EntityManager()->Text(m_TextPos, LifeTime, m_Value.to_string().c_str());
}