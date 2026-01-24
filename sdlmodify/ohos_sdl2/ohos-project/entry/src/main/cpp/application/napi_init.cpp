/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
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

#include <hilog/log.h>
#include "gloable.h"
#include "SDL_AdapterC.h"
using namespace SDL_EMBEDDED_WINDOW_ADAPTER;

#define NAPI_ARGO 0
#define NAPI_ARG1 1
#define NAPI_ARG2 2
#define NAPI_ARG3 3
#define NAPI_ARG4 4
#define NAPI_ARG5 5

typedef struct WindowParams {
    int x;
    int y;
    int width;
    int height;
} WindowParams;

const unsigned int NAPI_LOG_DOMAIN = 0xFF00;
Node *g_rootNode = nullptr;
void *WinId()
{
    if (!g_rootNode) {
        return nullptr;
    }
    return (void *)g_rootNode;
}

bool NAPI_GetStringUtf8(napi_env env, napi_value value, char *buf, size_t bufsize, size_t *result)
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

bool NAPI_GetInt64(napi_env env, napi_value value, int64_t *result)
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

bool NAPI_GetNodeInfo(napi_env env, napi_callback_info info, char *id, size_t idSize, WindowParams *wp)
{
    if ((env == nullptr) || (info == nullptr)) {
        return false;
    }

    size_t argCnt = 5, expectedArgCnt = 5;
    napi_value args[NAPI_ARG5] = {nullptr};
    if (napi_get_cb_info(env, info, &argCnt, args, nullptr, nullptr) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_get_cb_info error");
        return false;
    }
    if (argCnt != expectedArgCnt) {
        napi_throw_type_error(env, NULL, "arguments number error");
        return false;
    }

    size_t length = 0;
    if (!NAPI_GetStringUtf8(env, args[NAPI_ARGO], id, idSize, &length)) {
        return false;
    }

    int64_t w = 0, h = 0, x = 0, y = 0;
    if (!NAPI_GetInt64(env, args[NAPI_ARG1], &w)) {
        return false;
    }
    if (!NAPI_GetInt64(env, args[NAPI_ARG2], &h)) {
        return false;
    }
    if (!NAPI_GetInt64(env, args[NAPI_ARG3], &x)) {
        return false;
    }
    if (!NAPI_GetInt64(env, args[NAPI_ARG4], &y)) {
        return false;
    }
    wp->width = w;
    wp->height = h;
    wp->x = x;
    wp->y = y;
    return true;
}

bool NAPI_AddRootNode(char *xcompentId, WindowParams *windowPosition)
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
    std::shared_ptr<Node> node = adapter->CreateRootNode(params, true);
    if (node) {
        flag = true;
        g_rootNode = node.get();

        OH_NativeXComponent_AttachNativeRootNode(component, adapter->GetStack(node));
    }

    delete params;
    params = nullptr;
    return flag;
}

static void DerefItem(napi_env env, void *data, void *hint)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, NAPI_LOG_DOMAIN, "NAPI_CreateNativeNode", "Node-API DerefItem");
    (void)hint;
}

static napi_value NAPI_CreateNativeNode(napi_env env, napi_callback_info info)
{
    if ((env == nullptr) || (info == nullptr)) {
        return nullptr;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
    size_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    WindowParams stWindowParams = {0};
    bool flag = NAPI_GetNodeInfo(env, info, idStr, idSize, &stWindowParams);
    if (!flag) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, NAPI_LOG_DOMAIN, "NAPI_CreateNativeNode", "Error getting root node info.");
        return nullptr;
    }

    flag = NAPI_AddRootNode(idStr, &stWindowParams);
    if (!flag) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, NAPI_LOG_DOMAIN, "NAPI_CreateNativeNode", "Error creating root node.");
        return nullptr;
    }

    napi_status status;
    napi_value result;
    status = napi_create_object(env, &result);
    if (status != napi_ok) {
        return nullptr;
    }
    status = napi_wrap(env, result, g_rootNode, DerefItem, NULL, NULL);
    if (status != napi_ok) {
        return nullptr;
    }
    return result;
}

static napi_value NAPI_RemoveNode(napi_env env, napi_callback_info info)
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
    if (napi_typeof(env, args[NAPI_ARGO], &valuetype) != napi_ok) {
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
    if (napi_get_value_string_utf8(env, args[NAPI_ARGO], idStr, idSize, &length) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_get_value_int64 failed");
        return nullptr;
    }

    auto adapter = EWAdapterC::GetInstance();
    if (adapter == nullptr) {
        return nullptr;
    }

    adapter->RemoveNode((std::shared_ptr<Node>)g_rootNode);

    return nullptr;
}

static napi_value NAPI_SetScaledDensity(napi_env env, napi_callback_info info)
{
    if ((env == nullptr) || (info == nullptr)) {
        return nullptr;
    }

    size_t argCnt = 1;
    napi_value args[NAPI_ARG1] = {nullptr};
    napi_get_cb_info(env, info, &argCnt, args, nullptr, nullptr);
    if (argCnt != 1) {
        napi_throw_type_error(env, NULL, "Wrong number of arguments");
        return nullptr;
    }

    napi_valuetype valuetype;
    if (napi_typeof(env, args[NAPI_ARGO], &valuetype) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_typeof failed");
        return nullptr;
    }

    if (valuetype != napi_number) {
        napi_throw_type_error(env, NULL, "Wrong type of arguments");
        return nullptr;
    }

    double scaledDensity = 0;
    if (napi_get_value_double(env, args[NAPI_ARGO], &scaledDensity) != napi_ok) {
        napi_throw_type_error(env, NULL, "napi_get_value_int64 failed");
        return nullptr;
    }

    if (scaledDensity != 0) {
        EWAdapterC::scaledDensity = scaledDensity;
    }

    return nullptr;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Init", "Init begins");
    if ((nullptr == env) || (nullptr == exports)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, NAPI_LOG_DOMAIN, "Init", "env or exports is null");
        return nullptr;
    }

    napi_property_descriptor desc[] = {
        {"createNativeNode", nullptr, NAPI_CreateNativeNode, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"removeNode", nullptr, NAPI_RemoveNode, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setScaledDensity", nullptr, NAPI_SetScaledDensity, nullptr, nullptr, nullptr, napi_default, nullptr}
    };
    if (napi_ok != napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, NAPI_LOG_DOMAIN, "Init", "napi_define_properties failed");
        return nullptr;
    }

    // init EWAdapterC
    auto adapter = EWAdapterC::GetInstance();
    if (adapter != nullptr) {
        adapter->Init(env, exports);
    }

    return exports;
}
EXTERN_C_END

static napi_module nativerenderModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "nativerender",
    .nm_priv = ((void *)0),
    .reserved = { 0 }
};

extern "C" __attribute__((constructor)) void RegisterModule(void)
{
    napi_module_register(&nativerenderModule);
}