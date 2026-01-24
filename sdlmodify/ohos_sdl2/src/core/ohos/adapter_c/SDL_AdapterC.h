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

#ifndef EMBEDDED_WINDOW_ADAPTERC_H
#define EMBEDDED_WINDOW_ADAPTERC_H

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <arkui/native_node.h>
#include <arkui/native_type.h>
#include <arkui/native_interface.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <vector>
#include <string>
#include <thread>
#include <memory>

namespace SDL_EMBEDDED_WINDOW_ADAPTER {

const unsigned int LOG_PRINT_DOMAIN = 0xFF01;
#define LOG_PRINT_TAG "SDL/EWAdapterC"
#define TEST_EWNODE
#define EWNODE_HEX 16
#define EWNODE_BYTE 8

/**
 * NodeRect: Rect of Node
 */
struct NodeRect {
    int64_t offsetX = 0;
    int64_t offsetY = 0;
    int64_t width = 0;
    int64_t height = 0;
};

/**
 * XComponent component attribute configuration
 */
struct XComponentModel {
    std::string id = "";
    ArkUI_XComponentType type = ARKUI_XCOMPONENT_TYPE_SURFACE;
    XComponentModel(std::string id, ArkUI_XComponentType type) : id(id), type(type) {}
};

/**
 * Node attribute configuration
 */
class NodeAttribute {
public:
    NodeAttribute() {}
    NodeAttribute(int width, int height, int x, int y, XComponentModel *componentModel)
        : width(width), height(height), x(x), y(y), componentModel(componentModel) {}

    ~NodeAttribute()
    {
        if (componentModel != nullptr) {
            delete componentModel;
            componentModel = nullptr;
        }
    }
    int width = 0;
    int height = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    float widthPercent = 1.0;
    float heightPercent = 1.0;
    XComponentModel *componentModel = nullptr;
};

//struct Node {
//    ArkUI_NodeType nodeType;
//    ArkUI_NodeHandle node;
//    ArkUI_NodeHandle container = nullptr;
//    char nodeOwner[8];
//    void* nodePrivate;
//};


struct Node {
    ArkUI_NodeType nodeType;
    ArkUI_NodeHandle node = nullptr;
    ArkUI_NodeHandle container = nullptr;
    char nodeOwner[8] = {0};
    void* nodePrivate = nullptr;


};


// EWNode for EmbeddedWindow Node
class EWNode {
public:
    EWNode();
    EWNode(std::shared_ptr<Node> node, ArkUI_NativeNodeAPI_1 *nodeApi, NodeAttribute *nodeAttributes);
    ~EWNode();

   // Node *GetCurrent();
    std::shared_ptr<Node> GetCurrent();
    std::shared_ptr<Node> GetParent();
    void SetParent(std::shared_ptr<Node> parentNode);
    ArkUI_NodeHandle GetXComponent();
    ArkUI_NodeHandle GetStack();
    NodeAttribute *GetNodeParams();
    void AddChild(std::shared_ptr<Node> node);
    bool RemoveChild(std::shared_ptr<Node> node);
    int32_t GetZIndex();
    void SetZIndex(int32_t cur);
    std::vector<std::shared_ptr<Node>> GetChildList();

private:
   // Node *current = nullptr;
    std::shared_ptr<Node> current;
    std::shared_ptr<Node> parent = nullptr;
    std::vector<std::shared_ptr<Node>> childList = {};
    NodeAttribute *nodeAttributes = nullptr;
    ArkUI_NativeNodeAPI_1 *nodeApi = nullptr;
    int32_t zIndex = 0;
};


class EWAdapterC {
public:
    ~EWAdapterC();
    static EWAdapterC *GetInstance();
    static std::shared_ptr<Node> CreateRootNode(NodeAttribute *node, bool scale = false);
    static std::shared_ptr<Node> AddChildNode(std::shared_ptr<Node> parentNode, NodeAttribute *nodeParams, bool scale = false);
    static bool RemoveNode(std::shared_ptr<Node> node);
    static bool RaiseNode(std::shared_ptr<Node> node);
    static bool LowerNode(std::shared_ptr<Node> node);
    static bool ResizeNode(std::shared_ptr<Node> node, int width, int height);
    static bool ReParentNode(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent);
    static bool MoveNode(std::shared_ptr<Node> node, int x, int y);
    static bool SetNodeVisibility(std::shared_ptr<Node> node, bool showCurNode);
    static bool IsVisible(std::shared_ptr<Node> node);
    static bool ShowNode(std::shared_ptr<Node> node);
    static bool HideNode(std::shared_ptr<Node> node);
    static NodeRect GetNodeRect(std::shared_ptr<Node> node);
    static NodeRect GetNodeRectEx(Node *node);
    void SetNativeXComponent(const std::string &id, OH_NativeXComponent *nativeXComponent);
    OH_NativeXComponent *GetNativeXComponent(const std::string &id);
    void RemoveNativeXComponent(const std::string &id);
    void Init(napi_env env, napi_value exports);

