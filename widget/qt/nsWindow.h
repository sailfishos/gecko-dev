/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWindow_h__
#define __nsWindow_h__

#include <QPointF>

#include "nsBaseWidget.h"
#include "mozilla/EventForwards.h"

#include "nsGkAtoms.h"
#include "nsIIdleServiceInternal.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

#ifdef MOZ_LOGGING

#include "mozilla/Logging.h"
#include "nsTArray.h"

extern mozilla::LazyLogModule gWidgetLog;
extern mozilla::LazyLogModule gWidgetFocusLog;
extern mozilla::LazyLogModule gWidgetIMLog;
extern mozilla::LazyLogModule gWidgetDrawLog;

#define LOG(args) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, args)
#define LOGFOCUS(args) MOZ_LOG(gWidgetFocusLog, mozilla::LogLevel::Debug, args)
#define LOGIM(args) MOZ_LOG(gWidgetIMLog, mozilla::LogLevel::Debug, args)
#define LOGDRAW(args) MOZ_LOG(gWidgetDrawLog, mozilla::LogLevel::Debug, args)

#else

#ifdef DEBUG_WIDGETS

#define PR_LOG2(_args)         \
    PR_BEGIN_MACRO             \
      qDebug _args;            \
    PR_END_MACRO

#define LOG(args) PR_LOG2(args)
#define LOGFOCUS(args) PR_LOG2(args)
#define LOGIM(args) PR_LOG2(args)
#define LOGDRAW(args) PR_LOG2(args)

#else

#define LOG(args)
#define LOGFOCUS(args)
#define LOGIM(args)
#define LOGDRAW(args)

#endif

#endif /* MOZ_LOGGING */

class QCloseEvent;
class QFocusEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QResizeEvent;
class QShowEvent;
class QTabletEvent;
class QTouchEvent;
class QWheelEvent;

namespace mozilla {
namespace widget {
class MozQWidget;
class nsWindow : public nsBaseWidget
{
public:
    nsWindow();

    NS_DECL_ISUPPORTS_INHERITED

    // nsIWidget
    using nsBaseWidget::Create; // for Create signature not overridden here
    virtual MOZ_MUST_USE nsresult Create(nsIWidget* aParent,
                                         nsNativeWidget aNativeParent,
                                         const LayoutDeviceIntRect& aRect,
                                         nsWidgetInitData* aInitData) override;
    virtual void Destroy() override;

    virtual void Show(bool aState) override;
    virtual bool IsVisible() const;
    virtual void ConstrainPosition(bool aAllowSlop,
                                 int32_t *aX,
                                 int32_t *aY) override;
    virtual void Move(double aX, double aY) override;
    virtual void Resize(double aWidth,
                        double aHeight,
                        bool   aRepaint) override;
    virtual void Resize(double aX, double aY, double aWidth, double aHeight,
                        bool aRepaint) override;
    virtual void Enable(bool aState) override;
    // Some of the nsIWidget methods
    virtual bool IsEnabled() const override;
    virtual nsresult SetFocus(bool aRaise = false) override;
    virtual nsresult ConfigureChildren(
        const nsTArray<nsIWidget::Configuration>&) override;
    virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;
    virtual void* GetNativeData(uint32_t aDataType) override;
    virtual nsresult SetTitle(const nsAString &aTitle) override;
    virtual void SetCursor(nsCursor aCursor) override;
    virtual nsresult SetCursor(imgIContainer* aCursor, uint32_t aHotspotX,
                               uint32_t aHotspotY) override
    {
        return NS_OK;
    }
    virtual LayoutDeviceIntPoint WidgetToScreenOffset();
    virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                   nsEventStatus& aStatus) override;
    virtual void CaptureRollupEvents(nsIRollupListener *aListener,
                                   bool aDoCapture) override
    {
      (void)aListener;
      (void)aDoCapture;
    }

    virtual void ReparentNativeWidget(nsIWidget* aNewParent) override;

    virtual nsresult MakeFullScreen(bool aFullScreen,
                                    nsIScreen* aTargetScreen = nullptr) override;
    virtual mozilla::layers::LayerManager*
        GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                        LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT);

    virtual void SetInputContext(const InputContext& aContext,
                                 const InputContextAction& aAction) override;
    virtual InputContext GetInputContext() override;

    virtual uint32_t GetGLFrameBufferFormat() override;

    already_AddRefed<mozilla::gfx::DrawTarget> StartRemoteDrawing() override;

    // Widget notifications
    virtual void OnPaint();
    virtual nsEventStatus focusInEvent(QFocusEvent* aEvent);
    virtual nsEventStatus focusOutEvent(QFocusEvent* aEvent);
    virtual nsEventStatus hideEvent(QHideEvent* aEvent);
    virtual nsEventStatus showEvent(QShowEvent* aEvent);
    virtual nsEventStatus keyPressEvent(QKeyEvent* aEvent);
    virtual nsEventStatus keyReleaseEvent(QKeyEvent* aEvent);
    virtual nsEventStatus mouseDoubleClickEvent(QMouseEvent* aEvent);
    virtual nsEventStatus mouseMoveEvent(QMouseEvent* aEvent);
    virtual nsEventStatus mousePressEvent(QMouseEvent* aEvent);
    virtual nsEventStatus mouseReleaseEvent(QMouseEvent* aEvent);
    virtual nsEventStatus moveEvent(QMoveEvent* aEvent);
    virtual nsEventStatus resizeEvent(QResizeEvent* aEvent);
    virtual nsEventStatus touchEvent(QTouchEvent* aEvent);
    virtual nsEventStatus wheelEvent(QWheelEvent* aEvent);
    virtual nsEventStatus tabletEvent(QTabletEvent* event);

protected:
    virtual ~nsWindow();

