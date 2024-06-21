/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H
#define GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H

// forward declarations
class CGS;
class CPlayer;
class CVoteGroupHidden;
typedef void (*VoteOptionCallbackImpl)(CPlayer*, int, std::string, void*);
typedef struct { VoteOptionCallbackImpl m_Impl; void* m_pData; } VoteOptionCallback;

enum VoteDepthListStyles
{
	DEPTH_LIST_STYLE_DEFAULT = 0,
	DEPTH_LIST_STYLE_ROMAN,
	DEPTH_LIST_STYLE_BOLD,
	DEPTH_LIST_STYLE_CYRCLE,
	NUM_DEPTH_LIST_STYLES,
};

enum VoteDepthListStylesLevels
{
	DEPTH_LVL1 = 0,
	DEPTH_LVL2,
	DEPTH_LVL3,
	DEPTH_LVL4,
	DEPTH_LVL5,
};

enum VoteWrapperFlags
{
	VWF_DISABLED = 0, // regular title group
	VWF_SEPARATE = 1 << 1, // ends the group with a line
	VWF_ALIGN_TITLE = 1 << 2, // example: ---  title  ---
	VWF_STYLE_SIMPLE = 1 << 3, // example: ╭ │ ╰
	VWF_STYLE_DOUBLE = 1 << 4, // example: ╔ ═ ╚
	VWF_STYLE_STRICT = 1 << 5, // example: ┌ │ └
	VWF_STYLE_STRICT_BOLD = 1 << 6, // example: ┏ ┃ ┗
	VWF_OPEN = 1 << 7, // default open group
	VWF_CLOSED = 1 << 8, // default close group
	VWF_UNIQUE = 1 << 9, // default close group toggle unique groups
	VWF_SEPARATE_OPEN = VWF_OPEN | VWF_SEPARATE, // default open group with end line
	VWF_SEPARATE_CLOSED = VWF_CLOSED | VWF_SEPARATE, // default close group with end line
	VWF_SEPARATE_UNIQUE = VWF_UNIQUE | VWF_SEPARATE, // default close group with end line
};

class CVoteOption
{
public:
	char m_aDescription[VOTE_DESC_LENGTH] {};
	char m_aCommand[VOTE_CMD_LENGTH] {};
	int m_Depth {};
	int m_Extra1 { -1 };
	int m_Extra2 { -1 };
	bool m_Line { false };
	bool m_Title { false };
	VoteOptionCallback m_Callback {};
};

class CVoteGroup
{
	friend class VoteWrapper;

	struct NumeralDepth
	{
		int m_Value {};
		int m_Style {};
	};
	std::map<int, NumeralDepth> m_vDepthNumeral {};
	std::deque<CVoteOption> m_vpVotelist {};
	bool m_NextMarkedListItem {};
	int m_CurrentDepth {};

	CGS* m_pGS {};
	CPlayer* m_pPlayer {};
	CGS* GS() const { return m_pGS; }

	bool m_HasTitle {};
	int m_GroupSize {};
	int m_HiddenID {};
	int m_Flags {};
	int m_ClientID {};

	CVoteGroup(int ClientID, int Flags);

	void SetNumeralDepthStyles(std::initializer_list<std::pair<int, int>> vNumeralFlags);

	int NextPos() const { return m_GroupSize + 1; }
	bool IsEmpty() const { return m_GroupSize <= 0; }
	bool HasTitle() const { return m_HasTitle; }
	bool IsHidden() const;

	void SetVoteTitleImpl(const char* pCmd, int Extra1, int Extra2, const char* pText);
	void AddVoteImpl(const char* pCmd, int Extra1, int Extra2, const char* pText);
	void SetLastVoteCallback(const VoteOptionCallbackImpl& CallbackImpl, void* pUser) { m_vpVotelist.back().m_Callback = { CallbackImpl, pUser }; }

	void Reformatting(std::string& Buffer);

