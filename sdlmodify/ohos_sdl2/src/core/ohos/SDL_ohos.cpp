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

#ifdef __OHOS__

#include "node_api.h"
#include "SDL_napi.h"
#include "SDL_log.h"
#include <rawfile/raw_file_manager.h>
#include <unistd.h>
#include <hilog/log.h>
#include <dlfcn.h>
#include "cJSON.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "SDL.h"
#include "../../video/SDL_sysvideo.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_quit.h"
#ifdef __cplusplus
}
#endif

#include "SDL_ohos.h"
#include "SDL_ohosfile.h"
#include "../../video/ohos/SDL_ohosvideo.h"
#include "../../video/ohos/SDL_ohoskeyboard.h"
#include "../../audio/ohos/SDL_ohosaudio.h"
#include "adapter_c/SDL_AdapterC.h"
#include "SDL_ohosthreadsafe.h"
#include "SDL_ohos_xcomponent.h"
#include <map>
#include <memory>

#define OHOS_DELAY_FIFTY 50
#define OHOS_DELAY_TEN 10
#define OHOS_START_ARGS_INDEX 2
#define OHOS_INDEX_ARG0 0
#define OHOS_INDEX_ARG1 1
#define OHOS_INDEX_ARG2 2
#define OHOS_INDEX_ARG3 3
#define OHOS_INDEX_ARG4 4
#define OHOS_INDEX_ARG5 5
#define OHOS_INDEX_ARG6 6

using namespace std;
using namespace OHOS::SDL;
using namespace SDL_EMBEDDED_WINDOW_ADAPTER;

// Defines locks and semaphores.
using ThreadLockInfo = struct ThreadLockInfo {
    std::mutex mutex;
    std::condition_variable condition;
    bool ready = false;
};

SDL_DisplayOrientation displayOrientation;

SDL_atomic_t bPermissionRequestPending;
SDL_bool bPermissionRequestResult;
static SDL_atomic_t bQuit;

//std::shared_ptr<Node> rootNode = nullptr;
Node *rootNode = nullptr;
std::map<std::string, std::shared_ptr<Node>> m_map;

SDL_WindowResizeSync *g_ohosResizeSync = nullptr;

/* Lock / Unlock Mutex */
void OHOS_PAGEMUTEX_Lock()
{
    SDL_LockMutex(g_ohosPageMutex);
}

void OHOS_PAGEMUTEX_Unlock()
{
    SDL_UnlockMutex(g_ohosPageMutex);
}

void OHOS_PAGEMUTEX_LockRunning()
{
    int pauseSignaled = 0;
    int resumeSignaled = 0;
retry:
    SDL_LockMutex(g_ohosPageMutex);
    pauseSignaled = SDL_SemValue(g_ohosPauseSem);
    resumeSignaled = SDL_SemValue(g_ohosResumeSem);
    if (pauseSignaled > resumeSignaled) {
        SDL_UnlockMutex(g_ohosPageMutex);
        SDL_Delay(OHOS_DELAY_FIFTY);
        goto retry;
    }
}

void OHOS_SetDisplayOrientation(int orientation)
{
    displayOrientation = (SDL_DisplayOrientation)orientation;
}

SDL_DisplayOrientation OHOS_GetDisplayOrientation()
{
    return displayOrientation;
}

void OHOS_NAPI_SetWindowResize(int x, int y, int w, int h)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_WINDOWRESIZE);
    cJSON_AddNumberToObject(root, "x", x);
    cJSON_AddNumberToObject(root, "y", y);
    cJSON_AddNumberToObject(root, "w", w);
    cJSON_AddNumberToObject(root, "h", h);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_ShowTextInput(int x, int y, int w, int h)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SHOW_TEXTINPUT);
    cJSON_AddNumberToObject(root, "x", x);
    cJSON_AddNumberToObject(root, "y", y);
    cJSON_AddNumberToObject(root, "w", w);
    cJSON_AddNumberToObject(root, "h", h);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

SDL_bool OHOS_NAPI_RequestPermission(const char *permission)
{
    /* Wait for any pending request on another thread */
    while (SDL_AtomicGet(&bPermissionRequestPending) == SDL_TRUE) {
        SDL_Delay(OHOS_DELAY_TEN);
    }
    SDL_AtomicSet(&bPermissionRequestPending, SDL_TRUE);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_REQUEST_PERMISSION);
    cJSON_AddStringToObject(root, "permission", permission);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    /* Wait for the request to complete */
    while (SDL_AtomicGet(&bPermissionRequestPending) == SDL_TRUE) {
        SDL_Delay(OHOS_DELAY_TEN);
    }
    return bPermissionRequestResult;
}

