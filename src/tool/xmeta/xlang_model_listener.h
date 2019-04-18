#pragma once
#include "xmeta_models.h"
#include <map>

namespace xlang::xmeta
{
    class xlang_model_listener
    {
    public:
        virtual void listen_namespace_model(std::shared_ptr<namespace_model> const& model) = 0;
        virtual void listen_class_model(std::shared_ptr<class_model> const& model) = 0;
        virtual void listen_struct_model(std::shared_ptr<struct_model> const& model) = 0;
        virtual void listen_interface_model(std::shared_ptr<interface_model> const& model) = 0;
        virtual void listen_enum_model(std::shared_ptr<enum_model> const& model) = 0;
        virtual void listen_delegate_model(delegate_model const& model) = 0;
    };
}