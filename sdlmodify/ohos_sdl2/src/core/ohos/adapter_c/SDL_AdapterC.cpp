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
#include "../SDL_ohos_xcomponent.h"
#include "../../../events/SDL_windowevents_c.h"
#include "SDL_AdapterC.h"
#include <hilog/log.h>
#include <node_api.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <uv.h>
#include "SDL.h"

#define Z_INDEX_MAX 2147483647
#define Z_INDEX_SUBNODE_INIT 65536

namespace SDL_EMBEDDED_WINDOW_ADAPTER {
napi_env EWAdapterC::MainEnv = nullptr;
ArkUI_NativeNodeAPI_1 *EWAdapterC::nodeAPI = nullptr;
EWAdapterC EWAdapterC::adapterC;
std::thread::id EWAdapterC::mainJsThreadId;
float EWAdapterC::scaledDensity = 1.0;
std::unordered_map<std::string, EWNode *> ewNodeMap;

// Defines locks and semaphores.
using ThreadLockInfo = struct ThreadLockInfo {
    std::mutex mutex;
    std::condition_variable condition;
    bool ready = false;
};

// Define the parameter structure in the thread.
struct WorkParam {
    std::shared_ptr<Node> node = nullptr;
    std::shared_ptr<Node> parentNode = nullptr;
    NodeAttribute *nodeParams = nullptr;
    bool boolStatus = false;
    // return
   // Node *replyNode = nullptr;
    std::shared_ptr<Node> replyNode = nullptr;
    bool replyStatus = false;
    // lock structure
    std::shared_ptr<ThreadLockInfo> lockInfo;

    WorkParam( std::shared_ptr<Node> node,std::shared_ptr<Node> parentNode, NodeAttribute *nodeParams,
        bool boolStatus, std::shared_ptr<ThreadLockInfo> lockInfo)
    {
        this->node = node;
        this->parentNode = parentNode;
        this->nodeParams = nodeParams;
        this->boolStatus = boolStatus;
        this->lockInfo = lockInfo;
    }

    WorkParam() {}
};

bool GetThreadWorkResult(WorkParam *workParam, uv_after_work_cb uvAfterWork)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "RunCallbackType begin");
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(EWAdapterC::MainEnv, &loop);
    if (loop == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "Failed to get_uv_event_loop");
        return false;
    }
    uv_work_t *work = new uv_work_t;
    if (work == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "Failed to new uv_work_t");
        return false;
    }
    work->data = (void *)(workParam);
    uv_work_cb uvWork = [](uv_work_t *work) {};
    uv_queue_work(loop, work, uvWork, uvAfterWork);

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "childThread lock and wait");
    std::unique_lock<std::mutex> lock(workParam->lockInfo->mutex);
    workParam->lockInfo->condition.wait(lock, [&workParam] {
        return workParam->lockInfo->ready;
    });
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "wait work end");

    delete work;
    return workParam->replyStatus;
}

bool GetThreadWorkResultNonblocking(WorkParam *workParam, uv_async_cb uvAfterWork)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "RunCallbackType begin");
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(EWAdapterC::MainEnv, &loop);
    if (loop == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "Failed to get_uv_event_loop");
        return false;
    }
    uv_async_t *work = new uv_async_t;
    if (work == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "Failed to new uv_work_t");
        return false;
    }
    work->data = (void *)(workParam);
    uv_async_init(loop, work, uvAfterWork);
    uv_async_send(work);

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "childThread nonlocking and not wait");
    return true;
}

bool Compare(EWNode *a, EWNode *b)
{
    return a->GetZIndex() < b->GetZIndex();
}

int32_t GetSubNodeMaxZIndex(Node *node)
{
    if (node == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "getSubNodeMaxZIndex nullptr error");
        return 0;
    }
    return 0;
}

int32_t GetSubNodeMinZIndex(std::shared_ptr<Node> node)
{
    if (node == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "getSubNodeMinZIndex nullptr error");
        return 0;
    }
    return 0;
}

// just for test
static uint32_t GenerateRandomHex()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
    uint32_t num = dis(gen);
    std::stringstream ss;
    ss << std::hex << std::setw(EWNODE_BYTE) << std::setfill('0') << num;
    std::string hexStr = ss.str();
    return std::stoul(hexStr, nullptr, EWNODE_HEX);
}

EWNode::EWNode()
{
}


// std::shared_ptr<Node> node
//EWNode::EWNode(Node *node, ArkUI_NativeNodeAPI_1 *nodeApi, NodeAttribute *nodeAttributes)
 EWNode::EWNode(std::shared_ptr<Node> node, ArkUI_NativeNodeAPI_1 *nodeApi, NodeAttribute *nodeAttributes)
    : current(node), nodeApi(nodeApi), nodeAttributes(nodeAttributes)
{
}

EWNode::~EWNode()
{
    SDL_Log("~EWNode111111111111112 is failed");
    if (current->container != nullptr) {
        nodeApi->disposeNode(current->container);
        current->container = nullptr;
    }
    if (current->node != nullptr) {
        nodeApi->disposeNode(current->node);
        current->node = nullptr;
    }
    if (nodeAttributes != nullptr) {
        delete nodeAttributes;
        nodeAttributes = nullptr;
    }
}

