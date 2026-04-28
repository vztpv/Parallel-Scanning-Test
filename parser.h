
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


#include <immintrin.h>  // SSE4.2 / AVX2
#include <iostream>

namespace clau {


	// 구분자 위치를 비트마스크로 뽑아내는 함수 (simdjson stage 1 변형)
	inline uint32_t get_delimiter_mask_avx2(const __m256i chunk) {
		// 1. 따옴표(")와 백슬래시(\) 마스크
		__m256i quote = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('"'));
		__m256i slash = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\\'));

		// 2. 구조적 기호 ({, }, [, ], :, ,) 마스크
		// 기호들의 아스키 코드 규칙성을 이용한 비교 (단순화를 위해 cmpeq 여러 개 사용 가능)
		__m256i lbrace = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('{'));
		__m256i rbrace = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('}'));
		__m256i lbrack = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('['));
		__m256i rbrack = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(']'));
		__m256i colon = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(':'));
		__m256i comma = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(','));

		__m256i struct_chars = _mm256_or_si256(
			_mm256_or_si256(_mm256_or_si256(lbrace, rbrace), _mm256_or_si256(lbrack, rbrack)),
			_mm256_or_si256(colon, comma)
		);

		// 3. 공백 (Space, \t, \n, \r) 마스크 (simdjson의 유명한 최적화 방식)
		__m256i whitespace = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(' '));
		whitespace = _mm256_or_si256(whitespace, _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\n')));
		whitespace = _mm256_or_si256(whitespace, _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\r')));
		whitespace = _mm256_or_si256(whitespace, _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\t')));

		// 4. 모든 구분자를 하나로 합침
		__m256i all_delimiters = _mm256_or_si256(
			_mm256_or_si256(quote, slash),
			_mm256_or_si256(struct_chars, whitespace)
		);

		// 5. 256비트(32바이트) 결과를 32비트 정수형 마스크로 변환
		return _mm256_movemask_epi8(all_delimiters);
	}


	struct Token {
	private:
		uint32_t _start;
		uint32_t _len;
	public:
		uint32_t& start() {
			return _start;
		}
		uint32_t& len() {
			return _len;
		}
		uint32_t start() const {
			return _start;
		}
		uint32_t len() const {
			return _len;
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


		static void skipWhitespace(char* buf, int64_t& token_first, int64_t& token_last) {
			while (isWhitespace(buf[token_first]) && token_first <= token_last) {
				token_first++;
			}
			while (isWhitespace(buf[token_last]) && token_first <= token_last) {
				token_last--;
			}
		}

		// todo - rename.
		__forceinline // ch is no needed..
			static Token Get(int64_t position, int64_t length, const char* ch) {
			Token token;

			//token, text) = TokenType::END;
			token.len() = length;
			token.start() = position;

			return token;
		}

		__forceinline
			static TokenType GetType(const char ch) {
				{
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
						return  TokenType::RIGHT_BRACKET;
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
					case '\"':
						return  TokenType::QUOTED;
						break;

						break;
					}
				}
				return TokenType::STRING;
		}

		static void PrintToken(std::ostream& out, const char* buffer, Token token) {
			if (out) {
				//std::cout << std::string_view(buffer + token.start(), token.len());
				out << std::string_view(buffer + token.start(), token.len());
				//std::cout << Utility::GetIdx(token) << " " << Utility::GetLength(token) << "\n";
				//std::cout << std::string_view(buffer + Utility::GetIdx(token), Utility::GetLength(token));
			}
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

		static void ScanWithSimdJsonStyle(const char* text, int64_t num, int64_t length, Token* token_arr, int64_t& token_arr_size, int64_t* _quoted_count) {
			int64_t i = 0;
			int64_t token_first = 0;
			int64_t token_count = 0;
			int64_t quoted_count = 0;
			int32_t backslash_on = -1;

			auto flush_word = [&](int64_t end_idx) {
				if (end_idx > token_first) {
					token_arr[token_count++] = Utility::Get(token_first + num, end_idx - token_first, text);
				}
				};

			// 32바이트 단위로 처리
			while (i + 32 <= length) {
				__m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));

				// 위에서 정의한 함수로 32비트 마스크 획득
				uint32_t mask = get_delimiter_mask_avx2(chunk);

				// 마스크의 비트가 0이 될 때까지 (구분자가 있는 위치로 점프)
				while (mask != 0) {
					// 가장 낮은 자리에 있는 '1'의 인덱스 추출 (0 ~ 31)
					uint32_t bit_idx = _tzcnt_u32(mask);

					// 이전 backslash check.
					if (backslash_on >= 0) {
						if (i + bit_idx == backslash_on) {
							backslash_on = -1;
							mask = _blsr_u32(mask);
							continue;
						}
						backslash_on = -1;
					}

					// 현재 비트를 0으로 지움 (다음 반복을 위해)
					mask = _blsr_u32(mask);

					int64_t actual_idx = i + bit_idx;
					char ch = text[actual_idx];

					// 1. 구분자 이전까지의 글자들은 '단어(T_WORD)'로 토큰화
					flush_word(actual_idx);

					// 2. 구분자 자체 처리
					if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
						token_first = actual_idx + 1; // 공백은 버림 (또는 토큰화)
					}
					else if (ch == '\\') {
						// 이스케이프 문자 처리 (백슬래시 + 다음 글자 1개)
						//token_arr[token_count++] = Utility::Get(actual_idx, 1, text);
						//if (actual_idx + 1 < length) {
							//token_arr[token_count++] = Utility::Get(actual_idx + 1, 1, text);
						//}

						token_first = actual_idx + 2;

						backslash_on = actual_idx + 1;

						// 만약 이스케이프 문자가 청크 내에 있다면, 마스크에서 다음 비트를 지워야 할 수도 있음
						// (단순화를 위해 이 부분은 스칼라 예외 처리나 마스크 조작이 필요합니다)
					}
					else {
						quoted_count += !(ch - '"');

						// 따옴표나 기호({, }, [, ], :, ,)는 그 자체를 단일 토큰으로 추가
						token_arr[token_count++] = Utility::Get(actual_idx + num, 1, text);
						token_first = actual_idx + 1;
					}
				}

				// 32바이트 청크 안에 구분자가 아예 없는 경우 (mask == 0), 
				// flush하지 않고 그대로 통과하므로 token_first가 유지되어 긴 단어로 묶입니다.
				i += 32;
			}

			// (스칼라 루프) 32바이트로 나누어 떨어지지 않는 나머지 꼬리 부분 처리
			while (i < length) {

				// 기존 작성하신 스칼라 스캐닝 코드 적용...
				{
					char ch = text[i];

					switch (ch) {

					case ' ':
					case '\t':
					case '\r':
					case '\v':
					case '\f':
					case '\n':
						flush_word(i);
						token_first = i + 1;
						break;

					case '"':
						++quoted_count;

						flush_word(i);
						token_arr[token_count++] =
							Utility::Get(i + num, 1, nullptr);
						token_first = i + 1;
						break;
					case ',':
						flush_word(i);
						token_arr[token_count++] =
							Utility::Get(i + num, 1, nullptr);
						token_first = i + 1;
						break;

					case '\\':
					{
						flush_word(i);
						
						//token_arr[token_count++] =
						//		Utility::Get(i + num, 1, nullptr);

						if (i + 1 < length) {
							token_first = i + 2;
							++i;
							//token_arr[token_arr_count++] =
							//	Utility::Get(i + num, 1, p);
						}
						else {
							//token_arr[token_arr_count++] = Utility::Get(i + num, 1, p);

							token_first = i + 1;
						}
					}
					break;

					case LoadDataOption::LeftBrace:
					case LoadDataOption::LeftBracket:
					case LoadDataOption::RightBrace:
					case LoadDataOption::RightBracket:
					case LoadDataOption::Assignment:
						flush_word(i);
						token_arr[token_count++] =
							Utility::Get(i + num, 1, nullptr);
						token_first = i + 1;
						break;
					}
				}
				++i;
			}
			flush_word(length);
			token_arr_size = token_count;
			_quoted_count[0] = quoted_count;
		}

		// _Scanning의 핵심 루프를 교체
		static void _Scanning_SIMD(char* text, int64_t num, const int64_t length,
			Token*& token_arr, std::array<int64_t, 2>& _token_arr_size,
			bool is_last, std::array<int, 2>& _last_state)
		{
			if (length <= 0) { _token_arr_size[0] = 0; return; }

			int64_t token_arr_count = 0;
			int64_t token_first = 0;
			int64_t i = 0;

			// 구분자 집합 (16바이트 패턴으로 로드)
			// PCMPISTRI _SIDD_CMP_EQUAL_ANY 모드에 쓸 needle
			const __m128i delimiters = _mm_setr_epi8(
				' ', '\t', '\r', '\n', '\v', '\f',
				'{', '}', '[', ']', ':', ',', '"', '\\',
				0, 0  // padding
			);
			constexpr int mode = _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_LEAST_SIGNIFICANT;

			auto flush = [&](int64_t end_index) {
				int64_t len = end_index - token_first;
				if (len > 0) {
					token_arr[token_arr_count++] =
						Utility::Get(token_first + num, len, text + token_first);
				}
				};

			// --- SIMD 메인 루프 ---
			while (i + 16 <= length) {
				__m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text + i));

				// chunk 안에 구분자가 하나라도 있는지 빠르게 확인
				if (_mm_cmpistrc(chunk, delimiters, mode)) {
					// 있으면 16바이트 안을 스칼라로 처리
					int64_t end = i + 16;
					while (i < end) {
						char ch = text[i];
						switch (ch) {
						case ' ': case '\t': case '\r': case '\n': case '\v': case '\f':
							flush(i);
							token_first = i + 1;
							break;
						case '"': case ',':
							flush(i);
							token_arr[token_arr_count++] = Utility::Get(i + num, 1, text + i);
							token_first = i + 1;
							break;
						case '\\':
							if (i + 1 < length && (text[i + 1] == '\\' || text[i + 1] == '"')) {
								token_arr[token_arr_count++] = Utility::Get(i + num, 1, text + i);
								++i;
								token_arr[token_arr_count++] = Utility::Get(i + num, 1, text + i);
								token_first = i + 1;
							}
							break;
						case '{': case '[': case '}': case ']': case ':':
							flush(i);
							token_arr[token_arr_count++] = Utility::Get(i + num, 1, text + i);
							token_first = i + 1;
							break;
						}
						++i;
					}
				}
				else {
					// 구분자 없음 → 16바이트 전부 토큰 내부, 건너뜀
					i += 16;
				}
			}

			// --- 나머지 스칼라 처리 ---
			while (i < length) {
				char ch = text[i];
				switch (ch) {
				case ' ': case '\t': case '\r': case '\n': case '\v': case '\f':
					flush(i);
					token_first = i + 1;
					break;
				case '"': case ',':
					flush(i);
					token_arr[token_arr_count++] = Utility::Get(i + num, 1, text + i);
					token_first = i + 1;
					break;
				case '\\':
					if (i + 1 < length && (text[i + 1] == '\\' || text[i + 1] == '"')) {
						token_arr[token_arr_count++] = Utility::Get(i + num, 1, text + i);
						++i;
						token_arr[token_arr_count++] = Utility::Get(i + num, 1, text + i);
						token_first = i + 1;
					}
					break;
				case '{': case '[': case '}': case ']': case ':':
					flush(i);
					token_arr[token_arr_count++] = Utility::Get(i + num, 1, text + i);
					token_first = i + 1;
					break;
				}
				++i;
			}

			flush(length);
			_token_arr_size[0] = token_arr_count;
		}
		static void _Scanning(char* text, int64_t num, const int64_t length,
			Token*& token_arr, std::array<int64_t, 2>& _token_arr_size, bool is_last, std::array<int, 2>& _last_state, 
			int64_t* out_quote_count) {

			if (1) {
				if (length <= 0) {
					_token_arr_size[0] = 0;
					return;
				}

				int64_t token_arr_count = 0;
				int64_t token_first = 0;
				int64_t quote_count = 0;

				char* p = text;
				char* end = text + length;

				// flush helper
				auto flush = [&](int64_t end_index) {
					int64_t len = end_index - token_first;
					if (len > 0) {
						token_arr[token_arr_count++] =
							Utility::Get(token_first + num, len, text + token_first);
					}
					};

				while (p < end) {
					char ch = *p;
					int64_t i = p - text;

					switch (ch) {

					case ' ':
					case '\t':
					case '\r':
					case '\v':
					case '\f':
					case '\n':
						flush(i);
						token_first = i + 1;
						break;

					case '"':
						++quote_count;

						flush(i);
						token_arr[token_arr_count++] =
							Utility::Get(i + num, 1, p);
						token_first = i + 1;
						break;
					case ',':
						flush(i);
						token_arr[token_arr_count++] =
							Utility::Get(i + num, 1, p);
						token_first = i + 1;
						break;

					case '\\':
					{
						flush(i);
						if (p + 1 < end) { // && (p[1] == '\\' || p[1] == '"')) {
							//token_arr[token_arr_count++] =
							//	Utility::Get(i + num, 1, p);

							++p;

							//token_arr[token_arr_count++] =
							//	Utility::Get(i + num, 1, p);

							token_first = i;
						}
						else {
							//token_arr[token_arr_count++] = Utility::Get(i + num, 1, p);
							
							token_first = i;
						}
					}
					break;

					case LoadDataOption::LeftBrace:
					case LoadDataOption::LeftBracket:
					case LoadDataOption::RightBrace:
					case LoadDataOption::RightBracket:
					case LoadDataOption::Assignment:
						flush(i);
						token_arr[token_arr_count++] =
							Utility::Get(i + num, 1, p);
						token_first = i + 1;
						break;
					}

					++p;
				}

				flush(length);
				_token_arr_size[0] = token_arr_count;
				out_quote_count[0] = quote_count;
				return;
			}

			{
				static bool is_space[256] = { false };

				is_space[' '] = true;
				is_space['\t'] = true;
				is_space['\r'] = true;
				is_space['\v'] = true;
				is_space['\f'] = true;
				is_space['\n'] = true;

				if (1) {
					int state = 0; // if state == 1 then  \] or \[ ...

					int64_t token_first = 0;
					int64_t token_last = -1;

					int64_t token_arr_count = 0;

					for (int64_t i = 0; i < length; ++i) {

						const char ch = text[i];

						if (is_space[ch]) {
							token_last = i - 1;
							if (token_last - token_first + 1 > 0) {
								token_arr[token_arr_count] = Utility::Get(token_first + num, token_last - token_first + 1, text + token_first);
								token_arr_count++;
							}
							token_first = i + 1;
							token_last = i + 1;
						}

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


		static void _Scanning2(char* text, int64_t num, const int64_t length,
			Token*& token_arr, int64_t token_arr_size, std::array<int64_t, 1>& _token_arr_size, bool is_last, std::array<int, 1>& _last_state, int id,
			const int64_t quote_count) {

				if (quote_count % 2 == 1) { // start state == 1
					//std::cout << " odd \n";
					auto _text = text - num; // first of total text.
					int state = 1; Token* start_token = token_arr;
					int64_t count = 0;
					Token* token_arr_end = token_arr + token_arr_size;

					for (Token* p = token_arr; p != token_arr_end; ++p) {
						if (state == 0) {
							if (Utility::GetType(_text[p->start()]) == TokenType::QUOTED) {
								state = 1; start_token = p;
							}
							else {
								token_arr[count] = *p;
								count++;
							}
						}
						else { // state == 1
							if (Utility::GetType(_text[p->start()]) == TokenType::QUOTED) {
								token_arr[count].start() = start_token->start();
								token_arr[count].len() = p->start() - start_token->start() + 1;
								count++;

								state = 0;
							}
						}
					}

					if (state == 1) {
						token_arr[count].start() = start_token->start();
						token_arr[count].len() = length - 1 - (start_token->start() - num);
						count++;
					}

					_last_state[0] = state;
					_token_arr_size[0] = count;
				}

				if (quote_count % 2 == 0) {
					//std::cout << " even \n";
					auto _text = text - num;
					int state = 0; Token* start_token = token_arr;
					int64_t count = 0;
					Token* token_arr_end = token_arr + token_arr_size;
					for (Token* p = token_arr; p != token_arr_end; ++p) {
						if (state == 0) {
							if (Utility::GetType(_text[p->start()]) == TokenType::QUOTED) {
								state = 1; start_token = p;
							}
							else {
								token_arr[count] = *p;
								count++;
							}
						}
						else { // state == 1
							if (Utility::GetType(_text[p->start()]) == TokenType::QUOTED) {
								token_arr[count].start() = start_token->start();
								token_arr[count].len() = p->start() - start_token->start() + 1;
								count++;

								state = 0;
							}
						}
					}

					if (state == 1) {
						token_arr[count].start() = start_token->start();
						token_arr[count].len() = length - 1 - (start_token->start() - num);
						count++;
					}

					_last_state[0] = state;
					_token_arr_size[0] = count;
				}
		}


		static bool ScanningNew(char* text, int64_t length, int thr_num,
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
						if (Utility::isWhitespace(text[x]) ||
							LoadDataOption::LeftBracket == text[x] || LoadDataOption::RightBracket == text[x] ||
							LoadDataOption::Comma == text[x] ||
							LoadDataOption::LeftBrace == text[x] || LoadDataOption::RightBrace == text[x] ||
							LoadDataOption::Assignment == text[x]) {

							start[i] = x;
							break;
						}

						if (x == length) { // meet end of text?
							return false; // 
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
			Token* tokens_orig = nullptr;
			int64_t now_capacity = 1 * (length + 1);

			if (_tokens_orig) {
				if (now_capacity <= _tokens_orig_size) {
					tokens_orig = _tokens_orig;
				}
				else {
					free(_tokens_orig);

					tokens_orig = (Token*)calloc(1 * (length + 1), sizeof(Token));
					_tokens_orig_size = 1 * (length + 1);
				}
			}
			else {
				tokens_orig = (Token*)calloc(1 * (length + 1), sizeof(Token));
				_tokens_orig_size = 1 * (length + 1);
			}

			if (!tokens_orig) {
				return false;
			}

			std::vector<Token*> tokens(thr_num);
			tokens[0] = tokens_orig;
			for (int64_t i = 1; i < thr_num; ++i) {
				tokens[i] = tokens[i - 1] + 1 * (last[i - 1] - start[i - 1]);
			}

			int64_t token_count = 0;

			std::vector<std::array<int64_t, 1>> token_arr_size(thr_num);
			std::vector<std::array<int, 1>> last_state(thr_num);
			std::vector<int64_t> quote_count(thr_num, 0);

			for (int i = 0; i < thr_num; ++i) {
				thr[i] = std::thread(ScanWithSimdJsonStyle, text + start[i], start[i], last[i] - start[i], tokens[i], std::ref(token_arr_size[i][0]), &quote_count[i]);
				//thr[i] = std::thread(_Scanning, text + start[i], start[i], last[i] - start[i], std::ref(tokens[i]), std::ref(token_arr_size[i]),
					//i == thr_num - 1, std::ref(last_state[i]), &quote_count[i]);
			}

			for (int i = 0; i < thr_num; ++i) {
				thr[i].join();
			}
			auto b = std::chrono::steady_clock::now();

			{
				int i = 0;
				thr[i] = std::thread(_Scanning2, text + start[i], start[i], last[i] - start[i], std::ref(tokens[i]), std::ref(token_arr_size[i][0]), std::ref(token_arr_size[i]),
					i == thr_num - 1, std::ref(last_state[i]), i, 0);
			}

			for (int i = 1; i < thr_num; ++i) {
				quote_count[i] += quote_count[i - 1];
				thr[i] = std::thread(_Scanning2, text + start[i], start[i], last[i] - start[i], std::ref(tokens[i]), std::ref(token_arr_size[i][0]), std::ref(token_arr_size[i]),
					i == thr_num - 1, std::ref(last_state[i]), i, quote_count[i - 1]);
			}

			// chk quote_count[thr_num - 1] is even!

			for (int i = 0; i < thr_num; ++i) {
				thr[i].join();
			}

			auto c = std::chrono::steady_clock::now();

			int state = quote_count[0] % 2;

			for (int i = 1; i < thr_num; ++i) {
				if (state == 1) {
					auto& sz = token_arr_size[i - 1][0];

					if (sz > 0) {
						auto _start = tokens[i - 1][sz - 1].start();
						sz--;

						{ // state == 1
							tokens[i][0].len() = (tokens[i])[0].start() + (tokens[i])[0].len()
								- _start;
							tokens[i][0].start() = _start;
						}
											
					//	std::memcpy(tokens[i] + 1, tokens[i] + last[i] - start[i] + 1,
					//		sizeof(Token) * (token_arr_size[i][1] - 1));
					}
					else {
						std::cout << "chk ...........1\n";
					}
					
					//token_arr_size[i][0] = token_arr_size[i][1];
				}
				state = quote_count[i] % 2; // last_state[i][state];
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

			if (false) {
				std::ofstream outfile("output.json", std::ios::binary);

				if (outfile) {
					for (int t = 0; t < thr_num; ++t) {
						if (1) {
							for (int i = 0; i < token_arr_size[t][0]; ++i) {
								Utility::PrintToken(outfile, text, tokens[t][i]);
								outfile << "\n";

								//	std::cout << "|\n";
								//	getchar();
							}
						}
						real_token_arr_count += token_arr_size[t][0];
					}
					outfile.close();
				}
			}

			{
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
				int64_t length = _ftelli64(inFile);
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


