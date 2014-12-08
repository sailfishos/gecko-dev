
#ifndef __gen_EmbedLiteAppChildIface_h__
#define __gen_EmbedLiteAppChildIface_h__

#include "EmbedLiteViewChildIface.h"

class nsIWebBrowserChrome;
namespace mozilla {
namespace embedlite {

class EmbedLiteAppChildIface
{
public:
  virtual EmbedLiteViewChildIface* GetViewByID(uint32_t aId) = 0;
  virtual EmbedLiteViewChildIface* GetViewByChromeParent(nsIWebBrowserChrome* aParent) = 0;
  virtual bool CreateWindow(const uint32_t& parentId, const nsCString& uri, const uint32_t& chromeFlags, const uint32_t& contextFlags, uint32_t* createdID, bool* cancel) = 0;
};

}}

#endif // __gen_EmbedLiteAppChildIface_h__
