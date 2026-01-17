



#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <vector>
#include <array>
#include <set>
#include <queue>
#include <stack>
#include <string>
#include <cstring>
#include <fstream>

#include <algorithm>
#include <utility>
#include <thread>


namespace clau {
	enum TokenType {
		LEFT_BRACE = '{', RIGHT_BRACE = '}', LEFT_BRACKET = '[', RIGHT_BRACKET = ']', ASSIGNMENT = ':', COMMA = ',', COLON=':',
		BACK_SLUSH='\\', QUOTED ='\"',
		STRING, NUMBER, TRUE, FALSE, _NULL, END,
		//CHUNK_START, CHUNK_END,
		REAL_LENGTH, 
	};
	
	struct Token64 {
		uint64_t data;
	};

	struct Token8 {
	private:
		uint8_t data;
		// std::vector<Token8> tokens;
		// 1bit of tokens[x] - 0 : normal, 1 : special_token ( TOKEN_CHUNK_START, TOKEN_CHUNK_END, TOKEN_NEXT )
			// tokens[a] - type(4bit) : ex) string, "string", 3.14
			//							// fixed length (so, can omit tokens[a+1]) -> left_brace right_brace left_bracket right_bracket assignment comma
			// tokens[a+1] - length(1~128, 7bit)
	public:
		bool is_special_token() const {
			return data & 0x80;
		}
		TokenType get_type() const {
			return (TokenType)(data & 0x7F);
		}
		uint8_t get_length() const {
			return data & 0x7F;
		}
		void set_special_token(uint8_t value) {
			data = 0x80 | (value & 0x7F);
		}
		void set_general_token(uint8_t value) {
			data = value & 0x7F;
		}
	};


	[[nodiscard]]
	inline
	uint64_t write_token(Token8* tokens, uint64_t count, uint64_t length) {
		if (length < 128) {
			tokens[count].set_general_token(STRING); // pass type
			tokens[count + 1].set_general_token(static_cast<uint8_t>(length)); // pass length
			return count + 2;
		}
		else {
			// special token write
			tokens[count].set_special_token(REAL_LENGTH);
			Token64* ptr = reinterpret_cast<Token64*>(tokens + count + 1);
			ptr->data = length;

			return count + 1 + 8;
		}
	}

	[[nodiscard]]
	inline
	uint64_t write_token2(Token8* tokens, uint64_t count, uint8_t type) {
		tokens[count].set_general_token(type);
		return count + 1;
	}
	// 컴파일 타임 룩업 테이블
	static constexpr auto WHITESPACE_LUT = [] {
		std::array<bool, 256> table = {};
		table[' '] = table['\t'] = table['\r'] =
			table['\n'] = table['\v'] = table['\f'] = true;
		return table;
		}();

	static __forceinline bool isWhitespace(const char ch)
	{
		return WHITESPACE_LUT[static_cast<unsigned char>(ch)];
	}

	__forceinline
		void skip_whitespace(const char* buf, int64_t& token_first, int64_t& token_last) {
		// 조건 체크를 먼저
		if (token_first > token_last) return;

		// 앞에서부터
		const char* p = buf + token_first;
		const char* end = buf + token_last;
		while (p <= end && WHITESPACE_LUT[static_cast<unsigned char>(*p)]) {
			++p;
		}
		token_first = p - buf;

		// 뒤에서부터
		if (token_first > token_last) return;
		p = buf + token_last;
		const char* start = buf + token_first;
		while (p >= start && WHITESPACE_LUT[static_cast<unsigned char>(*p)]) {
			--p;
		}
		token_last = p - buf;
	}

	[[nodiscard]]
	__forceinline
		int64_t chk_whitespace(const char* buf, int64_t token_first, int64_t token_last) {
		skip_whitespace(buf, token_first, token_last);
		return (token_first <= token_last) ? (token_last - token_first + 1) : 0;
	}
	/*
	static __forceinline bool isWhitespace(const char ch)
	{
		switch (ch)
		{
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '\v':
		case '\f':
			return true;
			break;
		}
		return false;
	}

	__forceinline
	void skip_whitespace(const char* buf, int64_t& token_first, int64_t& token_last) {
		while (isWhitespace(buf[token_first]) && token_first <= token_last) {
			token_first++;
		}
		while (isWhitespace(buf[token_last]) && token_first <= token_last) {
			token_last--;
		}
	}

	[[nodiscard]]
	__forceinline
	int64_t chk_whitespace(const char* buf, int64_t token_first, int64_t token_last) {
		skip_whitespace(buf, token_first, token_last);
		if (token_first > token_last) {
			return 0;
		}
		return token_last - token_first + 1;
	}
	*/
	[[nodiscard]]
	inline
	uint64_t write_token(const char* buf, uint64_t buf_len, Token8* tokens) {
		int64_t token_start = 0;
		int64_t token_last = -1;
		uint64_t count = 0;
		
		uint64_t len = 0; // temp
		bool is_back_slush_on = false;
		uint64_t count_now_spaces = 0;

		for (uint64_t i = 0; i < buf_len; ++i) {
			if (is_back_slush_on) {
				is_back_slush_on = false;
				continue;
			}

			const int ch = buf[i];
	
			switch (ch) {
			case '{':
			case '}':
			case '[':
			case ']':
			case ':':
			case ',':
			case '\"':
				token_last = i - 1;
				if (count_now_spaces == token_last - token_start + 1) {
					//
				}
				else if (len = chk_whitespace(buf, token_start, token_last)) {
					// write token
					count = write_token(tokens, count, len);
				}
				// write quoted token
				count = write_token2(tokens, count, ch);
				token_start = i + 1;
				token_last = i + 1;

				count_now_spaces = 0;
				break;
			case '\\':
				token_last = i - 1;

				is_back_slush_on = true;
			
				token_last = i + 1;

				count_now_spaces = 0;
				
				break;
			case ' ':
			case '\t':
			case '\r':
			case '\v':
			case '\f':
			case '\n':
				token_last = i + 1;
				count_now_spaces++;
				break;
			}
		}

		if (len = chk_whitespace(buf, token_start, buf_len - 1)) {
			// write token
			count = write_token(tokens, count, len);
		}

		return count;
	}

