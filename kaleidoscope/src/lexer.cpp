#include <kaleidoscope/lexer.hpp>

#include <cctype>
#include <cstdio>

namespace hello_llvm
{
	int tokenizer::get_token()
	{
		static int last_char = ' ';

		// Skip any whitespace.
		while (std::isspace(last_char))
		{
			last_char = std::getchar();
		}

		// identifier: [a-zA-Z][a-zA-Z0-9]*
		if (std::isalpha(last_char))
		{
			identifier_str = static_cast<char>(last_char);
			while (std::isalnum((last_char = std::getchar())))
			{
				identifier_str += static_cast<char>(last_char);
			}

			if (identifier_str == "def")
			{
				return tok_def;
			}
			if (identifier_str == "extern")
			{
				return tok_extern;
			}
			if (identifier_str == "if")
			{
				return tok_if;
			}
			if (identifier_str == "then")
			{
				return tok_then;
			}
			if (identifier_str == "else")
			{
				return tok_else;
			}
			if (identifier_str == "for")
			{
				return tok_for;
			}
			if (identifier_str == "in")
			{
				return tok_in;
			}
			if (identifier_str == "binary")
			{
				return tok_binary;
			}
			if (identifier_str == "unary")
			{
				return tok_unary;
			}
			return tok_identifier;
		}

		// Number: [0-9.]+
		if (std::isdigit(last_char) || last_char == '.')
		{
			std::string num_str;
			do
			{
				num_str += static_cast<char>(last_char);
				last_char = std::getchar();
			} while (std::isdigit(last_char) || last_char == '.');

			num_val = std::strtod(num_str.c_str(), nullptr);
			return tok_number;
		}

		if (last_char == '#')
		{
			// Comment until end of line.
			do {
				last_char = std::getchar();
			} while (last_char != EOF && last_char != '\n' && last_char != '\r');

			if (last_char != EOF)
			{
				return get_token();
			}
		}

		// Check for end of file.  Don't eat the EOF.
		if (last_char == EOF)
		{
			return tok_eof;
		}

		// Otherwise, just return the character as its ascii value.
		const auto this_char = last_char;
		last_char	   = std::getchar();
		return this_char;
	}
}// namespace hello_llvm
