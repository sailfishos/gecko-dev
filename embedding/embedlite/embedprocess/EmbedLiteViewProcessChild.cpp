
#include "FrameMetrics.h"
#include "EmbedLiteViewProcessChild.h"
#include "EmbedLog.h"
#include "nsEmbedCID.h"
#include "nsIBaseWindow.h"
#include "EmbedLitePuppetWidget.h"
#include "nsIDOMWindowUtils.h"
#include "nsEmbedCID.h"
#include "nsIIOService.h"
#include "nsNetCID.h"

using namespace mozilla::layers;
using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

MOZ_IMPLICIT
EmbedLiteViewProcessChild::EmbedLiteViewProcessChild(const uint32_t& id, const uint32_t& parentId)
  : EmbedLiteViewBaseChild(id, parentId)
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteViewProcessChild);
}

MOZ_IMPLICIT EmbedLiteViewProcessChild::~EmbedLiteViewProcessChild()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteViewProcessChild);
}


}  // namespace embedlite
}  // namespace mozilla