	inline
		void print(Token8* tokens) {
		// file ouput test
		const int i = 0;
		if (tokens[i].is_special_token()) {
			// length 출력
			Token64* length_ptr = reinterpret_cast<Token64*>(tokens + i + 1);
			std::cout << "type : STRING-special, length : " << length_ptr->data << "\n";
		}
		else {
			uint8_t type = tokens[i].get_type();
			if (type == STRING) {
				uint8_t length = tokens[i + 1].get_length();
				std::cout << "type : STRING, length : " << static_cast<uint32_t>(length) << "\n";
			}
			else {
				std::cout << "type : " << static_cast<uint32_t>(type) << "\n";
			}
		}
	}
	inline
		void read_token(Token8* tokens, uint64_t tokens_len) {
		// file ouput test
		for (uint64_t i = 0; i < tokens_len; ) {
			if (tokens[i].is_special_token()) {
				// length 출력
				Token64* length_ptr = reinterpret_cast<Token64*>(tokens + i + 1);
				std::cout << "type : STRING-special, length : " << length_ptr->data << "\n";
				i += 1 + 8;
			}
			else {
				uint8_t type = tokens[i].get_type();
				if (type == STRING) {
					uint8_t length = tokens[i + 1].get_length();
					std::cout << "type : STRING, length : " << static_cast<uint32_t>(length) << "\n";
					i += 2;
				}
				else {
					std::cout << "type : " << static_cast<uint32_t>(type) << "\n";
					i += 1;
				}
			}
		}
	}


	struct Token {
	private:
		uint32_t _start;
		uint32_t _value; // 4bit type + 28bit length	
	public:
		uint32_t& start() {
			return _start;
		}
		uint32_t len() {
			return _value & 0x0FFFFFFF;
		}
		void set_len(uint32_t length) {
			_value = (_value & 0xF0000000) | (length & 0x0FFFFFFF);
		}
		TokenType get_type() {
			return (TokenType)((_value >> 28) & 0x0F);
		}
		void set_type(TokenType type) {
			_value = (_value & 0x0FFFFFFF) | ((static_cast<uint32_t>(type) & 0x0F) << 28);
		}
	};

	namespace LoadDataOption
	{
		//constexpr char LineComment = '#';	// # 
		constexpr char LeftBrace = '{';  // { [
		constexpr char RightBrace = '}';	// } ]
		constexpr char LeftBracket = '[';
		constexpr char RightBracket = ']';
		constexpr char Assignment = ':';	// = :
		constexpr char Comma = ',';
	}

	class Utility {
	private:
		class BomInfo
		{
		public:
			int64_t bom_size;
			char seq[5];
		};

		const static int64_t BOM_COUNT = 1;


		static const BomInfo bomInfo[1];

	public:
		enum class BomType { UTF_8, ANSI };

		static BomType ReadBom(FILE* file) {
			char btBom[5] = { 0, };
			int64_t readSize = fread(btBom, sizeof(char), 5, file);


			if (0 == readSize) {
				clearerr(file);
				fseek(file, 0, SEEK_SET);

				return BomType::ANSI;
			}

			BomInfo stBom = { 0, };
			BomType type = ReadBom(btBom, readSize, stBom);

			if (type == BomType::ANSI) { // ansi
				clearerr(file);
				fseek(file, 0, SEEK_SET);
				return BomType::ANSI;
			}

			clearerr(file);
			fseek(file, static_cast<long>(stBom.bom_size * sizeof(char)), SEEK_SET);
			return type;
		}

		static BomType ReadBom(const char* contents, int64_t length, BomInfo& outInfo) {
			char btBom[5] = { 0, };
			int64_t testLength = length < 5 ? length : 5;
			memcpy(btBom, contents, testLength);

			int64_t i, j;
			for (i = 0; i < BOM_COUNT; ++i) {
				const BomInfo& bom = bomInfo[i];

				if (bom.bom_size > testLength) {
					continue;
				}

				bool matched = true;

				for (j = 0; j < bom.bom_size; ++j) {
					if (bom.seq[j] == btBom[j]) {
						continue;
					}

					matched = false;
					break;
				}

				if (!matched) {
					continue;
				}

				outInfo = bom;

				return (BomType)i;
			}

			return BomType::ANSI;
		}


