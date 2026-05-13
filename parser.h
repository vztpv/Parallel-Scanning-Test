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
#include <chrono>       // 추가: std::chrono 사용
#include <algorithm>
#include <utility>
#include <thread>
#include <cstdlib>      // calloc, free
#include <new>          // std::nothrow
#include <ctime>        // clock()
#include <string_view>  // std::string_view

#include <immintrin.h>  // SSE4.2 / AVX2

// ══════════════════════════════════════════════════════════════════
//  크로스플랫폼 호환 레이어 (Windows MSVC ↔ Linux GCC/Clang)
// ══════════════════════════════════════════════════════════════════

// ── 1. __forceinline ─────────────────────────────────────────────
#ifndef _MSC_VER
#define __forceinline __attribute__((always_inline)) inline
#endif

// ── 2. 64비트 파일 오프셋 ────────────────────────────────────────
#ifdef _MSC_VER
    // MSVC: _ftelli64 내장
#define CLAU_FTELL64(f) _ftelli64(f)
#else
    // POSIX: -D_FILE_OFFSET_BITS=64 이면 ftello가 64비트
#include <sys/types.h>
#define CLAU_FTELL64(f) static_cast<int64_t>(ftello(f))
#endif

// ── 3. fopen (보안 버전) ──────────────────────────────────────────
#ifdef _MSC_VER
#define CLAU_FOPEN(fp, name, mode) fopen_s(&(fp), (name), (mode))
#else
#define CLAU_FOPEN(fp, name, mode) ((fp) = fopen((name), (mode)))
#endif

// ── 4. BSF / bit 조작 인트린식 ───────────────────────────────────
//  _tzcnt_u32  : 최하위 '1' 비트의 위치
//  _blsr_u32   : 최하위 '1' 비트를 0으로 지움  (x & (x-1))
#ifdef _MSC_VER
#include <intrin.h>
// MSVC: _tzcnt_u32 / _blsr_u32 모두 immintrin.h에 포함 → 추가 정의 불필요
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang: immintrin.h → bmiintrin.h 를 통해 이미 정의됨.
    // 단, 컴파일러가 BMI 인트린식을 지원하지 않는 희귀 환경을 위한 순수-C 폴백.
#ifndef __bmiintrin_h
namespace clau_compat {
    inline uint32_t tzcnt32(uint32_t x) { return static_cast<uint32_t>(__builtin_ctz(x)); }
    inline uint32_t blsr32(uint32_t x) { return x & (x - 1u); }
}
#define _tzcnt_u32(x) clau_compat::tzcnt32(x)
#define _blsr_u32(x)  clau_compat::blsr32(x)
#endif
#endif

// ════════════════════════════════════════════════════════════════

namespace clau {

    // 구분자 위치를 비트마스크로 뽑아내는 함수 (simdjson stage 1 변형)
    inline uint32_t get_delimiter_mask_avx2(const __m256i chunk) {
        __m256i quote = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('"'));
        __m256i slash = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\\'));

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

