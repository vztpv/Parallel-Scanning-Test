



#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <vector>
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

	struct Token { int64_t start; int64_t len; };

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

	enum TokenType {
		LEFT_BRACE, RIGHT_BRACE, LEFT_BRACKET, RIGHT_BRACKET, ASSIGNMENT, COMMA, COLON,
		BACK_SLUSH, QUOTED,
		STRING, NUMBER, TRUE, FALSE, _NULL, END
	};


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

		// todo - rename.
		__forceinline
			static Token Get(int64_t position, int64_t length, const char* ch) {
			Token token;

			//token, text) = TokenType::END;
			token.len = length;
			token.start = position;

			
			return token;
		}

		__forceinline 
		static TokenType GetType(Token token, const char* buf) {
			{
				char ch = buf[token.start];

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

			std::cout << std::string_view(buffer + token.start, token.len);
			//	outfile << std::string_view(buffer + token.start, token.len) << "\n";
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
		Token* token_orig = nullptr;
		int64_t token_orig_len = 0;

	public:
		~InFileReserver() {
			if (buffer) {
				delete[] (buffer);
			}
			if (token_orig) {
				free(token_orig);
			}
		}

	private:
		InFileReserver(const InFileReserver&) = delete;
		InFileReserver& operator=(const InFileReserver&) = delete;
	
	private:

		static void _Scanning(char* text, int64_t num, const int64_t length,
			Token*& token_arr, int64_t _token_arr_size[2], bool is_last, int _last_state[2]) {

			{
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

					case ' ':
					case '\t':
					case '\r':
					case '\v':
					case '\f':
					case '\n':
					case '\0':
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
					case LoadDataOption::LeftBrace:
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
					case LoadDataOption::LeftBracket:
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
					case LoadDataOption::RightBrace:
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
					case LoadDataOption::RightBracket:
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

		static void _Scanning2(char* text, int64_t num, const int64_t length,
			Token*& token_arr, int64_t token_arr_size, int64_t _token_arr_size[2], bool is_last, int _last_state[2]) {

			{
				auto _text = text - num;
				int state = 1; int64_t start_idx = 0;
				int64_t count = 0;
				for (int64_t j = 0; j < token_arr_size; ++j) {
					int64_t i = j;

					if (state == 0) {
						if (Utility::GetType(token_arr[i], _text) == TokenType::QUOTED) {
							state = 1; start_idx = i;
						}
						else {
							token_arr[length + count] = token_arr[i];
							count++;
						}
					}
					else { // state == 1
						if (Utility::GetType(token_arr[i], _text) == TokenType::QUOTED) {
							token_arr[length + count].start = token_arr[start_idx].start;
							token_arr[length + count].len = token_arr[i].start - token_arr[start_idx].start + 1;
							count++;

							state = 0;
						}
						else if (Utility::GetType(token_arr[i], _text) == TokenType::BACK_SLUSH) {
							++j;
						}
					}
				}

				if (state == 1) {
					token_arr[length + count].start = token_arr[start_idx].start;
					token_arr[length + count].len = length - 1 - (token_arr[start_idx].start - num); 
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
						if (Utility::GetType(token_arr[i], _text) == TokenType::QUOTED) {
							state = 1; start_idx = i;
						}
						else {
							token_arr[count] = token_arr[i];
							count++;
						}
					}
					else { // state == 1
						if (Utility::GetType(token_arr[i], _text) == TokenType::QUOTED) {
							token_arr[count].start = token_arr[start_idx].start;
							token_arr[count].len = token_arr[i].start - token_arr[start_idx].start + 1;
							count++;

							state = 0;
						}
						else if (Utility::GetType(token_arr[i], _text) == TokenType::BACK_SLUSH) {
							++j;
						}
					}
				}

				if (state == 1) {
					token_arr[count].start = token_arr[start_idx].start;
					token_arr[count].len = length - 1  - (token_arr[start_idx].start - num);
					count++;
				}

				_last_state[0] = state;
				_token_arr_size[0] = count;
			}
		}


		static bool ScanningNew(char* text, int64_t length, const int thr_num,
			Token*& _tokens_orig, int64_t& _tokens_orig_size, std::vector<Token*>& _token_arr, int64_t& _token_arr_size, bool use_simd)
		{
			std::vector<std::thread> thr(thr_num);
			std::vector<int64_t> start(thr_num);
			std::vector<int64_t> last(thr_num);

			
			{
				start[0] = 0;

				for (int i = 1; i < thr_num; ++i) {
					start[i] = length / thr_num * i;

					for (int64_t x = start[i]; x <= length; ++x) {
						if (Utility::isWhitespace(text[x]) || '\0' == text[x] ||
							LoadDataOption::LeftBracket == text[x] || LoadDataOption::RightBracket == text[x] ||
							LoadDataOption::Comma == text[x] ||
							LoadDataOption::LeftBrace == text[x] || LoadDataOption::RightBrace == text[x] ||
							LoadDataOption::Assignment == text[x]) {

							start[i] = x;
							break;
						}
					}
				}
				for (int i = 0; i < thr_num - 1; ++i) {
					last[i] = start[i + 1];
				}

				last[thr_num - 1] = length + 1;
			}
			int64_t real_token_arr_count = 0;
				
			auto a = std::chrono::steady_clock::now();
			Token* tokens_orig = nullptr;
			int64_t now_capacity = 2 * (length + 1 + thr_num);

			if (_tokens_orig) {
				if (now_capacity <= _tokens_orig_size) {
					tokens_orig = _tokens_orig;
				}
				else {
					free(_tokens_orig);

					tokens_orig = (Token*)calloc(2 * (length + 1 + thr_num), sizeof(Token));
					_tokens_orig_size = 2 * (length + 1 + thr_num);
				}
			}
			else {
				tokens_orig = (Token*)calloc(2 * (length + 1 + thr_num), sizeof(Token));
				_tokens_orig_size = 2 * (length + 1 + thr_num);
			}

			if (!tokens_orig) {
				return false;
			}

			std::vector<Token*> tokens(thr_num);
			tokens[0] = tokens_orig;
			for (int64_t i = 1; i < thr_num; ++i) {
				tokens[i] = tokens[i - 1] + 2 * (last[i] - start[i]);
			}
			
			int64_t token_count = 0;

			std::vector<int64_t[2]> token_arr_size(thr_num);
			std::vector<int[2]> last_state(thr_num);
			
			
			for (int i = 0; i < thr_num; ++i) {
				thr[i] = std::thread(_Scanning, text + start[i], start[i], last[i] - start[i], std::ref(tokens[i]), std::ref(token_arr_size[i]), 
					i == thr_num - 1, last_state[i]); 
			}

			for (int i = 0; i < thr_num; ++i) {
				thr[i].join();
			}
			auto b = std::chrono::steady_clock::now();

			{
				int i = 0;
				thr[i] = std::thread(_Scanning2, text + start[i], start[i], last[i] - start[i], std::ref(tokens[i]), token_arr_size[i][0], std::ref(token_arr_size[i]),
					i == thr_num - 1, last_state[i]);
			}

			for (int i = 1; i < thr_num; ++i) {
				thr[i] = std::thread(_Scanning2, text + start[i], start[i], last[i] - start[i], std::ref(tokens[i]), token_arr_size[i][0], std::ref(token_arr_size[i]),
					i == thr_num - 1, last_state[i]);
			}

			for (int i = 0; i < thr_num; ++i) {
				thr[i].join();
			}

			auto c = std::chrono::steady_clock::now();

			int state = last_state[0][0];

			for (int i = 1; i < thr_num; ++i) {
				if (state == 1) {
					auto& sz = token_arr_size[i - 1][0];

					if (sz > 0) {
						auto _start = tokens[i - 1][sz - 1].start;
						sz--;

						{ // state == 1
							tokens[i][0].len = (tokens[i] + last[i] - start[i])[0].start + (tokens[i] + last[i] - start[i])[0].len
								- _start;
							tokens[i][0].start = _start;
						}

						token_arr_size[i][0] = token_arr_size[i][1];
						std::memcpy(tokens[i] + 1, tokens[i] + last[i] - start[i] + 1,
							sizeof(Token) * (token_arr_size[i][1] - 1));
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
					//for (int i = 0; i < token_arr_size[t][0]; ++i) {
					//	Utility::PrintToken(text, tokens[t][i]);

					//	std::cout << "|\n";
					//	getchar();
					//}
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
						else if (Utility::isWhitespace(ch) || '\0' == ch) {
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
			char*& _buffer, int64_t& _buffer_len, Token*& _token_orig, int64_t& _token_orig_len, 
			std::vector<Token*>& _token_arr, int64_t& _token_arr_len, bool use_simd)
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
						//Scanning(buffer, file_length, token_arr, token_arr_size);
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
		bool operator() (const std::string& fileName, int thr_num, std::vector<Token*>& token_arr, int64_t& token_arr_len)
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
				std::vector<Token*> token_arr;

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


