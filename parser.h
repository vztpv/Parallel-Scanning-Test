



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


#include <intrin.h> // windows, todo : linux - x86intrin


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

			//token, text) = TokenType::END;
			token.len = length;
			token.start = position;

			
			return token;
		}

		inline static TokenType GetType(Token token, const char* buf) {
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

			std::cout << std::string_view(buffer + token.start, token.len) << "\t";
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


		// use simd - experimental..  - has bug : 2020.10.04
		static void _ScanningWithSimd(char* text, int64_t num, const int64_t length,
			Token*& token_arr, size_t& _token_arr_size, int*& save_r) {

			size_t token_arr_size = 0;

			if (_token_arr_size == 0) {
				//save_r = (int*)calloc(length, sizeof(int));
				token_arr = (Token*)calloc(length, sizeof(Token));
				Token buf[1024];
				int state = 0;

				int64_t token_first = 0;
				int64_t token_last = -1;

				size_t token_arr_count = 0;

				int64_t _i = 0;

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
				char ch5 = ' ';
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

					//	token_arr[num + _i].remain = r;
					//	save_r[_i] = r;

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
										buf[token_arr_count & 1023] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
										token_arr_count++;
										if ((token_arr_count & 1023) == 1023) {
											memcpy(token_arr + token_arr_count - 1023, buf, 1024);
										}
									}

									token_first = i;
									token_last = i;
								}
								if (((-x1.m256i_i8[start]) & 0b010) != 0) {
									{//
									//	if (((-x1.m256i_i8[start]) & 0b1000) != 0) {
									//		token_arr[token_arr_count] = 1;
									//	}
									//	else {
									//		token_arr[token_arr_count] = 0;
									//	}
										const char& ch = text[i];
										buf[token_arr_count & 1023] = Utility::Get(i + num, 1, &ch);
										token_arr_count++;
										if ((token_arr_count & 1023) == 1023) {
											memcpy(token_arr + token_arr_count - 1023, buf, 1024);
										}
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

				//default?
				for (; _i < length; _i = _i + 1) {
					int64_t i = _i;
					const char& ch = text[i];

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
							token_arr[token_arr_count] = Utility::Get(i + num, 1, &ch);
							token_arr_count++;
						}
						break;
					case '\\':
					{//
						token_arr[token_arr_count] = Utility::Get(i + num, 1, &ch);
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
							token_arr[token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i + 1;
						token_last = i + 1;

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
					case LoadDataOption::Comma:
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
				token_arr_size = token_arr_count;
			}

			{
				_token_arr_size = token_arr_size;
			}
		}

		static inline int f(Token* tokens) {
			return tokens->start - (tokens - 1)->start - (tokens - 1)->len;
		}

		static size_t _Scanning(const char* text, int64_t num, const int64_t length,
			Token*& token_arr, size_t& _token_arr_size) {

			int64_t count = 0;

			int64_t _i = 0;

			__m256i temp;
			__m256i _1st, _2nd, _3rd, _4th, _5th, _6th, _7th, _8th, _9th, _10th{}, _11th{}, _12th{}, _13th{}, _14th{}, _15th{}, _16th{}, _17th{}, _18th{}, _19th{}, _20th{}, _21th, _22th;

			char ch1 = '\"';
			_1st = _mm256_set_epi8(ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1,
				ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1, ch1);
			char ch2 = '\\';
			_2nd = _mm256_set_epi8(ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2,
				ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2, ch2);
			char ch3 = '{';
			_3rd = _mm256_set_epi8(ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3,
				ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3, ch3);
			char ch4 = '[';
			_4th = _mm256_set_epi8(ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4,
				ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4, ch4);
			char ch5 = '}';
			_5th = _mm256_set_epi8(ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5,
				ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5, ch5);
			char ch6 = ']';
			_6th = _mm256_set_epi8(ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6,
				ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6, ch6);

			char ch7 = 't';
			_7th = _mm256_set_epi8(ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7,
				ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7, ch7);
			char ch8 = 'f';
			_8th = _mm256_set_epi8(ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8,
				ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8, ch8);
			char ch9 = 'n';
			_9th = _mm256_set_epi8(ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9,
				ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9, ch9);
			/*
			char ch10 = '-';
			_10th = _mm256_set_epi8(ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10,
				ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10, ch10);

			char ch11 = '0';
			_11th = _mm256_set_epi8(ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11,
				ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11, ch11);

			char ch12 = '1';
			_12th = _mm256_set_epi8(ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12,
				ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12, ch12);

			char ch13 = '2';
			_13th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);

			ch13 = '3';
			_14th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);
			ch13 = '4';
			_15th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);
			ch13 = '5';
			_16th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);
			ch13 = '6';
			_17th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);
			ch13 = '7';
			_18th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);
			ch13 = '8';
			_19th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);
			ch13 = '9';
			_20th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);
			*/
			char ch13 = ':';
			_21th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);
			ch13 = ',';
			_22th = _mm256_set_epi8(ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13,
				ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13, ch13);

			for (; _i + 32 < length; _i = _i + 32) {
				temp = _mm256_setr_epi8(text[_i], text[_i + 1], text[_i + 2], text[_i + 3], text[_i + 4], text[_i + 5], text[_i + 6], text[_i + 7],
					text[_i + 8], text[_i + 9], text[_i + 10], text[_i + 11], text[_i + 12], text[_i + 13], text[_i + 14], text[_i + 15], text[_i + 16],
					text[_i + 17], text[_i + 18], text[_i + 19], text[_i + 20], text[_i + 21], text[_i + 22], text[_i + 23], text[_i + 24], text[_i + 25],
					text[_i + 26], text[_i + 27], text[_i + 28], text[_i + 29], text[_i + 30], text[_i + 31]);

				__m256i x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22;

				x1 = _mm256_cmpeq_epi8(temp, _1st);
				x2 = _mm256_cmpeq_epi8(temp, _2nd);
				x3 = _mm256_cmpeq_epi8(temp, _3rd);
				x4 = _mm256_cmpeq_epi8(temp, _4th);
				x5 = _mm256_cmpeq_epi8(temp, _5th);
				x6 = _mm256_cmpeq_epi8(temp, _6th);
				x7 = _mm256_cmpeq_epi8(temp, _7th);
				x8 = _mm256_cmpeq_epi8(temp, _8th);
				x9 = _mm256_cmpeq_epi8(temp, _9th);
				x21 = _mm256_cmpeq_epi8(temp, _21th);
				x22 = _mm256_cmpeq_epi8(temp, _22th);



				x1 = _mm256_add_epi8(x1, x2);
				x3 = _mm256_add_epi8(x3, x4);
				x5 = _mm256_add_epi8(x5, x6);
				x7 = _mm256_add_epi8(x7, x8);
				x7 = _mm256_add_epi8(x7, x9);
				x21 = _mm256_add_epi8(x21, x22);


				x1 = _mm256_add_epi8(x1, x3);
				x5 = _mm256_add_epi8(x5, x7);
				x5 = _mm256_add_epi8(x5, x21);

				x1 = _mm256_add_epi8(x1, x5);

				int start = 0;
				int r = _mm256_movemask_epi8(x1);

				TokenType before_type = TokenType::END;

				while (r != 0) {
					{
						int a = _tzcnt_u32(r); //

						r = r & (r - 1);

						start = a;

						const int64_t i = _i + start;



						token_arr[count] = Utility::Get(i, 1, &text[i]);
						++count;
					}
				}
			}

			if (_i < length) {
				char buf[32] = { 0 };
				memcpy(buf, text + _i, length - _i);
				auto& text = buf;
				temp = _mm256_setr_epi8(text[0], text[0 + 1], text[0 + 2], text[0 + 3], text[0 + 4], text[0 + 5], text[0 + 6], text[0 + 7],
					text[0 + 8], text[0 + 9], text[0 + 10], text[0 + 11], text[0 + 12], text[0 + 13], text[0 + 14], text[0 + 15], text[0 + 16],
					text[0 + 17], text[0 + 18], text[0 + 19], text[0 + 20], text[0 + 21], text[0 + 22], text[0 + 23], text[0 + 24], text[0 + 25],
					text[0 + 26], text[0 + 27], text[0 + 28], text[0 + 29], text[0 + 30], text[0 + 31]);

				__m256i x1, x2, x3, x4, x5, x6, x7, x8;

				x1 = _mm256_cmpeq_epi8(temp, _1st);
				x2 = _mm256_cmpeq_epi8(temp, _2nd);
				x3 = _mm256_cmpeq_epi8(temp, _3rd);
				x4 = _mm256_cmpeq_epi8(temp, _4th);
				x5 = _mm256_cmpeq_epi8(temp, _5th);
				x6 = _mm256_cmpeq_epi8(temp, _6th);
				x7 = _mm256_cmpeq_epi8(temp, _21th);
				x8 = _mm256_cmpeq_epi8(temp, _22th);

				x1 = _mm256_add_epi8(x1, x2);
				x3 = _mm256_add_epi8(x3, x4);
				x5 = _mm256_add_epi8(x5, x6);
				x7 = _mm256_add_epi8(x7, x8);

				x1 = _mm256_add_epi8(x1, x3);
				x5 = _mm256_add_epi8(x5, x7);


				x1 = _mm256_add_epi8(x1, x5);

				int start = 0;
				int r = _mm256_movemask_epi8(x1);

				while (r != 0) {
					{
						int a = _tzcnt_u32(r); //

						r = r & (r - 1);

						start = a;

						const int64_t i = _i + start;

						token_arr[count] = Utility::Get(i, 1, &text[start]);
						++count;
					}
				}
			}
			_token_arr_size = count;

			return 0;
		}

		static void _Scanning_old(char* text, int64_t num, const int64_t length,
			Token*& token_arr, size_t& _token_arr_size, bool is_last) {

			size_t token_arr_size = 0;
			token_arr = (Token*)calloc(length, sizeof(Token));
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
						if (i < length && (text[i + 1] == '\\') || text[i + 1] == '\"') {
							token_arr[token_arr_count] = Utility::Get(i + num, 1, text + i);
							token_arr_count++;
							++i;
							token_arr[token_arr_count] = Utility::Get(i + num, 1, text + i);
							token_arr_count++;

							token_first = i + 1;
							token_last = i + 1;
						}
						else if (is_last == false && (text[i + 1] == '\\') || text[i + 1] == '\"') {
							token_arr[token_arr_count] = Utility::Get(i + num, 1, text + i);
							token_arr_count++;
							++i;
							token_arr[token_arr_count] = Utility::Get(i + num, 1, text + i);
							token_arr_count++;

							token_first = i + 1;
							token_last = i + 1;
						}
						else if (is_last && i == length - 1) {
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

						token_arr[ token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					case LoadDataOption::LeftBracket:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[ token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}

						token_first = i;
						token_last = i;

						token_arr[ token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
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

						token_arr[ token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
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

						token_arr[ token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					case LoadDataOption::Assignment:
						token_last = i - 1;
						if (token_last - token_first + 1 > 0) {
							token_arr[ token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
							token_arr_count++;
						}
						token_first = i;
						token_last = i;

						token_arr[ token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
						token_arr_count++;

						token_first = i + 1;
						token_last = i + 1;
						break;
					}

				}

				if (length - 1 - token_first + 1 > 0) {
					token_arr[ token_arr_count] = Utility::Get(token_first + num, length - 1 - token_first + 1, text + token_first);
					token_arr_count++;
				}
				token_arr_size = token_arr_count;
			}

			{
				_token_arr_size = token_arr_size;
			}
		}


		static void ScanningNew(char* text, size_t length, const int thr_num,
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

			std::vector<Token*> tokens(thr_num); // (Token*)calloc(length + 1, sizeof(Token));

			int64_t token_count = 0;

			std::vector<size_t> token_arr_size(thr_num);
			std::vector<int*> save_r(thr_num);
			auto a = std::chrono::steady_clock::now();
			for (int i = 0; i < thr_num; ++i) {
				thr[i] = std::thread(_Scanning_old, text + start[i], start[i], last[i] - start[i], std::ref(tokens[i]), std::ref(token_arr_size[i]), i == thr_num - 1); // , std::ref(save_r[i]));
			}

			for (int i = 0; i < thr_num; ++i) {
				thr[i].join();
			}
			auto b = std::chrono::steady_clock::now();

			uint64_t sum = 0;
			for (int i = 0; i < thr_num; ++i) {
				sum += token_arr_size[i];
			}

			Token* real_tokens = (Token*)calloc(sum, sizeof(Token));

			int idx = -1;

			int state = 0;
			int start_idx = -1;
			
			for (size_t t = 0; t < thr_num; ++t) {
				size_t j = 0;
				for (; j < token_arr_size[t]; ++j) {
					size_t i = j;

					if (state == 0) {
						if (Utility::GetType(tokens[t][i], text) == TokenType::QUOTED) {
							state = 1; start_idx = i;
						}
						else {
							real_tokens[real_token_arr_count] = tokens[t][i];
							real_token_arr_count++;
						}
					}
					else { // state == 1
						if (Utility::GetType(tokens[t][i], text) == TokenType::QUOTED) {
							tokens[t][start_idx].len = tokens[t][i].start - tokens[t][start_idx].start + 1;
							real_tokens[real_token_arr_count] = tokens[t][start_idx];
							real_token_arr_count++;
							state = 0;
						}
						else if (Utility::GetType(tokens[t][i], text) == TokenType::BACK_SLUSH) {
							++j;
						}
					}
				}

			}
			auto c = std::chrono::steady_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(b - a);
			auto dur2 = std::chrono::duration_cast<std::chrono::milliseconds>(c - b);

			std::cout << dur.count() << "ms\n";
			std::cout << dur2.count() << "ms\n";

			//for (int i = 0; i < real_token_arr_count; ++i) {
			//	Utility::PrintToken(text, real_tokens[i]);
				//getchar();
			//}

			{
				for (int i = 0; i < thr_num; ++i) {
					free(tokens[i]);
				}
				_token_arr = real_tokens;
				_token_arr_size = real_token_arr_count;
			}
		}

		static void Scanning(char* text, const size_t length,
			Token*& _token_arr, size_t& _token_arr_size) {

			Token* token_arr = (Token*)calloc(length + 1, sizeof(Token));
			size_t token_arr_size = 0;

			{
				int state = 0;

				int64_t token_first = 0;
				int64_t token_last = -1;

				size_t token_arr_count = 0;

				for (size_t i = 0; i <= length; ++i) {
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
				std::cout << b - a << " " << file_length << "\n";
				fclose(inFile);
				buffer[file_length] = '\0';

				{
					Token* token_arr = nullptr;
					size_t token_arr_size;

					{
						ScanningNew(buffer, file_length, thr_num, token_arr, token_arr_size, use_simd);
						//Scanning(buffer, file_length, token_arr, token_arr_size);
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
				Token* token_arr = nullptr;

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