	public:
		static __forceinline bool isWhitespace(const char ch)
		{
			switch (ch)
			{
			case ' ':
			case '\t':
			case '\r':
			case '\n':
			case '\v':
			case '\f':
				return true;
				break;
			}
			return false;
		}

		static __forceinline int Equal(const int64_t x, const int64_t y)
		{
			if (x == y) {
				return 0;
			}
			return -1;
		}

	public:


		static void skipWhitespace(char* buf, int64_t& token_first, int64_t& token_last) {
			while (isWhitespace(buf[token_first]) && token_first <= token_last) {
				token_first++;
			}
			while (isWhitespace(buf[token_last]) && token_first <= token_last) {
				token_last++;
			}
		}

		// todo - rename.
		__forceinline // ch is no needed..
			static Token Get(int64_t position, int64_t length, const char* ch) {
			Token token;

			//token, text) = TokenType::END;
			token.set_len(static_cast<uint32_t>(length));
			token.start() = position;
			switch (*ch) {
			case '{':
				token.set_type(TokenType::LEFT_BRACE);
				break;
			case '}':
				token.set_type(TokenType::RIGHT_BRACE);
				break;
			case '[':
				token.set_type(TokenType::LEFT_BRACKET);
				break;
			case ']':
				token.set_type(TokenType::RIGHT_BRACE);
				break;
			case ':':
				token.set_type(TokenType::ASSIGNMENT);
				break;
			case ',':
				token.set_type(TokenType::COMMA);
				break;
			case '\\':
				token.set_type(TokenType::BACK_SLUSH);
				break;
			case '\"':
				token.set_type(TokenType::QUOTED);
				break;
			}

			return token;
		}

		__forceinline
			static TokenType GetType(Token token, const char* buf) {
				{
					char ch = buf[token.start()];

					switch (ch) {
					case LoadDataOption::LeftBrace:
						return  TokenType::LEFT_BRACE;
						break;
					case LoadDataOption::RightBrace:
						return  TokenType::RIGHT_BRACE;
						break;
					case LoadDataOption::LeftBracket:
						return  TokenType::LEFT_BRACKET;
						break;
					case LoadDataOption::RightBracket:
						return  TokenType::RIGHT_BRACE;
						break;
					case LoadDataOption::Assignment:
						return  TokenType::ASSIGNMENT;
						break;
					case LoadDataOption::Comma:
						return  TokenType::COMMA;
						break;
					case '\\':
						return  TokenType::BACK_SLUSH;

						break;
					case 't':
						return  TokenType::TRUE;

						break;
					case 'f':
						return  TokenType::FALSE;
						break;
					case 'n':
						return  TokenType::_NULL;
						break;
					case '\"':
						return  TokenType::QUOTED;
						break;
					case '-':
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						return  TokenType::NUMBER;
						break;
					}
				}
				return TokenType::END;
		}

		static void PrintToken(const char* buffer, Token token) {
			//std::ofstream outfile("output.txt", std::ios::app);

			std::cout << std::string_view(buffer + token.start(), token.len());
			//	outfile << std::string_view(buffer + token.start(), token.len()) << "\n";
				//std::cout << Utility::GetIdx(token) << " " << Utility::GetLength(token) << "\n";
				//std::cout << std::string_view(buffer + Utility::GetIdx(token), Utility::GetLength(token));
			//	outfile.close();
		}
	};

	inline uint8_t char_to_token_type[256];

	class InFileReserver
	{
	private:
		char* buffer = nullptr;
		int64_t buffer_len = 0;
		Token8* token_orig = nullptr;
		int64_t token_orig_len = 0;

	public:
		~InFileReserver() {
			if (buffer) {
				delete[](buffer);
			}
			if (token_orig) {
				free(token_orig);
			}
		}

	private:
		InFileReserver(const InFileReserver&) = delete;
		InFileReserver& operator=(const InFileReserver&) = delete;

	private:

		static void _Scanning_2(const char* chunk, const uint64_t chunk_length,
			Token8*& token_arr, std::array<int64_t, 2>& _token_arr_size) {

			_token_arr_size[0] = write_token(chunk, chunk_length, token_arr);
		}

