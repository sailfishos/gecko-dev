#include "EmbedLiteViewProcessParent.h"

#include "EmbedLog.h"

namespace mozilla {
namespace embedlite {

MOZ_IMPLICIT EmbedLiteViewProcessParent::EmbedLiteViewProcessParent(const uint32_t& windowId,
                                                                    const uint32_t& id,
                                                                    const uint32_t& parentId,
                                                                    const bool& isPrivateWindow,
                                                                    const bool& isDesktopMode)
  : EmbedLiteViewParent(windowId, id, parentId, isPrivateWindow, isDesktopMode)
{
    LOGT();
}

MOZ_IMPLICIT EmbedLiteViewProcessParent::~EmbedLiteViewProcessParent()
{
    LOGT();
}

}  // namespace embedlite
}  // namespace mozilla
