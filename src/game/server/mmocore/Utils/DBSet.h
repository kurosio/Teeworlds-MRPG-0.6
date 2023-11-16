#ifndef GAME_SERVER_MMOCORE_UTILS_DBSET_H
#define GAME_SERVER_MMOCORE_UTILS_DBSET_H

#include <string>
#include <unordered_set>

class DBSet
{
	// Create an empty string variable called m_Data to store the data
	std::string m_Data {};

	// Create an empty unordered_set variable called m_DataItems to store unique data items
	ska::unordered_set<std::string> m_DataItems {};

	// Initialize the function
	void Init()
	{
		// Check if m_Data is not empty
		if(!m_Data.empty())
		{
			// Set the delimiter as a comma
			const std::string delimiter = ",";

			// Set the starting and ending positions for finding the delimiter
			size_t start = 0;
			size_t end = m_Data.find(delimiter);

			// Reserve memory for m_DataItems to avoid unnecessary reallocations
			m_DataItems.reserve(m_Data.length() / delimiter.length() + 1);

			// Continue looping until all delimiters are found
			while(end != std::string::npos)
			{
				// Add a new item to m_DataItems by copying the substring between the start and end positions
				m_DataItems.emplace(m_Data.data() + start, end - start);

				// Update the start position to be after the current delimiter
				start = end + delimiter.length();

				// Find the next occurrence of the delimiter starting from the updated start position
				end = m_Data.find(delimiter, start);
			}

			// Add the remaining substring as a new item to m_DataItems
			m_DataItems.emplace(m_Data.data() + start);
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
	const ska::unordered_set<std::string>& GetDataItems() const
	{
		return m_DataItems;
	}
};

#endif //GAME_SERVER_MMO_UTILS_DBSET_H
