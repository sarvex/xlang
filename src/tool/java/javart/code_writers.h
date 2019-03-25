#pragma once

namespace xlang
{
    struct push_interface_generic_params
    {
        explicit push_interface_generic_params(writer& writer, interface_info const& info)
            : writer_stack(writer.generic_param_stack), interface_stack(info.generic_param_stack)
        {
            writer_stack.insert(writer_stack.end(), interface_stack.begin(), interface_stack.end());
        }

        ~push_interface_generic_params()
        {
            writer_stack.resize(writer_stack.size() - interface_stack.size());
        }

        std::vector<std::vector<std::string>>& writer_stack;
        std::vector<std::vector<std::string>> const& interface_stack;
    };

    template <typename T>
    constexpr auto to_underlying(T const value) noexcept
    {
        return static_cast<std::underlying_type_t<T>>(value);
    }

    static void write_license(writer& w)
    {
        auto format = R"(// WARNING: Please don't edit this file. It was generated by javart v%
)";

        w.write(format, XLANG_VERSION_STRING);
    }

    static void write_jni_prolog(writer& w, std::string_view const& ns)
    {
        auto format = R"(// Java/WinRT projection for % namespace

#include "pch.h"
#include "javart.h"
#include <string_view>
#include "winrt/%.h"

using namespace std::literals;
using namespace winrt;

#define JNI_EXPORT_NAMESPACE %

)";

        w.write(format, ns, ns, java_export{ ns });
    }

    template<type_system system>
    static void write_field_as(writer& w, Field const& field)
    {
        w.write_as(system, "% %", field, field.Name());
    }

    template<type_system system>
    static void write_param_as(writer& w, std::pair<Param, const ParamSig*> const& param)
    {
        w.write_as(system, "% %", param.second->Type(), param.first.Name());
    }

    template<type_system system>
    static void write_params_as(writer& w, std::vector<std::pair<Param, const ParamSig*>> const& params, bool appending)
    {
        w.write("%%", 
            !params.empty() && appending ? ", " : "",
            bind_list<write_param_as<system>>(", ", params));
    }

    template<type_system system>
    static void write_param_type_as(writer& w, std::pair<Param, ParamSig const*> const& param)
    {
        w.write_as(system, param.second->Type());
    }

    template<type_system system>
    static void write_type_as(writer& w, TypeSig const& typeSig)
    {
        w.write_as(system, typeSig);
    }

    template<type_system system>
    static void write_return_type_as(writer& w, RetTypeSig const& typeSig)
    {
        if (!typeSig)
        {
            w.write("void");
            return;
        }
        w.write_as(system, typeSig.Type());
    }

    template<type_system system>
    static void write_method_name_as(writer& w, MethodDef const& method)
    {
        auto name = is_constructor(method) ? "construct" : method.Name();
        if (method.SpecialName())
        {
            auto is_get = starts_with(name, "get_");
            auto is_put = !is_get && starts_with(name, "put_");
            if (is_get || is_put)
            {
                auto bare_name = name.substr(4);
                if constexpr (system == type_system::java_type)
                {
                    w.write(is_get ? "get" : "set");
                    w.write(mixed_case{ bare_name });
                }
                else
                {
                    w.write(bare_name);
                }
                return;
            }
        }
        if constexpr (system == type_system::java_type)
        {
            w.write(camel_case{ name });
        }
        else
        {
            w.write(name);
        }
    }

    static void write_method_arg(writer& w, std::pair<Param, ParamSig const*> const& param)
    {
        w.write(param.first.Name());
    }

    enum class generic_interface_type
    {
        Iterable,
        VectorView,
        Vector,
        ObvservableVector,
        MapView,
        Map,
        ObservableMap,
    };

    static constexpr struct 
    {
        std::string_view interface_name;
        std::string_view base_class;
        std::string_view base_trait;
        std::string_view projected;
        std::string_view java_class;
    }
    generic_interface_info[] =
    {
        {"IIterable"sv, "Iterable"sv, "iterator_type"sv, "Iterator"sv, "java.lang.Iterable"},
        {"IVectorView"sv, "VectorView"sv, "vector_view_type"sv, "VectorView"sv, "java.lang.VectorView"},
        {"IVector"sv, "Vector"sv, "vector_type"sv, "Vector"sv, "java.lang.Vector"},
        {"IObservableVector"sv, "ObservableVector"sv, "observable_vector_type"sv, "ObservableVector"sv, "javafx.collections.ObservableList"},
        {"IMapView"sv, "MapView"sv, "map_view_type"sv, "MapView"sv, "java.lang.MapView"},
        {"IMap"sv, "Map"sv, "map_type"sv, "Map"sv, "java.lang.Map"},
        {"IObservableMap"sv, "ObservableMap"sv, "observable_map_type"sv, "ObservableMap"sv, "javafx.collections.ObservableMap"},
    };

    static auto get_interface_info(writer& w, TypeDef const& type)
    {
        std::map<std::variant<generic_interface_type, std::string>, interface_info> interfaces;

        for (auto&&[interface_name, info] : get_interfaces(w, type))
        {
            static constexpr std::string_view collections_namespace("Windows.Foundation.Collections."sv);
            if (starts_with(interface_name, collections_namespace))
            {
                [&]{
                    auto relative_type = interface_name.substr(collections_namespace.size());
                    for (int i = 0; i < _countof(generic_interface_info); ++i)
                    {
                        if (starts_with(relative_type, generic_interface_info[i].interface_name))
                        {
                            interfaces[static_cast<generic_interface_type>(i)] = std::move(info);
                            return;
                        }
                    }
                    XLANG_ASSERT(false);
                }();
            }
            else
            {
                interfaces[interface_name] = std::move(info);
            }
        };
        return interfaces;
    }

    static void write_jni_generic_base(writer& w, std::pair<std::variant<generic_interface_type, std::string>, interface_info> const& ifc, std::string_view const& derived_type)
    {
        if (auto generic_interface = std::get_if<generic_interface_type>(&ifc.first))
        {
            w.write(", %<%>", 
                generic_interface_info[to_underlying(*generic_interface)].base_class, 
                derived_type
            );
        }
    }

    static void write_jni_export(writer& w, std::string_view const& type, std::string_view const& ns)
    {
        auto format = R"(
void JNICALL
%(jni_env* env, jclass cls) noexcept try
{
    #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
    %::jni_register(*env, cls);
}
catch (...)
{
    env->raise_java_exception("%");
})";
        auto export_name = std::string(ns) + "_" + std::string(type) + "_jni_register";
        w.write(format,
            export_name,
            type,
            export_name);
    }

    static void write_jni_stub_iterator(writer& w, generic_iterator const& gi)
    {
        auto format = R"(struct % : Projection<Windows::Foundation::Collections::IIterator<%>>, Iterator<%>
{
    static constexpr char projected_type[] = "%";
    static constexpr char element_type[] = "%";

    static void jni_register(jni_env& env, jclass cls)
    {
        Projection::jni_register(env, cls);
        Iterator::jni_register(env, cls);
    }
};
%

)";

        w.write(format,
            gi.name,
            gi.cpp_type,
            gi.name,
            java_type_name{ gi.name, w.current_namespace },
            gi.java_element,
            bind<write_jni_export>(gi.name, w.current_namespace),
            gi.name
        );

        w.add_unregister(gi.name);
    }

    static void write_jni_generic_trait(writer& w, std::pair<std::variant<generic_interface_type, std::string>, interface_info> const& ifc)
    {
        if (auto generic_interface = std::get_if<generic_interface_type>(&ifc.first))
        {
            // todo: make generic_param_stack a stack of TypeSigs to avoid all this string manipulation 
            auto cpp_params = w.write_temp("%", bind_list(",", ifc.second.generic_param_stack.back()));
            auto params = cpp_params;
            auto is_delimiter = [](auto&& x) { return x == '.' || x == ','; };
            params.erase(std::remove_if(params.begin(), params.end(), is_delimiter), params.end());
            auto name = w.write_temp("%%", params, generic_interface_info[to_underlying(*generic_interface)].projected);

            if (*generic_interface == generic_interface_type::Iterable)
            {
                std::string element_java_type;
                for (auto it = cpp_params.begin(); it != cpp_params.end(); ++it)
                {
                    element_java_type.append(1, *it == '.' ? '/' : *it);
                }
                if (element_java_type.find_first_of('/') == std::string::npos)
                {
                    element_java_type = w.write_temp("%", java_type_name{ element_java_type, w.current_namespace });
                }
                // record iterator for later implementation
                w.add_iterator({ name, cpp_params, element_java_type });
            }

            auto format = R"(    static constexpr char %[] = "%";
)";
            w.write(format,
                generic_interface_info[to_underlying(*generic_interface)].base_trait,
                java_type_name{ name, w.current_namespace });
        }
    }

    static void write_jni_generic_register(writer& w, std::pair<std::variant<generic_interface_type, std::string>, interface_info> const& ifc)
    {
        if (auto generic_interface = std::get_if<generic_interface_type>(&ifc.first))
        {
            auto format = R"(        %::jni_register(env, cls);
)";
            w.write(format, 
                generic_interface_info[to_underlying(*generic_interface)].base_class
            );
        }
    }

    static void write_jni_method_arg(writer& w, std::pair<Param, ParamSig const*> const& param)
    {
        call(param.second->Type().Type(),
            [&](ElementType type)
        {
            if (type == ElementType::String)
            {
                w.write("jstring_view{env, %}", param.first.Name());
                return;
            }
            w.write(param.first.Name());
        },
        //    [&](coded_index<TypeDefOrRef> const& type)
        //{
        //    w.write(param.first.Name());
        //},
        //    [&](GenericTypeIndex var)
        //{
        //    w.write(param.first.Name());
        //},
            [&](auto&&)
        {
            w.write(param.first.Name());
        });
    }

    static void write_jni_constructor(writer& w, MethodDef const& method)
    {
        auto format = R"(    static auto jni_construct%(jni_env&, jclass%)
    {
        return create_agile_ref(type{%});
    }

)";
        method_signature signature{ method };
        auto&& params = signature.params();
        w.write(format,
            bind_each<write_param_type_as<java_suffix>>(params),
            bind<write_params_as<jni_type>>(params, true),
            bind_list<write_jni_method_arg>(", ", params)
        );
    }

    static void write_jni_method(writer& w, MethodDef const& method)
    {
        auto format = R"(    static auto jni_%%(jni_env&, jobject, jlong abi%)
    {
        return resolve{abi}.%(%);
    }
        
)";
        method_signature signature{ method };
        auto&& params = signature.params();
        w.write(format,
            bind<write_method_name_as<java_type>>(method),
            bind_each<write_param_type_as<java_suffix>>(params),
            bind<write_params_as<jni_type>>(params, true),
            bind<write_method_name_as<jni_type>>(method),
            bind_list<write_jni_method_arg>(", ", params)
        );
    }

    static void write_jni_methods(writer& w, std::pair<std::variant<generic_interface_type, std::string>, interface_info> const& ifc)
    {
        if (auto interface_name = std::get_if<std::string>(&ifc.first))
        {
            push_interface_generic_params params(w, ifc.second);
            w.write("%", bind_each<write_jni_method>(ifc.second.type.MethodList()));
        }
    }

    static void write_jni_constructor_registration(writer& w, MethodDef const& method)
    {
        auto format = R"(            JNI_METHOD_(jni_construct%, "(%)J"),
)";
        method_signature signature{ method };
        auto&& params = signature.params();
        w.write(format,
            bind_each<write_param_type_as<java_suffix>>(params),
            bind_each<write_param_type_as<java_descriptor>>(signature.params())
        );
    }

    static void write_jni_method_registration(writer& w, MethodDef const& method)
    {
        auto format = R"(            JNI_METHOD_(jni_%%, "(J%)%"),
)";
        method_signature signature{ method };
        auto&& params = signature.params();
        w.write(format,
            bind<write_method_name_as<java_type>>(method),
            bind_each<write_param_type_as<java_suffix>>(params),
            bind_each<write_param_type_as<java_descriptor>>(signature.params()),
            bind<write_return_type_as<java_descriptor>>(signature.return_signature())
        );
    }

    static void write_jni_method_registrations(writer& w, std::pair<std::variant<generic_interface_type, std::string>, interface_info> const& ifc)
    {
        if (auto interface_name = std::get_if<std::string>(&ifc.first))
        {
            push_interface_generic_params params(w, ifc.second);
            w.write("%", bind_each<write_jni_method_registration>(ifc.second.type.MethodList()));
        }
    }

    static void write_jni_stub_inspectable(writer& w)
    {
        auto format = R"(struct Inspectable : Projection<Windows::Foundation::IInspectable>
{
    static constexpr char projected_type[] = "Windows/Foundation/Inspectable";

    static auto jni_AddRef(jni_env&, jobject, jlong abi)
    {
        if (auto obj = agile_abi_ref::from(abi))
        {
            obj->addref();
        }
    }

    static auto jni_Release(jni_env&, jobject, jlong abi)
    {
        if (auto obj = agile_abi_ref::from(abi))
        {
            obj->release();
        }
    }

    static auto jni_GetClassName(jni_env&, jobject, jlong abi)
    {
        return get_class_name(resolve{abi});
    }

    static auto jni_GetIdentity(jni_env&, jobject, jlong abi)
    {
        auto obj = resolve{abi};
        return obj ? reinterpret_cast<jlong>(get_abi(obj.as<::IUnknown>())) : jlong{};
    }

    static void jni_register(jni_env& env, jclass cls) 
    {
        Projection::jni_register(env, cls);
        static JNINativeMethod methods[] =
        {
            JNI_METHOD_(jni_AddRef, "(J)V"),
            JNI_METHOD_(jni_Release, "(J)V"),
            JNI_METHOD_(jni_GetClassName, "(J)Ljava/lang/String;"),
            JNI_METHOD_(jni_GetIdentity, "(J)J"),
        };
        env.register_natives(cls, methods);
    }
};
%

)";

        w.write(format,
            bind<write_jni_export>("Inspectable", "Windows_Foundation")
        );
        w.add_unregister("Inspectable");
    }

    static void write_jni_stub(writer& w, TypeDef const& type)
    {
        if (!settings.filter.includes(type))
        {
            return;
        }

        auto format = R"(struct % : Projection<@::%>%
{
    static constexpr char projected_type[] = "%";
%
%%    static void jni_register(jni_env& env, jclass cls)
    {
        Projection::jni_register(env, cls);
%        static JNINativeMethod methods[] =
        {
%%        };
        env.register_natives(cls, methods);
    }
};
%

)";

        auto interfaces = get_interface_info(w, type);

        w.write(format, 
            type.TypeName(),
            type.TypeNamespace(),
            type.TypeName(),
            bind_each<write_jni_generic_base>(interfaces, type.TypeName()),
            java_type_name{ type },
            bind_each<write_jni_generic_trait>(interfaces),
            bind_each<write_jni_constructor>(get_constructors(type)),
            bind_each<write_jni_methods>(interfaces),
            bind_each<write_jni_generic_register>(interfaces),
            bind_each<write_jni_constructor_registration>(get_constructors(type)),
            bind_each<write_jni_method_registrations>(interfaces),
            bind<write_jni_export>(type.TypeName(), w.current_namespace)
        );

        w.add_unregister(type.TypeName());
    }

    static bool is_foundation(std::string_view const& ns)
    {
        return ns == "Windows.Foundation";
    }

    static void write_jni_stubs_special(writer& w, std::string_view const& ns)
    {
        if (!is_foundation(ns))
        {
            return;
        }

        write_jni_stub_inspectable(w);
    }

    static void write_jni_stubs(writer& w, std::string_view const& ns, std::vector<TypeDef> const& classes)
    {
        write_jni_stubs_special(w, ns);
        w.write_each<write_jni_stub>(classes);
    }

    static void write_jni_unregister(writer& w, std::string_view const& type_name)
    {
        if (!settings.filter.includes(w.current_namespace, type_name))
        {
            return;
        }

        auto format = R"(    %::jni_unregister(env);
)";

        w.write(format, type_name);
    }

    static void write_jni_unregisters(writer& w, std::string_view const& ns)
    {
        auto format = R"(void %_Unregister(jni_env& env)
{
%}
extern "C" __declspec(allocate("javart$m")) 
auto %_UnregisterFunc = &%_Unregister;
)";
        w.write(format,
            java_export{ ns },
            bind_each<write_jni_unregister>(w.unregisters),
            java_export{ ns },
            java_export{ ns }
        );
    }

    static void write_java_implements(writer& w, std::map<std::variant<generic_interface_type, std::string>, interface_info> const& ifcs)
    {
        auto is_first{true};

        for (auto&&[key, info] : ifcs)
        {
            if (is_exclusive(info.type))
            {
                continue;
            }
            auto write_implements = [&](auto&& implements)
            {
                if (is_first)
                {
                    is_first = false;
                    w.write("implements %", implements);
                    return;
                }
                w.write(", %", implements);
            };

            if (auto generic_interface = std::get_if<generic_interface_type>(&key))
            {
                auto params = w.write_temp("%", bind_list(",", info.generic_param_stack.back()));
                auto implements = std::string(generic_interface_info[to_underlying(*generic_interface)].java_class) + "<" + params + ">";
                write_implements(implements);
            }
            else
            {
                write_implements(std::get<std::string>(key));
            }
        }
    }

    static void write_java_public_constructor(writer& w, MethodDef const& method, TypeDef const& type)
    {
        auto format = R"(    public %(%) {
        this(jni_construct%(%));
    }

)";
        method_signature signature{ method };
        auto&& params = signature.params();
        w.write(format,
            type.TypeName(),
            bind<write_params_as<java_type>>(params, false),
            bind_each<write_param_type_as<java_suffix>>(params),
            bind_list<write_method_arg>(", ", params)
        );
    }

    static void write_java_public_method(writer& w, MethodDef const& method)
    {
        auto format = R"(    public % %(%) {
        %jni_%%(%%);
    }

)";
        method_signature signature{ method };
        auto&& params = signature.params();
        auto has_return = signature.return_signature();
        bool isStatic = is_static(method);
        w.write(format,
            bind<write_return_type_as<java_type>>(signature.return_signature()),
            bind<write_method_name_as<java_type>>(method),
            bind<write_params_as<java_type>>(params, false),
            has_return ? "return " : "",
            bind<write_method_name_as<java_type>>(method),
            bind_each<write_param_type_as<java_suffix>>(params),
            isStatic ? "" : signature.params().empty() ? "abi" : "abi, ",
            bind_list<write_method_arg>(", ", params)
        );
    }

    static void write_java_public_methods(writer& w, std::pair<std::variant<generic_interface_type, std::string>, interface_info> const& ifc)
    {
        if (auto generic_interface = std::get_if<generic_interface_type>(&ifc.first))
        {
            if(*generic_interface == generic_interface_type::Iterable)
            {
                auto format = R"(    ^@Override
    public java.util.Iterator<%> iterator() {
        return abi_iterator(abi);
    }

)";
                auto params = w.write_temp("%", bind_list(",", ifc.second.generic_param_stack.back()));
                w.write(format, params);
            }
        }
        else
        {
            push_interface_generic_params params(w, ifc.second);
            w.write("%", bind_each<write_java_public_method>(ifc.second.type.MethodList()));
        }
    }
    
