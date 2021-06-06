



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

#ifdef USE_SIMD
#include <intrin.h> // windows, todo : linux - x86intrin
#endif

namespace clau {

	struct Token { int64_t start; int64_t len; int64_t type; int64_t next; };

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
		LEFT_BRACE, RIGHT_BRACE, LEFT_BRACKET, RIGHT_BRACKET, ASSIGNMENT, COMMA, 
		BACK_SLUSH, QUOTED,
		STRING, NUMBER, TRUE, FLALSE, _NULL, END
	};


	class Utility {
	private:
		class BomInfo
		{
		public:
			size_t bom_size;
			char seq[5];
		};

		const static size_t BOM_COUNT = 1;


		static const BomInfo bomInfo[1];

	public:
		enum class BomType { UTF_8, ANSI };

		static BomType ReadBom(FILE* file) {
			char btBom[5] = { 0, };
			size_t readSize = fread(btBom, sizeof(char), 5, file);


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

		static BomType ReadBom(const char* contents, size_t length, BomInfo& outInfo) {
			char btBom[5] = { 0, };
			size_t testLength = length < 5 ? length : 5;
			memcpy(btBom, contents, testLength);

			size_t i, j;
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
		static inline bool isWhitespace(const char ch)
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


		static inline int Equal(const int64_t x, const int64_t y)
		{
			if (x == y) {
				return 0;
			}
			return -1;
		}

	public:

		// todo - rename.
		inline static Token Get(int64_t position, int64_t length, const char* ch) {
			Token token;

			token.type = TokenType::END;
			token.len = length;
			token.start = position;

			{
				switch (ch[0]) {
				case LoadDataOption::LeftBrace:
					token.type = TokenType::LEFT_BRACE;
					break;
				case LoadDataOption::RightBrace:
					token.type = TokenType::RIGHT_BRACE;
					break;
				case LoadDataOption::LeftBracket:
					token.type = TokenType::LEFT_BRACKET;
					break;
				case LoadDataOption::RightBracket:
					token.type = TokenType::RIGHT_BRACE;
					break;
				case LoadDataOption::Assignment:
					token.type = TokenType::ASSIGNMENT;
					break;
				case LoadDataOption::Comma:
					token.type = TokenType::COMMA;
					break;
				case '\\':
					if (length == 1) {
						token.type = TokenType::BACK_SLUSH;
					}
					break;
				case 't':
					if (length == 4 && strcmp("true", ch) == 0) {
						token.type = TokenType::TRUE;
					}
					break;
				case 'f':
					if (length == 5 && strcmp("false", ch) == 0) {
						token.type = TokenType::TRUE;
					}
					break;
					break;
				case 'n':
					if (length == 4 && strcmp("null", ch) == 0) {
						token.type = TokenType::TRUE;
					}
					break;
				case '\"':
					token.type = TokenType::QUOTED;
					break;
				}
			}
			return token;
		}

		static void PrintToken(const char* buffer, Token token) {
		//	std::ofstream outfile("output.txt", std::ios::app);

		//std::cout << std::string_view(buffer + token.start, token.len) << "\n";
		//	outfile << std::string_view(buffer + token.start, token.len) << "\n";
			//std::cout << Utility::GetIdx(token) << " " << Utility::GetLength(token) << "\n";
			//std::cout << std::string_view(buffer + Utility::GetIdx(token), Utility::GetLength(token));
		//	outfile.close();
		}
	};

	class InFileReserver
	{
	private:

		/*
		// use simd - experimental..  - has bug : 2020.10.04
		static void _ScanningWithSimd(char* text, int64_t num, const int64_t length,
			int64_t*& token_arr, size_t& _token_arr_size) {

			size_t token_arr_size = 0;

			{
				int state = 0;

				int64_t token_first = 0;
				int64_t token_last = -1;

				size_t token_arr_count = 0;

				int64_t _i = 0;

#ifdef USE_SIMD
				__m256i temp;
				__m256i _1st, _2nd, _3rd, _4th, _5th, _6th, _7th, _8th, _9th, _10th, _11th, _12th, _13th;

				char ch1 = '\"';
				_1st = _mm256_set_epi8(ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1,
					ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1);
				char ch2 = '\\';
				_2nd = _mm256_set_epi8(ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2,
					ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2);
				char ch3 = '\n';
				_3rd = _mm256_set_epi8(ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3,
					ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3);
				char ch4 = '\0';
				_4th = _mm256_set_epi8(ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4,
					ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4);
				char ch5 = '#';
				_5th = _mm256_set_epi8(ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5,
					ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5);
				char ch6 = ' ';
				_6th = _mm256_set_epi8(ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6,
					ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6);
				char ch7 = '\t';
				_7th = _mm256_set_epi8(ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7,
					ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7);
				char ch8 = '\r';
				_8th = _mm256_set_epi8(ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8,
					ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8);
				char ch9 = '\v';
				_9th = _mm256_set_epi8(ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9,
					ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9);
				char ch10 = '\f';
				_10th = _mm256_set_epi8(ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10,
					ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10);

				char ch11 = '{';
				_11th = _mm256_set_epi8(ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11,
					ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11);

				char ch12 = '}';
				_12th = _mm256_set_epi8(ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12,
					ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12);

				char ch13 = '=';
				_13th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
					ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);



				__m256i mask1, mask2, mask3, mask4, mask5;
				int val = -7; // 111
				mask1 = _mm256_set_epi8(val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val,
					val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val);

				val = -2; // 010
				mask2 = _mm256_set_epi8(val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val,
					val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val);

				val = -5; // 101
				mask3 = _mm256_set_epi8(val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val,
					val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val);

				val = -10; // 1010
				mask4 = _mm256_set_epi8(val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val,
					val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val);

				val = -15; // 1111
				mask5 = _mm256_set_epi8(val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val,
					val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val);

				for (; _i + 32 < length; _i = _i + 32) {
					temp = _mm256_setr_epi8(text[_i], text[_i + 1], text[_i + 2], text[_i + 3], text[_i + 4], text[_i + 5], text[_i + 6], text[_i + 7],
						text[_i + 8], text[_i + 9], text[_i + 10], text[_i + 11], text[_i + 12], text[_i + 13], text[_i + 14], text[_i + 15], text[_i + 16],
						text[_i + 17], text[_i + 18], text[_i + 19], text[_i + 20], text[_i + 21], text[_i + 22], text[_i + 23], text[_i + 24], text[_i + 25],
						text[_i + 26], text[_i + 27], text[_i + 28], text[_i + 29], text[_i + 30], text[_i + 31]);

					__m256i x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13;

					x1 = _mm256_cmpeq_epi8(temp, _1st);
					x2 = _mm256_cmpeq_epi8(temp, _2nd);
					x3 = _mm256_cmpeq_epi8(temp, _3rd);
					x4 = _mm256_cmpeq_epi8(temp, _4th);
					x5 = _mm256_cmpeq_epi8(temp, _5th);
					x6 = _mm256_cmpeq_epi8(temp, _6th);
					x7 = _mm256_cmpeq_epi8(temp, _7th);
					x8 = _mm256_cmpeq_epi8(temp, _8th);
					x9 = _mm256_cmpeq_epi8(temp, _9th);
					x10 = _mm256_cmpeq_epi8(temp, _10th);
					x11 = _mm256_cmpeq_epi8(temp, _11th);
					x12 = _mm256_cmpeq_epi8(temp, _12th);
					x13 = _mm256_cmpeq_epi8(temp, _13th);

					x1 = _mm256_blendv_epi8(x1, mask5, x1);
					x2 = _mm256_blendv_epi8(x2, mask4, x2);
					x3 = _mm256_blendv_epi8(x3, mask5, x3);
					x4 = _mm256_blendv_epi8(x4, mask5, x4);
					x5 = _mm256_blendv_epi8(x5, mask5, x5);
					x6 = _mm256_blendv_epi8(x6, mask3, x6);
					x7 = _mm256_blendv_epi8(x7, mask3, x7);
					x8 = _mm256_blendv_epi8(x8, mask3, x8);
					x9 = _mm256_blendv_epi8(x9, mask3, x9);
					x10 = _mm256_blendv_epi8(x10, mask3, x10);
					x11 = _mm256_blendv_epi8(x11, mask1, x11);
					x12 = _mm256_blendv_epi8(x12, mask1, x12);
					x13 = _mm256_blendv_epi8(x13, mask1, x13);


					x1 = _mm256_add_epi8(x1, x2);
					x3 = _mm256_add_epi8(x3, x4);
					x5 = _mm256_add_epi8(x5, x6);
					x7 = _mm256_add_epi8(x7, x8);
					x9 = _mm256_add_epi8(x9, x10);
					x11 = _mm256_add_epi8(x11, x12);

					x1 = _mm256_add_epi8(x1, x3);
					x5 = _mm256_add_epi8(x5, x7);
					x9 = _mm256_add_epi8(x9, x11);

					x1 = _mm256_add_epi8(x1, x5);
					x9 = _mm256_add_epi8(x9, x13);

					x1 = _mm256_add_epi8(x1, x9);

					int start = 0;
					int r = _mm256_movemask_epi8(x1);

					while (r != 0) {
						{
							int a = _tzcnt_u32(r); //

							r = r & (r - 1);

							start = a;

							{
								const int64_t i = _i + start;

								if (((-x1.m256i_i8[start]) & 0b100) != 0) {
									token_last = i - 1;
									if (token_last - token_first + 1 > 0) {
										token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
										token_arr_count++;
									}

									token_first = i;
									token_last = i;
								}
								if (((-x1.m256i_i8[start]) & 0b010) != 0) {
									{//
										if (((-x1.m256i_i8[start]) & 0b1000) != 0) {
											token_arr[num + token_arr_count] = 1;
										}
										else {
											token_arr[num + token_arr_count] = 0;
										}
										const char ch = text[i];
										token_arr[num + token_arr_count] += Utility::Get(i + num, 1, ch);
										token_arr_count++;
									}
								}
								if (((-x1.m256i_i8[start]) & 0b001) != 0) {
									token_first = i + 1;
									token_last = i + 1;
								}


								continue;
							}
						}
					}
				}

#endif

				//default?
				for (; _i < length; _i = _i + 1) {
					int64_t i = _i;
					const char ch = text[i];

					switch (ch) {
					case '\"':
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}

						token_first = i;
						token_last = i;

						token_first = i + 1;
						token_last = i + 1;

						{//
							token_arr[num + token_arr_count] = 1;
							token_arr[num + token_arr_count] += Utility::Get(i + num, 1, ch);
							token_arr_count++;
						}
						break;
					case '\\':
					{//
						token_arr[num + token_arr_count] = 1;
						token_arr[num + token_arr_count] += Utility::Get(i + num, 1, ch);
						token_arr_count++;
					}
					break;
					case '\n':
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i + 1;
						token_last = i + 1;

						{//
							token_arr[num + token_arr_count] = 1;
							token_arr[num + token_arr_count] += Utility::Get(i + num, 1, ch);
							token_arr_count++;
						}
						break;
					case '\0':
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i + 1;
						token_last = i + 1;

						{//
							token_arr[num + token_arr_count] = 1;
							token_arr[num + token_arr_count] += Utility::Get(i + num, 1, ch);
							token_arr_count++;
						}
						break;
					case LoadDataOption::LineComment:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i + 1;
						token_last = i + 1;

						{//
							token_arr[num + token_arr_count] = 1;
							token_arr[num + token_arr_count] += Utility::Get(i + num, 1, ch);
							token_arr_count++;
						}

						break;
					case ' ':
					case '\t':
					case '\r':
					case '\v':
					case '\f':
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i + 1;
						token_last = i + 1;

						break;
					case LoadDataOption::Left:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}

						token_first = i;
						token_last = i;

						token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					case LoadDataOption::Right:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i;
						token_last = i;

						token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					case LoadDataOption::Assignment:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i;
						token_last = i;

						token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					}

				}

				if (length - 1 - token_first + 1 > 0) {
					token_arr[num + token_arr_count] = Utility::Get(token_first + num, length - 1 - token_first + 1, text + token_first);
					token_arr_count++;
				}
				token_arr_size = token_arr_count;
			}

			{
				_token_arr_size = token_arr_size;
			}
		}
		*/
		static inline int f(Token* tokens) {
			return tokens->start - (tokens - 1)->start - (tokens - 1)->len;
		}

		static void _Scanning(char* text, int64_t num, const int64_t length,
			Token*& token_arr, size_t& _token_arr_size) {

			size_t token_arr_size = 0;

			{
				int state = 0;

				int64_t token_first = 0;
				int64_t token_last = -1;

				size_t token_arr_count = 0;

				for (int64_t i = 0; i < length; ++i) {

					const char ch = text[i];

					switch (ch) {
					case '\"':
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}

						token_first = i;
						token_last = i;

						token_first = i + 1;
						token_last = i + 1;

						{//
							token_arr[num + token_arr_count] = Utility::Get(i + num, 1, text + i);
							token_arr_count++;
						}
						break;	
					case ',':
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
								token_arr_count++;
							}

							token_first = i;
							token_last = i;

							token_first = i + 1;
							token_last = i + 1;

							{//
								token_arr[num + token_arr_count] = Utility::Get(i + num, 1, text + i);
								token_arr_count++;
							}
							break;
					case '\\':
					{//
						token_arr[num + token_arr_count] = Utility::Get(i + num, 1, text + i);
						token_arr_count++;
					}
					break;
					case '\n':
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i + 1;
						token_last = i + 1;

						{//
							token_arr[num + token_arr_count] = Utility::Get(i + num, 1, text + i);
							token_arr_count++;
						}
						break;
					case '\0':
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i + 1;
						token_last = i + 1;

						{//
							token_arr[num + token_arr_count] = Utility::Get(i + num, 1, text + i);
							token_arr_count++;
						}
						break;

					case ' ':
					case '\t':
					case '\r':
					case '\v':
					case '\f':
					{
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i + 1;
						token_last = i + 1;

					}
					break;
					case LoadDataOption::LeftBrace:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}

						token_first = i;
						token_last = i;

						token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					case LoadDataOption::LeftBracket:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}

						token_first = i;
						token_last = i;

						token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					case LoadDataOption::RightBrace:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i;
						token_last = i;

						token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					case LoadDataOption::RightBracket:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i;
						token_last = i;

						token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					case LoadDataOption::Assignment:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i;
						token_last = i;

						token_arr[num + token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					}

				}

				if (length - 1 - token_first + 1 > 0) {
					token_arr[num + token_arr_count] = Utility::Get(token_first + num, length - 1 - token_first + 1, text + token_first);
					token_arr_count++;
				}
				token_arr_size = token_arr_count;
			}

			{
				_token_arr_size = token_arr_size;
			}
		}


		static void ScanningNew(char* text, const size_t length, const int thr_num,
			Token*& _token_arr, size_t& _token_arr_size, bool use_simd)
		{
			std::vector<std::thread> thr(thr_num);
			std::vector<size_t> start(thr_num);
			std::vector<size_t> last(thr_num);

			{
				start[0] = 0;

				for (int i = 1; i < thr_num; ++i) {
					start[i] = length / thr_num * i;

					for (size_t x = start[i]; x <= length; ++x) {
						if (Utility::isWhitespace(text[x]) || '\0' == text[x] ||
							LoadDataOption::LeftBracket == text[x] || LoadDataOption::RightBracket == text[x] ||
							LoadDataOption::LeftBrace == text[x] || LoadDataOption::RightBrace == text[x] || LoadDataOption::Assignment == text[x]) {
							start[i] = x;
							break;
						}
					}
				}
				for (int i = 0; i < thr_num - 1; ++i) {
					last[i] = start[i + 1];
					for (size_t x = last[i]; x <= length; ++x) {
						if (Utility::isWhitespace(text[x]) || '\0' == text[x] ||
							LoadDataOption::LeftBracket == text[x] || LoadDataOption::RightBracket == text[x] ||
							LoadDataOption::LeftBrace == text[x] || LoadDataOption::RightBrace == text[x] || LoadDataOption::Assignment == text[x]) {
							last[i] = x;
							break;
						}
					}
				}

				last[thr_num - 1] = length + 1;
			}
			size_t real_token_arr_count = 0;

			size_t tokens_max = (32 + (length + 1) / thr_num) * thr_num;

			Token* tokens = (Token*)calloc(length + 1, sizeof(Token));

			int64_t token_count = 0;

			std::vector<size_t> token_arr_size(thr_num);
			auto a = std::chrono::steady_clock::now();
			for (int i = 0; i < thr_num; ++i) {
				thr[i] = std::thread(_Scanning, text + start[i], start[i], last[i] - start[i], std::ref(tokens), std::ref(token_arr_size[i]));
			}

			for (int i = 0; i < thr_num; ++i) {
				thr[i].join();
			}
			auto b = std::chrono::steady_clock::now();

#ifdef USE_SIMD

			int idx = -1;

			int state = 0;
			int start_idx = -1;

			__m256i _1st, _2nd, _3rd;

			char ch1 = (char)TokenType::BACK_SLUSH;
			_1st = _mm256_set_epi8(ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1,
				ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1);
			char ch2 = (char)TokenType::QUOTED;
			_2nd = _mm256_set_epi8(ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2,
				ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2);
			char ch3 = 1;
			_3rd = _mm256_set_epi8(ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3,
				ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3);

			for (size_t t = 0; t < thr_num; ++t) {
				for (size_t j = 0; j < token_arr_size[t]; j = j + 32) {
					__m256i _slush, _q, _even;
					int slush, even = 0, odd = 0, s, es, ec, ece, od1, os, oc, oce, od2,
						od, q;

					const int i = start[t] + j;

					_slush = _mm256_setr_epi8(tokens[i].type, tokens[i + 1].type, tokens[i + 2].type, tokens[i + 3].type, tokens[i + 4].type, tokens[i + 5].type, tokens[i + 6].type
						, tokens[i + 7].type, tokens[i + 8].type, tokens[i + 9].type, tokens[i + 10].type, tokens[i + 11].type, tokens[i + 12].type, tokens[i + 13].type, tokens[i + 14].type
						, tokens[i + 15].type, tokens[i + 16].type, tokens[i + 17].type, tokens[i + 18].type, tokens[i + 19].type, tokens[i + 20].type, tokens[i + 21].type, tokens[i + 22].type
						, tokens[i + 23].type, tokens[i + 24].type, tokens[i + 25].type, tokens[i + 26].type, tokens[i + 27].type, tokens[i + 28].type, tokens[i + 29].type, tokens[i + 30].type
						, tokens[i + 31].type);

					_q = _slush;

					slush = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_slush, _1st));
					q = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_q, _2nd));

					_even = _mm256_setr_epi8(f(&tokens[i]), f(&tokens[i + 1]), f(&tokens[i + 2]), f(&tokens[i + 3]), f(&tokens[i + 4]), f(&tokens[i + 5]), f(&tokens[i + 6])
						, f(&tokens[i + 7]), f(&tokens[i + 8]), f(&tokens[i + 9]), f(&tokens[i + 10]), f(&tokens[i + 11]), f(&tokens[i + 12]), f(&tokens[i + 13]), f(&tokens[i + 14])
						, f(&tokens[i + 15]), f(&tokens[i + 16]), f(&tokens[i + 17]), f(&tokens[i + 18]), f(&tokens[i + 19]), f(&tokens[i + 20]), f(&tokens[i + 21]), f(&tokens[i + 22])
						, f(&tokens[i + 23]), f(&tokens[i + 24]), f(&tokens[i + 25]), f(&tokens[i + 26]), f(&tokens[i + 27]), f(&tokens[i + 28]), f(&tokens[i + 29]), f(&tokens[i + 30])
						, f(&tokens[i + 31]));
					even = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_q, _3rd));

					odd = ~even;
					s = slush & (~(slush >> 1));
					es = s & even;
					ec = slush + es;
					ece = ec & (~slush);
					od1 = ece & (~slush);
					
					os = s & odd;
					oc = slush + os;
					oce = oc & (~slush);
					od2 = oce * even;

					od = od1 | od2;

					q = q & (~od);

					int x = q;

					int itemp = 0;

					while (x != 0) {
						//  here  " ~
						if (0 == state) {
							idx = _tzcnt_u64(x);

							for (int k = itemp; k < idx; ++k) {
								tokens[real_token_arr_count] = tokens[i + k];
								real_token_arr_count++;
							}

							start_idx = i + idx;
							state = 1;
						}
						// " here ~
						else {
							idx = _tzcnt_u64(x);
							
							Token temp = tokens[start_idx];
							temp.len = tokens[i + idx].start - temp.start + 1;

							tokens[real_token_arr_count] = temp;
							real_token_arr_count++;

							itemp = idx + 1;

							state = 0;
						}
						
						x = x & (x - 1);
					}
				}
			}
