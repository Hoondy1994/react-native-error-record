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

#include "../../SDL_internal.h"
#include "../../core/ohos/SDL_ohosplugin_c.h"
#include "SDL_log.h"
#include "SDL_timer.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <locale.h>
#include <unistd.h>

#define OHOS_EGL_ALPHA_SIZE_DEFAULT 8
#define OHOS_GETWINDOW_DELAY_TIME 2
#define TIMECONSTANT 3000
#define OHOS_XCOMPONENT_STRING "SDL2_XComponent"
#define OHOS_WAIT_COUNT 700
#define OHOS_WAIT_TIME 10

#ifdef SDL_VIDEO_DRIVER_OHOS
#if SDL_VIDEO_DRIVER_OHOS
#endif

#include "napi/native_api.h"
#include "SDL_system.h"
#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../core/ohos/SDL_ohos.h"
#include "../../core/ohos/SDL_ohosplugin_c.h"

#include "SDL_ohosvideo.h"
#include "SDL_ohoswindow.h"
#include "SDL_hints.h"
#include <pthread.h>

static char *OHOS_GenerateNewXComponentId()
{
    static int id = 1;
    char *pXCompentId = (char *)SDL_malloc(OH_XCOMPONENT_ID_LEN_MAX + 1);
    if (!pXCompentId) {
        return NULL;
    }

    SDL_memset(pXCompentId, 0, OH_XCOMPONENT_ID_LEN_MAX + 1);
    SDL_snprintf(pXCompentId, OH_XCOMPONENT_ID_LEN_MAX, "%s_%d", OHOS_XCOMPONENT_STRING, id);
    id++;

    SDL_Log("Generate a new XCompentId = %s.", pXCompentId);
    return pXCompentId;
}

int OHOS_CreateWindow(SDL_VideoDevice *thisDevice, SDL_Window * window)
{
    void *parentWindowNode = NULL;
    WindowPosition *windowPosition = NULL;
    if (window->ohosHandle == NULL) {
        OHOS_GetRootNode(&parentWindowNode);
        if (parentWindowNode == NULL) {
            return -1;
        }
        windowPosition = (WindowPosition*)SDL_malloc(sizeof(WindowPosition));
        windowPosition->height = window->h;
        windowPosition->width = window->w;
        windowPosition->x = window->x;
        windowPosition->y = window->y;
        window->xcompentId = OHOS_GenerateNewXComponentId();
        OHOS_NAPI_AddChildNode(parentWindowNode, &window->ohosHandle, window->xcompentId, windowPosition);
        SDL_free(windowPosition);
    } else {
        parentWindowNode = window->ohosHandle;
    }
    OHOS_CreateWindowFrom(thisDevice, window, parentWindowNode);
    return 0;
}

void OHOS_SetWindowTitle(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    OHOS_NAPI_SetTitle(window->title);
}

void OHOS_SetWindowFullscreen(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_VideoDisplay *display,
                              SDL_bool fullscreen)
{
    SDL_WindowData *data;
    SDL_LockMutex(g_ohosPageMutex);

    /* If the window is being destroyed don't change visible state */
    if (!window->is_destroying) {
        OHOS_NAPI_SetWindowStyle(fullscreen);
    }

    data = (SDL_WindowData *)window->driverdata;

    if (!data || !data->native_window) {
        if (data && !data->native_window) {
            SDL_SetError("Missing native window");
        }
        goto endfunction;
    }

endfunction:

    SDL_UnlockMutex(g_ohosPageMutex);
}

void OHOS_MinimizeWindow(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
}

void OHOS_DestroyWindow(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    SDL_Log("Destroy window is Calling.");
    SDL_LockMutex(g_ohosPageMutex);

    if (((window->flags & SDL_WINDOW_RECREATE) == 0) &&
        ((window->flags & SDL_WINDOW_FOREIGN_OHOS) == 0)) {
        if (window->xcompentId != NULL) {
            OHOS_NAPI_RemoveChildNode(window->xcompentId);
            OHOS_ClearPluginData(window->xcompentId);
            SDL_free(window->xcompentId);
            window->xcompentId = NULL;
        }
    }

    if (window->driverdata) {
        SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
        if (data->egl_xcomponent != EGL_NO_SURFACE) {
            SDL_EGL_DestroySurface(thisDevice, data->egl_xcomponent);
            data->egl_xcomponent = EGL_NO_SURFACE;
        }
        SDL_free(window->driverdata);
        window->driverdata = NULL;
    }

    SDL_UnlockMutex(g_ohosPageMutex);
}

SDL_bool OHOS_GetWindowWMInfo(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_SysWMinfo *info)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (info->version.major == SDL_MAJOR_VERSION &&
        info->version.minor == SDL_MINOR_VERSION) {
        info->subsystem = SDL_SYSWM_OHOS;
        info->info.ohos.window = data->native_window;
        info->info.ohos.surface = data->egl_xcomponent;
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}

void OHOS_SetWindowResizable(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_bool resizable)
{
    if (resizable) {
        OHOS_NAPI_SetWindowResize(window->windowed.x, window->windowed.y, window->windowed.w, window->windowed.h);
    }
}

void OHOS_SetWindowSize(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    SDL_LockMutex(g_ohosPageMutex);
    OHOS_NAPI_ResizeNode(window->xcompentId, window->w, window->h);
    SDL_UnlockMutex(g_ohosPageMutex);
}