std::shared_ptr<Node>EWNode::GetCurrent()
{
    return this->current;
}

std::shared_ptr<Node> EWNode::GetParent()
{
    return this->parent;
}

void EWNode::SetParent(std::shared_ptr<Node> parentNode)
{
    this->parent = parentNode;
}

ArkUI_NodeHandle EWNode::GetXComponent()
{
    return current->node;
}

ArkUI_NodeHandle EWNode::GetStack()
{
    return current->container;
}

NodeAttribute *EWNode::GetNodeParams()
{
    return this->nodeAttributes;
}

void EWNode::AddChild(std::shared_ptr<Node> node)
{
    this->childList.push_back(node);
}

bool EWNode::RemoveChild(std::shared_ptr<Node> node)
{
    auto iter = std::find(this->childList.begin(), this->childList.end(), node);
    if (iter == this->childList.end()) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeChild not find node");
        return false;
    }
    this->childList.erase(iter);
    return true;
}

int32_t EWNode::GetZIndex()
{
    return this->zIndex;
}

void EWNode::SetZIndex(int32_t cur)
{
    this->zIndex = cur;
}

std::vector<std::shared_ptr<Node>> EWNode::GetChildList()
{
    return this->childList;
}

EWAdapterC::~EWAdapterC()
{
}

float EWAdapterC::Px2vp(int px)
{
    if (EWAdapterC::scaledDensity == 0) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "scaledDensity error");
        return px;
    }
    return px / EWAdapterC::scaledDensity;
}

EWAdapterC *EWAdapterC::GetInstance()
{
    return &EWAdapterC::adapterC;
}

std::shared_ptr<Node> EWAdapterC::CreateRootNode(NodeAttribute *nodeParams, bool scale)
{
   // Node *res = nullptr;
    std::shared_ptr<Node> res;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "createRootNode mainJsThreadId");
        res = CreateEWNode(nullptr, nodeParams, scale);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "createRootNode not in mainJsThreadId");
        WorkParam *workParam =
            new WorkParam(nullptr, nullptr, nodeParams, scale, std::make_shared<ThreadLockInfo>());
        uv_after_work_cb afterFunc = [](uv_work_t *work, int _status) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyNode = CreateEWNode(nullptr, param->nodeParams, param->boolStatus);
            param->lockInfo->ready = true;
            param->lockInfo->condition.notify_all();
        };
        GetThreadWorkResult(workParam, afterFunc);
        res = workParam->replyNode;
        delete workParam;
        workParam = nullptr;
    }
    return res;
}

//Node *EWAdapterC::CreateEWNode(Node *parentNode, NodeAttribute *nodeParams, bool scale)
std::shared_ptr<Node> EWAdapterC::CreateEWNode(std::shared_ptr<Node> parentNode, NodeAttribute *nodeParams, bool scale)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "createEWNode: begin!");
    int32_t curZINdex = GetNodeZIndex(parentNode) + 1;
    ArkUI_NodeHandle stack = nodeAPI->createNode(ARKUI_NODE_STACK);
    if (stack == nullptr) {
        return nullptr;
    }
      SDL_Log("~CreateEWNode1111111111 is failed");
    SetStackSize(stack, nodeParams, scale);
    SetStackAlignContent(stack);
    SetStackZIndex(stack, curZINdex);

    SetStackPosition(stack, nodeParams->x, nodeParams->y);
    SetStackHitTestMode(stack);

    ArkUI_NodeHandle xc = nodeAPI->createNode(ARKUI_NODE_XCOMPONENT);
    if (xc == nullptr) {
        nodeAPI->disposeNode(stack);
        return nullptr;
    }
     SDL_Log("~CreateEWNode11111111113333333333 is failed");
    SetXCompontId(xc, nodeParams->componentModel->id.c_str());
    SetXCompontType(xc, nodeParams->componentModel->type);
    SetXCompontFocusable(xc);

//    Node *node = static_cast<Node *>(SDL_malloc(sizeof(Node)));
//    if (node == nullptr) {
//        nodeAPI->disposeNode(stack);
//        nodeAPI->disposeNode(xc);
//        return nullptr;
//    }
    std::shared_ptr<Node> node = std::make_shared<Node>();
    node->nodeType = ARKUI_NODE_XCOMPONENT;
    node->node = xc;
    node->container = stack;
    SDL_memset(node->nodeOwner, 0, sizeof(node->nodeOwner));
    SDL_snprintf(node->nodeOwner, sizeof(node->nodeOwner), "SDL2");
    SDL_Log("~CreateEWNode111111111144444444444444 is failed");
    EWNode *ewNode = new EWNode(node, nodeAPI, nodeParams);
    if (ewNode == nullptr) {
        nodeAPI->disposeNode(stack);
        nodeAPI->disposeNode(xc);
       // SDL_free(node);
        node = nullptr;
        return nullptr;
    }
    ewNode->SetZIndex(curZINdex);

    nodeAPI->addChild(stack, xc);
    if (parentNode != nullptr) {
        nodeAPI->addChild(parentNode->container, stack);
        ewNode->SetParent(parentNode);
    }

    ewNodeMap[nodeParams->componentModel->id] = ewNode;
     SDL_Log("~CreateEWNode1111111111333333333344444 is failed");
    return node;
}

