/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SHOP_DATA_H
#define GAME_SERVER_COMPONENT_SHOP_DATA_H

struct CAuctionItem
{
	int m_ItemID;
	int m_Value;
	int m_Price;
	int m_Enchant;
};

struct CShop
{
	int m_StorageID;

	static std::map< int, CShop > ms_aShopList;
};

#endif