SDL_bool OHOS_NAPI_AddChildNode(void *parent, void **child, char *xcompentId, WindowPosition *windowPosition)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_ADD_CHILD_NODE);
  //  cJSON_AddNumberToObject(root, "parent", (long long)parent.get());
    unsigned long long parentnode = (unsigned long long)parent;
    cJSON_AddStringToObject(root, "parent", std::to_string(parentnode).c_str());
    // cJSON_AddNumberToObject(root, "child", (long long)child);
    
        unsigned long long childnode = (unsigned long long)child;
    cJSON_AddStringToObject(root, "child", std::to_string(childnode).c_str());
    
    cJSON_AddStringToObject(root, "xcompentid", xcompentId);
   // cJSON_AddNumberToObject(root, "position", (long long)windowPosition);
    
    unsigned long long position = (unsigned long long)windowPosition;
      SDL_Log("data->position11111 ======%p.",position);
    cJSON_AddStringToObject(root, "position", std::to_string(position).c_str());
    
    ThreadLockInfo *lockInfo = new ThreadLockInfo();
    unsigned long long lockpo = (unsigned long long)lockInfo;
    SDL_Log("enter data->position11111 ======%p.",position);
    cJSON_AddStringToObject(root, OHOS_JSON_ASYN, std::to_string(lockpo).c_str());
    
   // std::uintptr_t lockInfoPointer = reinterpret_cast<std::uintptr_t>(lockInfo);
    //cJSON_AddNumberToObject(root, OHOS_JSON_ASYN, (double)lockInfoPointer);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        delete lockInfo;
        cJSON_free(root);
        return SDL_FALSE;
    }
    std::unique_lock<std::mutex> lock(lockInfo->mutex);
    lockInfo->condition.wait_for(lock, std::chrono::seconds(OHOS_INDEX_ARG6), [lockInfo] { return lockInfo->ready; });
    delete lockInfo;
    return SDL_TRUE;
}

SDL_bool OHOS_NAPI_RemoveChildNode(char *xcompentId)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_REMOVE_CHILD_NODE);
    cJSON_AddStringToObject(root, "xcompentid", xcompentId);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_TRUE;
}

SDL_bool OHOS_NAPI_ResizeNode(char *xcompentId, int width, int height)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_RESIZE_NODE);
    cJSON_AddStringToObject(root, "xcompentid", xcompentId);
    cJSON_AddNumberToObject(root, "width", width);
    cJSON_AddNumberToObject(root, "height", height);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_TRUE;
}

SDL_bool OHOS_NAPI_MoveNode(char *xcompentId, int x, int y)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_MOVE_NODE);
    cJSON_AddStringToObject(root, "xcompentid", xcompentId);
    cJSON_AddNumberToObject(root, "x", x);
    cJSON_AddNumberToObject(root, "y", y);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_TRUE;
}

SDL_bool OHOS_NAPI_ShowNode(char *xcompentId)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SHOW_NODE);
    cJSON_AddStringToObject(root, "xcompentid", xcompentId);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_TRUE;
}

SDL_bool OHOS_NAPI_HideNode(char *xcompentId)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_HIDE_NODE);
    cJSON_AddStringToObject(root, "xcompentid", xcompentId);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_TRUE;
}

SDL_bool OHOS_NAPI_RaiseNode(char *xcompentId)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_RAISE_NODE);
    cJSON_AddStringToObject(root, "xcompentid", xcompentId);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_TRUE;
}

void OHOS_NAPI_HideTextInput(int flag)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_HIDE_TEXTINPUT);
    cJSON_AddNumberToObject(root, "flag", flag);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_ShouldMinimizeOnFocusLoss(int flag)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SHOULD_MINIMIZEON_FOCUSLOSS);
    cJSON_AddNumberToObject(root, "flag", flag);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_SetTitle(const char *title)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_TITLE);
    cJSON_AddStringToObject(root, "title", title);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_SetWindowStyle(SDL_bool fullscreen)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_WINDOWSTYLE);
    cJSON_AddBoolToObject(root, "fullscreen", fullscreen);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_ShowTextInputKeyboard(SDL_bool isshow)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SHOW_TEXTINPUTKEYBOARD);
    cJSON_AddBoolToObject(root, "isshow", isshow);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_SetOrientation(int w, int h, int resizable, const char *hint)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_ORIENTATION);
    cJSON_AddNumberToObject(root, "w", w);
    cJSON_AddNumberToObject(root, "h", h);
    cJSON_AddNumberToObject(root, "resizable", resizable);
    cJSON_AddStringToObject(root, "hint", hint);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

int OHOS_CreateCustomCursor(SDL_Surface *xcomponent, int hotX, int hotY)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return -1;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_CREATE_CUSTOMCURSOR);
    cJSON_AddNumberToObject(root, "hot_x", hotX);
    cJSON_AddNumberToObject(root, "hot_y", hotY);
    cJSON_AddNumberToObject(root, "BytesPerPixel", xcomponent->format->BytesPerPixel);
    cJSON_AddNumberToObject(root, "w", xcomponent->w);
    cJSON_AddNumberToObject(root, "h", xcomponent->h);
    size_t bufferSize = xcomponent->w * xcomponent->h * xcomponent->format->BytesPerPixel;
    void *buff = SDL_malloc(bufferSize);
    SDL_memcpy(buff, xcomponent->pixels, bufferSize);
