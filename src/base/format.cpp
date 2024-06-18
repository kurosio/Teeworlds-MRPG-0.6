#include "format.h"
#include <base/system.h>

void fmt_init_handler_func(HandlerFmtCallbackFunc* pCallback, void* pData)
{
	struct_format_implement::handler_fmt::init(pCallback, pData);
}

void fmt_set_flags(int flags)
{
	struct_format_implement::handler_fmt::use_flags(flags);
}

std::string pluralize(int count, const std::vector<std::string>& forms)
{
	const size_t numForms = forms.size();
	if(numForms == 2)
		return (count == 1) ? forms[0] : forms[1];

	if(numForms == 3)
	{
		if(forms.size() == 1 || count % 10 == 1 && count % 100 != 11)
			return forms[0];
		if(forms.size() == 2 || count % 10 >= 2 && count % 10 <= 4 && (count % 100 < 10 || count % 100 >= 20))
			return forms[1];
		return forms[2];
	}

	return "";
}

std::vector<std::string> collect_argument_variants(size_t startPos, const std::string& Argument)
{
	// initialize variables
	std::vector<std::string> values;
	size_t pipePos = Argument.find('|', startPos);

	// collect data
	while(pipePos != std::string::npos)
	{
		values.emplace_back(Argument, startPos, pipePos - startPos);
		startPos = pipePos + 1;
		pipePos = Argument.find('|', startPos);
		if(pipePos == std::string::npos)
		{
			
		}
	}
	values.emplace_back(Argument.substr(startPos));
	return values;
}

void struct_format_implement::prepare_result(const description&, const std::string& Text, std::string* pResult, std::vector<std::string>& vPack)
{
    enum { arg_default, arg_plural, arg_reverse, arg_repeat, arg_uppercase, arg_lowercase, arg_truncate, arg_truncate_full, arg_truncate_custom };
    std::string argument;
    size_t argumentPosition = 0;
    bool argumentProcessing = false;
    int argumentType = arg_default;

    for(char iterChar : Text)
    {
        // start argument processing
        if(iterChar == '{')
        {
            argumentProcessing = true;
            continue;
        }

        // truncate type
    	if(iterChar == '~')
        {
            argumentType = arg_truncate;
            continue;
        }

        // plural type
    	if(iterChar == '#')
        {
            argumentType = arg_plural;
            continue;
        }

        // reverse type
        if(iterChar == 'R')
        {
	        argumentType = arg_reverse;
			continue;
        }

        // repeat type
        if(iterChar == '*')
        {
            argumentType = arg_repeat;
            continue;
        }

        // lower case
        if(iterChar == 'L')
        {
            argumentType = arg_lowercase;
            continue;
        }

        // upper case
        if(iterChar == 'U')
        {
            argumentType = arg_uppercase;
            continue;
        }

        // end argument processing
    	if(iterChar == '}')
        {
            argumentProcessing = false;
            if(argumentPosition < vPack.size())
            {
                std::string argumentResult = vPack[argumentPosition++];

                // truncate type
                if(argumentType == arg_truncate)
                {
                    std::string truncationString;
                    char truncationAfterChar = '\0';

                    for(char j : argument)
                    {
                        if(j == '%' && argumentPosition < vPack.size())
                        {
                            truncationString = argumentResult;
                            argumentResult = vPack[argumentPosition++];
                            continue;
                        }
                        if(isdigit(j))
                        {
                            truncationString += j;
                            continue;
                        }
                        truncationAfterChar = j;
                    }

                    const int truncateNum = truncationString.empty() ? 0 : str_toint(truncationString.c_str());
                    argumentType = (truncationAfterChar == '\0') ? arg_truncate_full : arg_truncate_custom;
                    if(argumentType == arg_truncate_full && argumentResult.size() > static_cast<size_t>(truncateNum))
                    {
                        argumentResult.resize(truncateNum);
                    }
                    else if(argumentType == arg_truncate_custom && argumentResult.find(truncationAfterChar) != std::string::npos)
                    {
                        const size_t dotPos = argumentResult.find(truncationAfterChar) + 1;
                        argumentResult = argumentResult.substr(0, dotPos + truncateNum);
                    }
                }
                // plural type
                else if(argumentType == arg_plural)
                {
                    std::string& collectFrom = argument.empty() && argumentPosition < vPack.size() ? vPack[argumentPosition++] : argument;
                    argumentResult += " " + pluralize(str_toint(argumentResult.c_str()), collect_argument_variants(0, collectFrom));
                }
                // reverse type
                else if(argumentType == arg_reverse)
                {
                    std::ranges::reverse(argumentResult);
                }
                // repeat type
                else if(argumentType == arg_repeat)
                {
                    int count = maximum(1, str_toint(argument.c_str()));
                    std::string original = argumentResult;
                    for(int i = 1; i < count; ++i)
                        argumentResult += original;
                }
                // upper case
                else if(argumentType == arg_uppercase)
                {
                    std::ranges::transform(argumentResult, argumentResult.begin(), ::toupper);
                }
                // lower case
                if(argumentType == arg_lowercase)
                {
                    std::ranges::transform(argumentResult, argumentResult.begin(), ::tolower);
                }

                // reset and add result
                argumentType = arg_default;
                (*pResult) += argumentResult;
            }

            argument.clear();
            continue;
        }

        // collect
        if(argumentProcessing)
            argument += iterChar;
        else
            (*pResult) += iterChar;
    }
}