		static void _Scanning(char* text, int64_t num, const int64_t length,
			Token*& token_arr, std::array<int64_t, 2>& _token_arr_size, bool is_last, std::array<int, 2>& _last_state) {

				{
					if (1) {
						int state = 0; // if state == 1 then  \] or \[ ...

						int64_t token_first = 0;
						int64_t token_last = -1;

						int64_t token_arr_count = 0;

						for (int64_t i = 0; i < length; ++i) {

							const char ch = text[i];

							switch (ch) {
							case '\"':
								token_last = i - 1;
								if (token_last - token_first + 1 > 0) {
									token_arr[token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
									token_arr_count++;
								}

								token_first = i;
								token_last = i;

								token_first = i + 1;
								token_last = i + 1;

								{//
									token_arr[token_arr_count] = Utility::Get(i + num, 1, text + i);
									token_arr_count++;
								}
								break;
							case ',':
								token_last = i - 1;
								if (token_last - token_first + 1 > 0) {
									token_arr[token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
									token_arr_count++;
								}

								token_first = i;
								token_last = i;

								token_first = i + 1;
								token_last = i + 1;

								{//
									token_arr[token_arr_count] = Utility::Get(i + num, 1, text + i);
									token_arr_count++;
								}
								break;
							case '\\':
							{
								// divide by { } [ ] , : whitespace and (is_last == false) -> no last item is '\\'
								if (i + 1 < length && (text[i + 1] == '\\' || text[i + 1] == '\"')) {
									token_arr[token_arr_count] = Utility::Get(i + num, 1, text + i);
									token_arr_count++;
									++i;
									token_arr[token_arr_count] = Utility::Get(i + num, 1, text + i);
									token_arr_count++;

									token_first = i + 1;
									token_last = i + 1;
								}
							}
							break;
							/*
														case ' ':
														case '\t':
														case '\r':
														case '\v':
														case '\f':
														case '\n':
														{
															token_last = i - 1;
															if (token_last - token_first + 1 > 0) {
																token_arr[token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
																token_arr_count++;
															}
															token_first = i + 1;
															token_last = i + 1;

														}
														break;
								*/
							case LoadDataOption::LeftBrace:
							case LoadDataOption::LeftBracket:
							case LoadDataOption::RightBrace:
							case LoadDataOption::RightBracket:
							case LoadDataOption::Assignment:
								token_last = i - 1;
								if (token_last - token_first + 1 > 0) {
									token_arr[token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
									token_arr_count++;
								}
								token_first = i;
								token_last = i;

								token_arr[token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
								token_arr_count++;

								token_first = i + 1;
								token_last = i + 1;
								break;
							}

						}

						if (length - 1 - token_first + 1 > 0) {
							token_arr[token_arr_count] = Utility::Get(token_first + num, length - 1 - token_first + 1, text + token_first);
							token_arr_count++;
						}

						_token_arr_size[0] = token_arr_count;
					}
				}
		}
		
		static void _Scanning2_2(char* text, int64_t num, const int64_t length,
			Token8*& token_arr, int64_t token_arr_size, std::array<int64_t, 2>& _token_arr_size, std::array<int, 2>& _last_state, 
			std::array<int64_t, 2>& _last_token_idx) {
			{
				auto _text = text - num;
				int state = 1; uint64_t quoted_len = 0;
				int64_t count = 0; 
				int64_t last_token_idx = -1;

				for (int64_t j = 0; j < token_arr_size; ) {
					int64_t i = j;

					last_token_idx = count;

					if (state == 0) {
						if (token_arr[i].is_special_token() == false && token_arr[i].get_type() == TokenType::QUOTED) {
							state = 1; quoted_len = 0;
							j++;
						}
						else {
							if (token_arr[i].is_special_token() && token_arr[i].get_type() == TokenType::REAL_LENGTH) {
								token_arr[length + count] = token_arr[i];
								for (int k = 1; k <= 8; ++k) {
									token_arr[length + count + k] = token_arr[i + k];
								}
								j += 1 + 8; count += 9;
							}
							else {
								if (token_arr[i].get_type() != TokenType::STRING) {
									token_arr[length + count] = token_arr[i];
									j++; count += 1;
								}
								else {
									token_arr[length + count] = token_arr[i]; // pass type
									token_arr[length + count + 1] = token_arr[i + 1]; // pass length
									j += 2; count += 2;
								}
							}
						}
					}
					else { // state == 1
						if (token_arr[i].is_special_token() == false && token_arr[i].get_type() == TokenType::QUOTED) {
							uint64_t str_len = quoted_len;
							count = write_token(token_arr + length, count, str_len);
							++j;
							state = 0;
							quoted_len = 0;
						}
						//else if (token_arr[i].is_special_token() == false && token_arr[i].get_type() == TokenType::BACK_SLUSH) {
						//	++j;
						//	quoted_len += 1;
						//}
						else if (token_arr[i].is_special_token() && token_arr[i].get_type() == TokenType::REAL_LENGTH) {
							token_arr[length + count] = token_arr[i]; // pass type
							Token64* length_ptr = reinterpret_cast<Token64*>(token_arr + i + 1);
							quoted_len += length_ptr->data;
							j += 1 + 8;
						}
						else {
							if (token_arr[i].get_type() != TokenType::STRING) {
								quoted_len += 1;
								j += 1;
							}
							else {
								quoted_len += token_arr[i + 1].get_length();
								j += 2;
							}
						}
					}
				}

				if (state == 1) {
					count = write_token(token_arr + length, count, quoted_len);
					count++;
				}

				_last_state[1] = state;
				_token_arr_size[1] = count;
				_last_token_idx[1] = last_token_idx;
			}
			{
				auto _text = text - num;
				int state = 0; uint64_t quoted_len = 0;
				int64_t count = 0;
				int64_t last_token_idx = -1;

				for (int64_t j = 0; j < token_arr_size; ) {
					last_token_idx = count;
					//std::cout << j << ", state " << state << "\n";
					//print(&token_arr[count]);
					//std::cout << "\n";
					int64_t i = j;
					if (state == 0) {
						if (token_arr[i].is_special_token() == false && token_arr[i].get_type() == TokenType::QUOTED) {
							state = 1; quoted_len = 0;
							j++;
						}
						else {
							if (token_arr[i].is_special_token() && token_arr[i].get_type() == TokenType::REAL_LENGTH) {
								token_arr[count] = token_arr[i];
								for (int k = 1; k <= 8; ++k) {
									token_arr[count + k] = token_arr[i + k];
								}
								j += 1 + 8; count += 9;
							}
							else {
								if (token_arr[i].get_type() != TokenType::STRING) {
									token_arr[count] = token_arr[i];
									j++; count += 1;
								}
								else {
									token_arr[count] = token_arr[i]; // pass type
									token_arr[count + 1] = token_arr[i + 1]; // pass length
									j += 2; count += 2;
								}
							}
						}
					}
					else { // state == 1
						if (token_arr[i].is_special_token() == false && token_arr[i].get_type() == TokenType::QUOTED) {
							uint64_t str_len = quoted_len;
							count = write_token(token_arr, count, str_len);
							
							++j;
							
							state = 0;
							quoted_len = 0;
						}
						else if (token_arr[i].is_special_token() && token_arr[i].get_type() == TokenType::REAL_LENGTH) {
							token_arr[count] = token_arr[i]; // pass type
							Token64* length_ptr = reinterpret_cast<Token64*>(token_arr + i + 1);
							quoted_len += length_ptr->data;
							j += 9;
						}
						else {
							if (token_arr[i].get_type() != TokenType::STRING) {
								quoted_len += 1;
								j += 1;
							}
							else {
								quoted_len += token_arr[i + 1].get_length();
								j += 2;
							}
						}
					}
				}

				if (state == 1) {
					count = write_token(token_arr, count, quoted_len);
					count++;
				}

				_last_state[0] = state;
				_token_arr_size[0] = count;
				_last_token_idx[0] = last_token_idx;
			}
		}
		
		static void _Scanning2(char* text, int64_t num /*text_chunk_start*/, const int64_t length/*text_chunk_length*/,
			Token*& token_arr, int64_t token_arr_size, std::array<int64_t, 2>& _token_arr_size, bool is_last, std::array<int, 2>& _last_state, int id) {

				{
					auto _text = text - num;
					int state = 1; int64_t start_idx = 0;
					int64_t count = 0;
					for (int64_t j = 0; j < token_arr_size; ++j) {
						const int64_t i = j;

						if (state == 0) {
							if ((token_arr[i].get_type()) == TokenType::QUOTED) {
								state = 1; start_idx = i;
							}
							else {
								token_arr[length + count] = token_arr[i];
								count++;
							}
						}
						else { // state == 1
							if ((token_arr[i].get_type()) == TokenType::QUOTED) {
								token_arr[length + count].start() = token_arr[start_idx].start();
								token_arr[length + count].set_len(token_arr[i].start() - token_arr[start_idx].start() + 1);
								count++;

								state = 0;
							}
							else if ((token_arr[i].get_type()) == TokenType::BACK_SLUSH) {
								++j;
							}
						}
					}

					if (state == 1) {
						token_arr[length + count].start() = token_arr[start_idx].start();
						token_arr[length + count].set_len(length - 1 - (token_arr[start_idx].start() - num));
						count++;
					}

					_last_state[1] = state;
					_token_arr_size[1] = count;
				}

				{
					auto _text = text - num;
					int state = 0; int64_t start_idx = 0;
					int64_t count = 0;
					for (int64_t j = 0; j < token_arr_size; ++j) {
						int64_t i = j;

						if (state == 0) {
							if ((token_arr[i].get_type()) == TokenType::QUOTED) {
								state = 1; start_idx = i;
							}
							else {
								token_arr[count] = token_arr[i];
								count++;
							}
						}
						else { // state == 1
							if ((token_arr[i].get_type()) == TokenType::QUOTED) {
								token_arr[count].start() = token_arr[start_idx].start();
								token_arr[count].set_len(token_arr[i].start() - token_arr[start_idx].start() + 1);
								count++;

								state = 0;
							}
							else if ((token_arr[i].get_type()) == TokenType::BACK_SLUSH) {
								++j;
							}
						}
					}

					if (state == 1) {
						token_arr[count].start() = token_arr[start_idx].start();
						token_arr[count].set_len(length - 1 - (token_arr[start_idx].start() - num));
						count++;
					}

					_last_state[0] = state;
					_token_arr_size[0] = count;
				}
		}

		static bool ScanningNew(char* text, int64_t length, int thr_num,
			Token8*& _tokens_orig, int64_t& _tokens_orig_size, std::vector<Token8*>& _token_arr, int64_t& _token_arr_size, bool use_simd)
		{
			std::vector<std::thread> thr(thr_num);
			std::vector<int64_t> start(thr_num);
			std::vector<int64_t> last(thr_num);

			{
				start[0] = 0;

				for (int i = 1; i < thr_num; ++i) {
					start[i] = length / thr_num * i;

					for (int64_t x = start[i]; x <= length; ++x) {
						if (Utility::isWhitespace(text[x]) ||
							LoadDataOption::LeftBracket == text[x] || LoadDataOption::RightBracket == text[x] ||
							LoadDataOption::Comma == text[x] ||
							LoadDataOption::LeftBrace == text[x] || LoadDataOption::RightBrace == text[x] ||
							LoadDataOption::Assignment == text[x]) {

							start[i] = x;
							break;
						}

						if (x == length) { // meet end of text?
							return x;
						}
					}
				}

				std::set<int64_t> _set;
				_set.insert(start.begin(), start.end());

				thr_num = _set.size();
				start.clear();
				for (auto x : _set) {
					start.push_back(x);
				}

				for (int i = 0; i < thr_num - 1; ++i) {
					last[i] = start[i + 1];
				}

				last[thr_num - 1] = length;
			}
			int64_t real_token_arr_count = 0;

			auto a = std::chrono::steady_clock::now();
			Token8* tokens_orig = nullptr;
			int64_t now_capacity = 2 * (length + 1);

			if (_tokens_orig) {
				if (now_capacity <= _tokens_orig_size) {
					tokens_orig = _tokens_orig;
				}
				else {
					free(_tokens_orig);

					tokens_orig = (Token8*)calloc(2 * (length + 1), 9); // sizeof(Token8));
					_tokens_orig_size = 2 * (length + 1);
				}
			}
			else {
				tokens_orig = (Token8*)calloc(2 * (length + 1), 9); // sizeof(Token8));
				_tokens_orig_size = 2 * (length + 1);
			}

			if (!tokens_orig) {
				return false;
			}

			std::vector<Token8*> tokens(thr_num);
			tokens[0] = tokens_orig;
			for (int64_t i = 1; i < thr_num; ++i) {
				tokens[i] = tokens[i - 1] + 2 * (last[i - 1] - start[i - 1]);
			}

			int64_t token_count = 0;

			std::vector<std::array<int64_t, 2>> token_arr_size(thr_num);
			std::vector<std::array<int, 2>> last_state(thr_num);
			std::vector<std::array<int64_t, 2>> token_last_idx(thr_num);

			for (int i = 0; i < thr_num; ++i) {
				thr[i] = std::thread(_Scanning_2, text + start[i], /*start[i],*/ last[i] - start[i], std::ref(tokens[i]), std::ref(token_arr_size[i]));
				//,
				//	i == thr_num - 1, std::ref(last_state[i]));
			}

			for (int i = 0; i < thr_num; ++i) {
				thr[i].join();
			}
			auto b = std::chrono::steady_clock::now();

			///debug..
			//read_token(tokens[0], token_arr_size[0][0]);
			//std::cout << "end of debug..\n";;
			{
				int i = 0;
				thr[i] = std::thread(_Scanning2_2, text + start[i], start[i], last[i] - start[i], std::ref(tokens[i]), std::ref(token_arr_size[i][0]), std::ref(token_arr_size[i]),
					/*i == thr_num - 1,*/ std::ref(last_state[i]), std::ref(token_last_idx[i]) /*, i*/);
			}

			for (int i = 1; i < thr_num; ++i) {
				thr[i] = std::thread(_Scanning2_2, text + start[i], start[i], last[i] - start[i], std::ref(tokens[i]), std::ref(token_arr_size[i][0]), std::ref(token_arr_size[i]),
					/*i == thr_num - 1,*/ std::ref(last_state[i]), std::ref(token_last_idx[i]) /*, i*/);
			}

			for (int i = 0; i < thr_num; ++i) {
				thr[i].join();
			}

			auto c = std::chrono::steady_clock::now();

			int state = last_state[0][0]; // last_state[thread_no][state] // state <- 0 or 1

			for (int i = 1; i < thr_num; ++i) {
				if (state == 1) {
					auto& last_idx = token_last_idx[i - 1][0];
					auto& sz = token_arr_size[i - 1][0];
					uint64_t str_len = 0;

					if (last_idx > 0) {
						const uint64_t find_last_value_idx = last_idx - 1;
						auto _start = 0;
						if (tokens[i - 1][find_last_value_idx].is_special_token() && tokens[i - 1][find_last_value_idx].get_type() == REAL_LENGTH) {
							Token64* length_ptr = reinterpret_cast<Token64*>(tokens[i - 1] + find_last_value_idx + 1);
							str_len += length_ptr->data;
						}
						else {
							str_len += tokens[i - 1][find_last_value_idx + 1].get_length();
						}

						if (tokens[i][0].is_special_token() && tokens[i][0].get_type() == REAL_LENGTH) {
							Token64* length_ptr = reinterpret_cast<Token64*>(tokens[i] + 1);
							str_len += length_ptr->data;
						}
						else {
							str_len += tokens[i][1].get_length();
						}

						if (tokens[i - 1][find_last_value_idx].is_special_token() && tokens[i - 1][find_last_value_idx].get_type() == REAL_LENGTH) {
							Token64* length_ptr = reinterpret_cast<Token64*>(tokens[i - 1] + find_last_value_idx + 1);
							length_ptr->data = str_len;
						}
						else {
							sz = write_token(tokens[i - 1], sz - 2, str_len);
						}
						token_arr_size[i][0] = token_arr_size[i][1];
					}
					else {
						std::cout << "chk ...........1\n";
					}
				}
				state = last_state[i][state];
			}

			int idx = -1;

			int start_idx = -1;

			auto d = std::chrono::steady_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(b - a);
			auto dur2 = std::chrono::duration_cast<std::chrono::milliseconds>(c - b);
			auto dur3 = std::chrono::duration_cast<std::chrono::milliseconds>(d - c);

			std::cout << "토큰 후보 배열 구성(parallel) \t" << dur.count() << "ms\n";
			std::cout << "토큰 배열 구성(parallel) \t" << dur2.count() << "ms\n";
			std::cout << "state에 맞게 연결?(sequential) \t" << dur3.count() << "ms\n";

			std::cout << "state is " << state << "\n";

			{
				for (int t = 0; t < thr_num; ++t) {
					if (1) {
					//	read_token(tokens[t], token_arr_size[t][0]);
						//getchar();
					}
					real_token_arr_count += token_arr_size[t][0];
				}
				_token_arr = tokens;
				_token_arr_size = real_token_arr_count;
				_tokens_orig = tokens_orig;
			}

			return true;
		}

		static void Scanning(char* text, const int64_t length,
			Token*& _token_arr, int64_t& _token_arr_size) {

			Token* token_arr = (Token*)calloc(length + 1, sizeof(Token));
			int64_t token_arr_size = 0;

			{
				int state = 0;

				int64_t token_first = 0;
				int64_t token_last = -1;

				int64_t token_arr_count = 0;

				for (int64_t i = 0; i <= length; ++i) {
					const char ch = text[i];

					if (0 == state) {
						if ('\"' == ch) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}

							token_first = i;
							token_last = i;

							state = 1;
						}
						else if (Utility::isWhitespace(ch)) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}
							token_first = i + 1;
							token_last = i + 1;
						}
						else if (LoadDataOption::LeftBrace == ch || LoadDataOption::LeftBracket == ch) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}

							token_first = i;
							token_last = i;

							token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
							token_arr_count++;

							token_first = i + 1;
							token_last = i + 1;
						}
						else if (LoadDataOption::RightBrace == ch || LoadDataOption::RightBracket == ch) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}
							token_first = i;
							token_last = i;

							token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
							token_arr_count++;

							token_first = i + 1;
							token_last = i + 1;

						}
						else if (LoadDataOption::Assignment == ch || LoadDataOption::Comma == ch) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}
							token_first = i;
							token_last = i;