	void AddLineImpl();
	void AddEmptylineImpl();
	void AddBackpageImpl();
	void AddItemValueImpl(int ItemID);

};

#define FMT_LOCALIZE_STR(clientid, text, args) fmt_localize(clientid, text, args).c_str()

/**
	 * @brief The VoteWrapper class provides a convenient way to create and manage voting options and groups.
	 *
	 * The VoteWrapper class allows you to easily create and manage voting options and groups in your game server.
	 * It provides a fluent interface for adding and configuring voting options, setting titles, and controlling the structure of the voting menu.
	 *
	 * Usage example:
	 * @code{.cpp}
	 * VoteWrapper vote(0); // Create a VoteWrapper instance for client ID 0
	 * vote.SetTitle("Main Menu") // Set the title of the voting menu
	 *     .AddOption("kick", "Kick a player") // Add a voting option to kick a player
	 *     .AddOption("ban", "Ban a player") // Add a voting option to ban a player
	 *     .AddLine() // Add a line separator
	 *     .AddOption("nextmap", "Change the map") // Add a voting option to change the map
	 *     .AddOption("restart", "Restart the game") // Add a voting option to restart the game
	 *     .Add("Nickname: {}", Server()->ClientName(0)) // Add a voting message uses format
	 *     .AddMenu(MENU_INFO, "Info"); // Add menu list
	 * @endcode
*/
class VoteWrapper : public MultiworldIdentifiableData<std::map<int, std::deque<CVoteGroup*>>>
{
	CVoteGroup* m_pGroup {};

public:
	/**
	 * @brief Constructs a new VoteWrapper instance for the specified client ID.
	 *
	 * This constructor creates a new VoteWrapper instance for the specified client ID.
	 * It initializes the VoteWrapper with a disabled vote group for the client.
	 *
	 * @param ClientID The ID of the client for which the VoteWrapper is created.
	 */
	VoteWrapper(int ClientID)
	{
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid ClientID");
		m_pGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		m_pData[ClientID].push_back(m_pGroup);
	}

	/**
	 * @brief Constructs a new VoteWrapper instance for the specified client ID.
	 *
	 * This constructor creates a new VoteWrapper instance for the specified client ID.
	 * It initializes the VoteWrapper with the given flags for the client.
	 *
	 * @tparam T The type of the flags.
	 * @param ClientID The ID of the client for which the VoteWrapper is created.
	 * @param Flags The flags to initialize the VoteWrapper with.
	 */
	template <typename T = int>
	VoteWrapper(int ClientID, T Flags)
	{
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid ClientID");
		m_pGroup = new CVoteGroup(ClientID, Flags);
		m_pData[ClientID].push_back(m_pGroup);
	}

	/**
	 * @brief Constructs a new VoteWrapper instance for the specified client ID.
	 *
	 * This constructor creates a new VoteWrapper instance for the specified client ID.
	 * It initializes the VoteWrapper with the given title and optional arguments for localization.
	 *
	 * @tparam Ts The types of the optional arguments.
	 * @param ClientID The ID of the client for which the VoteWrapper is created.
	 * @param pTitle The title of the voting menu.
	 * @param args The optional arguments for localization.
	 */
	template<typename ... Ts>
	VoteWrapper(int ClientID, const char* pTitle, const Ts&... args)
	{
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid ClientID");
		m_pGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		m_pGroup->SetVoteTitleImpl("null", NOPE, NOPE, FMT_LOCALIZE_STR(ClientID, pTitle, args...));
		m_pData[ClientID].push_back(m_pGroup);
	}