//    long long xcomponentpixel = (long long)buff;
//    cJSON_AddNumberToObject(root, "xcomponentpixel", (double)xcomponentpixel);
    
        unsigned long long xcomponentpixel = (unsigned long long)buff;
    SDL_Log("xcomponentpixel1111 ======%ld.", xcomponentpixel);
    SDL_Log("(double)xcomponentpixel1111 ======%s.",std::to_string(xcomponentpixel).c_str());
    
   // cJSON_AddNumberToObject(root, "xcomponentpixel", (double)xcomponentpixel);
    cJSON_AddStringToObject(root, "xcomponentpixel", std::to_string(xcomponentpixel).c_str());
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return 1;
}

SDL_bool OHOS_SetCustomCursor(int cursorID)
{
    if (SDL_AtomicGet(&bQuit) == SDL_TRUE) {
        return SDL_TRUE;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, "cursorID", cursorID);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_FALSE;
}

SDL_bool OHOS_SetSystemCursor(int cursorID)
{
    if (SDL_AtomicGet(&bQuit) == SDL_TRUE) {
        return SDL_TRUE;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_SYSTEMCURSOR);
    cJSON_AddNumberToObject(root, "cursorID", cursorID);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_TRUE;
}

/* Relative mouse support */
SDL_bool OHOS_SupportsRelativeMouse(void)
{
    return SDL_TRUE;
}

SDL_bool OHOS_SetRelativeMouseEnabled(SDL_bool enabled)
{
    return SDL_TRUE;
}

napi_value SDLNapi::OHOS_SetResourceManager(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[OHOS_INDEX_ARG2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    size_t len = 0;
    napi_get_value_string_utf8(env, args[0], g_path, 0, &len);
    
    if (g_path != nullptr) {
        delete g_path;
        g_path = nullptr;
    }
    
    g_path = new char[len + 1];
    napi_get_value_string_utf8(env, args[0], g_path, len + 1, &len);
    
    gCtx = SDL_AllocRW();
    NativeResourceManager *nativeResourceManager = OH_ResourceManager_InitNativeResourceManager(env, args[1]);
    gCtx->hidden.ohosio.nativeResourceManager = nativeResourceManager;
    return nullptr;
}

napi_value SDLNapi::OHOS_NativeSetScreenResolution(napi_env env, napi_callback_info info)
{
    size_t argc = 7;
    napi_value args[7];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int xcomponentWidth;
    int xcomponentHeight;
    int deviceWidth;
    int deviceHeight;
    int format;
    double rate;
    double screenDensity;
    napi_get_value_int32(env, args[OHOS_INDEX_ARG0], &xcomponentWidth);
    napi_get_value_int32(env, args[OHOS_INDEX_ARG1], &xcomponentHeight);
    napi_get_value_int32(env, args[OHOS_INDEX_ARG2], &deviceWidth);
    napi_get_value_int32(env, args[OHOS_INDEX_ARG3], &deviceHeight);
    napi_get_value_int32(env, args[OHOS_INDEX_ARG4], &format);
    napi_get_value_double(env, args[OHOS_INDEX_ARG5], &rate);
    napi_get_value_double(env, args[OHOS_INDEX_ARG6], &screenDensity);
    SDL_LockMutex(g_ohosPageMutex);
    OHOS_SetScreenResolution(deviceWidth, deviceHeight, format, rate, screenDensity);
    SDL_UnlockMutex(g_ohosPageMutex);
    return nullptr;
}

napi_value SDLNapi::OHOS_OnNativeResize(napi_env env, napi_callback_info info)
{
    return nullptr;
}

napi_value SDLNapi::OHOS_TextInput(napi_env env, napi_callback_info info)
{
    size_t requireArgc = 2;
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    char* inputBuffer = nullptr;
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    napi_valuetype valuetype0;
    napi_typeof(env, args[0], &valuetype0);
    
    napi_valuetype valuetype1;
    napi_typeof(env, args[1], &valuetype1);
    
	//atkts传过来的文本框的内容的长度
    int bufSize;
    napi_get_value_int32(env, args[0], &bufSize);
    size_t stringLength = bufSize + 1;
    size_t length;
    
    inputBuffer = new char[stringLength];
    
    napi_get_value_string_utf8(env, args[1], inputBuffer, stringLength, &length);
    
    SDL_Event keyEvent;
    keyEvent.type = SDL_KEYDOWN;
    keyEvent.key.keysym.sym = SDLK_RETURN;
    SDL_PushEvent(&keyEvent);

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(SDL_Event)); // 清空event结构体
    event.type = SDL_TEXTINPUT;
    
    SDL_strlcpy(event.text.text, inputBuffer, sizeof(inputBuffer));
    
    SDL_PushEvent(&event); // 推送事件到事件队列
    
    delete inputBuffer;
    return nullptr;
}

napi_value SDLNapi::OHOS_KeyDown(napi_env env, napi_callback_info info)
{
    int keycode;
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    napi_get_value_int32(env, args[0], &keycode);
    OHOS_OnKeyDown(keycode);
    return nullptr;
}

napi_value SDLNapi::OHOS_KeyUp(napi_env env, napi_callback_info info)
{
    int keycode;
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    napi_get_value_int32(env, args[0], &keycode);
    OHOS_OnKeyUp(keycode);
    return nullptr;
}

