#pragma once

#include <functional>
#include <vector>

#include "ConfigItemImplBase.h"

namespace wupsext {

    struct ConfigToggleItem;
    struct ConfigToggleItemImpl;
    struct ConfigToggleItemGroup;

    struct ConfigToggleItemImpl : ConfigItemImplBase {

        using CallbackSignature = void (ConfigToggleItemImpl &);
        using CallbackFunction = std::move_only_function<CallbackSignature>;

        bool value;
        CallbackFunction changedCallback;
        ConfigToggleItemGroup* group;

        ConfigToggleItemImpl(const std::string& displayName_,
                             const std::string& identifier_,
                             bool value_,
                             CallbackFunction changedCallback_,
                             ConfigToggleItemGroup* group_);

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

    struct ConfigToggleItem : public WUPSConfigItem {

        ConfigToggleItem(const std::string& displayName,
                         const std::string& identifier,
                         bool value,
                         ConfigToggleItemImpl::CallbackFunction changedCallback,
                         ConfigToggleItemGroup* group = nullptr);

    };

    struct ConfigToggleItemGroup {

        enum class Mode {
            Multi,
            Single,
        };

        using ForEachCallbackSignature = void (ConfigToggleItemImpl& item);
        using ForEachCallbackFunction = std::function<ForEachCallbackSignature>;

        ConfigToggleItemGroup(Mode mode_ = Mode::Multi);

        Mode
        getMode()
            const;

        void
        setMode(Mode mode_);

        void
        add(ConfigToggleItemImpl* item);

        void
        unselectAll();

        void
        notifyEnabled(ConfigToggleItemImpl* item);

        void
        forEach(const ForEachCallbackFunction& callback);

    private:

        Mode mode = Mode::Single;
        std::vector<ConfigToggleItemImpl*> items;

    };

} // namespace wupsext