							token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
							token_arr_count++;

							token_first = i + 1;
							token_last = i + 1;
						}
					}
					else if (1 == state) {
						if ('\\' == ch) {
							state = 2;
						}
						else if ('\"' == ch) {
							token_last = i;

							token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
							token_arr_count++;

							token_first = i + 1;
							token_last = i + 1;

							state = 0;
						}
					}
					else if (2 == state) {
						state = 1;
					}
				}

				if (length - 1 - token_first + 1 > 0) {
					token_arr[token_arr_count] = Utility::Get(token_first, length - 1 - token_first + 1, text);
					token_arr_count++;
				}

				token_arr_size = token_arr_count;

				if (0 != state) {
					std::cout << "[" << state << "] state is not zero.\n";
				}
			}

			{
				_token_arr = token_arr;
				_token_arr_size = token_arr_size;
			}
		}

		static void Scanning2(char* text, const int64_t length,
			Token*& _token_arr, int64_t& _token_arr_size) {

			Token* token_arr = (Token*)calloc(length + 1, sizeof(Token));
			int64_t token_arr_size = 0;

			{
				int state = 0;

				int64_t token_first = 0;
				int64_t token_last = -1;

				int64_t token_arr_count = 0;

				for (int64_t i = 0; i <= length; ++i) {
					const char ch = text[i];

					{
						if ('\"' == ch) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}

							token_first = i;
							token_last = i;

							for (; i <= length; ++i) {
								const char ch = text[i];
								if ('\\' == ch) {
									// i >= length -> error.
									++i;
								}
								else if ('\"' == ch) {
									token_last = i;

									token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
									token_arr_count++;

									token_first = i + 1;
									token_last = i + 1;

									break;
								}
							}
						}
						else if (Utility::isWhitespace(ch)) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}
							token_first = i + 1;
							token_last = i + 1;
						}
						else if (LoadDataOption::LeftBrace == ch || LoadDataOption::LeftBracket == ch) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}

							token_first = i;
							token_last = i;

							token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
							token_arr_count++;

							token_first = i + 1;
							token_last = i + 1;
						}
						else if (LoadDataOption::RightBrace == ch || LoadDataOption::RightBracket == ch) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}
							token_first = i;
							token_last = i;

							token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
							token_arr_count++;

							token_first = i + 1;
							token_last = i + 1;

						}
						else if (LoadDataOption::Assignment == ch || LoadDataOption::Comma == ch) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
								token_arr_count++;
							}
							token_first = i;
							token_last = i;

							token_arr[token_arr_count] = Utility::Get(token_first, token_last - token_first + 1, text);
							token_arr_count++;

							token_first = i + 1;
							token_last = i + 1;
						}
					}
				}

				if (length - 1 - token_first + 1 > 0) {
					token_arr[token_arr_count] = Utility::Get(token_first, length - 1 - token_first + 1, text);
					token_arr_count++;
				}

				token_arr_size = token_arr_count;

				if (0 != state) {
					std::cout << "[" << state << "] state is not zero.\n";
				}
			}

			{
				_token_arr = token_arr;
				_token_arr_size = token_arr_size;
			}
		}

		static std::pair<bool, int> Scan(FILE* inFile, int thr_num,
			char*& _buffer, int64_t& _buffer_len, Token8*& _token_orig, int64_t& _token_orig_len,
			std::vector<Token8*>& _token_arr, int64_t& _token_arr_len, bool use_simd)
		{
			if (inFile == nullptr) {
				return { false, 0 };
			}

			int64_t* arr_count = nullptr; //
			int64_t arr_count_size = 0;

			std::string temp;
			char* buffer = nullptr;
			int64_t file_length;

			{
				fseek(inFile, 0, SEEK_END);
				int64_t length = ftell(inFile);
				fseek(inFile, 0, SEEK_SET);

				Utility::BomType x = Utility::ReadBom(inFile);

				//	clau_parser11::Out << "length " << length << "\n";
				if (x == Utility::BomType::UTF_8) {
					length = length - 3;
				}

				file_length = length;

				if (_buffer) {
					if (_buffer_len < length) {
						delete[] _buffer; _buffer = nullptr;
						buffer = new (std::nothrow)char[file_length + 1];
					}
					else {
						buffer = _buffer;
					}
				}
				else {
					buffer = new (std::nothrow)char[file_length + 1]; // 
				}

				if (!buffer) {
					fclose(inFile);
					return { false, 1 };
				}

				int a = clock();
				// read data as a block:
				fread(buffer, sizeof(char), file_length, inFile);
				int b = clock();
				std::cout << "load file \t" << b - a << "ms \tfile size " << file_length << "\n";
				fclose(inFile);
				buffer[file_length] = '\0';

				{
					int64_t token_arr_size;

					{
						ScanningNew(buffer, file_length, thr_num, _token_orig, _token_orig_len, _token_arr, token_arr_size, use_simd);
						//Token* token_arr = nullptr;
						//Scanning2(buffer, file_length, token_arr, token_arr_size);
					}

					_buffer = buffer;
					_token_arr_len = token_arr_size;
					_buffer_len = file_length;
				}
			}

			return{ true, 1 };
		}

	public:
		explicit InFileReserver()
		{
			//
		}
	public:
		bool operator() (const std::string& fileName, int thr_num, std::vector<Token8*>& token_arr, int64_t& token_arr_len)
		{
			FILE* inFile = nullptr;

#ifdef _WIN32 
			fopen_s(&inFile, fileName.c_str(), "rb");
#else
			inFile = fopen(fileName.c_str(), "rb");
#endif

			if (!inFile)
			{
				return false;
			}

			// in Scan, close inFile;
			bool x = Scan(inFile, thr_num, buffer, buffer_len, token_orig, token_orig_len, token_arr, token_arr_len, false).second > 0;

			return x;
		}
	};

	class LoadData
	{
	private:
		InFileReserver ifReserver;
	public:
		LoadData() {
			//
		}
	public:
		bool LoadDataFromFile(const std::string& fileName, int lex_thr_num = 1, int parse_thr_num = 1, bool use_simd = false) /// global should be empty
		{
			if (lex_thr_num <= 0) {
				lex_thr_num = std::thread::hardware_concurrency();
			}
			if (lex_thr_num <= 0) {
				lex_thr_num = 1;
			}

			if (parse_thr_num <= 0) {
				parse_thr_num = std::thread::hardware_concurrency();
			}
			if (parse_thr_num <= 0) {
				parse_thr_num = 1;
			}

			int a = clock();

			bool success = true;

			try {
				int64_t token_arr_len;
				std::vector<Token8*> token_arr;

				ifReserver(fileName, lex_thr_num, token_arr, token_arr_len);

				int b = clock();

				std::cout << b - a << "ms\n";
			}
			catch (const char* err) { std::cout << err << "\n"; return false; }
			catch (const std::string& e) { std::cout << e << "\n"; return false; }
			catch (const std::exception& e) { std::cout << e.what() << "\n"; return false; }
			catch (...) { std::cout << "not expected error" << "\n"; return false; }

			return true;
		}

	};
}



#endif


