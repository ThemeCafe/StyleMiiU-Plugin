#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>

#include <wups/config_api.h>

#include <cafe_glyphs.h>

#include "ConfigBooleanVarItem.h"

namespace wupsext {

    ConfigBooleanVarItemImpl::ConfigBooleanVarItemImpl(
        const std::string &displayName_,
        const std::string &identifier_,
        bool &variable_,
        CallbackFunction changedCallback_,
        const std::optional<std::string> &trueLabel_,
        const std::optional<std::string> &falseLabel_
    ) :
        ConfigItemImplBase{displayName_, identifier_},
        variable(variable_),
        changedCallback{std::move(changedCallback_)},
        trueLabel{trueLabel_.value_or("yes")},
        falseLabel{falseLabel_.value_or("no")}
    {}

    void
    ConfigBooleanVarItemImpl::toggleValue()
    {
        variable = !variable;
        if (changedCallback)
            changedCallback(*this);
    }

    void
    ConfigBooleanVarItemImpl::getCurrentValueDisplay(std::span<char> output)
    {
        std::snprintf(output.data(),
                      output.size(),
                      "%s",
                      variable ? trueLabel.data() : falseLabel.data());
    }

    void
    ConfigBooleanVarItemImpl::getCurrentValueSelectedDisplay(std::span<char> output)
    {
        if (variable)
            std::snprintf(output.data(),
                          output.size(),
                          "%s  %s  %s",
                          CAFE_GLYPH_BTN_DPAD_LEFT,
                          trueLabel.data(),
                          CAFE_GLYPH_BTN_DPAD_NEUTRAL);
        else
            std::snprintf(output.data(),
                          output.size(),
                          "  %s  %s  %s",
                          CAFE_GLYPH_BTN_DPAD_NEUTRAL,
                          falseLabel.data(),
                          CAFE_GLYPH_BTN_DPAD_RIGHT);
    }

    void
    ConfigBooleanVarItemImpl::onInput(const WUPSConfigSimplePadData& input)
    {
        if ((input.buttons_d & WUPS_CONFIG_BUTTON_A) == WUPS_CONFIG_BUTTON_A)
            toggleValue();
        else if (input.buttons_d & WUPS_CONFIG_BUTTON_LEFT && variable)
            toggleValue();
        else if ((input.buttons_d & WUPS_CONFIG_BUTTON_RIGHT) && !variable)
            toggleValue();
    }

    ConfigBooleanVarItem::ConfigBooleanVarItem(
        const std::string &displayName,
        const std::string &identifier,
        bool &variable,
        ConfigBooleanVarItemImpl::CallbackFunction callback,
        const std::optional<std::string> &trueLabel,
        const std::optional<std::string> &falseLabel
    ) :
        WUPSConfigItem{
            ConfigItemImplBase::make_handle(
                std::make_unique<ConfigBooleanVarItemImpl>(
                    displayName,
                    identifier,
                    variable,
                    std::move(callback),
                    trueLabel,
                    falseLabel
                )
            )
        }
    {}

} // namespace wupsext
