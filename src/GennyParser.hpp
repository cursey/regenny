// SdkGenny - Genny.hpp - A single file header framework for generating C++ compatible SDKs
// https://github.com/cursey/sdkgenny
// GennyParser.hpp is an optional extra for SdkGenny that parses .genny formatted files into a usable genny::Sdk.

#pragma once

#include <cstdlib>
#include <optional>

#include <tao/pegtl.hpp>

#include "Genny.hpp"

namespace genny::parser {
using namespace tao::pegtl;

struct ShortComment : disable<two<'/'>, until<eolf>> {};
struct LongComment : disable<one<'/'>, one<'*'>, until<seq<one<'*'>, one<'/'>>>> {};
struct Comment : sor<ShortComment, LongComment> {};
struct Sep : sor<space, Comment> {};
struct Seps : star<Sep> {};

struct Endl : star<one<';'>> {};

struct HexNum : seq<one<'0'>, one<'x'>, plus<xdigit>> {};
struct DecNum : plus<digit> {};
struct Num : sor<HexNum, DecNum> {};

struct Metadata : plus<not_one<',', ']'>> {};
struct MetadataDecl : if_must<two<'['>, Seps, list<Metadata, one<','>, Sep>, Seps, two<']'>> {};

struct IncludeId : TAO_PEGTL_STRING("#include") {};
struct IncludePath : plus<not_one<'"'>> {};
struct IncludeDecl : if_must<IncludeId, Seps, one<'"'>, IncludePath, one<'"'>> {};

struct NsId : TAO_PEGTL_STRING("namespace") {};
struct NsName : identifier {};
struct NsNameList : list_must<NsName, one<'.'>, Sep> {};
struct NsDecl : if_must<NsId, Seps, opt<NsNameList>> {};
struct NsExpr;
struct EnumExpr;
struct StructExpr;
struct NsExprs : list<sor<Sep, NsExpr, EnumExpr, StructExpr>, Seps> {};
struct NsExpr : if_must<NsDecl, Seps, one<'{'>, Seps, opt<NsExprs>, Seps, one<'}'>, Endl> {};

struct TypeId : TAO_PEGTL_STRING("type") {};
struct TypeName : identifier {};
struct TypeSize : Num {};
struct TypeDecl : if_must<TypeId, Seps, TypeName, Seps, TypeSize, Seps, opt<MetadataDecl>, Endl> {};

struct EnumVal : Num {};
struct EnumValName : identifier {};
struct EnumValDecl : seq<EnumValName, Seps, one<'='>, Seps, EnumVal> {};
struct EnumVals : seq<list<EnumValDecl, one<','>, Sep>, opt<one<','>>> {};
struct EnumId : TAO_PEGTL_STRING("enum") {};
struct EnumClassId : TAO_PEGTL_STRING("class") {};
struct EnumName : identifier {};
struct EnumType : identifier {};
struct EnumDecl : if_must<EnumId, Seps, opt<EnumClassId>, Seps, EnumName, Seps, opt<one<':'>, Seps, EnumType>> {};
struct EnumExpr : if_must<EnumDecl, Seps, one<'{'>, Seps, EnumVals, Seps, one<'}'>, Endl> {};

struct StructId : sor<TAO_PEGTL_STRING("struct"), TAO_PEGTL_STRING("class")> {};
struct StructName : identifier {};
struct StructParentPart : identifier {};
struct StructParent : list_must<StructParentPart, one<'.'>> {};
struct StructParentList : list<StructParent, one<','>, Sep> {};
struct StructParentListDecl : seq<one<':'>, Seps, StructParentList> {};
struct StructSize : Num {};
struct StructDecl : if_must<StructId, Seps, StructName, Seps, opt<StructParentListDecl>, Seps, opt<StructSize>> {};
struct StructPrivacyDecl
    : disable<sor<TAO_PEGTL_STRING("public"), TAO_PEGTL_STRING("private"), TAO_PEGTL_STRING("protected")>, Seps,
          one<':'>> {};
struct StructExpr;
struct FnDecl;
struct VarDecl;
struct StructExprs : list<sor<StructPrivacyDecl, FnDecl, VarDecl, EnumExpr, StructExpr>, Seps> {};
struct StructExpr : if_must<StructDecl, Seps, one<'{'>, Seps, opt<StructExprs>, Seps, one<'}'>, Endl> {};

struct VarTypeNamePart : identifier {};
struct VarTypeName : list_must<VarTypeNamePart, one<'.'>> {};
struct VarTypePtr : one<'*'> {};
struct VarTypeArrayCount : Num {};
struct VarTypeArray : if_must<one<'['>, VarTypeArrayCount, one<']'>> {};
struct VarTypeHint : sor<TAO_PEGTL_STRING("struct"), TAO_PEGTL_STRING("class"), TAO_PEGTL_STRING("enum class"),
                         TAO_PEGTL_STRING("enum")> {};
struct VarType : seq<opt<VarTypeHint>, Seps, VarTypeName, Seps, star<VarTypePtr>, star<VarTypeArray>> {};
struct VarName : identifier {};
struct VarOffset : Num {};
struct VarOffsetDecl : if_must<one<'@'>, Seps, VarOffset> {};
struct VarDelta : Num {};
struct VarDeltaDecl : if_must<one<'+'>, Seps, VarDelta> {};
struct VarOffsetDeltaDecl : sor<VarOffsetDecl, VarDeltaDecl> {};
struct VarBitSize : Num {};
struct VarBitSizeDecl : seq<one<':'>, Seps, VarBitSize> {};
struct VarDecl : seq<VarType, Seps, VarName, star<VarTypeArray>, Seps, opt<VarBitSizeDecl>, Seps,
                     opt<VarOffsetDeltaDecl>, Seps, opt<MetadataDecl>, Endl> {};

struct FnVoidId : TAO_PEGTL_STRING("void") {};
struct FnRetType : VarType {};
struct FnRet : sor<FnVoidId, FnRetType> {};
struct FnName : identifier {};
struct FnParamType : VarType {};
struct FnParamName : identifier {};
struct FnParam : seq<FnParamType, Seps, FnParamName> {};
struct FnParamList : list_must<FnParam, one<','>, Sep> {};
struct FnParams : if_must<one<'('>, Seps, opt<FnParamList>, Seps, one<')'>> {};
struct FnStaticId : TAO_PEGTL_STRING("static") {};
struct FnVirtualId : TAO_PEGTL_STRING("virtual") {};
struct FnPrefix : sor<FnStaticId, FnVirtualId> {};
struct FnDecl : seq<opt<FnPrefix>, Seps, FnRet, Seps, FnName, Seps, FnParams, Endl> {};

struct StaticAssert : disable<TAO_PEGTL_STRING("static_assert"), until<one<';'>>> {};

struct Decl : sor<IncludeDecl, TypeDecl, NsExpr, EnumExpr, StructExpr, StaticAssert> {};
struct Grammar : until<eof, sor<eolf, Sep, Decl>> {};

struct State {
    std::filesystem::path filepath{std::filesystem::current_path()};
    std::vector<Object*> parents{};
    std::stack<int> namespace_depth{};