    static ArkUI_NodeHandle GetXComponent(std::shared_ptr<Node> node);
    static ArkUI_NodeHandle GetStack(std::shared_ptr<Node> node);
    static std::shared_ptr<Node> GetParent(std::shared_ptr<Node> node);
    static void SetParent(std::shared_ptr<Node> node, std::shared_ptr<Node> parentNode);
    static NodeAttribute *GetNodeParams(std::shared_ptr<Node> node);
    static void AddChild(std::shared_ptr<Node>node, std::shared_ptr<Node>child);
    static bool RemoveChild(std::shared_ptr<Node> node, std::shared_ptr<Node> child);
    static int32_t GetZIndex(std::shared_ptr<Node> node);
    static void SetZIndex(std::shared_ptr<Node> node, int32_t cur);

private:
    EWAdapterC() {}
    static EWAdapterC adapterC;
    static std::thread::id mainJsThreadId;
    // static Node *CreateEWNode(Node *parentNode, NodeAttribute *nodeParamsn, bool scale = false);  
    static std::shared_ptr<Node> CreateEWNode(std::shared_ptr<Node> parentNode, NodeAttribute *nodeParamsn, bool scale = false);
    static bool RemoveEWNode(std::shared_ptr<Node>);
    //static bool RemoveChildNode(Node *node);
    static bool RemoveChildNode(std::shared_ptr<Node> node);
    static bool RemoveRootNode(std::shared_ptr<Node> node);
    static bool RaiseEwNode(std::shared_ptr<Node> node);
    static bool LowerEWNode(std::shared_ptr<Node>node);
    static bool ResizeEWNode(std::shared_ptr<Node>node, int width, int height);
    static bool MoveEWNode(std::shared_ptr<Node> node, int x, int y);
    static bool SetEWNodeVisibility(std::shared_ptr<Node> nodenode, bool showCurNode);
    static bool GetEWNodeVisibility(std::shared_ptr<Node> node);
    static bool ReparentChildNode(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent);
    static float Px2vp(int px);

    static int32_t SetStackSize(ArkUI_NodeHandle stack, NodeAttribute *nodeParamsn, bool scale);
    static int32_t SetStackAlignContent(ArkUI_NodeHandle stack);
    static int32_t SetStackZIndex(ArkUI_NodeHandle stack, int zIndex);
    static int32_t SetStackPosition(ArkUI_NodeHandle stack, int x, int y);

    static int32_t SetXCompontId(ArkUI_NodeHandle xc, const char* id);
    static int32_t SetXCompontType(ArkUI_NodeHandle xc, int type);
    static int32_t SetXCompontFocusable(ArkUI_NodeHandle xc);

    static int32_t GetNodeZIndex(std::shared_ptr<Node> node);
    static int32_t SetStackHitTestMode(ArkUI_NodeHandle stack);

public:
    static ArkUI_NativeNodeAPI_1 *nodeAPI;
    static float scaledDensity;
    static napi_env MainEnv;
    std::unordered_map<std::string, OH_NativeXComponent *> nativeXComponentMap;
};

} // namespace EMBEDDED_WINDOW_ADAPTER
#endif // EMBEDDED_WINDOW_ADAPTERC_H
