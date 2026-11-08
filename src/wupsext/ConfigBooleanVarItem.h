// -*- mode: c++ -*-

#pragma once

/**
 * ConfigBooleanVarItem: config item for a bool variable.
 *
 * This differs from WUPSConfigItemBoolean:
 * - State is stored in a user-provided boolean variable.
 * - Said state is updated immediately after input.
 * - The changed callback is invoked immediately after the value changes.
 * - The default true/false labels are "yes/no".
 * - We don't bother making it C-friendly.
 */

#include <functional>
#include <optional>

#include "ConfigItemImplBase.h"

namespace wupsext {

    struct ConfigBooleanVarItemImpl : ConfigItemImplBase {

        using CallbackSignature = void (ConfigBooleanVarItemImpl &);
        using CallbackFunction = std::move_only_function<CallbackSignature>;

        // WUPSConfigItemHandle handle;
        // const std::string identifier;
        bool &variable;
        CallbackFunction changedCallback;
        const std::string trueLabel;
        const std::string falseLabel;

        ConfigBooleanVarItemImpl(const std::string &displayName_,
                                 const std::string &identifier_,
                                 bool &variable_,
                                 CallbackFunction changedCallback_,
                                 const std::optional<std::string> &trueLabel_,
                                 const std::optional<std::string> &falseLabel_);

        void
        toggleValue();

        void
        getCurrentValueDisplay(std::span<char> output)
            override;

        void
        getCurrentValueSelectedDisplay(std::span<char> output)
            override;

        void
        onInput(const WUPSConfigSimplePadData& input)
            override;

    };


    struct ConfigBooleanVarItem : public WUPSConfigItem {

        ConfigBooleanVarItem(const std::string &displayName,
                             const std::string &identifier,
                             bool &variable,
                             ConfigBooleanVarItemImpl::CallbackFunction callback = {},
                             const std::optional<std::string> &trueLabel = {},
                             const std::optional<std::string> &falseLabel = {});

    };

} // namespace wupsext