    std::vector<std::string> metadata_parts{};
    std::vector<std::string> metadata{};

    std::string include_path{};

    std::vector<std::string> ns_parts{};
    std::vector<std::string> ns{};

    std::string type_name{};
    int type_size{};

    std::string enum_name{};
    std::string enum_type{};
    bool enum_class{};
    std::string enum_val_name{};
    uint32_t enum_val{};
    std::vector<std::tuple<std::string, uint64_t>> enum_vals{};

    std::string struct_name{};
    std::vector<std::string> struct_parent{};
    std::vector<genny::Struct*> struct_parents{};
    std::optional<size_t> struct_size{};
    bool struct_is_class{};

    genny::Type* cur_type{};
    std::string var_type_hint{};
    std::vector<std::string> var_type{};
    std::optional<size_t> var_type_array_count{};
    std::vector<size_t> var_type_array_counts{};
    std::string var_name{};
    std::optional<uintptr_t> var_offset{};
    std::optional<uintptr_t> var_delta{};
    std::optional<uintptr_t> var_bit_offset{};
    std::optional<size_t> var_bit_size{};

    genny::Type* fn_ret_type{};
    std::string fn_name{};
    bool fn_is_static{};
    bool fn_is_virtual{};

    struct Param {
        genny::Type* type{};
        std::string name{};
    };