	/**
	 * @brief Constructs a new VoteWrapper instance for the specified client ID.
	 *
	 * This constructor creates a new VoteWrapper instance for the specified client ID.
	 * It initializes the VoteWrapper with the given title and optional arguments for localization.
	 *
	 * @tparam Ts The types of the optional arguments.
	 * @param ClientID The ID of the client for which the VoteWrapper is created.
	 * @param Flags The flags to initialize the VoteWrapper with.
	 * @param pTitle The title of the voting menu.
	 * @param args The optional arguments for localization.
	 */
	template<typename ... Ts>
	VoteWrapper(int ClientID, int Flags, const char* pTitle, const Ts&... args)
	{
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid ClientID");
		m_pGroup = new CVoteGroup(ClientID, Flags);
		m_pGroup->SetVoteTitleImpl("null", NOPE, NOPE, FMT_LOCALIZE_STR(ClientID, pTitle, args...));
		m_pData[ClientID].push_back(m_pGroup);
	}

	/**
	 * @brief Gets the next position in the voting menu.
	 *
	 * This method returns the next position in the voting menu.
	 *
	 * @return The next position in the voting menu.
	 */
	int NextPos() const { return m_pGroup->NextPos(); }

	/**
	 * @brief Checks if the voting menu is empty.
	 *
	 * This method checks if the voting menu is empty.
	 *
	 * @return True if the voting menu is empty, false otherwise.
	 */
	bool IsEmpty() const { return m_pGroup->IsEmpty(); }

	/**
	 * @brief Checks if the voting menu title is set.
	 *
	 * This method checks if the voting menu title is set.
	 *
	 * @return True if the voting menu title is set, false otherwise.
	 */
	bool IsTittleSet() const { return m_pGroup->HasTitle(); }