napi_value SDLNapi::OHOS_OnNativeKeyboardFocusLost(napi_env env, napi_callback_info info)
{
    SDL_StopTextInput();
    return nullptr;
}

void DestroyWindowResizeSync(SDL_WindowResizeSync* sync)
{
    if (!sync) return;

    if (sync->sizeChangeMutex) {
        SDL_DestroyMutex(sync->sizeChangeMutex);
    }

    if (sync->sizeChangeCond) {
        SDL_DestroyCond(sync->sizeChangeCond);
    }

    free(sync);
}

static void OHOS_NativeQuit(void)
{
    const char *str;
    if (g_ohosPageMutex) {
        SDL_DestroyMutex(g_ohosPageMutex);
        g_ohosPageMutex = nullptr;
    }

    if (g_ohosResizeSync) {
        DestroyWindowResizeSync(g_ohosResizeSync);
        g_ohosResizeSync = nullptr;
    }

    if (g_ohosPauseSem) {
        SDL_DestroySemaphore(g_ohosPauseSem);
        g_ohosPauseSem = nullptr;
    }

    if (g_ohosResumeSem) {
        SDL_DestroySemaphore(g_ohosResumeSem);
        g_ohosResumeSem = nullptr;
    }

    str = SDL_GetError();
    if (str && str[0]) {
    } else {
    }
    return;
}

napi_value SDLNapi::OHOS_NativeSendQuit(napi_env env, napi_callback_info info)
{
    SDL_Log("Ohos page is destroyed.");
    SDL_AtomicSet(&bQuit, SDL_TRUE);
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    napi_value sum = 0;
    SDL_SendQuit();
    OHOS_ThreadExit();
    SDL_SendAppEvent(SDL_APP_TERMINATING);
    OHOS_NativeQuit();
    while (SDL_SemTryWait(g_ohosPauseSem) == 0) {
    }
    SDL_SemPost(g_ohosResumeSem);
    return nullptr;
}

napi_value SDLNapi::OHOS_NativeResume(napi_env env, napi_callback_info info)
{
    SDL_SemPost(g_ohosResumeSem);
    OHOSAUDIO_PageResume();
    return nullptr;
}

napi_value SDLNapi::OHOS_NativePause(napi_env env, napi_callback_info info)
{
    SDL_SemPost(g_ohosPauseSem);
    OHOSAUDIO_PagePause();
    return nullptr;
}

napi_value SDLNapi::OHOS_NativePermissionResult(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    bool result;
    napi_get_value_bool(env, args[0], &result);
    bPermissionRequestResult = result ? SDL_TRUE : SDL_FALSE;
    SDL_AtomicSet(&bPermissionRequestPending, SDL_FALSE);
    return nullptr;
}

napi_value SDLNapi::OHOS_OnNativeOrientationChanged(napi_env env, napi_callback_info info)
{
    int orientation;
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    napi_get_value_int32(env, args[0], &orientation);
    SDL_LockMutex(g_ohosPageMutex);
    OHOS_SetDisplayOrientation(orientation);
    SDL_VideoDisplay *display = SDL_GetDisplay(0);
    SDL_SendDisplayEvent(display, SDL_DISPLAYEVENT_ORIENTATION, orientation);
    SDL_UnlockMutex(g_ohosPageMutex);
    return nullptr;
}

napi_value SDLNapi::OHOS_OnNativeFocusChanged(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    bool focus;
    napi_get_value_bool(env, args[0], &focus);
    
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_Window *curWindow = _this->windows;
    SDL_SendWindowEvent(curWindow,
        (focus = SDL_TRUE ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST), 0, 0);
    return nullptr;
}

SDL_WindowResizeSync* InitWindowResizeSync()
{
    SDL_WindowResizeSync* sync = (SDL_WindowResizeSync*)malloc(sizeof(SDL_WindowResizeSync));
    if (!sync) {
        SDL_Log("Failed to allocate memory for SDL_WindowResizeSync");
        return nullptr;
    }

    sync->sizeChangeMutex = SDL_CreateMutex();
    if (!sync->sizeChangeMutex) {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
        free(sync);
        return nullptr;
    }

    sync->sizeChangeCond = SDL_CreateCond();
    if (!sync->sizeChangeCond) {
        SDL_Log("Failed to create condition variable: %s", SDL_GetError());
        SDL_DestroyMutex(sync->sizeChangeMutex); // 销毁已分配的互斥锁
        free(sync);
        return nullptr;
    }

    sync->isChangeing = false;

    return sync;
}

static void OHOS_NAPI_NativeSetup(void)
{
    SDL_setenv("SDL_VIDEO_GL_DRIVER", "libGLESv3.so", 1);
    SDL_setenv("SDL_VIDEO_EGL_DRIVER", "libEGL.so", 1);
    SDL_setenv("SDL_ASSERT", "ignore", 1);
    SDL_AtomicSet(&bPermissionRequestPending, SDL_FALSE);
    SDL_AtomicSet(&bQuit, SDL_FALSE);

    g_ohosPageMutex = SDL_CreateMutex();
    g_ohosResizeSync = InitWindowResizeSync();
    return;
}