//Node *EWAdapterC::AddChildNode(Node *parentNode, NodeAttribute *nodeParams, bool scale)
std::shared_ptr<Node> EWAdapterC::AddChildNode(std::shared_ptr<Node> parentNode, NodeAttribute *nodeParams, bool scale)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "addChildNode enter");
   // Node *res = nullptr;
    std::shared_ptr<Node> res = nullptr;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "addChildNode mainJsThreadId");
        res = CreateEWNode(parentNode, nodeParams, scale);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "addChildNode not in mainJsThreadId");
        WorkParam *workParam =
            new WorkParam(nullptr, parentNode, nodeParams, scale, std::make_shared<ThreadLockInfo>());
        uv_after_work_cb afterFunc = [](uv_work_t *work, int _status) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyNode = CreateEWNode(param->parentNode, param->nodeParams, param->boolStatus);
            param->lockInfo->ready = true;
            param->lockInfo->condition.notify_all();
        };

        GetThreadWorkResult(workParam, afterFunc);
        res = workParam->replyNode;
        delete workParam;
        workParam = nullptr;
    }
    return res;
}

// 初始化XComponent
void EWAdapterC::Init(napi_env env, napi_value exports)
{
    EWAdapterC::MainEnv = env;
    if ((env == nullptr) || (exports == nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "init: env or exports is null");
        return;
    }

    napi_value exportInstance = nullptr;
    if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "init: napi_get_named_property fail");
        return;
    }

    mainJsThreadId = std::this_thread::get_id();
    EWAdapterC::nodeAPI = reinterpret_cast<ArkUI_NativeNodeAPI_1 *>(
        OH_ArkUI_QueryModuleInterfaceByName(ARKUI_NATIVE_NODE, "ArkUI_NativeNodeAPI_1"));

    OH_NativeXComponent *nativeXComponent = nullptr;
    if (napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent)) != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "init: napi_unwrap fail");
        return;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NativeXComponent_GetXComponentId(nativeXComponent, idStr, &idSize) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "init: OH_NativeXComponent_GetXComponentId fail");
        return;
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "EWAdapterC EWAdapterC::nodeAPI");

    std::string id(idStr);
    if (nativeXComponent != nullptr) {
        SetNativeXComponent(id, nativeXComponent);
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "init: nativeXComponent nullptr error");
    }
}

// 记录NativeXComponent
void EWAdapterC::SetNativeXComponent(const std::string &id, OH_NativeXComponent *nativeXComponent)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "setNativeXComponent: begin!");
    if (nativeXComponent == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
            "setNativeXComponent: nativeXComponent nullptr error");
        return;
    }

    auto it = nativeXComponentMap.find(id);
    if (it == nativeXComponentMap.end()) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "setNativeXComponent: add new");
        nativeXComponentMap[id] = nativeXComponent;
        return;
    }

    if (nativeXComponentMap[id] != nativeXComponent) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "setNativeXComponent: remove old");
        nativeXComponentMap[id] = nativeXComponent;
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "setNativeXComponent: end");
}

OH_NativeXComponent *EWAdapterC::GetNativeXComponent(const std::string &id)
{
    return nativeXComponentMap[id];
}

// 移除记录NativeXComponent
void EWAdapterC::RemoveNativeXComponent(const std::string &id)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeNativeXComponent: begin");
    auto it = nativeXComponentMap.find(id);
    if (it != nativeXComponentMap.end()) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeNativeXComponent: remove old");
        nativeXComponentMap.erase(it);
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeNativeXComponent: end");
}

bool EWAdapterC::RemoveEWNode(std::shared_ptr<Node> node)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeEWNode: begin!");
    if (node == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeEWNode: node nullptr error");
        return false;
    }

    if (GetParent(node) != nullptr) {
        return RemoveChildNode(node);
    } else {
        return RemoveRootNode(node);
    }
}

bool EWAdapterC::RemoveNode(std::shared_ptr<Node> node)
{
    bool res = false;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeNode mainJsThreadId");
        res = RemoveEWNode(node);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeNode not in mainJsThreadId");
        WorkParam *workParam =
            new WorkParam(node, nullptr, nullptr, false, std::make_shared<ThreadLockInfo>());
        uv_async_cb afterFunc = [](uv_async_t *work) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyStatus = RemoveEWNode(param->node);
            delete param;
            param = nullptr;
            uv_close((uv_handle_t *)work, [](uv_handle_t *work) {
                delete (uv_async_t *)work;
                work = nullptr;
            });
        };
        res = GetThreadWorkResultNonblocking(workParam, afterFunc);
    }
    return res;
}

bool EWAdapterC::RemoveRootNode(std::shared_ptr<Node> node)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeRootNode: begin");
    if (node == nullptr || GetParent(node) != nullptr ||
        nodeAPI == nullptr || nodeAPI->removeChild == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeRootNode param nullptr error");
        return false;
    }
    if (GetStack(node) != nullptr && GetXComponent(node) != nullptr) {
        nodeAPI->removeChild(GetStack(node), GetXComponent(node));
    }
    // SDL_free(node);
    node = nullptr;
    return true;
}

