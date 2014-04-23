/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "GLContext.h"
#include "EmbedLiteRenderTarget.h"
#include "GLContextProvider.h"

using namespace mozilla::gl;
using namespace mozilla::embedlite;

EmbedLiteRenderTarget::EmbedLiteRenderTarget(void* aContext, void* aSurface)
{
  nsRefPtr<GLContext> ctx = GLContextProvider::CreateWrappingExisting(aContext, aSurface);

  MOZ_ASSERT(ctx);
  mGLContext = ctx;
}

bool EmbedLiteRenderTarget::EnsureInitialized()
{
  MOZ_ASSERT(mGLContext);
  return mGLContext->Init();
}

EmbedLiteRenderTarget::~EmbedLiteRenderTarget()
{
}