        __m256i whitespace = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(' '));
        whitespace = _mm256_or_si256(whitespace, _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\n')));
        whitespace = _mm256_or_si256(whitespace, _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\r')));
        whitespace = _mm256_or_si256(whitespace, _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\t')));

        __m256i all_delimiters = _mm256_or_si256(
            _mm256_or_si256(quote, slash),
            _mm256_or_si256(struct_chars, whitespace)
        );

        return static_cast<uint32_t>(_mm256_movemask_epi8(all_delimiters));
    }

    using Token = uint32_t;

    namespace LoadDataOption {
        constexpr char LeftBrace = '{';
        constexpr char RightBrace = '}';
        constexpr char LeftBracket = '[';
        constexpr char RightBracket = ']';
        constexpr char Assignment = ':';
        constexpr char Comma = ',';
    }

    enum TokenType {
        LEFT_BRACE, RIGHT_BRACE, LEFT_BRACKET, RIGHT_BRACKET,
        ASSIGNMENT, COMMA, COLON,
        BACK_SLUSH, QUOTED,
        STRING, NUMBER, TRUE, FALSE, _NULL, END
    };


    class Utility {
    private:
        class BomInfo {
        public:
            int64_t bom_size;
            char seq[5];
        };

        static const int64_t BOM_COUNT = 1;
        static const BomInfo bomInfo[1];

    public:
        enum class BomType { UTF_8, ANSI };

        static BomType ReadBom(FILE* file) {
            char btBom[5] = { 0 };
            int64_t readSize = static_cast<int64_t>(fread(btBom, sizeof(char), 5, file));

            if (0 == readSize) {
                clearerr(file);
                fseek(file, 0, SEEK_SET);
                return BomType::ANSI;
            }

            BomInfo stBom = { 0 };
            BomType type = ReadBom(btBom, readSize, stBom);

            if (type == BomType::ANSI) {
                clearerr(file);
                fseek(file, 0, SEEK_SET);
                return BomType::ANSI;
            }

            clearerr(file);
            fseek(file, static_cast<long>(stBom.bom_size * sizeof(char)), SEEK_SET);
            return type;
        }

        static BomType ReadBom(const char* contents, int64_t length, BomInfo& outInfo) {
            char btBom[5] = { 0 };
            int64_t testLength = length < 5 ? length : 5;
            memcpy(btBom, contents, static_cast<size_t>(testLength));

            for (int64_t i = 0; i < BOM_COUNT; ++i) {
                const BomInfo& bom = bomInfo[i];
                if (bom.bom_size > testLength) continue;

                bool matched = true;
                for (int64_t j = 0; j < bom.bom_size; ++j) {
                    if (bom.seq[j] != btBom[j]) { matched = false; break; }
                }
                if (!matched) continue;

                outInfo = bom;
                return static_cast<BomType>(i);
            }
            return BomType::ANSI;
        }

    public:
        static __forceinline bool isWhitespace(const char ch) {
            switch (ch) {
            case ' ': case '\t': case '\r': case '\n': case '\v': case '\f':
                return true;
            }
            return false;
        }

        static __forceinline int Equal(const int64_t x, const int64_t y) {
            return (x == y) ? 0 : -1;
        }

        static void skipWhitespace(char* buf, int64_t& token_first, int64_t& token_last) {
            while (token_first <= token_last && isWhitespace(buf[token_first])) token_first++;
            while (token_first <= token_last && isWhitespace(buf[token_last]))  token_last--;
        }

        static __forceinline Token Get(int64_t position, int64_t /*length*/, const char* /*ch*/) {
            return static_cast<Token>(position);
        }

        static __forceinline TokenType GetType(const char ch) {
            switch (ch) {
            case LoadDataOption::LeftBrace:    return TokenType::LEFT_BRACE;
            case LoadDataOption::RightBrace:   return TokenType::RIGHT_BRACE;
            case LoadDataOption::LeftBracket:  return TokenType::LEFT_BRACKET;
            case LoadDataOption::RightBracket: return TokenType::RIGHT_BRACKET;
            case LoadDataOption::Assignment:   return TokenType::ASSIGNMENT;
            case LoadDataOption::Comma:        return TokenType::COMMA;
            case '\\':                         return TokenType::BACK_SLUSH;
            case '\"':                         return TokenType::QUOTED;
            default:                           return TokenType::STRING;
            }
        }

        static void PrintToken(std::ostream& out, const char* buffer, const Token& token) {
            if (out) {
                uint32_t len = *((&token) + 1) - token;
                out << std::string_view(buffer + token, len);
            }
        }
    };

    // BomInfo 정의 (UTF-8 BOM: EF BB BF)
    inline const Utility::BomInfo Utility::bomInfo[1] = {
        { 3, { '\xEF', '\xBB', '\xBF', 0, 0 } }
    };

    inline uint8_t char_to_token_type[256];


    class InFileReserver {
    private:
        char* buffer = nullptr;
        int64_t buffer_len = 0;
        Token* token_orig = nullptr;
        int64_t token_orig_len = 0;

    public:
        ~InFileReserver() {
            delete[] buffer;
            free(token_orig);
        }

    private:
        InFileReserver(const InFileReserver&) = delete;
        InFileReserver& operator=(const InFileReserver&) = delete;

        // ── Stage 1: AVX2로 토큰 후보 추출 ────────────────────────────
        static void ScanWithSimdJsonStyle(const char* text, int64_t num, int64_t length,
            Token* token_arr, int64_t& token_arr_size,
            int64_t* _quoted_count)
        {
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

            while (i + 32 <= length) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                uint32_t mask = get_delimiter_mask_avx2(chunk);

                while (mask != 0) {
                    uint32_t bit_idx = _tzcnt_u32(mask);

                    if (backslash_on >= 0) {
                        if (i + static_cast<int64_t>(bit_idx) == backslash_on) {
                            backslash_on = -1;
                            mask = _blsr_u32(mask);
                            continue;
                        }
                        backslash_on = -1;
                    }

                    mask = _blsr_u32(mask);

                    int64_t actual_idx = i + static_cast<int64_t>(bit_idx);
                    char ch = text[actual_idx];

                    flush_word(actual_idx);

                    if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
                        token_first = actual_idx + 1;
                    }
                    else if (ch == '\\') {
                        token_first = actual_idx;
                        backslash_on = static_cast<int32_t>(actual_idx + 1);
                    }
                    else {
                        quoted_count += !(ch - '"');
                        token_arr[token_count++] = Utility::Get(actual_idx + num, 1, text);
                        token_first = actual_idx + 1;
                    }
                }
                i += 32;
            }

            // 나머지 스칼라 처리
            while (i < length) {
                char ch = text[i];
                switch (ch) {
                case ' ': case '\t': case '\r': case '\v': case '\f': case '\n':
                    flush_word(i);
                    token_first = i + 1;
                    break;
                case '"':
                    ++quoted_count;
                    flush_word(i);
                    token_arr[token_count++] = Utility::Get(i + num, 1, nullptr);
                    token_first = i + 1;
                    break;
                case ',':
                    flush_word(i);
                    token_arr[token_count++] = Utility::Get(i + num, 1, nullptr);
                    token_first = i + 1;
                    break;
                case '\\':
                    flush_word(i);
                    token_first = i;
                    break;
                case LoadDataOption::LeftBrace:  case LoadDataOption::LeftBracket:
                case LoadDataOption::RightBrace: case LoadDataOption::RightBracket:
                case LoadDataOption::Assignment:
                    flush_word(i);
                    token_arr[token_count++] = Utility::Get(i + num, 1, nullptr);
                    token_first = i + 1;
                    break;
                }
                ++i;
            }
            flush_word(length);
            token_arr_size = token_count;
            _quoted_count[0] = quoted_count;
        }

        // ── Stage 1 (SSE4.2 경로) ──────────────────────────────────────
        static void _Scanning_SIMD(char* text, int64_t num, const int64_t length,
            Token*& token_arr,
            std::array<int64_t, 2>& _token_arr_size,
            bool /*is_last*/, std::array<int, 2>& /*_last_state*/)
        {
            if (length <= 0) { _token_arr_size[0] = 0; return; }

            int64_t token_arr_count = 0;
            int64_t token_first = 0;
            int64_t i = 0;

            const __m128i delimiters = _mm_setr_epi8(
                ' ', '\t', '\r', '\n', '\v', '\f',
                '{', '}', '[', ']', ':', ',', '"', '\\',
                0, 0
            );
            constexpr int mode = _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_LEAST_SIGNIFICANT;

            auto flush = [&](int64_t end_index) {
                int64_t len = end_index - token_first;
                if (len > 0)
                    token_arr[token_arr_count++] =
                    Utility::Get(token_first + num, len, text + token_first);
                };

            while (i + 16 <= length) {
                __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text + i));

                if (_mm_cmpistrc(chunk, delimiters, mode)) {
                    int64_t end = i + 16;
                    while (i < end) {
                        char ch = text[i];
                        switch (ch) {
                        case ' ': case '\t': case '\r': case '\n': case '\v': case '\f':
                            flush(i); token_first = i + 1; break;
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
                    i += 16;
                }
            }

            while (i < length) {
                char ch = text[i];
                switch (ch) {
                case ' ': case '\t': case '\r': case '\n': case '\v': case '\f':
                    flush(i); token_first = i + 1; break;
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

        // ── Stage 1 (스칼라 경로) ──────────────────────────────────────
        static void _Scanning(char* text, int64_t num, const int64_t length,
            Token*& token_arr,
            std::array<int64_t, 2>& _token_arr_size,
            bool /*is_last*/, std::array<int, 2>& /*_last_state*/,
            int64_t* out_quote_count)
        {
            if (length <= 0) { _token_arr_size[0] = 0; return; }

            int64_t token_arr_count = 0;
            int64_t token_first = 0;
            int64_t quote_count = 0;

            char* p = text;
            char* end = text + length;

            auto flush = [&](int64_t end_index) {
                int64_t len = end_index - token_first;
                if (len > 0)
                    token_arr[token_arr_count++] =
                    Utility::Get(token_first + num, len, text + token_first);
                };

            while (p < end) {
                char    ch = *p;
                int64_t i = p - text;

                switch (ch) {
                case ' ': case '\t': case '\r': case '\v': case '\f': case '\n':
                    flush(i); token_first = i + 1; break;

                case '"':
                    ++quote_count;
                    flush(i);
                    token_arr[token_arr_count++] = Utility::Get(i + num, 1, p);
                    token_first = i + 1;
                    break;

                case ',':
                    flush(i);
                    token_arr[token_arr_count++] = Utility::Get(i + num, 1, p);
                    token_first = i + 1;
                    break;

                case '\\':
                    flush(i);
                    if (p + 1 < end) {
                        ++p;
                        token_first = i;
                    }
                    else {
                        token_first = i;
                    }
                    break;

                case LoadDataOption::LeftBrace:  case LoadDataOption::LeftBracket:
                case LoadDataOption::RightBrace: case LoadDataOption::RightBracket:
                case LoadDataOption::Assignment:
                    flush(i);
                    token_arr[token_arr_count++] = Utility::Get(i + num, 1, p);
                    token_first = i + 1;
                    break;
                }
                ++p;
            }

            flush(length);
            _token_arr_size[0] = token_arr_count;
            out_quote_count[0] = quote_count;
        }

        // ── Stage 2: 따옴표 쌍 병합 ────────────────────────────────────
        static void _Scanning2(char* text, int64_t start, int64_t /*length*/,
            Token*& token_arr, int64_t token_arr_size,
            std::array<int64_t, 1>& _token_arr_size,
            bool /*is_last*/, std::array<int, 1>& _last_state,
            int /*id*/, const int64_t quote_count)
        {
            auto _text = text - start;
            int   state = (quote_count % 2 == 1) ? 1 : 0;
            Token* start_token = token_arr;
            int64_t count = 0;
            Token* token_arr_end = token_arr + token_arr_size;

            for (Token* p = token_arr; p != token_arr_end; ++p) {
                if (state == 0) {
                    if (Utility::GetType(_text[*p]) == TokenType::QUOTED) {
                        state = 1; start_token = p;
                    }
                    else if (Utility::GetType(_text[*p]) == TokenType::BACK_SLUSH) {
                        // error
                    }
                    else {
                        token_arr[count++] = *p;
                    }
                }
                else {
                    if (Utility::GetType(_text[*p]) == TokenType::QUOTED) {
                        token_arr[count++] = *start_token;
                        state = 0;
                    }
                }
            }

            if (state == 1) {
                token_arr[count++] = *start_token;
            }

            _last_state[0] = state;
            _token_arr_size[0] = count;
        }

        // ── 병렬 스캐닝 메인 ───────────────────────────────────────────
        static bool ScanningNew(char* text, int64_t length, int thr_num,
            Token*& _tokens_orig, int64_t& _tokens_orig_size,
            std::vector<Token*>& _token_arr, int64_t& _token_arr_size,
            bool /*use_simd*/)
        {
            // 청크 경계 계산
            std::vector<int64_t> start(thr_num);
            std::vector<int64_t> last(thr_num);

            start[0] = 0;
            for (int i = 1; i < thr_num; ++i) {
                start[i] = length / thr_num * i;
                for (int64_t x = start[i]; x <= length; ++x) {
                    if (Utility::isWhitespace(text[x]) ||
                        LoadDataOption::LeftBracket == text[x] ||
                        LoadDataOption::RightBracket == text[x] ||
                        LoadDataOption::Comma == text[x] ||
                        LoadDataOption::LeftBrace == text[x] ||
                        LoadDataOption::RightBrace == text[x] ||
                        LoadDataOption::Assignment == text[x]) {
                        start[i] = x; break;
                    }
                    if (x == length) return false;
                }
            }

            // 중복 제거
            {
                std::set<int64_t> _set(start.begin(), start.end());
                thr_num = static_cast<int>(_set.size());
                start.clear();
                for (auto x : _set) start.push_back(x);
                last.resize(thr_num);
            }
            for (int i = 0; i < thr_num - 1; ++i) last[i] = start[i + 1];
            last[thr_num - 1] = length;

            // 토큰 버퍼 확보
            int64_t now_capacity = length + thr_num + 1;
            Token* tokens_orig = nullptr;

            if (_tokens_orig) {
                if (_tokens_orig_size >= now_capacity) {
                    tokens_orig = _tokens_orig;
                }
                else {
                    free(_tokens_orig);
                    tokens_orig = static_cast<Token*>(calloc(static_cast<size_t>(now_capacity), sizeof(Token)));
                    _tokens_orig_size = now_capacity;
                }
            }
            else {
                tokens_orig = static_cast<Token*>(calloc(static_cast<size_t>(now_capacity), sizeof(Token)));
                _tokens_orig_size = now_capacity;
            }
            if (!tokens_orig) return false;

            std::vector<Token*> tokens(thr_num);
            tokens[0] = tokens_orig;
            for (int64_t i = 1; i < thr_num; ++i)
                tokens[i] = tokens[i - 1] + (last[i - 1] - start[i - 1]);

            std::vector<std::array<int64_t, 1>> token_arr_size(thr_num);
            std::vector<std::array<int, 1>>     last_state(thr_num);
            std::vector<int64_t>                quote_count(thr_num, 0);

            // ── Stage 1 병렬 ─────────────────────────────────────────────
            {
                auto a = std::chrono::steady_clock::now();
                std::vector<std::thread> thr(thr_num);
                for (int i = 0; i < thr_num; ++i) {
                    thr[i] = std::thread(ScanWithSimdJsonStyle,
                        text + start[i], start[i], last[i] - start[i],
                        tokens[i], std::ref(token_arr_size[i][0]), &quote_count[i]);
                }
                for (int i = 0; i < thr_num; ++i) thr[i].join();

                auto b = std::chrono::steady_clock::now();
                std::cout << "토큰 후보 배열 구성(parallel) \t"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count()
                    << "ms\n";
            }

            // ── Stage 2 병렬 ─────────────────────────────────────────────
            {
                auto a = std::chrono::steady_clock::now();
                std::vector<std::thread> thr(thr_num);

                thr[0] = std::thread(_Scanning2,
                    text + start[0], start[0], last[0] - start[0],
                    std::ref(tokens[0]), token_arr_size[0][0],
                    std::ref(token_arr_size[0]),
                    0 == thr_num - 1, std::ref(last_state[0]), 0, int64_t(0));

                for (int i = 1; i < thr_num; ++i) {
                    quote_count[i] += quote_count[i - 1];
                    thr[i] = std::thread(_Scanning2,
                        text + start[i], start[i], last[i] - start[i],
                        std::ref(tokens[i]), token_arr_size[i][0],
                        std::ref(token_arr_size[i]),
                        i == thr_num - 1, std::ref(last_state[i]), i,
                        quote_count[i - 1]);
                }
                for (int i = 0; i < thr_num; ++i) thr[i].join();

                auto b = std::chrono::steady_clock::now();
                std::cout << "토큰 배열 구성(parallel) \t"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count()
                    << "ms\n";
            }

            // ── 청크 간 state 연결 (sequential) ──────────────────────────
            {
                auto a = std::chrono::steady_clock::now();

                int state = static_cast<int>(quote_count[0] % 2);
                for (int i = 1; i < thr_num; ++i) {
                    if (state == 1) {
                        auto& sz = token_arr_size[i - 1][0];
                        if (sz > 0) {
                            auto _start = tokens[i - 1][sz - 1];
                            sz--;
                            tokens[i][0] = _start;
                        }
                        else {
                            std::cout << "chk ...........1\n";
                        }
                    }
                    state = static_cast<int>(quote_count[i] % 2);
                }

                auto b = std::chrono::steady_clock::now();
                std::cout << "state에 맞게 연결?(sequential) \t"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count()
                    << "ms\n";
                std::cout << "state is " << state << "\n";
            }

            // 센티넬(다음 토큰 시작 위치) 설정
            int64_t real_token_arr_count = 0;
            for (int t = 0; t < thr_num; ++t) real_token_arr_count += token_arr_size[t][0];

            for (int t = 0; t < thr_num; ++t) {
                if (t < thr_num - 1 && token_arr_size[t][0] > 0)
                    tokens[t][token_arr_size[t][0]] = tokens[t + 1][0];
                else if (token_arr_size[t][0] > 0)
                    tokens[t][token_arr_size[t][0]] = static_cast<Token>(length);
            }

            _token_arr = tokens;
            _token_arr_size = real_token_arr_count;
            _tokens_orig = tokens_orig;
            return true;
        }

        // ── 단일 스레드 스캐너 (Scanning / Scanning2) ─────────────────
        static void Scanning(char* text, const int64_t length,
            Token*& _token_arr, int64_t& _token_arr_size)
        {
            Token* token_arr = static_cast<Token*>(calloc(static_cast<size_t>(length + 1), sizeof(Token)));
            int64_t token_arr_count = 0;
            int     state = 0;
            int64_t token_first = 0;
            int64_t token_last = -1;

            for (int64_t i = 0; i <= length; ++i) {
                const char ch = text[i];

                if (state == 0) {
                    if ('"' == ch) {
                        token_last = i - 1;
                        if (token_last - token_first + 1 > 0)
                            token_arr[token_arr_count++] = Utility::Get(token_first, token_last - token_first + 1, text);
                        token_first = i; token_last = i; state = 1;
                    }
                    else if (Utility::isWhitespace(ch)) {
                        token_last = i - 1;
                        if (token_last - token_first + 1 > 0)
                            token_arr[token_arr_count++] = Utility::Get(token_first, token_last - token_first + 1, text);
                        token_first = token_last = i + 1;
                    }
                    else if (LoadDataOption::LeftBrace == ch || LoadDataOption::LeftBracket == ch ||
                        LoadDataOption::RightBrace == ch || LoadDataOption::RightBracket == ch ||
                        LoadDataOption::Assignment == ch || LoadDataOption::Comma == ch) {
                        token_last = i - 1;
                        if (token_last - token_first + 1 > 0)
                            token_arr[token_arr_count++] = Utility::Get(token_first, token_last - token_first + 1, text);
                        token_arr[token_arr_count++] = Utility::Get(i, 1, text);
                        token_first = token_last = i + 1;
                    }
                }
                else if (state == 1) {
                    if ('\\' == ch) {
                        state = 2;
                    }
                    else if ('"' == ch) {
                        token_last = i;
                        token_arr[token_arr_count++] = Utility::Get(token_first, token_last - token_first + 1, text);
                        token_first = token_last = i + 1;
                        state = 0;
                    }
                }
                else if (state == 2) {
                    state = 1;
                }
            }

            if (length - 1 - token_first + 1 > 0)
                token_arr[token_arr_count++] = Utility::Get(token_first, length - 1 - token_first + 1, text);

            _token_arr = token_arr;
            _token_arr_size = token_arr_count;
        }

        // ── 파일 로드 & 스캔 ───────────────────────────────────────────
        static std::pair<bool, int> Scan(FILE* inFile, int thr_num,
            char*& _buffer, int64_t& _buffer_len,
            Token*& _token_orig, int64_t& _token_orig_len,
            std::vector<Token*>& _token_arr, int64_t& _token_arr_len,
            bool use_simd)
        {
            if (!inFile) return { false, 0 };

            fseek(inFile, 0, SEEK_END);
            int64_t file_length = CLAU_FTELL64(inFile);
            fseek(inFile, 0, SEEK_SET);

            if (Utility::ReadBom(inFile) == Utility::BomType::UTF_8)
                file_length -= 3;

            char* buffer = nullptr;
            if (_buffer && _buffer_len >= file_length) {
                buffer = _buffer;
            }
            else {
                delete[] _buffer; _buffer = nullptr;
                buffer = new (std::nothrow) char[file_length + 1];
                _buffer = buffer;
            }

            if (!buffer) { fclose(inFile); return { false, 1 }; }

            int a = clock();
            fread(buffer, sizeof(char), static_cast<size_t>(file_length), inFile);
            int b = clock();
            std::cout << "load file \t" << b - a << "ms \tfile size " << file_length << "\n";
            fclose(inFile);
            buffer[file_length] = '\0';

            int64_t token_arr_size = 0;
            ScanningNew(buffer, file_length, thr_num,
                _token_orig, _token_orig_len,
                _token_arr, token_arr_size, use_simd);

            _buffer = buffer;
            _buffer_len = file_length;
            _token_arr_len = token_arr_size;

            return { true, 1 };
        }

    public:
        explicit InFileReserver() = default;

        bool operator()(const std::string& fileName, int thr_num,
            std::vector<Token*>& token_arr, int64_t& token_arr_len)
        {
            // Windows / POSIX 공용 fopen
            FILE* inFile = nullptr;
            CLAU_FOPEN(inFile, fileName.c_str(), "rb");
            if (!inFile) return false;

            return Scan(inFile, thr_num,
                buffer, buffer_len,
                token_orig, token_orig_len,
                token_arr, token_arr_len, false).second > 0;
        }
    };


    class LoadData {
    private:
        InFileReserver ifReserver;
    public:
        LoadData() = default;

        bool LoadDataFromFile(const std::string& fileName,
            int lex_thr_num = 1,
            int parse_thr_num = 1,
            bool use_simd = false)
        {
            if (lex_thr_num <= 0)
                lex_thr_num = static_cast<int>(std::thread::hardware_concurrency());
            if (lex_thr_num <= 0) lex_thr_num = 1;

            if (parse_thr_num <= 0)
                parse_thr_num = static_cast<int>(std::thread::hardware_concurrency());
            if (parse_thr_num <= 0) parse_thr_num = 1;

            int a = clock();
            try {
                int64_t token_arr_len = 0;
                std::vector<Token*> token_arr;
                ifReserver(fileName, lex_thr_num, token_arr, token_arr_len);
                int b = clock();
                std::cout << b - a << "ms\n";
            }
            catch (const char* err) { std::cout << err << "\n";       return false; }
            catch (const std::string& e) { std::cout << e << "\n";         return false; }
            catch (const std::exception& e) { std::cout << e.what() << "\n";  return false; }
            catch (...) { std::cout << "unexpected error\n"; return false; }

            return true;
        }
    };

} // namespace clau

#endif // PARSER_H