bool EWAdapterC::RemoveChildNode(std::shared_ptr<Node> node)
{
    if (node == nullptr || GetParent(node) == nullptr ||
        nodeAPI == nullptr || nodeAPI->removeChild == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "removeChildNode param nullptr error");
        return false;
    }
    RemoveChild(GetParent(node), node);

    auto stack = GetStack(node);
    if (stack != nullptr && GetXComponent(node) != nullptr) {
        nodeAPI->removeChild(stack, GetXComponent(node));
    }

    auto parentNode = GetParent(node);
    auto parentStack = parentNode->container;
    if (parentStack != nullptr && stack != nullptr) {
        nodeAPI->removeChild(parentStack, stack);
    }

    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode && (ewNode->GetCurrent() == node)) {
           // delete ewNode;
            ewNode = nullptr;
            ewNodeMap.erase(iter);
            break;
        }
    }
    // SDL_free(node);
    node = nullptr;
    return true;
}

// NODE_Z_INDEX up
bool EWAdapterC::RaiseEwNode(std::shared_ptr<Node> node)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "raiseEwNode: begin!");
    if (node == nullptr || GetParent(node) == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "raiseEwNode node nullptr error");
        return false;
    }

    if (nodeAPI == nullptr || nodeAPI->setAttribute == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "raiseEwNode nodeAPI nullptr error");
        return false;
    }

    int32_t newZIndex = GetNodeZIndex(GetParent(node)) + 1;
    if (newZIndex == GetZIndex(node)) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN,
            LOG_PRINT_TAG, "node already on the top, index: %{public}d", newZIndex);
        return true;
    }
    if (newZIndex >= Z_INDEX_MAX) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN,
            LOG_PRINT_TAG, "raiseEwNode index invalid, index: %{public}d", newZIndex);
        return false;
    }
    newZIndex++;
    ArkUI_NumberValue zIndexValue[] = {{.i32 = newZIndex}};
    ArkUI_AttributeItem zIndexItem = {zIndexValue, 1};
    nodeAPI->setAttribute(GetStack(node), NODE_Z_INDEX, &zIndexItem);
    SetZIndex(node, newZIndex);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN,
        LOG_PRINT_TAG, "raiseEwNode newZIndex = %{public}d", newZIndex);
    return true;
}

// NODE_Z_INDEX up
bool EWAdapterC::RaiseNode(std::shared_ptr<Node> node)
{
    bool res = false;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "raiseNode mainJsThreadId");
        res = RaiseEwNode(node);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "raiseNode not in mainJsThreadId");
        WorkParam *workParam =
            new WorkParam(node, nullptr, nullptr, false, std::make_shared<ThreadLockInfo>());
        uv_async_cb afterFunc = [](uv_async_t *work) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyStatus = RaiseEwNode(param->node);
            delete param;
            param = nullptr;
            uv_close((uv_handle_t *)work, [](uv_handle_t *work) {
                delete (uv_async_t *)work;
                work = nullptr;
            });
        };
        res = GetThreadWorkResultNonblocking(workParam, afterFunc);
    }
    return res;
}

// NODE_Z_INDEX down
bool EWAdapterC::LowerEWNode(std::shared_ptr<Node>node)
{
    if (node == nullptr || GetParent(node) == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "lowerEWNode node nullptr error");
        return false;
    }
    int32_t newZIndex = GetSubNodeMinZIndex(GetParent(node));
    if (newZIndex == GetZIndex(node)) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN,
            LOG_PRINT_TAG, "lowerEWNode already at the bottom, index: %{public}d", newZIndex);
        return true;
    }
    if (newZIndex <= 1) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN,
            LOG_PRINT_TAG, "lowerEWNode node index error, index: %{public}d", newZIndex);
        return false;
    }
    if (nodeAPI == nullptr || nodeAPI->setAttribute == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "lowerEWNode nodeAPI nullptr error");
        return false;
    }
    newZIndex = newZIndex - 1;
    ArkUI_NumberValue zIndexValue[] = {{.i32 = newZIndex}};
    ArkUI_AttributeItem zIndexItem = {zIndexValue, 1};
    nodeAPI->setAttribute(GetStack(node), NODE_Z_INDEX, &zIndexItem);
    SetZIndex(node, newZIndex);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN,
        LOG_PRINT_TAG, "lowerEWNode lowerNode = %{public}d", newZIndex);
    return true;
}

// NODE_Z_INDEX down
bool EWAdapterC::LowerNode(std::shared_ptr<Node> node)
{
    bool res = false;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "lowerNode mainJsThreadId");
        res = LowerEWNode(node);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "lowerNode not in mainJsThreadId");
        WorkParam *workParam =
            new WorkParam(node, nullptr, nullptr, false, std::make_shared<ThreadLockInfo>());
        uv_after_work_cb afterFunc = [](uv_work_t *work, int _status) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyStatus = LowerEWNode(param->node);
            param->lockInfo->ready = true;
            param->lockInfo->condition.notify_all();
        };
        GetThreadWorkResult(workParam, afterFunc);
        res = workParam->replyStatus;
        delete workParam;
        workParam = nullptr;
    }
    return res;
}