    Param cur_param{};
    std::vector<Param> fn_params{};

    // Searches for the type identified by a vector of names.
    template <typename T> T* lookup(const std::vector<std::string>& names) {
        std::function<T*(Object*, int)> search = [&](Object* parent, int i) -> T* {
            if (names.empty()) {
                return dynamic_cast<T*>(parents.front());
            } else if (i >= names.size()) {
                return nullptr;
            }

            const auto& name = names[i];

            // Search for the name.
            auto child = parent->find<Object>(name);

            if (child == nullptr) {
                return nullptr;
            }

            // We found the name. Is this the type we were looking for?
            if (i == names.size() - 1) {
                return dynamic_cast<T*>(child);
            }

            return search(child, ++i);
        };

        for (auto it = parents.rbegin(); it != parents.rend(); ++it) {
            if (auto type = search(*it, 0)) {
                return type;
            }
        }

        return nullptr;
    }
};

template <typename Rule> struct Action : nothing<Rule> {};

template <> struct Action<Metadata> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.metadata_parts.emplace_back(in.string_view());
    }
};

template <> struct Action<MetadataDecl> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.metadata = std::move(s.metadata_parts);
    }
};

template <> struct Action<IncludePath> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.include_path = in.string_view();
    }
};

template <> struct Action<IncludeDecl> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        auto backup_filepath = s.filepath;

        try {
            auto include_path = std::move(s.include_path);
            s.filepath = (s.filepath.has_extension() ? s.filepath.parent_path() : s.filepath) / include_path;
            file_input f{s.filepath};

            if (!parse<genny::parser::Grammar, genny::parser::Action>(f, s)) {
                throw parse_error{"Failed to parse file '" + include_path + "'", in};
            }
        } catch (const parse_error& e) {
            throw e;
        } catch (const std::exception& e) {
            throw parse_error{std::string{"Failed to include file: "} + e.what(), in};
        }

        s.filepath = backup_filepath;
    }
};

template <> struct Action<NsName> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.ns_parts.emplace_back(in.string_view());
    }
};

template <> struct Action<NsNameList> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) { s.ns = std::move(s.ns_parts); }
};

template <> struct Action<NsDecl> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        if (auto cur_ns = dynamic_cast<Namespace*>(s.parents.back())) {
            auto depth = 0;

            for (auto&& ns : s.ns) {
                cur_ns = cur_ns->namespace_(ns);
                s.parents.push_back(cur_ns);
                ++depth;
            }

            s.namespace_depth.push(depth);
            s.ns.clear();
        } else {
            throw parse_error{"Can only declare a namespace within the context of another namespace", in};
        }
    }
};

template <> struct Action<NsExpr> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        auto depth = s.namespace_depth.top();

        s.namespace_depth.pop();

        for (auto i = 0; i < depth; ++i) {
            s.parents.pop_back();
        }
    }
};

template <> struct Action<TypeSize> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.type_size = std::stoi(in.string(), nullptr, 0);
    }
};

template <> struct Action<TypeName> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.type_name = in.string_view();
    }
};

template <> struct Action<TypeDecl> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        if (auto ns = dynamic_cast<Namespace*>(s.parents.back())) {
            auto type = ns->type(s.type_name);

            type->size(s.type_size);

            if (!s.metadata.empty()) {
                type->metadata() = std::move(s.metadata);
            }

            s.type_name.clear();
            s.type_size = -1;
        } else {
            throw parse_error{"Can only declare a type within the context of a namespace", in};
        }
    }
};

template <> struct Action<EnumName> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.enum_name = in.string_view();
    }
};

template <> struct Action<EnumType> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.enum_type = in.string_view();
    }
};

template <> struct Action<EnumClassId> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) { s.enum_class = true; }
};