void OHOS_SetWindowPosition(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    SDL_LockMutex(g_ohosPageMutex);
    OHOS_NAPI_MoveNode(window->xcompentId, window->x, window->y);
    SDL_UnlockMutex(g_ohosPageMutex);
}

void OHOS_ShowWindow(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    SDL_LockMutex(g_ohosPageMutex);
    OHOS_NAPI_ShowNode(window->xcompentId);
    SDL_UnlockMutex(g_ohosPageMutex);
}

void OHOS_HideWindow(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    SDL_LockMutex(g_ohosPageMutex);
    OHOS_NAPI_HideNode(window->xcompentId);
    SDL_UnlockMutex(g_ohosPageMutex);
}

void OHOS_RaiseWindow(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    SDL_LockMutex(g_ohosPageMutex);
    OHOS_NAPI_RaiseNode(window->xcompentId);
    SDL_UnlockMutex(g_ohosPageMutex);
}

static void OHOS_WaitGetNativeXcompent(const char *strID, pthread_t tid, OH_NativeXComponent **nativeXComponent)
{
    int cnt = OHOS_WAIT_COUNT;
    while (!OHOS_FindNativeXcomPoment(strID, nativeXComponent)) {
        if (cnt-- == 0) {
            break;
        }
        SDL_Delay(OHOS_WAIT_TIME);
    }
}

static void OHOS_WaitGetNativeWindow(const char *strID, pthread_t tid, SDL_WindowData **windowData,
    OH_NativeXComponent *nativeXComponent)
{
    int cnt = OHOS_WAIT_COUNT;
    while (!OHOS_FindNativeWindow(nativeXComponent, windowData)) {
        if (cnt-- == 0) {
            SDL_Log("Failed get native window.");
            break;
        }
        SDL_Delay(OHOS_WAIT_TIME);
    }
    SDL_Log("Successful get native window.");
}

static void OHOS_SetRealWindowPosition(SDL_Window *window, SDL_WindowData *windowData)
{
    window->x = windowData->x;
    window->y = windowData->y;
    window->w = windowData->width;
    window->h = windowData->height;
}

int OHOS_CreateWindowFrom(SDL_VideoDevice *thisDevice, SDL_Window *window, const void *data)
{
    WindowPosition *windowPosition = NULL;
    char *strID = NULL;
    pthread_t tid;
    OH_NativeXComponent *nativeXComponent = NULL;
    SDL_WindowData *windowData = NULL;
    SDL_WindowData *sdlWindowData = NULL;

    if (data != NULL && window->ohosHandle == NULL) {
         SDL_Log("Successful get windowdata, native_window2222222222222222244 1=.");
        windowPosition = (WindowPosition *)SDL_malloc(sizeof(WindowPosition));
        OHOS_GetXComponentPos(data, windowPosition);
        window->xcompentId = OHOS_GenerateNewXComponentId();
        SDL_Log("Successful get windowdata, native_window22222222222222222999 1=.");
        OHOS_NAPI_AddChildNode(data, &window->ohosHandle, window->xcompentId, windowPosition);
        SDL_Log("Successful get windowdata, native_window22222222222222222999444 1=.");
        SDL_free(windowPosition);
    }

    strID = window->xcompentId;
    OHOS_FindNativeXcomPoment(strID, &nativeXComponent);
    if ((window->flags & SDL_WINDOW_RECREATE) == 0) {
        tid = pthread_self();
        SDL_Log("Successful get windowdata, native_window22222222222222222333367666 1=.");
        OHOS_AddXcomPomentIdForThread(strID, tid);
        SDL_Log("Successful get windowdata, native_window222222222222222223333 1=.");
        OHOS_WaitGetNativeWindow(strID, tid, &windowData, nativeXComponent);
    } else {
        SDL_Log("Successful get windowdata, native_window222222222222222224444 1=.");
        OHOS_FindNativeWindow(nativeXComponent, &windowData);
    }
  //  SDL_Log("Successful get windowdata, native_window222222222222222224444333 1=%d.",windowData->x);
    sdlWindowData = (SDL_WindowData *)SDL_malloc(sizeof(SDL_WindowData));
    SDL_LockMutex(g_ohosPageMutex);
    OHOS_SetRealWindowPosition(window, windowData);
    sdlWindowData->native_window = windowData->native_window;
    if (!sdlWindowData->native_window) {
        goto endfunction;
    }
    SDL_Log("Successful get windowdata, native_window = %p.", sdlWindowData->native_window);
    if ((window->flags & SDL_WINDOW_OPENGL) != 0) {
        if (thisDevice->gl_config.alpha_size == 0) {
            thisDevice->gl_config.alpha_size = OHOS_EGL_ALPHA_SIZE_DEFAULT;
        }
        sdlWindowData->egl_xcomponent =
            (EGLSurface)SDL_EGL_CreateSurface(thisDevice, (NativeWindowType)windowData->native_window);
        windowData->egl_xcomponent = sdlWindowData->egl_xcomponent;
        if (sdlWindowData->egl_xcomponent == EGL_NO_SURFACE) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create eglsurface");
            SDL_free(windowData);
            goto endfunction;
        }
    }
    window->driverdata = sdlWindowData;
endfunction:
     SDL_UnlockMutex(g_ohosPageMutex);
     return 0;
}

char *OHOS_GetWindowTitle(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    char *title = NULL;
    title = window->title;
    SDL_Log("sdlthread OHOS_GetWindowTitle");
    if (title) {
        return title;
    } else {
        return "Title is NULL";
    }
}
#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
