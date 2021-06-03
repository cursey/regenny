#include <tao/pegtl.hpp>

#include "Utility.hpp"

namespace address_parser {
using namespace tao::pegtl;

struct HexNum : seq<one<'0'>, one<'x'>, plus<xdigit>> {};
struct DecNum : plus<digit> {};
struct Num : sor<HexNum, DecNum> {};
struct Name : seq<plus<alnum>, opt<one<'.'>, plus<alnum>>> {};
struct ModOffset : seq<one<'<'>, Name, one<'>'>, opt<one<'+'>, Num>> {};
struct Grammar : sor<Num, ModOffset> {};

struct State {
    std::string name{};
    uintptr_t num{};
};

template <typename Rule> struct Action : nothing<Rule> {};
template <> struct Action<Num> {
    template <typename Input> static void apply(const Input& in, State& s) {
        s.num = std::stoull(in.string(), nullptr, 0);
    }
};
template <> struct Action<Name> {
    template <typename Input> static void apply(const Input& in, State& s) { s.name = in.string_view(); }
};
} // namespace address_parser

std::variant<std::monostate, uintptr_t, ModuleOffset> parse_address(const std::string& str) {
    std::variant<std::monostate, uintptr_t, ModuleOffset> result{};

    try {
        address_parser::State s{};
        tao::pegtl::memory_input in{str, ""};

        if (tao::pegtl::parse<address_parser::Grammar, address_parser::Action>(in, s)) {
            if (s.name.empty()) {
                result = s.num;
            } else {
                result = ModuleOffset{s.name, s.num};
            }
        }
    } catch (const tao::pegtl::parse_error& e) {
    }

    return result;
}
