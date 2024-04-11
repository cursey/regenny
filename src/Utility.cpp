#include <tao/pegtl.hpp>

#include "Utility.hpp"

namespace address_parser {
using namespace tao::pegtl;

struct HexNum : seq<one<'0'>, one<'x'>, plus<xdigit>> {};
struct DecNum : plus<digit> {};
struct Num : sor<HexNum, DecNum> {};
struct NameChar : sor<alnum, one<' ', '(', ')', '_', '-', ',', '.', '/', '\\', ':'>> {};
struct Name : seq<plus<NameChar>> {};
struct Offset : seq<Num, opt<one<'-'>, one<'>'>, struct Offset>> {};
struct ModOffset : seq<one<'<'>, Name, one<'>'>, opt<one<'+'>, Offset>> {};
struct Grammar : sor<Offset, ModOffset> {};

template <typename Rule> struct Action : nothing<Rule> {};
template <> struct Action<Num> {
    template <typename Input> static void apply(const Input& in, ParsedAddress& s) {
        try {
            auto num = std::stoull(in.string(), nullptr, 0);
            s.offsets.push_back(num);
        } catch (...) {
            // Failed to convert.
        }
    }
};

template <> struct Action<Name> {
    template <typename Input> static void apply(const Input& in, ParsedAddress& s) { s.name = in.string_view(); }
};

} // namespace address_parser

std::optional<ParsedAddress> parse_address(const std::string& str) {
    try {
        ParsedAddress s{};
        tao::pegtl::memory_input in{str, ""};

        if (tao::pegtl::parse<address_parser::Grammar, address_parser::Action>(in, s)) {
            return s;
        }
    } catch (const tao::pegtl::parse_error& e) {
    }

    return std::nullopt;
}
