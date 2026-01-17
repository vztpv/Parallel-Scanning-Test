
#include "mimalloc-new-delete.h"


#include <iostream>



#include "parser.h"

#include <coroutine>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <future>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace clau_test {

    inline bool isspace(int x) {
        return x == ' ' || x == '\t' || x == '\r' || x == '\n';
    }

    // ----- Generator (토큰/Chunk 생산자) -----
    template <typename T>
    struct Generator {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        struct promise_type {
            std::optional<T> current_value;
            auto get_return_object() { return Generator{ handle_type::from_promise(*this) }; }
            std::suspend_always initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            std::suspend_always yield_value(T value) {
                current_value = std::move(value);
                return {};
            }
            void return_void() {}
            void unhandled_exception() { std::exit(1); }
        };

        handle_type coro;
        Generator(handle_type h) : coro(h) {}
        ~Generator() { if (coro) coro.destroy(); }

        std::optional<T> next() {
            if (!coro.done()) coro.resume();
            return coro.done() ? std::nullopt : coro.promise().current_value;
        }
    };

    struct Wrap {
        char buf[4096];
        uint64_t len;
    };

    Generator<Wrap> Read(const char* fileName) {
        std::ifstream file;
        uint64_t len = 0;
        file.open(fileName, std::ios::binary);
        if (!file) {
            co_return; // 파일 열기 실패
        }

        while (true) {
            Wrap w{};
            file.read(w.buf, sizeof(w.buf));
            std::streamsize n = file.gcount(); // 실제 읽은 바이트 수
            if (n <= 0) {
                break; // EOF
            }
            w.len = static_cast<uint64_t>(n);
            co_yield w;
        }
    }

    // ----- 토큰 타입 -----
    struct Token {
        int x;
    };

    // ----- 개선된 토크나이저 (바로 토큰 묶음 생성) -----
    Generator<std::vector<Token>> tokenize_chunks(Generator<Wrap>& input) {
        std::vector<Token> current_chunk;
        current_chunk.reserve(10240);
        std::string current_token;
        int token_count = 0;

        while (auto x = input.next()) {
            for (char c : std::string_view(x->buf, x->len)) {
                if (c == '{' || c == '}' || c == '[' || c == ']' ||
                    c == ':' || c == ',') {

                    // 현재 토큰이 있다면 먼저 추가
                    if (!current_token.empty()) {
                        current_chunk.push_back(Token{ current_token[0] });
                        current_token.clear();
                        token_count++;
                    }

                    // 구분자 토큰 추가
                    current_chunk.push_back(Token{ c });
                    token_count++;

                    // 콤마를 만나고 충분한 토큰이 쌓였다면 청크 생성
                    if (c == ',' && token_count > 10240) {
                        if (!current_chunk.empty()) {
                            co_yield current_chunk;
                            current_chunk.clear();
                            current_chunk.reserve(10240);
                            token_count = 0;
                        }
                    }
                }
                else if (!isspace(c)) {
                    if (current_token.empty()) {
                        current_token.push_back(c);
                    }
                }
            }
        }

        // 마지막 토큰 처리
        if (!current_token.empty()) {
            current_chunk.push_back(Token{ current_token[0] });
        }

        // 마지막 청크 출력
        if (!current_chunk.empty()) {
            co_yield current_chunk;
        }
    }

    // ----- 비동기 파싱 워커 -----
    std::string parse_subtree(const std::vector<Token>& tokens) {
        std::string result;
        return result;
    }

    // ----- Merger (비동기 태스크 실행 + 결과 합치기) -----
    void merger(Generator<std::vector<Token>>& chunks) {
        std::vector<std::future<std::string>> tasks;

        int count = 0;
        while (auto chunk = chunks.next()) {
            //tasks.push_back(std::async(std::launch::async, [chunk]() {
             //   return parse_subtree(*chunk);
              //  }));
            ++count;
        }

        // 결과 합성 (여기서는 단순 출력)
        std::cout << "=== AST Results ===\n";
        for (auto& t : tasks) {
            t.get();
        }

        std::cout << count << "\n";
    }

    void run() {
        int a = clock();
        auto read_gen = Read("citylots.json");
        auto chunk_gen = tokenize_chunks(read_gen);  // dispatcher 단계 제거
        merger(chunk_gen);

        int b = clock();
        std::cout << b - a << "ms\n";
    }

}

int main(void)
{
    clau_test::run();

    //return 0;

	clau::LoadData test;

	for (int i = 0; i < 3; ++i) {
		int a = clock();
		test.LoadDataFromFile("citylots.json", 0, 0, true);
		int b = clock();

		std::cout << "test end " << b - a << "ms\n";
	}
	return 0;
}