// NODE_WIDTH | NODE_HEIGHT
bool EWAdapterC::ResizeEWNode(std::shared_ptr<Node>node, int width, int height)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                 "resizeEWNode: (width:%{public}d, height:%{public}d)", width, height);
    if (node == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "resizeEWNode node nullptr error");
        return false;
    }
    if (nodeAPI == nullptr || nodeAPI->setAttribute == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "resizeEWNode nodeAPI nullptr error");
        return false;
    }
    ArkUI_NumberValue widthValue[] = {{.f32 = Px2vp(width)}};
    ArkUI_AttributeItem widthItem = {widthValue, 1};
    nodeAPI->setAttribute(GetStack(node), NODE_WIDTH, &widthItem);
    ArkUI_NumberValue heightValue[] = {{.f32 = Px2vp(height)}};
    ArkUI_AttributeItem heightItem = {heightValue, 1};
    nodeAPI->setAttribute(GetStack(node), NODE_HEIGHT, &heightItem);
    GetNodeParams(node)->width = width;
    GetNodeParams(node)->height = height;
    return true;
}

// NODE_WIDTH | NODE_HEIGHT
bool EWAdapterC::ResizeNode(std::shared_ptr<Node>node, int width, int height)
{
    bool res = false;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "resizeNode mainJsThreadId");
        res = ResizeEWNode(node, width, height);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "resizeNode not in mainJsThreadId");
        WorkParam *workParam = new WorkParam(node, nullptr,
            new NodeAttribute(width, height, 0, 0, nullptr), false, std::make_shared<ThreadLockInfo>());
        uv_async_cb afterFunc = [](uv_async_t *work) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyStatus = ResizeEWNode(param->node, param->nodeParams->width, param->nodeParams->height);
            delete param;
            param = nullptr;
            uv_close((uv_handle_t *)work, [](uv_handle_t *work) {
                delete (uv_async_t *)work;
                work = nullptr;
            });
        };
        res = GetThreadWorkResultNonblocking(workParam, afterFunc);
    }
    return res;
}

bool EWAdapterC::ReparentChildNode(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent)
{
    if (node == nullptr || newParent == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "reparentChildNode param nullptr error");
        return false;
    }

    if (nodeAPI == nullptr || nodeAPI->removeChild == nullptr || nodeAPI->addChild == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "reparentChildNode nodeAPI nullptr error");
        return false;
    }
    auto stack = GetStack(node);
    if (stack == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "reparentChildNode stack nullptr error");
        return false;
    }
    auto parentNode = GetParent(node);
    if (parentNode != nullptr && GetStack(parentNode) != nullptr) {
        nodeAPI->removeChild(GetStack(parentNode), stack);
        RemoveChild(parentNode, node);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "reparentChildNode do not need removeChild");
    }
    auto newParentStack = GetStack(newParent);
    if (newParentStack != nullptr) {
        nodeAPI->addChild(newParentStack, stack);
        AddChild(newParent, node);
    } else {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "reparentChildNode addChild error");
    }
    SetParent(node, newParent);
    return true;
}

int32_t EWAdapterC::GetNodeZIndex(std::shared_ptr<Node> node)
{
    if (node == nullptr || nodeAPI == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "getNodeZIndex nullptr error");
        return 0;
    }
    
    SDL_Log("node->container111 = ");
  //   SDL_Log("node1111111111111 = %p", node);
   
    //SDL_Log("node->container111111111111111 = %p", node ? node->container : nullptr);
    if ( node->container == nullptr ) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "getNodeZIndex node->container error");
        return 0;
    }
    
      SDL_Log("node->container11133333 = ");

    const ArkUI_AttributeItem* item = nodeAPI->getAttribute(node->container, NODE_Z_INDEX);
    if (item == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "getNodeZIndex item nullptr error");
        return 0;
    }
    return item->value[0].i32;
}

// move Node from ParentA to ParnetB
bool EWAdapterC::ReParentNode(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent)
{
    bool res = true;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "reParentNode mainJsThreadId");
        res = ReparentChildNode(node, newParent);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "reParentNode not in mainJsThreadId");
        WorkParam *workParam =
            new WorkParam(node, newParent, nullptr, false, std::make_shared<ThreadLockInfo>());
        uv_after_work_cb afterFunc = [](uv_work_t *work, int _status) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyStatus = ReparentChildNode(param->node, param->parentNode);
            param->lockInfo->ready = true;
            param->lockInfo->condition.notify_all();
        };

        GetThreadWorkResult(workParam, afterFunc);
        res = workParam->replyStatus;
        delete workParam;
        workParam = nullptr;
    }
    return res;
}

NodeRect EWAdapterC::GetNodeRect(std::shared_ptr<Node>node)
{
    NodeRect rect = {-1, -1, -1, -1};
    if (node == nullptr || GetXComponent(node) == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "getNodeRect node nullptr error");
        return rect;
    }

    ArkUI_IntSize size = {0, 0};
    OH_ArkUI_NodeUtils_GetLayoutSize(GetXComponent(node), &size);
    rect.width = size.width;
    rect.height = size.height;

    ArkUI_IntOffset pos = {0, 0};
    OH_ArkUI_NodeUtils_GetLayoutPositionInWindow(GetXComponent(node), &pos);
    rect.offsetX = pos.x;
    rect.offsetY = pos.y;

    if (GetParent(node) != nullptr && GetXComponent(GetParent(node)) != nullptr) {
        ArkUI_IntOffset parentPos = {0, 0};
        OH_ArkUI_NodeUtils_GetPositionWithTranslateInWindow(GetXComponent(GetParent(node)), &parentPos);
        rect.offsetX = pos.x - parentPos.x;
        rect.offsetY = pos.y - parentPos.y;
    }
    return rect;
}