static napi_value OHOS_NAPI_Init(napi_env env, napi_callback_info info)
{
    if (g_napiCallback == nullptr) {
        g_napiCallback = std::make_unique<NapiCallbackContext>();
    }
    g_napiCallback->mainThreadId = std::this_thread::get_id();
    OHOS_NAPI_NativeSetup();
    g_napiCallback->env = env;
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    napi_create_reference(env, args[0], 1, &g_napiCallback->callbackRef);

    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "SDLThreadSafe", NAPI_AUTO_LENGTH, &resourceName);

    napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr, nullptr, OHOS_TS_Call,
                                    &g_napiCallback->tsfn);
    return nullptr;
}

bool OHOS_NAPI_GetStringUtf8(napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result)
{
    if ((env == nullptr) || (value == nullptr)) {
        return false;
    }

    napi_valuetype valuetype;
    if (napi_typeof(env, value, &valuetype) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_typeof error");
        return false;
    }
    if (valuetype != napi_string) {
        napi_throw_type_error(env, NULL, "napi_string error");
        return false;
    }

    if (napi_get_value_string_utf8(env, value, buf, bufsize, result) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_get_value_string_utf8 error");
        return false;
    }
    if (result == 0) {
        napi_throw_error(env, NULL, "string utf8 result error");
        return false;
    }

    return true;
}

bool OHOS_NAPI_GetInt64(napi_env env, napi_value value, int64_t* result)
{
    if ((env == nullptr) || (value == nullptr)) {
        return false;
    }

    napi_valuetype valuetype;
    if (napi_typeof(env, value, &valuetype) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_typeof error");
        return false;
    }
    if (valuetype != napi_number) {
        napi_throw_type_error(env, NULL, "napi_bigint error");
        return false;
    }

    if (napi_get_value_int64(env, value, result) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_get_value_int64 error");
        return false;
    }

    return true;
}

bool OHOS_NAPI_GetNodeInfo(napi_env env, napi_callback_info info, char* id, size_t idSize, WindowPosition* wp)
{
    if ((env == nullptr) || (info == nullptr)) {
        return false;
    }

    size_t argCnt = 5, expectedArgCnt = 5;
    napi_value args[ARGV_TYPE_FIVE] = {nullptr};
    if (napi_get_cb_info(env, info, &argCnt, args, nullptr, nullptr) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_get_cb_info error");
        return false;
    }
    if (argCnt != expectedArgCnt) {
        napi_throw_type_error(env, NULL, "arguments number error");
        return false;
    }

    size_t length = 0;
    if (!OHOS_NAPI_GetStringUtf8(env, args[ARGV_TYPE_ZERO], id, idSize, &length)) {
        return false;
    }

    int64_t w = 0, h = 0, x = 0, y = 0;
    if (!OHOS_NAPI_GetInt64(env, args[ARGV_TYPE_ONE], &w)) {
        return false;
    }
    if (!OHOS_NAPI_GetInt64(env, args[ARGV_TYPE_TWO], &h)) {
        return false;
    }
    if (!OHOS_NAPI_GetInt64(env, args[ARGV_TYPE_THREE], &x)) {
        return false;
    }
    if (!OHOS_NAPI_GetInt64(env, args[ARGV_TYPE_FOUR], &y)) {
        return false;
    }
    wp->width = w;
    wp->height = h;
    wp->x = x;
    wp->y = y;
    return true;
}

static napi_value OHOS_NAPI_CreateNativeNode(napi_env env, napi_callback_info info)
{
    if ((env == nullptr) || (info == nullptr)) {
        return nullptr;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
    size_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    WindowPosition windowPosition = {0};
    bool flag = OHOS_NAPI_GetNodeInfo(env, info, idStr, idSize, &windowPosition);
    if (!flag) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error getting root node info.");
        return nullptr;
    }

    flag = OHOS_AddRootNode(idStr, &windowPosition);
    if (!flag) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating root node.");
    }
    return nullptr;
}

static napi_value OHOS_NAPI_AddChildNode(napi_env env, napi_callback_info info)
{
    if ((env == nullptr) || (info == nullptr)) {
        return nullptr;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
    size_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    WindowPosition windowPosition = {0};
    bool flag = OHOS_NAPI_GetNodeInfo(env, info, idStr, idSize, &windowPosition);
    if (!flag) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error getting child node info.");
        return nullptr;
    }

    return nullptr;
}

static napi_value OHOS_NAPI_RemoveNode(napi_env env, napi_callback_info info)
{
    if ((env == nullptr) || (info == nullptr)) {
        return nullptr;
    }

    size_t argCnt = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argCnt, args, nullptr, nullptr);
    if (argCnt != 1) {
        napi_throw_type_error(env, NULL, "Wrong number of arguments");
        return nullptr;
    }

    napi_valuetype valuetype;
    if (napi_typeof(env, args[ARGV_TYPE_ZERO], &valuetype) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_typeof failed");
        return nullptr;
    }
    if (valuetype != napi_string) {
        napi_throw_type_error(env, NULL, "Wrong type of arguments");
        return nullptr;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
    constexpr uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    size_t length;
    if (napi_get_value_string_utf8(env, args[ARGV_TYPE_ZERO], idStr, idSize, &length) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_get_value_int64 failed");
        return nullptr;
    }

    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return nullptr;
    }

    auto it = m_map.find(std::string(idStr));
    if (it == m_map.end()) {
        return nullptr;
    }

    auto node = it->second;
    adapter->RemoveNode(node);
    m_map.erase(it);

    return nullptr;
}