#endif

			auto c = std::chrono::steady_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(b - a);
			auto dur2 = std::chrono::duration_cast<std::chrono::milliseconds>(c - b);

			std::cout << dur.count() << "ms\n";
			std::cout << dur2.count() << "ms\n";

			for (int i = 0; i < real_token_arr_count; ++i) {
				Utility::PrintToken(text, tokens[i]);
			}

			{
				_token_arr = tokens;
				_token_arr_size = real_token_arr_count;
			}
		}

		static std::pair<bool, int> Scan(FILE* inFile, int thr_num,
			char*& _buffer, size_t* _buffer_len, Token*& _token_arr, size_t* _token_arr_len, bool use_simd)
		{
			if (inFile == nullptr) {
				return { false, 0 };
			}

			int64_t* arr_count = nullptr; //
			size_t arr_count_size = 0;

			std::string temp;
			char* buffer = nullptr;
			size_t file_length;

			{
				fseek(inFile, 0, SEEK_END);
				size_t length = ftell(inFile);
				fseek(inFile, 0, SEEK_SET);

				Utility::BomType x = Utility::ReadBom(inFile);

				//	clau_parser11::Out << "length " << length << "\n";
				if (x == Utility::BomType::UTF_8) {
					length = length - 3;
				}

				file_length = length;
				buffer = new char[file_length + 1]; // 

				int a = clock();
				// read data as a block:
				fread(buffer, sizeof(char), file_length, inFile);
				int b = clock();
				std::cout << b - a << " " << file_length <<"\n";

				buffer[file_length] = '\0';

				{
					Token* token_arr;
					size_t token_arr_size;

					{
						ScanningNew(buffer, file_length, thr_num, token_arr, token_arr_size, use_simd);
					}

					_buffer = buffer;
					_token_arr = token_arr;
					*_token_arr_len = token_arr_size;
					*_buffer_len = file_length;
				}
			}

			return{ true, 1 };
		}

	private:
		FILE* pInFile;
		bool use_simd;
	public:
		explicit InFileReserver(FILE* inFile, bool use_simd)
		{
			pInFile = inFile;
			this->use_simd = use_simd;
		}
	public:
		bool operator() (int thr_num, char*& buffer, size_t* buffer_len, Token*& token_arr, size_t* token_arr_len)
		{
			bool x = Scan(pInFile, thr_num, buffer, buffer_len, token_arr, token_arr_len, use_simd).second > 0;

			return x;
		}
	};
	class LoadData
	{

	public:
		static bool LoadDataFromFile(const std::string& fileName, int lex_thr_num = 1, int parse_thr_num = 1, bool use_simd = false) /// global should be empty
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
			FILE* inFile;

#ifdef _WIN32 
			fopen_s(&inFile, fileName.c_str(), "rb");
#else
			inFile = fopen(fileName.c_str(), "rb");
#endif

			if (!inFile)
			{
				return false;
			}

			try {

				InFileReserver ifReserver(inFile, use_simd);
				char* buffer = nullptr;
				size_t buffer_len, token_arr_len;
				Token* token_arr;

				ifReserver(lex_thr_num, buffer, &buffer_len, token_arr, &token_arr_len);


				int b = clock();
				
				std::cout << b - a << "ms\n";

				delete[] buffer;
				free(token_arr);

				fclose(inFile);
				
			}
			catch (const char* err) { std::cout << err << "\n"; fclose(inFile); return false; }
			catch (const std::string& e) { std::cout << e << "\n"; fclose(inFile); return false; }
			catch (const std::exception& e) { std::cout << e.what() << "\n"; fclose(inFile); return false; }
			catch (...) { std::cout << "not expected error" << "\n"; fclose(inFile); return false; }

			return true;
		}

	};
}



#endif



