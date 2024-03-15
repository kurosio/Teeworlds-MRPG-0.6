#ifndef GAME_SERVER_MMOCORE_UTILS_DBSET_H
#define GAME_SERVER_MMOCORE_UTILS_DBSET_H

#include <string>

class DBSet
{
	// Create an empty string variable called m_Data to store the data
	std::string m_Data {};

	// Create an empty unordered_set variable called m_DataItems to store unique data items
	ska::flat_hash_map<std::string, size_t> m_DataItems {};

	// Initialize the function
	void Init()
	{
		// Check if m_Data is not empty
		if(!m_Data.empty())
		{
			// Set the delimiter, starting and ending positions for finding the delimiter
			const std::string delimiter = ",";
			size_t start = 0;
			int iteration = 0;
			
			// Reserve memory for m_DataItems to avoid unnecessary reallocations
			m_DataItems.reserve(m_Data.length() / delimiter.length() + 1);

			// Continue looping until all delimiters are found
			while(start < m_Data.size())
			{
				// Find the next occurrence of the delimiter starting from the current start position
				auto end = m_Data.find(delimiter, start);
				if(end == std::string::npos)
					end = m_Data.size();

				// Add a new item to m_DataItems by copying the substring between the start and end positions
				// and remove leading and trailing spaces from the substring
				auto substring = m_Data.substr(start, end - start);
				while(substring.find_first_of(' ') == 0)
					substring.erase(0, 1);
				while(substring.find_last_of(' ') == substring.size() - 1)
					substring.erase(substring.size() - 1, 1);
				m_DataItems[substring] = (size_t)1 << iteration++;

				// Update the start position for the next iteration
				start = end + delimiter.length();
			}
		}
	}

public:
	// Default
	DBSet() = default;

	// Parameterized constructor that takes an lvalue reference to a string
	DBSet(const std::string& pData) : m_Data(pData)
	{
		Init();
	}

	// Parameterized constructor that takes an rvalue reference to a string
	DBSet(std::string&& pData) : m_Data(std::move(pData))
	{
		Init();
	}

	// Operator bitwise AND overload for checking if a specific flag exists in the data items collection
	bool operator&(const size_t& flag) const
	{
		return std::any_of(m_DataItems.begin(), m_DataItems.end(), [flag](const auto& item)
		{
			const auto& [key, value] = item;
			return flag & value;
		});
	}

	// Assignment operator overload for assigning a std::string to a DBSet object
	DBSet& operator=(const std::string& set)
	{
		// Create a temporary DBSet object with the given std::string
		DBSet tmp(set);

		// Move assign the temporary object to the current object using std::move
		*this = std::move(tmp);

		// Return the current object
		return *this;
	}

	// Move assignment operator overload for assigning an rvalue std::string to a DBSet object
	DBSet& operator=(std::string&& set)
	{
		// Move assign the rvalue std::string to the data member of the current object
		m_Data = std::move(set);

		// Initialize the current object (call the Init() function)
		Init();

		// Return the current object
		return *this;
	}

	// Checks if a specific set exists in the data items collection.
	bool hasSet(const std::string& pSet) const
	{
		return m_DataItems.find(pSet) != m_DataItems.end();
	}

	// Return a constant reference to the unordered_set<std::string> data member m_DataItems
	const ska::flat_hash_map<std::string, size_t>& GetDataItems() const
	{
		return m_DataItems;
	}
};

#endif //GAME_SERVER_MMO_UTILS_DBSET_H