static napi_value OHOS_NAPI_DestroyNode(napi_env env, napi_callback_info info)
{
    if ((env == nullptr) || (info == nullptr)) {
        return nullptr;
    }

    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return nullptr;
    }

    auto it = m_map.begin();
    while (it != m_map.end()) {
        adapter->RemoveNode(it->second);
        it = m_map.erase(it);
    }
    m_map.clear();

    return nullptr;
}

static napi_value OHOS_NAPI_SetScaledDensity(napi_env env, napi_callback_info info)
{
    if ((env == nullptr) || (info == nullptr)) {
        return nullptr;
    }

    size_t argCnt = 1;
    napi_value args[ARGV_TYPE_ONE] = {nullptr};
    napi_get_cb_info(env, info, &argCnt, args, nullptr, nullptr);
    if (argCnt != 1) {
        napi_throw_type_error(env, NULL, "Wrong number of arguments");
        return nullptr;
    }

    napi_valuetype valuetype;
    if (napi_typeof(env, args[ARGV_TYPE_ZERO], &valuetype) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_typeof failed");
        return nullptr;
    }

    if (valuetype != napi_number) {
        napi_throw_type_error(env, NULL, "Wrong type of arguments");
        return nullptr;
    }

    double scaledDensity = 0;
    if (napi_get_value_double(env, args[ARGV_TYPE_ZERO], &scaledDensity) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_get_value_int64 failed");
        return nullptr;
    }

    if (scaledDensity != 0) {
        EWAdapterC::scaledDensity = scaledDensity;
    }

    return nullptr;
}

static int OHOS_NAPI_GetInfo(napi_env &env, napi_callback_info &info, napi_value *argv, char **library_file,
    char **function_name)
{
    napi_status status;
    napi_valuetype valuetype;
    size_t buffer_size;
    size_t argc = 10;
    
    status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDLAppEntry():failed to obtained argument!");
        return -1;
    }
    
    status = napi_typeof(env, argv[ARGV_TYPE_ZERO], &valuetype);
    if (status != napi_ok || valuetype != napi_string) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDLAppEntry():invalid type of argument!");
        return -1;
    }
    napi_get_value_string_utf8(env, argv[ARGV_TYPE_ZERO], nullptr, 0, &buffer_size);
    *library_file = (char *)SDL_malloc(buffer_size + 1);
    SDL_memset(*library_file, 0, buffer_size + 1);
    napi_get_value_string_utf8(env, argv[ARGV_TYPE_ZERO], *library_file, buffer_size + 1, &buffer_size);

    status = napi_typeof(env, argv[ARGV_TYPE_ONE], &valuetype);
    if (status != napi_ok || valuetype != napi_string) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDLAppEntry():invalid type of argument!");
        SDL_free(library_file);
        return -1;
    }
    napi_get_value_string_utf8(env, argv[ARGV_TYPE_ONE], nullptr, 0, &buffer_size);
    *function_name = (char *)SDL_malloc(buffer_size + 1);
    SDL_memset(*function_name, 0, buffer_size + 1);
    napi_get_value_string_utf8(env, argv[ARGV_TYPE_ONE], *function_name, buffer_size + 1, &buffer_size);
    return argc;
}

static int OHOS_NAPI_SetArgs(napi_env& env, char **argvs, int &argcs, size_t &argc, napi_value* argv)
{
    napi_status status;
    napi_valuetype valuetype;
    size_t buffer_size;
    int i = 2;
    
    argvs[argcs++] = SDL_strdup("SDL_main");
    for (i = OHOS_START_ARGS_INDEX; i < argc; ++i) {
        char *arg = NULL;
        status = napi_typeof(env, argv[i], &valuetype);
        if (status != napi_ok || valuetype != napi_string) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDLAppEntry():invalid type of argument!");
            break;
        }
        napi_get_value_string_utf8(env, argv[i], nullptr, 0, &buffer_size);

        arg = (char *)SDL_malloc(buffer_size + 1);
        SDL_memset(arg, 0, buffer_size + 1);
        napi_get_value_string_utf8(env, argv[i], arg, buffer_size + 1, &buffer_size);
        if (!arg) {
            arg = SDL_strdup("");
        }
        argvs[argcs++] = arg;
    }
    return i;
}

static napi_value OHOS_NAPI_SDLRootNode(napi_env env, napi_callback_info info)
{
    napi_status status;
    size_t argc = 1;
    napi_value argv;

    status = napi_get_cb_info(env, info, &argc, &argv, nullptr, nullptr);
    if (status != napi_ok) {
        return nullptr;
    }
    status = napi_unwrap(env, argv, (void **)&rootNode);
    if (status != napi_ok || rootNode == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error getting root node.");
    }
    return nullptr;
}

