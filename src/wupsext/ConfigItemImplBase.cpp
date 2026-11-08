#include <cstdint>

#include <wups/config_api.h>

#include "ConfigItemImplBase.h"

#include "Error.h"

namespace wupsext {

    namespace {

        std::int32_t
        wrap_getCurrentValueDisplay(void* ctx,
                                    char* out_buf,
                                    std::int32_t out_size)
        {
            try {
                auto self = reinterpret_cast<ConfigItemImplBase*>(ctx);
                assert(self);
                self->getCurrentValueDisplay(
                    std::span{out_buf, static_cast<std::size_t>(out_size)}
                );
                return 0;
            }
            catch (...) {
                return -1;
            }
        }

        std::int32_t
        wrap_getCurrentValueSelectedDisplay(void *ctx,
                                            char *out_buf,
                                            std::int32_t out_size)
        {
            try {
                auto self = reinterpret_cast<ConfigItemImplBase*>(ctx);
                assert(self);
                self->getCurrentValueSelectedDisplay(
                    std::span{out_buf, static_cast<std::size_t>(out_size)}
                );
                return 0;
            }
            catch (...) {
                return -1;
            }
        }

        void
        wrap_onInput(void *ctx,
                     WUPSConfigSimplePadData input)
        {
            try {
                auto self = reinterpret_cast<ConfigItemImplBase*>(ctx);
                assert(self);
                self->onInput(input);
            }
            catch (...) {}
        }

        void
        wrap_onDelete(void *ctx)
        {
            auto self = reinterpret_cast<ConfigItemImplBase*>(ctx);
            delete self;
        }

    } // namespace

    ConfigItemImplBase::ConfigItemImplBase(const std::string& displayName_,
                                           const std::string& identifier_) :
        displayName{displayName_},
        identifier{identifier_}
    {}

    ConfigItemImplBase::~ConfigItemImplBase()
        noexcept
    {}

    void
    ConfigItemImplBase::getCurrentValueSelectedDisplay(std::span<char> output)
    {
        getCurrentValueDisplay(output);
    }

    void
    ConfigItemImplBase::onInput(const WUPSConfigSimplePadData&)
    {}

    WUPSConfigItemHandle
    ConfigItemImplBase::make_handle(std::unique_ptr<ConfigItemImplBase>&& impl)
    {
        WUPSConfigAPIItemCallbacksV2 callbacks = {
            .getCurrentValueDisplay         = &wrap_getCurrentValueDisplay,
            .getCurrentValueSelectedDisplay = &wrap_getCurrentValueSelectedDisplay,
            .onSelected                     = nullptr,
            .restoreDefault                 = nullptr,
            .isMovementAllowed              = nullptr,
            .onCloseCallback                = nullptr,
            .onInput                        = &wrap_onInput,
            .onInputEx                      = nullptr,
            .onDelete                       = &wrap_onDelete,
        };

        WUPSConfigAPIItemOptionsV2 options = {
            .displayName = impl->displayName.data(),
            .context     = impl.get(),
            .callbacks   = callbacks,
        };

        WUPSConfigItemHandle result;
        if (auto e = WUPSConfigAPI_Item_Create(options, &result))
            throw Error{e, "WUPSConfigAPI_Item_Create() failed"};

        impl.release();

        return result;
    }

} // namespace wupsext