    nsWindow* mParent;
    bool  mVisible;
    InputContext mInputContext;
    nsCOMPtr<nsIIdleServiceInternal> mIdleService;
    MozQWidget* mWidget;

private:
    // event handling code
    nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);
    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchActivateEventOnTopLevelWindow(void);
    void DispatchDeactivateEventOnTopLevelWindow(void);
    void DispatchResizeEvent(LayoutDeviceIntRect &aRect,
                             nsEventStatus &aStatus);

    // Remember the last sizemode so that we can restore it when
    // leaving fullscreen
    nsSizeMode mLastSizeMode;
    // is this widget enabled?
    bool mEnabled;

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();
    MozQWidget* createQWidget(MozQWidget* parent,
                              nsWidgetInitData* aInitData);

public:
    // Old QtWidget only
    virtual void SetParent(nsIWidget* aNewParent) override;
    virtual nsIWidget *GetParent() override;
    virtual float GetDPI() override;
    virtual void SetModal(bool aModal) override;

    virtual void SetSizeMode(nsSizeMode aMode) override;
    virtual LayoutDeviceIntRect GetScreenBounds() override;

    virtual void HideWindowChrome(bool aShouldHide) override;
    virtual void SetIcon(const nsAString& aIconSpec) override;
    virtual void CaptureMouse(bool aCapture) override;
    virtual void SetWindowClass(const nsAString& xulWinType) override;
    virtual MOZ_MUST_USE nsresult GetAttention(int32_t aCycleCount) override;

    //
    // utility methods
    //
    void QWidgetDestroyed();
    // called when we are destroyed
    virtual void OnDestroy(void) override;
    // called to check and see if a widget's dimensions are sane
    bool AreBoundsSane(void);
private:
    // Is this a toplevel window?
    bool mIsTopLevel;
    // Has this widget been destroyed yet?
    bool mIsDestroyed;
    // This flag tracks if we're hidden or shown.
    bool mIsShown;
    // Has anyone set an x/y location for this widget yet? Toplevels
    // shouldn't be automatically set to 0,0 for first show.
    bool mPlaced;
    /**
     * Event handlers (proxied from the actual qwidget).
     * They follow normal Qt widget semantics.
     */
    void Initialize(MozQWidget *widget);
    virtual nsEventStatus OnCloseEvent(QCloseEvent *);
    void NativeResize(int32_t aWidth,
                      int32_t aHeight,
                      bool aRepaint);
    void NativeResize(int32_t aX,
                      int32_t aY,
                      int32_t aWidth,
                      int32_t aHeight,
                      bool aRepaint);
    void NativeShow(bool aAction);

private:
    typedef struct {
        QPointF pos;
        Qt::KeyboardModifiers modifiers;
        bool needDispatch;
    } MozCachedMoveEvent;

    nsIWidgetListener* GetPaintListener();
    bool CheckForRollup(double aMouseX, double aMouseY, bool aIsWheel);
    void* SetupPluginPort(void);
    nsresult SetWindowIconList(const nsTArray<nsCString> &aIconList);
    void SetDefaultIcon(void);

    nsEventStatus DispatchCommandEvent(nsAtom* aCommand);
    nsEventStatus DispatchContentCommandEvent(mozilla::EventMessage aMsg);
    void SetSoftwareKeyboardState(bool aOpen, const InputContextAction& aAction);
    void ClearCachedResources();

    uint32_t mActivatePending : 1;
    int32_t mSizeState;

    // all of our DND stuff
    // this is the last window that had a drag event happen on it.
    void InitDragEvent(mozilla::WidgetMouseEvent& aEvent);

    // this is everything we need to be able to fire motion events
    // repeatedly
    uint32_t mKeyDownFlags[8];

    /* Helper methods for DOM Key Down event suppression. */
    uint32_t* GetFlagWord32(uint32_t aKeyCode, uint32_t* aMask) {
        /* Mozilla DOM Virtual Key Code is from 0 to 224. */
        NS_ASSERTION((aKeyCode <= 0xFF), "Invalid DOM Key Code");
        aKeyCode &= 0xFF;

        /* 32 = 2^5 = 0x20 */
        *aMask = uint32_t(1) << (aKeyCode & 0x1F);
        return &mKeyDownFlags[(aKeyCode >> 5)];
    }

    bool IsKeyDown(uint32_t aKeyCode) {
        uint32_t mask;
        uint32_t* flag = GetFlagWord32(aKeyCode, &mask);
        return ((*flag) & mask) != 0;
    }

    void SetKeyDownFlag(uint32_t aKeyCode) {
        uint32_t mask;
        uint32_t* flag = GetFlagWord32(aKeyCode, &mask);
        *flag |= mask;
    }

    void ClearKeyDownFlag(uint32_t aKeyCode) {
        uint32_t mask;
        uint32_t* flag = GetFlagWord32(aKeyCode, &mask);
        *flag &= ~mask;
    }
    int32_t mQCursor;


    void ProcessMotionEvent();
    void DispatchMotionToMainThread();

    bool mNeedsResize;
    bool mNeedsMove;
    bool mListenForResizes;
    bool mNeedsShow;
    MozCachedMoveEvent mMoveEvent;
    bool mTimerStarted;
};

}}

#endif /* __nsWindow_h__ */
