
#include "EmbedLiteViewProcessChild.h"

namespace mozilla {
namespace embedlite {

MOZ_IMPLICIT
EmbedLiteViewProcessChild::EmbedLiteViewProcessChild(const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow)
  : EmbedLiteViewBaseChild(id, parentId, isPrivateWindow)
{
  LOGT();
}

MOZ_IMPLICIT EmbedLiteViewProcessChild::~EmbedLiteViewProcessChild()
{
  LOGT();
}

}  // namespace embedlite
}  // namespace mozilla
