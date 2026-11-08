// -*- mode: c++ -*-

#pragma once

#include <memory>
#include <span>
#include <string>

#include <wups/config.h>

namespace wupsext {

    struct ConfigItemImplBase {

        std::string displayName;
        std::string identifier;

        ConfigItemImplBase(const std::string& displayName_,
                           const std::string& identifier_);

        virtual
        ~ConfigItemImplBase()
            noexcept;

        // Prevent moving
        ConfigItemImplBase(ConfigItemImplBase&&) = delete;

        virtual
        void
        getCurrentValueDisplay(std::span<char> output) = 0;

        virtual
        void
        getCurrentValueSelectedDisplay(std::span<char> output);

        virtual
        void
        onInput(const WUPSConfigSimplePadData& input);


        static
        WUPSConfigItemHandle
        make_handle(std::unique_ptr<ConfigItemImplBase>&& impl);

    };

} // namespace wupsext