template <> struct Action<EnumExpr> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        Enum* enum_{};

        if (auto p = dynamic_cast<Namespace*>(s.parents.back())) {
            if (s.enum_class) {
                enum_ = p->enum_class(s.enum_name);
            } else {
                enum_ = p->enum_(s.enum_name);
            }
        } else if (auto p = dynamic_cast<Struct*>(s.parents.back())) {
            if (s.enum_class) {
                enum_ = p->enum_class(s.enum_name);
            } else {
                enum_ = p->enum_(s.enum_name);
            }
        } else {
            throw parse_error{"Cannot create an enum here. Parent must be a namespace or a struct.", in};
        }

        for (auto&& [name, val] : s.enum_vals) {
            enum_->value(name, val);
        }

        s.enum_vals.clear();
        s.enum_name.clear();
        s.enum_type.clear();
        s.enum_class = false;
    }
};

template <> struct Action<EnumVal> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.enum_val = std::stoul(in.string(), nullptr, 0);
    }
};

template <> struct Action<EnumValName> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.enum_val_name = in.string_view();
    }
};

template <> struct Action<EnumValDecl> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.enum_vals.emplace_back(s.enum_val_name, s.enum_val);
    }
};

template <> struct Action<StructId> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.struct_is_class = in.string_view() == "class";
    }
};

template <> struct Action<StructName> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.struct_name = in.string_view();
    }
};

template <> struct Action<StructParentPart> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.struct_parent.emplace_back(in.string_view());
    }
};

template <> struct Action<StructParent> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        auto parent = s.lookup<genny::Struct>(s.struct_parent);

        if (parent == nullptr) {
            throw parse_error{"Can't find parent struct type with name '" + s.struct_parent.back() + "'", in};
        }

        s.struct_parents.emplace_back(parent);
        s.struct_parent.clear();
    }
};

template <> struct Action<StructSize> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.struct_size = std::stoull(in.string(), nullptr, 0);
    }
};

template <> struct Action<StructDecl> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        Struct* struct_{};

        if (auto p = dynamic_cast<Namespace*>(s.parents.back())) {
            if (s.struct_is_class) {
                struct_ = p->class_(s.struct_name);
            } else {
                struct_ = p->struct_(s.struct_name);
            }
        } else if (auto p = dynamic_cast<Struct*>(s.parents.back())) {
            if (s.struct_is_class) {
                struct_ = p->class_(s.struct_name);
            } else {
                struct_ = p->struct_(s.struct_name);
            }
        } else {
            throw parse_error{"Structs can only be declared within the context of a namespace or another struct", in};
        }

        for (auto&& parent : s.struct_parents) {
            struct_->parent(parent);
        }

        if (s.struct_size) {
            struct_->size(*s.struct_size);
        }

        s.parents.push_back(struct_);
        s.struct_name.clear();
        s.struct_parents.clear();
        s.struct_size = std::nullopt;
        s.struct_is_class = false;
    }
};

template <> struct Action<StructExpr> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) { s.parents.pop_back(); }
};

template <> struct Action<VarTypeHint> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.var_type_hint = in.string_view();
    }
};

template <> struct Action<VarTypeNamePart> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.var_type.emplace_back(in.string_view());
    }
};

template <> struct Action<VarTypeName> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.cur_type = s.lookup<Type>(s.var_type);

        if (s.cur_type == nullptr && !s.var_type_hint.empty()) {
            auto owner_type = s.var_type;
            owner_type.pop_back();
            auto owner = s.lookup<Namespace>(owner_type);

            if (owner != nullptr) {
                if (s.var_type_hint == "struct") {
                    s.cur_type = owner->struct_(s.var_type.back());
                } else if (s.var_type_hint == "class") {
                    s.cur_type = owner->class_(s.var_type.back());
                } else if (s.var_type_hint == "enum") {
                    s.cur_type = owner->enum_(s.var_type.back());
                } else if (s.var_type_hint == "enum class") {
                    s.cur_type = owner->enum_class(s.var_type.back());
                }
            }
        }

        if (s.cur_type == nullptr) {
            throw parse_error{"Can't find type with name '" + s.var_type.back() + "'", in};
        }

        s.var_type.clear();
        s.var_type_array_counts.clear();
    }
};