static napi_value OHOS_NAPI_SDLAppEntry(napi_env env, napi_callback_info info)
{
    char *library_file;
    char *function_name;
    napi_value argv[10];
    size_t argc = 10;
    char **argvs;
    int argcs = 0;
    bool isstack;
    int i;

    argc = OHOS_NAPI_GetInfo(env, info, argv, &library_file, &function_name);
    if (argc == -1)
        return nullptr;
    argvs = SDL_small_alloc(char *, argc, &isstack);
    i = OHOS_NAPI_SetArgs(env, argvs, argcs, argc, argv);

    bool isRunThread = true;
    OhosSDLEntryInfo *entry = NULL;
    if (i == argc) {
        argvs[argcs] = NULL;
        entry = (OhosSDLEntryInfo *)SDL_malloc(sizeof(OhosSDLEntryInfo));
        if (entry != NULL) {
            entry->argcs = argcs;
            entry->argvs = argvs;
            entry->functionName = function_name;
            entry->libraryFile = library_file;
            isRunThread = OHOS_RunThread(entry);
        } else {
            isRunThread = false;
        }
    }

    // Not run SDL thread succuss then free memory, if run SDL thread succuss, SDL thread deal the memory.
    if (!isRunThread) {
        for (i = 0; i < argcs; ++i) {
            SDL_free(argvs[i]);
        }
        SDL_small_free(argvs, isstack);
        SDL_free(info);
        SDL_free(function_name);
        if (library_file != nullptr) {
            dlclose(library_file);
            SDL_free(library_file);
            library_file = nullptr;
        }
        SDL_free(entry);
    }

    return nullptr;
}

void OHOS_SetRootNode(void **parent)
{
    if (parent && *parent) {
        rootNode = (Node *)*parent;
    }
}

void OHOS_GetRootNode(void **parent)
{
    if (rootNode) {
        *parent = rootNode;
    }
}
static std::unordered_map<std::string, std::shared_ptr<Node>> g_nodeMap;
//void OHOS_SetRootNode(std::shared_ptr<Node> parent)
//{
//    if (parent) {
//        rootNode = parent;
//    }
//}
//
//void OHOS_GetRootNode(std::shared_ptr<Node> parent)
//{
//    if (rootNode) {
//        parent = rootNode;
//    }
//}

bool OHOS_AddRootNode(char *xcompentId, WindowPosition *windowPosition)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    OH_NativeXComponent *component = adapter->GetNativeXComponent(xcompentId);
    if (component == nullptr) {
        return false;
    }

    XComponentModel *model = new XComponentModel(xcompentId, ARKUI_XCOMPONENT_TYPE_SURFACE);
    if (model == nullptr) {
        return false;
    }

    NodeAttribute *params =
        new NodeAttribute(windowPosition->width, windowPosition->height, windowPosition->x, windowPosition->y, model);
    if (params == nullptr) {
        delete model;
        model = nullptr;
        return false;
    }

    bool flag = false;
    auto node = adapter->CreateRootNode(params, true);
    if (node) {
        flag = true;
        m_map.emplace(std::string(xcompentId), node);
        if (std::strcmp(xcompentId, "RootXC") == 0) {
            rootNode = node.get();
        }

        OH_NativeXComponent *nativeXComponent = OH_NativeXComponent_GetNativeXComponent(adapter->GetXComponent(node));
        if (nativeXComponent) {
            OHOS_SetNativeXcomponent(xcompentId, nativeXComponent);
        }
        OH_NativeXComponent_AttachNativeRootNode(component, adapter->GetStack(node));
    }

    delete params;
    params = nullptr;
    return flag;
}


bool OHOS_AddChildNode(void *parent, void **child, char *xcompentId, WindowPosition *windowPosition)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    auto it = m_map.find(std::string(xcompentId));
    if (it != m_map.end()) {
        return false;
    }

    XComponentModel *model = new XComponentModel(xcompentId, ARKUI_XCOMPONENT_TYPE_SURFACE);
    if (model == nullptr) {
        return false;
    }
    SDL_Log("width11122222222222222222222222");
    if (windowPosition == nullptr) {
        SDL_Log("windowPosition11111111111111111111111111");
    }
    SDL_Log("width111: %d, height: %d, x: %d, y: %d", windowPosition->width, windowPosition->height, windowPosition->x, windowPosition->y);

    NodeAttribute *params =
        new NodeAttribute(windowPosition->width, windowPosition->height, windowPosition->x, windowPosition->y, model);
    if (params == nullptr) {
         SDL_Log("NodeAttribute111111111111111111111111111 is failed");
        delete model;
        model = nullptr;
        return false;
    }

    bool flag = false;
    Node *parent1= (Node *)parent;
    std::shared_ptr<Node> ptr(parent1);
    auto node = adapter->AddChildNode(ptr , params);
    if (node) {
        flag = true;
        *child = node.get();
        // child = node;
        m_map.emplace(std::string(xcompentId), node);

        OH_NativeXComponent *nativeXComponent = OH_NativeXComponent_GetNativeXComponent(adapter->GetXComponent(node));
        if (nativeXComponent) {
            OHOS_SetNativeXcomponent(xcompentId, nativeXComponent);
        }
    }

    return flag;
}

