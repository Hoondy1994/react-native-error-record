/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License,Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SDL_OHOS_H
#define SDL_OHOS_H
#include "SDL_ohosfile.h"
#include <EGL/eglplatform.h>
#include "SDL_system.h"
#include "SDL_audio.h"
#include "SDL_rect.h"
#include "SDL_video.h"
#include "../../SDL_internal.h"
#include "napi/native_api.h"
#include "SDL_mutex.h"



#define ARGV_TYPE_ZERO 0
#define ARGV_TYPE_ONE 1
#define ARGV_TYPE_TWO 2
#define ARGV_TYPE_THREE 3
#define ARGV_TYPE_FOUR 4
#define ARGV_TYPE_FIVE 5

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef struct {
    int x;
    int y;
    int width;
    int height;
} WindowPosition;

typedef struct SDL_WindowResizeSync {
    SDL_mutex   *sizeChangeMutex;
    SDL_cond    *sizeChangeCond;
    bool        isChangeing;
} SDL_WindowResizeSync;



extern SDL_WindowResizeSync *g_ohosResizeSync;

extern SDL_DisplayOrientation displayOrientation;
extern SDL_atomic_t bPermissionRequestPending;
extern SDL_bool bPermissionRequestResult;

/* Cursor support */
extern int OHOS_CreateCustomCursor(SDL_Surface *xcomponent, int hotX, int hotY);
extern SDL_bool OHOS_SetCustomCursor(int cursorID);
extern SDL_bool OHOS_SetSystemCursor(int cursorID);

/* Relative mouse support */
extern SDL_bool OHOS_SupportsRelativeMouse(void);
extern SDL_bool OHOS_SetRelativeMouseEnabled(SDL_bool enabled);

extern void OHOS_PAGEMUTEX_Lock(void);
extern void OHOS_PAGEMUTEX_Unlock(void);
extern void OHOS_PAGEMUTEX_LockRunning(void);

extern void OHOS_SetDisplayOrientation(int orientation);
extern SDL_DisplayOrientation OHOS_GetDisplayOrientation();
extern void OHOS_NAPI_ShowTextInput(int x, int y, int w, int h);
extern SDL_bool OHOS_NAPI_RequestPermission(const char *Permission);
extern SDL_bool OHOS_NAPI_AddChildNode(void* parent, void **child, char *xcompentId, WindowPosition *windowPosition);
extern SDL_bool OHOS_NAPI_RemoveChildNode(char *xcompentId);
extern SDL_bool OHOS_NAPI_ResizeNode(char *xcompentId, int width, int height);
extern SDL_bool OHOS_NAPI_MoveNode(char *xcompentId, int x, int y);
extern SDL_bool OHOS_NAPI_ShowNode(char *xcompentId);
extern SDL_bool OHOS_NAPI_HideNode(char *xcompentId);
extern SDL_bool OHOS_NAPI_RaiseNode(char *xcompentId);
extern void OHOS_NAPI_HideTextInput(int a);
extern void OHOS_NAPI_ShouldMinimizeOnFocusLoss(int a);
extern void OHOS_NAPI_SetTitle(const char *title);
extern void OHOS_NAPI_SetWindowStyle(SDL_bool fullscreen);
extern void OHOS_NAPI_SetOrientation(int w, int h, int resizable, const char *hint);
extern void OHOS_NAPI_ShowTextInputKeyboard(SDL_bool fullscreen);
extern void OHOS_NAPI_SetWindowResize(int x, int y, int w, int h);

extern void OHOS_SetRootNode(void **parent);
extern void OHOS_GetRootNode(void **parent);
extern bool OHOS_AddRootNode(char *xcompentId, WindowPosition *windowPosition);
extern bool OHOS_AddChildNode(void *parent, void **child, char *xcompentId, WindowPosition *windowPosition);
extern bool OHOS_RemoveChildNode(char *xcompentId);
extern bool OHOS_ResizeNode(char *xcompentId, int width, int height);
extern bool OHOS_ReParentNode(char *xcompentParentId, char *xcompentId);
extern bool OHOS_MoveNode(char *xcompentId, int x, int y);
extern bool OHOS_ShowNode(char *xcompentId);
extern bool OHOS_HideNode(char *xcompentId);
extern bool OHOS_RaiseNode(char *xcompentId);
extern bool OHOS_LowerNode(char *xcompentId);

extern bool OHOS_GetXComponentPos(const void *node, WindowPosition *windowPosition);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}

//#include <memory>
//#include "adapter_c/SDL_AdapterC.h"
//extern bool OHOS_AddChildNode(void *parent, std::shared_ptr<SDL_EMBEDDED_WINDOW_ADAPTER::Node> child, char *xcompentId, WindowPosition *windowPosition);
/* *INDENT-ON* */
#endif

#endif /* SDL_OHOS_H */

/* vi: set ts=4 sw=4 expandtab: */