template <> struct Action<VarTypePtr> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        if (s.cur_type == nullptr) {
            throw parse_error{"The current type is null", in};
        }

        s.cur_type = s.cur_type->ptr();
    }
};

template <> struct Action<VarTypeArrayCount> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.var_type_array_count = std::stoull(in.string(), nullptr, 0);
    }
};

template <> struct Action<VarTypeArray> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        if (!s.var_type_array_count) {
            throw parse_error{"The array count is invalid", in};
        }

        s.var_type_array_counts.emplace_back(*s.var_type_array_count);
        s.var_type_array_count = std::nullopt;
    }
};

template <> struct Action<VarName> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.var_name = in.string_view();
    }
};

template <> struct Action<VarOffset> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.var_offset = std::stoull(in.string(), nullptr, 0);
    }
};

template <> struct Action<VarDelta> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.var_delta = std::stoull(in.string(), nullptr, 0);
    }
};

template <> struct Action<VarBitSize> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.var_bit_size = std::stoull(in.string(), nullptr, 0);
    }
};

template <> struct Action<VarDecl> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        if (s.cur_type == nullptr) {
            throw parse_error{"The current type is null", in};
        }

        // Reverse the order because we want to adhear to how multi-dimensional arrays are declared in C/C++.
        std::reverse(s.var_type_array_counts.begin(), s.var_type_array_counts.end());

        for (auto&& count : s.var_type_array_counts) {
            s.cur_type = s.cur_type->array_(count);
        }

        s.var_type_array_counts.clear();

        if (auto struct_ = dynamic_cast<Struct*>(s.parents.back())) {
            auto var = struct_->variable(s.var_name);

            var->type(s.cur_type);

            if (s.var_bit_size) {
                var->bit_size(*s.var_bit_size);
            }

            if (s.var_offset) {
                var->offset(*s.var_offset);
            } else {
                var->append();

                if (s.var_delta) {
                    var->offset(var->offset() + *s.var_delta);
                }
            }

            if (var->is_bitfield()) {
                var->bit_append();
            }

            if (!s.metadata.empty()) {
                var->metadata() = std::move(s.metadata);
            }

            s.var_name.clear();
            s.cur_type = nullptr;
            s.var_offset = std::nullopt;
            s.var_delta = std::nullopt;
            s.var_bit_size = std::nullopt;
            s.var_bit_offset = std::nullopt;
            s.var_type_hint.clear();
        } else {
            throw parse_error{"Can't declare a variable outside of a struct", in};
        }
    }
};

template <> struct Action<FnRetType> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) { s.fn_ret_type = s.cur_type; }
};

template <> struct Action<FnName> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) { s.fn_name = in.string_view(); }
};

template <> struct Action<FnParamType> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.cur_param.type = s.cur_type;
    }
};

template <> struct Action<FnParamName> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.cur_param.name = in.string_view();
    }
};

template <> struct Action<FnParam> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        s.fn_params.emplace_back(std::move(s.cur_param));
    }
};

template <> struct Action<FnStaticId> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) { s.fn_is_static = true; }
};

template <> struct Action<FnVirtualId> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) { s.fn_is_virtual = true; }
};

template <> struct Action<FnDecl> {
    template <typename ActionInput> static void apply(const ActionInput& in, State& s) {
        if (auto struct_ = dynamic_cast<Struct*>(s.parents.back())) {
            Function* fn{};

            if (s.fn_is_static) {
                fn = struct_->static_function(s.fn_name);
            } else if (s.fn_is_virtual) {
                fn = struct_->virtual_function(s.fn_name);
            } else {
                fn = struct_->function(s.fn_name);
            }

            if (s.fn_ret_type) {
                fn->returns(s.fn_ret_type);
            }

            fn->defined(false);

            for (auto&& param : s.fn_params) {
                fn->param(param.name)->type(param.type);
            }

            s.fn_name.clear();
            s.fn_ret_type = nullptr;
            s.fn_params.clear();
            s.fn_is_static = false;
            s.fn_is_virtual = false;
        } else {
            throw parse_error{"Can't declare a function outside of a struct", in};
        }
    }
};
} // namespace genny::parser