NodeRect EWAdapterC::GetNodeRectEx(Node *node)
{
    NodeRect rect = {-1, -1, -1, -1};
    if (node == nullptr || node->node == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "getNodeRect node nullptr error");
        return rect;
    }

    ArkUI_IntSize size = {0, 0};
    OH_ArkUI_NodeUtils_GetLayoutSize(node->node, &size);
    rect.width = size.width;
    rect.height = size.height;

    ArkUI_IntOffset pos = {0, 0};
    OH_ArkUI_NodeUtils_GetLayoutPositionInWindow(node->node, &pos);
    rect.offsetX = pos.x;
    rect.offsetY = pos.y;
    return rect;
}

bool EWAdapterC::MoveEWNode(std::shared_ptr<Node>node, int x, int y)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                 "moveEWNode: (x:%{public}d, y:%{public}d)", x, y);
    if (node == nullptr || GetStack(node) == nullptr ||
        nodeAPI == nullptr || nodeAPI->setAttribute == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "moveEWNode param nullptr error");
        return false;
    }
    // set stack position
    ArkUI_NumberValue positionValue[] = {{.f32 = Px2vp(x)}, {.f32 = Px2vp(y)}};
    ArkUI_AttributeItem positionItem = {positionValue, 2};
    int res = nodeAPI->setAttribute(GetStack(node), NODE_POSITION, &positionItem);
    GetNodeParams(node)->x = x;
    GetNodeParams(node)->y = y;
    return res == ARKUI_ERROR_CODE_NO_ERROR;
}

bool EWAdapterC::MoveNode(std::shared_ptr<Node> node, int x, int y)
{
    bool res = true;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "moveNode mainJsThreadId");
        res = MoveEWNode(node, x, y);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "moveNode not in mainJsThreadId");
        WorkParam *workParam = new WorkParam(node, nullptr,
            new NodeAttribute(0, 0, x, y, nullptr), false, std::make_shared<ThreadLockInfo>());
        uv_async_cb afterFunc = [](uv_async_t *work) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyStatus = MoveEWNode(param->node, param->nodeParams->x, param->nodeParams->y);
            delete param;
            param = nullptr;
            uv_close((uv_handle_t *)work, [](uv_handle_t *work) {
                delete (uv_async_t *)work;
                work = nullptr;
            });
        };
        res = GetThreadWorkResultNonblocking(workParam, afterFunc);
    }
    return res;
}

bool EWAdapterC::SetEWNodeVisibility(std::shared_ptr<Node> node, bool showCurNode)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "setEWNodeVisibility: %{public}d", showCurNode);
    if (node == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "setEWNodeVisibility node nullptr error");
        return false;
    }
    if (nodeAPI == nullptr || nodeAPI->setAttribute == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "setEWNodeVisibility nodeAPI nullptr error");
        return false;
    }
    ArkUI_NumberValue value[] = {ARKUI_VISIBILITY_VISIBLE};
    ArkUI_AttributeItem item = {value, 1};
    int res = 0;
    if (showCurNode) {
        res = nodeAPI->setAttribute(node->container, NODE_VISIBILITY, &item);
    } else {
        value[0].i32 = ARKUI_VISIBILITY_HIDDEN;
        res = nodeAPI->setAttribute(node->container, NODE_VISIBILITY, &item);
    }
    return res == ARKUI_ERROR_CODE_NO_ERROR;
}

bool EWAdapterC::SetNodeVisibility(std::shared_ptr<Node> node, bool showCurNode)
{
    bool res = true;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "setNodeVisibility mainJsThreadId");
        res = SetEWNodeVisibility(node, showCurNode);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "setNodeVisibility not in mainJsThreadId");
        WorkParam *workParam = new WorkParam(node, nullptr, nullptr, showCurNode, std::make_shared<ThreadLockInfo>());
        uv_async_cb afterFunc = [](uv_async_t *work) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyStatus = SetEWNodeVisibility(param->node, param->boolStatus);
            delete param;
            param = nullptr;
            uv_close((uv_handle_t *)work, [](uv_handle_t *work) {
                delete (uv_async_t *)work;
                work = nullptr;
            });
        };
        res = GetThreadWorkResultNonblocking(workParam, afterFunc);
    }
    return res;
}

bool EWAdapterC::GetEWNodeVisibility(std::shared_ptr<Node> node)
{
    if (node == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "getEWNodeVisibility node nullptr error");
        return false;
    }
    if (nodeAPI == nullptr || nodeAPI->getAttribute == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "getEWNodeVisibility nodeAPI nullptr error");
        return false;
    }

    const ArkUI_AttributeItem *item = nodeAPI->getAttribute(node->container, NODE_VISIBILITY);
    if (item->value[0].i32 == ARKUI_VISIBILITY_VISIBLE) {
        return true;
    }
    return false;
}