//    static void write_java_public_method(writer& w, MethodDef const& method)
//    {
//        if (is_constructor(method))
//        {
//            return;
//        }
//        
//        auto format = R"(    public % %(%) {
//        %jni_%%(%%);
//    }
//
//)";
//        method_signature signature{ method };
//        auto&& params = signature.params();
//        auto has_return = signature.return_signature();
//        bool isStatic = is_static(method);
//        w.write(format,
//            bind<write_return_type_as<java_type>>(signature.return_signature()),
//            bind<write_method_name_as<java_type>>(method),
//            bind_list<write_param_as<java_type>>(", ", params),
//            has_return ? "return " : "",
//            bind<write_method_name_as<java_type>>(method),
//            bind_each<write_param_type_as<java_suffix>>(params),
//            isStatic ? "" : signature.params().empty() ? "abi" : "abi, ",
//            bind_list<write_method_arg>(", ", params));
//    }

//    static void write_java_native_method(writer& w, MethodDef const& method)
//    {
//        auto format = R"(    private %native % jni_%%(%%);
//
//)";
//        method_signature signature{ method };
//        auto&& params = signature.params();
//        bool isStatic = is_static(method) || is_constructor(method);
//        auto return_type = is_constructor(method) ? "long" : w.write_temp("%", bind<write_return_type_as<java_type>>(signature.return_signature()));
//        w.write(format,
//            isStatic ? "static " : "",
//            return_type,
//            bind<write_method_name_as<java_type>>(method),
//            bind_each<write_param_type_as<java_suffix>>(params),
//            isStatic ? "" : "long abi",
//            bind_list<write_param_as<java_type>>(", ", params)
//        );
//    }

    static void write_java_native_method(writer& w, MethodDef const& method)
    {
        auto format = R"(    private %native % jni_%%(%%);

)";
        method_signature signature{ method };
        auto&& params = signature.params();
        bool isStatic = is_static(method) || is_constructor(method);
        auto return_type = is_constructor(method) ? "long" : w.write_temp("%", bind<write_return_type_as<java_type>>(signature.return_signature()));
        w.write(format,
            isStatic ? "static " : "",
            return_type,
            bind<write_method_name_as<java_type>>(method),
            bind_each<write_param_type_as<java_suffix>>(params),
            isStatic ? "" : "long abi",
            bind<write_params_as<java_type>>(params, !isStatic)
        );
    }

    static void write_java_native_methods(writer& w, std::pair<std::variant<generic_interface_type, std::string>, interface_info> const& ifc)
    {
        if (auto generic_interface = std::get_if<generic_interface_type>(&ifc.first))
        {
            if (*generic_interface == generic_interface_type::Iterable)
            {
                auto format = R"(    private native Iterator<%> jni_iterator(long abi);

)";
                auto params = w.write_temp("%", bind_list(",", ifc.second.generic_param_stack.back()));
                w.write(format, params);
            }
        }
        else
        {
            push_interface_generic_params params(w, ifc.second);
            w.write("%", bind_each<write_java_native_method>(ifc.second.type.MethodList()));
        }
    }

    static auto write_java_proxy_iterator(std::string_view const& name_space, generic_iterator const& gi)
    {
        auto format = R"(package %;

public class % extends Inspectable implements java.util.Iterator<%> {

    public %(long abi) {
        super(abi);
    }
    
    public %(Inspectable that) {
        super(that);
    }

    ^@Override
    public boolean hasNext() {
        return jni_hasNext(abi);
    }

    ^@Override
    public % next() {
        return jni_next(abi);
    }

    private native boolean jni_hasNext(long abi);
    private native % jni_next(long abi);
    
    private static native void jni_register();

    static {
        System.loadLibrary("%");
        jni_register();
    }
})";

        auto java_class_descriptor = gi.java_element;
        for (auto it = java_class_descriptor.begin(); it != java_class_descriptor.end(); ++it)
        {
            if (*it == '/')
            {
                *it = '.';
            }
        }

        writer w;
        w.current_namespace = name_space;
        w.write(format,
            name_space,
            gi.name,
            java_class_descriptor,
            gi.name,
            gi.name,
            java_class_descriptor,
            java_class_descriptor,
            settings.shared_lib.empty() ? w.write_temp("%", lower_case{ name_space }) : settings.shared_lib
        );
        return std::move(w);
    }

    static auto write_java_proxy(TypeDef const& type)
    {
        auto format = R"(package %;

public class % extends Inspectable %
{
    public %(long abi) {
        super(abi);
    }

    public %(% that) {
        super(that);
    }

%    ^@Override
    public boolean equals(Object arg0) {
        if (arg0 instanceof %) {
            return super.equals(arg0);
        }
        return false;
    }

%%    private static native void jni_register();

    static {
        System.loadLibrary("%");
        jni_register();
    }
};
        )";
        
        writer w(type);
        auto package = settings.package_base + std::string(type.TypeNamespace());
        auto interfaces = get_interface_info(w, type);
        w.write(format,
            lower_case{ package },
            type.TypeName(),
            bind<write_java_implements>(interfaces),
            type.TypeName(),
            type.TypeName(),
            type.TypeName(),
            bind_each<write_java_public_constructor>(get_constructors(type), type),
            type.TypeName(),
            bind_each<write_java_public_methods>(interfaces),
            //bind_each<write_java_public_method>(type.MethodList()),
            bind_each<write_java_native_methods>(interfaces),
            //bind_each<write_java_native_method>(type.MethodList()),
            settings.shared_lib.empty() ? w.write_temp("%", lower_case{ package }) : settings.shared_lib
        );
        return std::move(w);
    }

    static void write_java_interface_method(writer& w, MethodDef const& method)
    {
        auto format = R"(    public % %(%);

)";
        method_signature signature{ method };
        auto&& params = signature.params();
        w.write(format,
            bind<write_return_type_as<java_type>>(signature.return_signature()),
            bind<write_method_name_as<java_type>>(method),
            bind<write_params_as<java_type>>(params, false)
        );
    }

    static auto write_java_interface(TypeDef const& type)
    {
        auto format = R"(package %;

public interface % {

%}
)";

        writer w(type);
        auto interfaces = get_interface_info(w, type);
        w.write(format,
            lower_case{ settings.package_base + std::string(type.TypeNamespace()) },
            type.TypeName(),
            bind_each<write_java_interface_method>(type.MethodList())
        );
        return std::move(w);
    }

    static void write_java_enum_fields(writer& w, TypeDef const& type)
    {
        auto is_first{true};
        for (auto&& field: type.FieldList())
        {
            if (auto constant = field.Constant())
            {
                w.write(is_first ? "%(%)" : ",\n    %(%)", field.Name(), *constant);
                is_first = false;
            }
        }
    }

    static auto write_java_enum(TypeDef const& type)
    {
        auto format = R"(package %;

public enum % {
	%;
	
	private final % value;

	%(% value){
		this.value = value;
	}

	public % value() {
		return value;
	}
}
)";
        // TODO: project flags enum to EnumSet?
        writer w(type);
        auto underlying_type = "int";
        w.write(format,
            lower_case{ settings.package_base + std::string(type.TypeNamespace()) },
            type.TypeName(),
            bind<write_java_enum_fields>(type),
            underlying_type,
            type.TypeName(),
            underlying_type,
            underlying_type
        );
        return std::move(w);
    }

    static void write_java_struct_field(writer& w, Field const& field)
    {
        w.write("    public %;\n", bind<write_field_as<java_type>>(field));
    }

    static auto write_java_struct(TypeDef const& type)
    {
        auto format = R"(package %;

public class % {
%}
)";
        writer w(type);
        w.write(format,
            lower_case{ settings.package_base + std::string(type.TypeNamespace()) },
            type.TypeName(),
            bind_each<write_java_struct_field>(type.FieldList())
        );
        return std::move(w);
    }

    static auto write_java_delegate(TypeDef const& type)
    {
        writer w(type);
        // TODO
        return std::move(w);
    }
}
