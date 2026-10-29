#include "themeSelector.h"
#include "globals.h"
#include <content_redirection/redirection.h>
#include <string>
#include <utils/logger.h>

using namespace std::literals;

bool ReplaceContent(const std::filesystem::path& men,
                    const std::filesystem::path& men2,
                    const std::filesystem::path& cafeBarista,
                    const std::filesystem::path& contentExtraPath,
                    const std::flat_set<std::filesystem::path>& bfstmSet) {
    const auto layerName = "StyleMiiU /vol/content"s;

    bool result = false;

    if (!contentExtraPath.empty()) {
        const auto target = "/vol/content"s;
        auto e = ContentRedirection_AddFSLayerEx(&gContentLayerHandle,
                                                 layerName.c_str(),
                                                 target.c_str(),
                                                 contentExtraPath.c_str(),
                                                 FS_LAYER_TYPE_EX_MERGE_DIRECTORY);
        if (e)
            DEBUG_FUNCTION_LINE_ERR("Failed to redirect \"%s\" to \"%s\": %s",
                                    target.c_str(),
                                    cafeBarista.c_str(),
                                    ContentRedirection_GetStatusStr(e));
        else
            result = true;
    }

    if (!men.empty()) {
        const auto target = "/vol/content/Common/Package/Men.pack"s;
        auto e = ContentRedirection_AddFSLayerEx(&gContentLayerHandle,
                                                 layerName.c_str(),
                                                 target.c_str(),
                                                 men.c_str(),
                                                 FS_LAYER_TYPE_EX_REPLACE_FILE);
        if (e)
            DEBUG_FUNCTION_LINE_ERR("Failed to redirect \"%s\" to \"%s\": %s",
                                    target.c_str(),
                                    men.c_str(),
                                    ContentRedirection_GetStatusStr(e));
        else
            result = true;
    }

    if (!men2.empty()) {
        const auto target = "/vol/content/Common/Package/Men2.pack"s;
        auto e = ContentRedirection_AddFSLayerEx(&gContentLayerHandle,
                                                 layerName.c_str(),
                                                 target.c_str(),
                                                 men2.c_str(),
                                                 FS_LAYER_TYPE_EX_REPLACE_FILE);
        if (e)
            DEBUG_FUNCTION_LINE_ERR("Failed to redirect \"%s\" to \"%s\": %s",
                                    target.c_str(),
                                    men2.c_str(),
                                    ContentRedirection_GetStatusStr(e));
        else
            result = true;

    }

    if (!cafeBarista.empty()) {
        const auto target = "/vol/content/Common/Sound/Men/cafe_barista_men.bfsar"s;
        auto e = ContentRedirection_AddFSLayerEx(&gContentLayerHandle,
                                                 layerName.c_str(),
                                                 target.c_str(),
                                                 cafeBarista.c_str(),
                                                 FS_LAYER_TYPE_EX_REPLACE_FILE);
        if (e)
            DEBUG_FUNCTION_LINE_ERR("Failed to redirect \"%s\" to \"%s\": %s",
                                    target.c_str(),
                                    cafeBarista.c_str(),
                                    ContentRedirection_GetStatusStr(e));
        else
            result = true;
    }

    for (const auto& bfstm : bfstmSet) {
        auto target = "/vol/content/Common/Sound/Men/stream" / bfstm.filename();
        auto e = ContentRedirection_AddFSLayerEx(&gContentLayerHandle,
                                                 layerName.c_str(),
                                                 target.c_str(),
                                                 bfstm.c_str(),
                                                 FS_LAYER_TYPE_EX_REPLACE_FILE);
        if (e)
            DEBUG_FUNCTION_LINE_ERR("Failed to redirect \"%s\" to \"%s\": %s",
                                    target.c_str(),
                                    bfstm.c_str(),
                                    ContentRedirection_GetStatusStr(e));
        else
            result = true;
    }

    return result;
}