bool EWAdapterC::IsVisible(std::shared_ptr<Node> node)
{
    bool res = true;
    std::thread::id curThreadId = std::this_thread::get_id();
    if (curThreadId == mainJsThreadId) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "isVisible mainJsThreadId");
        res = GetEWNodeVisibility(node);
    } else {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, LOG_PRINT_TAG, "isVisible not in mainJsThreadId");
        WorkParam *workParam = new WorkParam(node, nullptr, nullptr, false, std::make_shared<ThreadLockInfo>());
        uv_after_work_cb afterFunc = [](uv_work_t *work, int _status) {
            WorkParam *param = (WorkParam *)(work->data);
            param->replyStatus = GetEWNodeVisibility(param->node);
            param->lockInfo->ready = true;
            param->lockInfo->condition.notify_all();
        };

        GetThreadWorkResult(workParam, afterFunc);
        res = workParam->replyStatus;
        delete workParam;
        workParam = nullptr;
    }
    return res;
}

bool EWAdapterC::ShowNode(std::shared_ptr<Node> node)
{
    return SetNodeVisibility(node, true);
}
bool EWAdapterC::HideNode(std::shared_ptr<Node> node)
{
    return SetNodeVisibility(node, false);
}


ArkUI_NodeHandle EWAdapterC::GetXComponent(std::shared_ptr<Node> node)
{
    ArkUI_NodeHandle xc = nullptr;
    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode->GetCurrent() == node) {
            xc = ewNode->GetXComponent();
            break;
        }
    }
    return xc;
}

ArkUI_NodeHandle EWAdapterC::GetStack(std::shared_ptr<Node> node)
{
    ArkUI_NodeHandle stack = nullptr;
    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode->GetCurrent() == node) {
            stack = ewNode->GetStack();
            break;
        }
    }
    return stack;
}

std::shared_ptr<Node> EWAdapterC::GetParent(std::shared_ptr<Node> node)
{
   std::shared_ptr<Node> parent = nullptr;
    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode->GetCurrent() == node) {
            parent = ewNode->GetParent();
            break;
        }
    }
    return parent;
}

void EWAdapterC::SetParent(std::shared_ptr<Node> node, std::shared_ptr<Node> parentNode)
{
    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode->GetCurrent() == node) {
            ewNode->SetParent(parentNode);
            break;
        }
    }
}

NodeAttribute *EWAdapterC::GetNodeParams(std::shared_ptr<Node> node)
{
    NodeAttribute *attr = nullptr;
    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode->GetCurrent() == node) {
            attr = ewNode->GetNodeParams();
            break;
        }
    }
    return attr;
}

void EWAdapterC::AddChild(std::shared_ptr<Node> node, std::shared_ptr<Node> child)
{
    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode->GetCurrent() == node) {
            ewNode->AddChild(child);
            break;
        }
    }
}

bool EWAdapterC::RemoveChild(std::shared_ptr<Node> node, std::shared_ptr<Node> child)
{
    bool flag = false;
    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode->GetCurrent() == node) {
            flag = ewNode->RemoveChild(child);
            break;
        }
    }
    return flag;
}

int32_t EWAdapterC::GetZIndex(std::shared_ptr<Node> node)
{
    int32_t zIndex = 0;
    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode->GetCurrent() == node) {
            zIndex = ewNode->GetZIndex();
            break;
        }
    }
    return zIndex;
}

void EWAdapterC::SetZIndex(std::shared_ptr<Node> node, int32_t cur)
{
    EWNode *ewNode = nullptr;
    for (auto iter = ewNodeMap.begin(); iter != ewNodeMap.end(); ++iter) {
        ewNode = iter->second;
        if (ewNode->GetCurrent() == node) {
            ewNode->SetZIndex(cur);
            break;
        }
    }
}

int32_t EWAdapterC::SetStackSize(ArkUI_NodeHandle stack, NodeAttribute *nodeParamsn, bool scale)
{
    int32_t res = ARKUI_ERROR_CODE_NO_ERROR;
    if (scale) {
        ArkUI_NumberValue widthPercentValue[] = {{.f32 = nodeParamsn->widthPercent}};
        ArkUI_AttributeItem widthPercentItem = {widthPercentValue, 1};
        res = nodeAPI->setAttribute(stack, NODE_WIDTH_PERCENT, &widthPercentItem);
        if (res != ARKUI_ERROR_CODE_NO_ERROR) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                         "createEWNode: setAttribute NODE_WIDTH_PERCENT failed!");
        }

        ArkUI_NumberValue heightPercentValue[] = {{.f32 = nodeParamsn->heightPercent}};
        ArkUI_AttributeItem heightPercentItem = {heightPercentValue, 1};
        res = nodeAPI->setAttribute(stack, NODE_HEIGHT_PERCENT, &heightPercentItem);
        if (res != ARKUI_ERROR_CODE_NO_ERROR) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                         "createEWNode: setAttribute NODE_HEIGHT_PERCENT failed!");
        }
    } else {
        ArkUI_NumberValue widthValue[] = {{.f32 = Px2vp(nodeParamsn->width)}};
        ArkUI_AttributeItem widthItem = {widthValue, 1};
        res = nodeAPI->setAttribute(stack, NODE_WIDTH, &widthItem);
        if (res != ARKUI_ERROR_CODE_NO_ERROR) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                         "createEWNode: setAttribute NODE_WIDTH failed!");
        }

        ArkUI_NumberValue heightValue[] = {{.f32 = Px2vp(nodeParamsn->height)}};
        ArkUI_AttributeItem heightItem = {heightValue, 1};
        res = nodeAPI->setAttribute(stack, NODE_HEIGHT, &heightItem);
        if (res != ARKUI_ERROR_CODE_NO_ERROR) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                         "createEWNode: setAttribute NODE_HEIGHT failed!");
        }
    }
    return res;
}

