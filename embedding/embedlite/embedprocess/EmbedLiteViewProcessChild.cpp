
#include "EmbedLiteViewProcessChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/layers/ImageBridgeChild.h"

using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {

MOZ_IMPLICIT
EmbedLiteViewProcessChild::EmbedLiteViewProcessChild(const uint32_t &windowId,
                                                     const uint32_t &id,
                                                     const uint32_t &parentId,
                                                     mozilla::dom::BrowsingContext *parentBrowsingContext,
                                                     const bool &isPrivateWindow,
                                                     const bool &isDesktopMode,
                                                     const bool &isHidden)
  : EmbedLiteViewChild(windowId, id, parentId, parentBrowsingContext, isPrivateWindow, isDesktopMode, isHidden)
{
  LOGT();
}

MOZ_IMPLICIT EmbedLiteViewProcessChild::~EmbedLiteViewProcessChild()
{
  LOGT();
}

void
EmbedLiteViewProcessChild::OnGeckoWindowInitialized()
{
  LOGT();

  // Pushing layers transactions directly to a separate
  // compositor context.
  CompositorBridgeChild* compositorChild = CompositorBridgeChild::Get();
  if (!compositorChild) {
    NS_WARNING("failed to get CompositorBridgeChild instance");
    return;
  }

  TextureFactoryIdentifier textureFactoryIdentifier;
  nsTArray<LayersBackend> backends;
  backends.AppendElement(LayersBackend::LAYERS_WR);

  //FIXME
  WebWidget()->GetWindowRenderer();

  ImageBridgeChild::IdentifyCompositorTextureHost(textureFactoryIdentifier);
}

}  // namespace embedlite
}  // namespace mozilla
