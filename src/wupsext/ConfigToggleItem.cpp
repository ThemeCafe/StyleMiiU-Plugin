#include <cstdio>
#include <utility>

#include <wups/config_api.h>

#include <cafe_glyphs.h>

#include "ConfigToggleItem.h"
#include "Error.h"
#include "utils/logger.h"


namespace wupsext {

    ConfigToggleItemImpl::ConfigToggleItemImpl(const std::string& displayName_,
                                               const std::string& identifier_,
                                               bool value_,
                                               CallbackFunction changedCallback_,
                                               ConfigToggleItemGroup* group_) :
        ConfigItemImplBase{displayName_, identifier_},
        value{value_},
        changedCallback{std::move(changedCallback_)},
        group{group_}
    {
        if (group)
            group->add(this);
    }

    void
    ConfigToggleItemImpl::toggleValue()
    {
        value = !value;
        if (changedCallback)
            changedCallback(*this);
        if (group && value)
            group->notifyEnabled(this);
    }

    void
    ConfigToggleItemImpl::getCurrentValueDisplay(std::span<char> output)
    {
        using enum ConfigToggleItemGroup::Mode;
        auto mode = group ? group->getMode() : Multi;
        switch (mode) {
            case Single:
                std::snprintf(output.data(),
                              output.size(),
                              "%s",
                              value ? "◉" : "○");
                break;
            case Multi:
                std::snprintf(output.data(),
                              output.size(),
                              "%s",
                              value ? "■" : "□");
                break;
        }
    }

    void
    ConfigToggleItemImpl::getCurrentValueSelectedDisplay(std::span<char> output)
    {
        using enum ConfigToggleItemGroup::Mode;
        auto mode = group ? group->getMode() : Multi;
        switch (mode) {
            case Single:
                std::snprintf(output.data(),
                              output.size(),
                              "(Press %s to change) %s",
                              CAFE_GLYPH_BTN_A,
                              value ? "◉" : "○");
                break;
            case Multi:
                std::snprintf(output.data(),
                              output.size(),
                              "(Press %s to change) %s",
                              CAFE_GLYPH_BTN_A,
                              value ? "■" : "□");
                break;
        }
    }

    void
    ConfigToggleItemImpl::onInput(const WUPSConfigSimplePadData& input)
    {
        if ((input.buttons_d & WUPS_CONFIG_BUTTON_A) == WUPS_CONFIG_BUTTON_A) {
            toggleValue();
        }
    }

    ConfigToggleItem::ConfigToggleItem(
        const std::string& displayName,
        const std::string& identifier,
        bool value,
        ConfigToggleItemImpl::CallbackFunction changedCallback,
        ConfigToggleItemGroup* group
    ) :
        WUPSConfigItem{
            ConfigItemImplBase::make_handle(
                std::make_unique<ConfigToggleItemImpl>(
                    displayName,
                    identifier,
                    value,
                    std::move(changedCallback),
                    group
                )
            )
        }
    {}

    ConfigToggleItemGroup::ConfigToggleItemGroup(Mode mode_) :
        mode{mode_}
    {}

    ConfigToggleItemGroup::Mode
    ConfigToggleItemGroup::getMode()
        const
    {
        return mode;
    }

    void
    ConfigToggleItemGroup::setMode(Mode mode_)
    {
        mode = mode_;
        if (mode == Mode::Single) {
            // Ensure no more than one item is selected.
            bool found_one = false;
            for (auto item : items) {
                if (!item->value)
                    continue;

                if (found_one) {
                    item->value = false;
                } else {
                    found_one = true;
                }
            }
        }
    }

    void
    ConfigToggleItemGroup::add(ConfigToggleItemImpl* item)
    {
        items.push_back(item);
    }

    void
    ConfigToggleItemGroup::unselectAll()
    {
        for (auto item : items)
            item->value = false;
    }

    void
    ConfigToggleItemGroup::notifyEnabled(ConfigToggleItemImpl* item)
    {
        if (mode == Mode::Single) {
            for (auto it : items)
                if (it != item) [[likely]]
                    it->value = false;
        }
    }

    void
    ConfigToggleItemGroup::forEach(const ForEachCallbackFunction& callback)
    {
        if (!callback) [[unlikely]]
            return;
        for (auto it : items)
            callback(*it);
    }

} // namespace wupsext
