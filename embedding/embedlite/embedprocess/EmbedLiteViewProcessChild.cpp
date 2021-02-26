
#include "EmbedLiteViewProcessChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/ImageBridgeChild.h"

using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {

MOZ_IMPLICIT
EmbedLiteViewProcessChild::EmbedLiteViewProcessChild(const uint32_t& windowId,
		                                     const uint32_t& id,
						     const uint32_t& parentId,
						     const bool& isPrivateWindow)
  : EmbedLiteViewChild(windowId, id, parentId, isPrivateWindow)
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
  PLayerTransactionChild* shadowManager = nullptr;
  nsTArray<LayersBackend> backends;
  backends.AppendElement(LayersBackend::LAYERS_OPENGL);

  shadowManager = compositorChild->SendPLayerTransactionConstructor(backends, 1);
  if (!shadowManager) {
    NS_WARNING("failed to construct LayersChild");
    // This results in |remoteFrame| being deleted.
    return;
  }

  ShadowLayerForwarder* lf =
    WebWidget()->GetLayerManager(shadowManager, textureFactoryIdentifier.mParentBackend)->AsShadowForwarder();

  NS_ASSERTION(lf && lf->HasShadowManager(),
                    "PuppetWidget should have shadow manager");
  lf->IdentifyTextureHost(textureFactoryIdentifier);
  ImageBridgeChild::IdentifyCompositorTextureHost(textureFactoryIdentifier);
}

}  // namespace embedlite
}  // namespace mozilla
