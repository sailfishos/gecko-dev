/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUserIdleServiceQt_h__
#define nsUserIdleServiceQt_h__

#include "nsUserIdleService.h"

#if defined(MOZ_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct {
    Window window;              // Screen saver window
    int state;                  // ScreenSaver(Off,On,Disabled)
    int kind;                   // ScreenSaver(Blanked,Internal,External)
    unsigned long til_or_since; // milliseconds since/til screensaver kicks in
    unsigned long idle;         // milliseconds idle
    unsigned long event_mask;   // event stuff
} XScreenSaverInfo;
#endif

class nsUserIdleServiceQt : public nsUserIdleService
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    bool PollIdleTime(uint32_t* aIdleTime);

    static already_AddRefed<nsUserIdleServiceQt> GetInstance()
    {
        RefPtr<nsUserIdleServiceQt> idleService =
            nsUserIdleService::GetInstance().downcast<nsUserIdleServiceQt>();
        if (!idleService) {
            idleService = new nsUserIdleServiceQt();
        }

        return idleService.forget();
    }

private:
#if defined(MOZ_X11)
    XScreenSaverInfo* mXssInfo;
#endif

protected:
    nsUserIdleServiceQt();
    virtual ~nsUserIdleServiceQt();
    bool UsePollMode();
};

#endif // nsUserIdleServiceQt_h__