bool OHOS_RemoveChildNode(char *xcompentId)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    auto it = m_map.find(std::string(xcompentId));
    if (it != m_map.end()) {
        auto node = it->second;
        adapter->RemoveNode(node);
        m_map.erase(it);
    }

    return true;
}

bool OHOS_ResizeNode(char *xcompentId, int width, int height)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    auto it = m_map.find(std::string(xcompentId));
    if (it != m_map.end() && g_ohosResizeSync) {
        auto node = it->second;
        SDL_LockMutex(g_ohosResizeSync->sizeChangeMutex);
        g_ohosResizeSync->isChangeing = true;
        adapter->ResizeNode(node, width, height);
        SDL_UnlockMutex(g_ohosResizeSync->sizeChangeMutex);
    }

    return true;
}

bool OHOS_ReParentNode(char *xcompentParentId, char *xcompentId)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    auto nodeIt = m_map.find(std::string(xcompentId));
    if (nodeIt == m_map.end()) {
        return false;
    }

    auto parentIt = m_map.find(std::string(xcompentParentId));
    if (parentIt == m_map.end()) {
        return false;
    }

    adapter->ReParentNode(nodeIt->second, parentIt->second);
    return true;
}

bool OHOS_MoveNode(char *xcompentId, int x, int y)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    auto it = m_map.find(std::string(xcompentId));
    if (it != m_map.end()) {
        auto node = it->second;
        adapter->MoveNode(node, x, y);
    }

    return true;
}

bool OHOS_ShowNode(char *xcompentId)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    auto it = m_map.find(std::string(xcompentId));
    if (it != m_map.end()) {
        auto node = it->second;
        adapter->ShowNode(node);
    }

    return true;
}

bool OHOS_HideNode(char *xcompentId)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    auto it = m_map.find(std::string(xcompentId));
    if (it != m_map.end()) {
        auto node = it->second;
        adapter->HideNode(node);
    }

    return true;
}

bool OHOS_RaiseNode(char *xcompentId)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    auto it = m_map.find(std::string(xcompentId));
    if (it != m_map.end()) {
        auto node = it->second;
        adapter->RaiseNode(node);
    }

    return true;
}

bool OHOS_LowerNode(char *xcompentId)
{
    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    auto it = m_map.find(std::string(xcompentId));
    if (it != m_map.end()) {
        auto node = it->second;
        adapter->LowerNode(node);
    }

    return true;
}

bool OHOS_GetXComponentPos(const void *node, WindowPosition *windowPosition)
{
    NodeRect rect;
    Node *pNode = (Node *)node;

    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return false;
    }

    rect = adapter->GetNodeRectEx(pNode);
    windowPosition->width = rect.width;
    windowPosition->height = rect.height;
    windowPosition->x = 0;
    windowPosition->y = 0;
    return true;
}

napi_value SDLNapi::Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"sdlRootNode", nullptr, OHOS_NAPI_SDLRootNode, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sdlAppEntry", nullptr, OHOS_NAPI_SDLAppEntry, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"nativeSetScreenResolution", nullptr, OHOS_NativeSetScreenResolution, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"onNativeResize", nullptr, OHOS_OnNativeResize, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"textInput", nullptr, OHOS_TextInput, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"keyDown", nullptr, OHOS_KeyDown, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"keyUp", nullptr, OHOS_KeyUp, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onNativeKeyboardFocusLost", nullptr, OHOS_OnNativeKeyboardFocusLost, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"nativeSendQuit", nullptr, OHOS_NativeSendQuit, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"nativeResume", nullptr, OHOS_NativeResume, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"nativePause", nullptr, OHOS_NativePause, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"nativePermissionResult", nullptr, OHOS_NativePermissionResult, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"onNativeOrientationChanged", nullptr, OHOS_OnNativeOrientationChanged, nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"setResourceManager", nullptr, OHOS_SetResourceManager, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onNativeFocusChanged", nullptr, OHOS_OnNativeFocusChanged, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"init", nullptr, OHOS_NAPI_Init, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"createNativeNode", nullptr, OHOS_NAPI_CreateNativeNode, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"addChildNode", nullptr, OHOS_NAPI_AddChildNode, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"removeNode", nullptr, OHOS_NAPI_RemoveNode, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"destroyNode", nullptr, OHOS_NAPI_DestroyNode, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setScaledDensity", nullptr, OHOS_NAPI_SetScaledDensity, nullptr, nullptr, nullptr, napi_default, nullptr}
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

    // init EWAdapterC
    auto adapter = EWAdapterC::GetInstance();
    if (adapter != nullptr) {
        adapter->Init(env, exports);
    }

    OHOS_XcomponentExport(env, exports);
    return exports;
}

EXTERN_C_START
static napi_value SDLNapiInit(napi_env env, napi_value exports)
{
    return SDLNapi::Init(env, exports);
}
EXTERN_C_END

napi_module OHOSNapiModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = SDLNapiInit,
    .nm_modname = "SDL2",
    .nm_priv = ((void *)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&OHOSNapiModule);
}

#endif /* __OHOS__ */

/* vi: set ts=4 sw=4 expandtab: */