int32_t EWAdapterC::SetStackAlignContent(ArkUI_NodeHandle stack)
{
    int32_t res = ARKUI_ERROR_CODE_NO_ERROR;
    ArkUI_NumberValue alignContentValue[] = {{.i32 = ARKUI_ALIGNMENT_TOP_START}};
    ArkUI_AttributeItem alignContentItem = {alignContentValue, 1};
    res = nodeAPI->setAttribute(stack, NODE_STACK_ALIGN_CONTENT, &alignContentItem);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_STACK_ALIGN_CONTENT failed!");
    }
    return res;
}

int32_t EWAdapterC::SetStackZIndex(ArkUI_NodeHandle stack, int zIndex)
{
    int32_t res = ARKUI_ERROR_CODE_NO_ERROR;
    ArkUI_NumberValue zIndexValue[] = {{.i32 = zIndex}};
    ArkUI_AttributeItem zIndexItem = {zIndexValue, 1};
    res = nodeAPI->setAttribute(stack, NODE_Z_INDEX, &zIndexItem);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_Z_INDEX failed!");
    }
    return res;
}

int32_t EWAdapterC::SetStackPosition(ArkUI_NodeHandle stack, int x, int y)
{
    int32_t res = ARKUI_ERROR_CODE_NO_ERROR;
    ArkUI_NumberValue positionValue[] = {{.f32 = Px2vp(x)}, {.f32 = Px2vp(y)}};
    ArkUI_AttributeItem positionItem = {positionValue, 2};
    res = nodeAPI->setAttribute(stack, NODE_POSITION, &positionItem);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_POSITION failed!");
    }
    return res;
}

int32_t EWAdapterC::SetStackHitTestMode(ArkUI_NodeHandle stack)
{
    int32_t res = ARKUI_ERROR_CODE_NO_ERROR;
    ArkUI_NumberValue value[] = {{.i32 = static_cast<int32_t>(ARKUI_HIT_TEST_MODE_TRANSPARENT)}};
    ArkUI_AttributeItem item = {value, 1};
    res = nodeAPI->setAttribute(stack, NODE_HIT_TEST_BEHAVIOR, &item);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_HIT_TEST_BEHAVIOR failed!");
    }
    return res;
}

int32_t EWAdapterC::SetXCompontId(ArkUI_NodeHandle xc, const char* id)
{
    int32_t res = ARKUI_ERROR_CODE_NO_ERROR;
    ArkUI_AttributeItem idItem = {.string = id};
    res = nodeAPI->setAttribute(xc, NODE_ID, &idItem);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_XCOMPONENT_ID failed!");
    }

    res = nodeAPI->setAttribute(xc, NODE_XCOMPONENT_ID, &idItem);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_XCOMPONENT_ID failed!");
    }
    return res;
}

int32_t EWAdapterC::SetXCompontType(ArkUI_NodeHandle xc, int type)
{
    int32_t res = ARKUI_ERROR_CODE_NO_ERROR;
    ArkUI_NumberValue typeValue[] = {{.i32 = type}};
    ArkUI_AttributeItem typeItem = {typeValue, 1};
    res = nodeAPI->setAttribute(xc, NODE_XCOMPONENT_TYPE, &typeItem);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_XCOMPONENT_TYPE failed!");
    }
    return res;
}

int32_t EWAdapterC::SetXCompontFocusable(ArkUI_NodeHandle xc)
{
    int32_t res = ARKUI_ERROR_CODE_NO_ERROR;
    ArkUI_NumberValue focusableValue[] = {{.i32 = 1}};
    ArkUI_AttributeItem focusableItem = {focusableValue, 1};
    res = nodeAPI->setAttribute(xc, NODE_FOCUSABLE, &focusableItem);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_FOCUSABLE failed!");
    }

    // set xcomponent focus on touch
    ArkUI_NumberValue focusOnTouchValue[] = {{.i32 = 1}};
    ArkUI_AttributeItem focusOnTouchItem = {focusOnTouchValue, 1};
    res = nodeAPI->setAttribute(xc, NODE_FOCUS_ON_TOUCH, &focusOnTouchItem);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_FOCUS_ON_TOUCH failed!");
    }

    // set background color for test
    ArkUI_NumberValue xcBackgroundColor[] = {{.u32 = 0}};
    ArkUI_AttributeItem xcBackgroundColorItem = {xcBackgroundColor, 1};
    res = nodeAPI->setAttribute(xc, NODE_BACKGROUND_COLOR, &xcBackgroundColorItem);
    if (res != ARKUI_ERROR_CODE_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, LOG_PRINT_TAG,
                     "createEWNode: setAttribute NODE_BACKGROUND_COLOR failed!");
    }
    return res;
}


} // namespace SDL_EMBEDDED_WINDOW_ADAPTER