	/**
	 * @brief Sets the title of the voting menu.
	 *
	 * This method sets the title of the voting menu. The title can be localized using optional arguments.
	 *
	 * @tparam Ts The types of the optional arguments.
	 * @param pTitle The title of the voting menu.
	 * @param args The optional arguments for localization.
	 * @return A reference to the VoteWrapper instance.
	 */
	template<typename ... Ts>
	VoteWrapper& SetTitle(const char* pTitle, const Ts&... args) noexcept
	{
		m_pGroup->SetVoteTitleImpl("null", NOPE, NOPE, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pTitle, args...));
		return *this;
	}

	/**
	 * @brief Sets the title of the voting menu with the specified flags.
	 *
	 * This method sets the title of the voting menu with the specified flags. The title can be localized using optional arguments.
	 *
	 * @tparam Ts The types of the optional arguments.
	 * @param Flags The flags to initialize the VoteWrapper with.
	 * @param pTitle The title of the voting menu.
	 * @param args The optional arguments for localization.
	 * @return A reference to the VoteWrapper instance.
	 */
	template<typename ... Ts>
	VoteWrapper& SetTitle(int Flags, const char* pTitle, const Ts&... args) noexcept
	{
		m_pGroup->m_Flags = Flags;
		m_pGroup->SetVoteTitleImpl("null", NOPE, NOPE, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pTitle, args...));
		return *this;
	}

	/**
	 * @brief Reinitializes the numeral depth styles for the voting menu.
	 *
	 * This method reinitializes the numeral depth styles for the voting menu.
	 * It takes an initializer list of pairs, where each pair consists of a depth level and a style flag.
	 * The depth level determines the indentation level of the voting options, and the style flag determines the visual style of the indentation.
	 *
	 * Example usage:
	 * @code{.cpp}
	 * vote.ReinitNumeralDepthStyles({{DEPTH_LVL1, VWF_STYLE_SIMPLE}, {DEPTH_LVL2, VWF_STYLE_DOUBLE}});
	 * @endcode
	 *
	 * @param vNumeralFlags The initializer list of pairs containing the depth level and style flag.
	 */
	void ReinitNumeralDepthStyles(std::initializer_list<std::pair<int, int>> vNumeralFlags) const
	{
		dbg_assert(m_pGroup != nullptr, "For initialize depth, first needed to initialize vote wrapper");
		m_pGroup->SetNumeralDepthStyles(vNumeralFlags);
	}

	/**
	 * @brief Marks the next voting option as a list item.
	 *
	 * This method marks the next voting option as a list item.
	 * List items are displayed with a bullet point or other visual indicator to indicate that they are part of a list.
	 *
	 * Example usage:
	 * @code{.cpp}
	 * VoteWrapper vote(0);
	 * vote.MarkList().Add("Numeral list")
	 *     .AddOption("value1", "Value 1")
	 *     .AddOption("value2", "Value 2");
	 * @endcode
	 *
	 * @return A reference to the VoteWrapper instance.
	 */
	VoteWrapper& MarkList() noexcept
	{
		m_pGroup->m_NextMarkedListItem = true;
		return *this;
	}

	/**
	 * @brief Begins a new depth level for the voting menu.
	 *
	 * This method begins a new depth level for the voting menu.
	 * It increases the current depth level by one.
	 *
	 * @return A reference to the VoteWrapper instance.
	 */
	VoteWrapper& BeginDepth() noexcept
	{
		m_pGroup->m_CurrentDepth++;
		return *this;
	}

	/**
	 * @brief Ends the current depth level for the voting menu.
	 *
	 * This method ends the current depth level for the voting menu.
	 * It decreases the current depth level by one.
	 *
	 * @return A reference to the VoteWrapper instance.
	 */
	VoteWrapper& EndDepth() noexcept
	{
		m_pGroup->m_CurrentDepth--;
		return *this;
	}

	/**
	 * @brief Adds a line separator to the voting menu.
	 *
	 * This method adds a line separator to the voting menu.
	 * The line separator visually separates different sections or options in the menu.
	 *
	 * @return A reference to the VoteWrapper instance.
	 */
	VoteWrapper& AddLine() noexcept
	{
		m_pGroup->AddLineImpl();
		return *this;
	}

	/**
	 * @brief Adds an empty line to the voting menu.
	 *
	 * This method adds an empty line to the voting menu.
	 * The empty line visually separates different sections or options in the menu.
	 *
	 * @return A reference to the VoteWrapper instance.
	 */
	VoteWrapper& AddEmptyline() noexcept
	{
		m_pGroup->AddEmptylineImpl();
		return *this;
	}

	/**
	 * @brief Adds an item value to the voting menu.
	 *
	 * This method adds an item value to the voting menu. The item value is displayed as a voting option in the menu.
	 *
	 * @param ItemID The ID of the item value to add.
	 * @return A reference to the VoteWrapper instance.
	 */
	VoteWrapper& AddItemValue(int ItemID) noexcept
	{
		m_pGroup->AddItemValueImpl(ItemID);
		return *this;
	}

	/**
	 * @brief Adds a voting option to the voting menu.
	 *
	 * This method adds a voting option to the voting menu with the specified text and optional arguments for localization.
	 * The voting option is displayed as a selectable option in the menu.
	 *
	 * @tparam Ts The types of the optional arguments.
	 * @param pText The text of the voting option.
	 * @param args The optional arguments for localization.
	 * @return A reference to the VoteWrapper instance.
	 *
	 * @code{.cpp}
	 * VoteWrapper vote(0);
	 * vote.Add("Option 1")
	 *     .Add("Option 2")
	 *     .Add("Option 3");
	 * @endcode
	 */
	template<typename ... Ts>
	VoteWrapper& Add(const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("null", NOPE, NOPE, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	/**
	 * @brief Adds a menu option to the voting menu.
	 *
	 * This method adds a menu option to the voting menu with the specified menu ID and text.
	 * The menu ID is used to identify the selected menu option when handling the vote.
	 * The text parameter supports formatting using additional arguments.
	 *
	 * @tparam Ts The types of the additional arguments for text formatting.
	 * @param MenuID The ID of the menu option.
	 * @param pText The text of the menu option.
	 * @param args The additional arguments for text formatting.
	 * @return A reference to the VoteWrapper instance.
	 *
	 * @code{.cpp}
	 * VoteWrapper vote(0);
	 * vote.AddMenu(MENU_LIST_1, "List 1");
	 * vote.AddMenu(MENU_LIST_2, "List 2");
	 * @endcode
	 */
	template<typename ... Ts>
	VoteWrapper& AddMenu(int MenuID, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("MENU", MenuID, NOPE, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	/**
	 * @brief Adds a menu option to the voting menu with a specific group ID.
	 *
	 * This method adds a menu option to the voting menu with the specified menu ID, group ID, and text.
	 * The menu ID is used to identify the selected menu option when handling the vote.
	 * The group ID is used to group related menu options together.
	 * The text parameter supports formatting using additional arguments.
	 *
	 * @tparam Ts The types of the additional arguments for text formatting.
	 * @param MenuID The ID of the menu option.
	 * @param GroupID The ID of the group to which the menu option belongs.
	 * @param pText The text of the menu option.
	 * @param args The additional arguments for text formatting.
	 * @return A reference to the VoteWrapper instance.
	 *
	 * @code{.cpp}
	 * VoteWrapper vote(0);
	 * vote.AddMenu(MENU_LIST_1, 0, "List 1");
	 * vote.AddMenu(MENU_LIST_2, 0, "List 2");
	 * vote.AddMenu(MENU_LIST_3, 1, "List 3");
	 * @endcode
	 */
	template<typename ... Ts>
	VoteWrapper& AddMenu(int MenuID, int GroupID, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("MENU", MenuID, GroupID, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	/**
	* @brief Adds a voting option to the current vote group.
	*
	* This method adds a voting option to the current vote group.
	* The voting option is identified by the specified command and is displayed with the specified text.
	* Additional arguments can be provided to format the text using localization.
	*
	* @tparam Ts The types of the additional arguments.
	* @param pCmd The command associated with the voting option.
	* @param pText The text displayed for the voting option.
	* @param args Additional arguments used for formatting the text.
	* @return A reference to the VoteWrapper instance.
	*
	* @example
	* @code{.cpp}
	* VoteWrapper vote(0); // Create a VoteWrapper instance for client ID 0
	* vote.AddOption("kick", "Kick a player"); // Add a voting option to kick a player
	* @endcode
	*/
	template<typename ... Ts>
	VoteWrapper& AddOption(const char* pCmd, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl(pCmd, NOPE, NOPE, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	/**
	 * @brief Adds a voting option with extra data to the current vote group.
	 *
	 * This method adds a voting option to the current vote group.
	 * The voting option is identified by the specified command and is displayed with the specified text.
	 * Additional arguments can be provided to format the text using localization.
	 * The extra data can be used to provide additional information or parameters for the voting option.
	 *
	 * @tparam Ts The types of the additional arguments.
	 * @param pCmd The command associated with the voting option.
	 * @param Extra The extra data associated with the voting option.
	 * @param pText The text displayed for the voting option.
	 * @param args Additional arguments used for formatting the text.
	 * @return A reference to the VoteWrapper instance.
	 *
	 * @example
	 * @code{.cpp}
	 * VoteWrapper vote(0); // Create a VoteWrapper instance for client ID 0
	 * vote.AddOption("kick", 1, "Kick a player"); // Add a voting option to kick a player with extra data 1
	 * @endcode
	 */
	template<typename ... Ts>
	VoteWrapper& AddOption(const char* pCmd, int Extra, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl(pCmd, Extra, NOPE, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	/**
	 * @brief Adds a voting option with two extra data values to the current vote group.
	 *
	 * This method adds a voting option to the current vote group.
	 * The voting option is identified by the specified command and is displayed with the specified text.
	 * Additional arguments can be provided to format the text using localization.
	 * The two extra data values can be used to provide additional information or parameters for the voting option.
	 *
	 * @tparam Ts The types of the additional arguments.
	 * @param pCmd The command associated with the voting option.
	 * @param Extra1 The first extra data associated with the voting option.
	 * @param Extra2 The second extra data associated with the voting option.
	 * @param pText The text displayed for the voting option.
	 * @param args Additional arguments used for formatting the text.
	 * @return A reference to the VoteWrapper instance.
	 *
	 * @example
	 * @code{.cpp}
	 * VoteWrapper vote(0); // Create a VoteWrapper instance for client ID 0
	 * vote.AddOption("kick", 1, 2, "Kick a player"); // Add a voting option to kick a player with extra data 1 and 2
	 * @endcode
	 */
	template<typename ... Ts>
	VoteWrapper& AddOption(const char* pCmd, int Extra1, int Extra2, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl(pCmd, Extra1, Extra2, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	/**
	 * @brief Adds a voting option with a callback function to the voting menu.
	 *
	 * This method adds a voting option to the voting menu with a specified callback function.
	 * The callback function will be called when the voting option is selected by a player.
	 *
	 * @tparam Ts The types of the arguments for formatting the voting option text.
	 * @param pUser A pointer to user-defined data that will be passed to the callback function.
	 * @param CallbackImpl The callback function that will be called when the voting option is selected.
	 * @param pText The text of the voting option, which can include format specifiers.
	 * @param args The arguments for formatting the voting option text.
	 * @return A reference to the VoteWrapper instance.
	 *
	 * Example usage:
	 * @code{.cpp}
	 * VoteWrapper vote(0);
	 * vote.AddOptionCallback(nullptr, MyCallback, "Option 1");
	 * @endcode
	 */
	template<typename ... Ts>
	VoteWrapper& AddOptionCallback(void* pUser, const VoteOptionCallbackImpl& CallbackImpl, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("CALLBACK_IMPL", NOPE, NOPE, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		m_pGroup->SetLastVoteCallback(CallbackImpl, pUser);
		return *this;
	}

	/**
	 * @brief Adds a voting option with a callback function and an extra parameter to the voting menu.
	 *
	 * This method adds a voting option to the voting menu with a specified callback function and an extra parameter.
	 * The callback function will be called when the voting option is selected by a player, and the extra parameter will be passed to the callback function.
	 *
	 * @tparam Ts The types of the arguments for formatting the voting option text.
	 * @param pUser A pointer to user-defined data that will be passed to the callback function.
	 * @param CallbackImpl The callback function that will be called when the voting option is selected.
	 * @param Extra The extra parameter to be passed to the callback function.
	 * @param pText The text of the voting option, which can include format specifiers.
	 * @param args The arguments for formatting the voting option text.
	 * @return A reference to the VoteWrapper instance.
	 *
	 * Example usage:
	 * @code{.cpp}
	 * VoteWrapper vote(0);
	 * vote.AddOptionCallback(nullptr, MyCallback, 1, "Option 1");
	 * @endcode
	 */
	template<typename ... Ts>
	VoteWrapper& AddOptionCallback(void* pUser, const VoteOptionCallbackImpl& CallbackImpl, int Extra, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("CALLBACK_IMPL", Extra, NOPE, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		m_pGroup->SetLastVoteCallback(CallbackImpl, pUser);
		return *this;
	}

	/**
	 * @brief Adds a voting option to the voting menu with a callback function.
	 *
	 * This method adds a voting option to the voting menu with a callback function.
	 * The voting option is displayed as a selectable option in the menu.
	 * When the option is selected, the specified callback function is called.
	 *
	 * @tparam Ts The types of the optional arguments.
	 * @param pUser A pointer to user-defined data that will be passed to the callback function.
	 * @param CallbackImpl The callback function to be called when the option is selected.
	 * @param Extra1 The first extra parameter to be passed to the callback function.
	 * @param Extra2 The second extra parameter to be passed to the callback function.
	 * @param pText The text of the voting option.
	 * @param args The optional arguments for localization.
	 * @return A reference to the VoteWrapper instance.
	 *
	 * Example usage:
	 * @code{.cpp}
	 * VoteWrapper vote(0);
	 * vote.AddOptionCallback(nullptr, MyCallback, 1, 2, "Option 1");
	 * @endcode
	 */
	template<typename ... Ts>
	VoteWrapper& AddOptionCallback(void* pUser, const VoteOptionCallbackImpl& CallbackImpl, int Extra1, int Extra2, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("CALLBACK_IMPL", Extra1, Extra2, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		m_pGroup->SetLastVoteCallback(CallbackImpl, pUser);
		return *this;
	}
	
	static void AddLine(int ClientID) noexcept
	{
		const auto pVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		pVoteGroup->AddLineImpl();
		m_pData[ClientID].push_back(pVoteGroup);
	}
	
	static void AddBackpage(int ClientID) noexcept
	{
		const auto pVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		pVoteGroup->AddBackpageImpl();
		m_pData[ClientID].push_back(pVoteGroup);
	}
	
	static void AddEmptyline(int ClientID) noexcept
	{
		const auto pVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		pVoteGroup->AddEmptylineImpl();
		m_pData[ClientID].push_back(pVoteGroup);
	}
	
	static void AddItemValue(int ClientID, int ItemID) noexcept
	{
		const auto pVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		pVoteGroup->AddItemValueImpl(ItemID);
		m_pData[ClientID].push_back(pVoteGroup);
	}
	
	static void RebuildVotes(int ClientID);
	static CVoteOption* GetOptionVoteByAction(int ClientID, const char* pActionName);
};
#undef FMT_LOCALIZE_STR

class CVotePlayerData
{
	friend class CVoteGroup;
	friend class VoteWrapper;

	struct VoteGroupHidden
	{
		bool m_State {};
		int m_Flag {};
	};

	CGS* m_pGS {};
	CPlayer* m_pPlayer {};
	int m_LastMenuID{};
	int m_CurrentMenuID{};
	int m_ExtraID {};
	std::thread m_VoteUpdater {};
	enum class STATE_UPDATER { WAITING, RUNNING, DONE };
	std::atomic<STATE_UPDATER> m_VoteUpdaterStatus{ STATE_UPDATER::WAITING };
	ska::unordered_map<int, ska::unordered_map<int, VoteGroupHidden>> m_aHiddenGroup{};

	VoteGroupHidden* EmplaceHidden(int ID, int Type);
	VoteGroupHidden* GetHidden(int ID);
	void ResetHidden(int MenuID);
	void ResetHidden() { ResetHidden(m_CurrentMenuID); }
	static void ThreadVoteUpdater(CVotePlayerData* pData);

public:
	CVotePlayerData()
	{
		m_CurrentMenuID = MENU_MAIN;
		m_LastMenuID = MENU_MAIN;
	}

	~CVotePlayerData()
	{
		if(m_VoteUpdater.joinable())
			m_VoteUpdater.join();

		ClearVotes();
		m_pGS = nullptr;
		m_pPlayer = nullptr;
		m_aHiddenGroup.clear();
	}

	void Init(CGS* pGS, CPlayer* pPlayer)
	{
		m_pGS = pGS;
		m_pPlayer = pPlayer;
	}

	void ApplyVoteUpdaterData();
	void UpdateVotes(int MenuID);
	void UpdateVotesIf(int MenuID);
	void UpdateCurrentVotes() { UpdateVotes(m_CurrentMenuID); }
	void ClearVotes() const;

	void SetCurrentMenuID(int MenuID) { m_CurrentMenuID = MenuID; }
	int GetCurrentMenuID() const { return m_CurrentMenuID; }
	int GetExtraID() const { return m_ExtraID; }

	void SetLastMenuID(int MenuID) { m_LastMenuID = MenuID; }
	int GetLastMenuID() const { return m_LastMenuID; }

	bool DefaultVoteCommands(const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason);
};

#endif
