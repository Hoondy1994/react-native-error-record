# NativeXComponent 和 NativeWindow 一一对应关系

## ✅ 确认：是一一对应的！

**关系：**
```
1 个 XComponent (ArkTS)
    ↓ 一一对应
1 个 OH_NativeXComponent (C++)
    ↓ 一一对应
1 个 OHNativeWindow (系统)
```

---

## 1. 代码证据

### 1.1 通过 ID 关联

**每个 XComponent 有唯一的 ID：**
```typescript
// SkiaDomView.ets:26
this.xComponentId = X_COMPONENT_ID + '_' + this.nativeID;
// 例如："SkiaDomView_123"
```

**C++ 层通过 ID 管理实例：**
```cpp
// plugin_render.h:228
static std::unordered_map<std::string, std::shared_ptr<PluginRender>> m_instance;
//                    ↑
//                    key = XComponent ID
```

### 1.2 OnSurfaceCreated 回调

**一个 XComponent 对应一个 NativeWindow：**
```cpp
// plugin_render.cpp:43
void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window) {
    // 1. 从 component 获取 ID
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
    OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    std::string id(idStr);  // ← XComponent 的唯一 ID
    
    // 2. 通过 ID 获取对应的 PluginRender 实例
    auto render = PluginRender::GetInstance(id);
    
    // 3. 保存 NativeWindow（一个实例对应一个 window）
    render->m_window = static_cast<OHNativeWindow *>(window);
    //                    ↑
    //                    一个 PluginRender 实例只有一个 m_window
}
```

**关键点：**
- 每个 `OH_NativeXComponent*` 有唯一的 ID
- 每个 ID 对应一个 `PluginRender` 实例
- 每个 `PluginRender` 实例只有一个 `m_window`

### 1.3 PluginRender 实例管理

**通过 ID 管理，确保一一对应：**
```cpp
// plugin_render.cpp:32
std::shared_ptr<PluginRender> PluginRender::GetInstance(std::string &id) {
    if (m_instance.find(id) == m_instance.end()) {
        // 如果不存在，创建新实例
        auto instance = std::make_shared<PluginRender>(...);
        m_instance[id] = instance;  // ← 用 ID 作为 key
        return instance;
    } else {
        // 如果已存在，返回已有实例
        return m_instance[id];  // ← 同一个 ID 返回同一个实例
    }
}
```

**销毁时也通过 ID：**
```cpp
// plugin_render.cpp:127
void PluginRender::Release(std::string &id) {
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        m_instance.erase(m_instance.find(id));  // ← 通过 ID 删除
    }
}
```

---

## 2. 完整对应关系

### 2.1 关系链

```
┌─────────────────────────────────────────────────────────┐
│  ArkTS 层                                                │
│  ┌───────────────────────────────────────────────────┐ │
│  │  XComponent                                       │ │
│  │  id: "SkiaDomView_123"                            │ │
│  │  (唯一标识)                                        │ │
│  └───────────────────────────────────────────────────┘ │
└───────────────────┬─────────────────────────────────────┘
                    ↓ 通过 NAPI
┌─────────────────────────────────────────────────────────┐
│  C++ 层                                                  │
│  ┌───────────────────────────────────────────────────┐ │
│  │  OH_NativeXComponent*                             │ │
│  │  id: "SkiaDomView_123"                            │ │
│  │  (通过 GetXComponentId 获取)                      │ │
│  └───────────────────────────────────────────────────┘ │
└───────────────────┬─────────────────────────────────────┘
                    ↓ OnSurfaceCreated 回调
┌─────────────────────────────────────────────────────────┐
│  系统层                                                  │
│  ┌───────────────────────────────────────────────────┐ │
│  │  OHNativeWindow*                                  │ │
│  │  (通过 OnSurfaceCreated 的 window 参数获取)       │ │
│  └───────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### 2.2 存储结构

```cpp
// PluginRender 实例
class PluginRender {
    OHNativeWindow *m_window;  // ← 一个实例只有一个 window
    // ...
};

// 实例管理
std::unordered_map<std::string, std::shared_ptr<PluginRender>> m_instance;
//                    ↑                    ↑
//                    ID              一个实例
//                 (唯一)            (一个 window)
```

**关系：**
- 1 个 ID → 1 个 PluginRender 实例
- 1 个 PluginRender 实例 → 1 个 m_window
- 因此：1 个 ID → 1 个 NativeWindow

---

## 3. 为什么是一一对应？

### 3.1 系统设计

**HarmonyOS 系统设计：**
- 一个 XComponent 只能有一个原生窗口
- 系统为每个 XComponent 创建一个对应的 NativeWindow
- 通过 OnSurfaceCreated 回调传递，确保对应关系

### 3.2 生命周期绑定

**创建时：**
```
XComponent 创建
    ↓
系统创建 NativeWindow
    ↓
OnSurfaceCreated 回调
    ↓
通过 component 获取 ID
    ↓
通过 ID 获取 PluginRender 实例
    ↓
保存 window 到实例的 m_window
```

**销毁时：**
```
XComponent 销毁
    ↓
OnSurfaceDestroyed 回调
    ↓
通过 component 获取 ID
    ↓
通过 ID 获取 PluginRender 实例
    ↓
销毁实例和 window
```

**关键：** 整个生命周期都是通过 ID 绑定，确保一一对应

### 3.3 代码验证

**OnSurfaceCreated 中：**
```cpp
// 1. 获取 component 的 ID
OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
std::string id(idStr);

// 2. 通过 ID 获取实例（如果不存在会创建）
auto render = PluginRender::GetInstance(id);

// 3. 保存 window（一个实例只有一个 window）
render->m_window = nativeWindow;
```

**RegisterView 中：**
```cpp
// 使用同一个 ID 获取实例
auto instance = m_instance[id];

// 使用同一个 window
view->surfaceAvailable(instance->m_window, 1, 1);
```

---

## 4. 实际例子

### 4.1 单个 XComponent

```
场景：页面上有一个 Canvas 组件

XComponent (id: "SkiaDomView_123")
    ↓ 一一对应
OH_NativeXComponent* (id: "SkiaDomView_123")
    ↓ 一一对应
OHNativeWindow* (通过 OnSurfaceCreated 获取)
    ↓ 一一对应
PluginRender 实例 (id: "SkiaDomView_123", m_window: window)
```

### 4.2 多个 XComponent

```
场景：页面上有两个 Canvas 组件

XComponent 1 (id: "SkiaDomView_123")
    ↓ 一一对应
OH_NativeXComponent* 1 (id: "SkiaDomView_123")
    ↓ 一一对应
OHNativeWindow* 1
    ↓ 一一对应
PluginRender 实例 1 (id: "SkiaDomView_123", m_window: window1)

XComponent 2 (id: "SkiaDomView_456")
    ↓ 一一对应
OH_NativeXComponent* 2 (id: "SkiaDomView_456")
    ↓ 一一对应
OHNativeWindow* 2
    ↓ 一一对应
PluginRender 实例 2 (id: "SkiaDomView_456", m_window: window2)
```

**关键：** 每个 XComponent 都有独立的对应关系

---

## 5. 总结

### 5.1 一一对应关系

```
1 个 XComponent (ArkTS)
    ↓ 通过 ID 绑定
1 个 OH_NativeXComponent* (C++)
    ↓ 通过 OnSurfaceCreated 回调
1 个 OHNativeWindow* (系统)
    ↓ 存储在
1 个 PluginRender 实例 (m_window)
```

### 5.2 关键点

1. **通过 ID 绑定**：每个 XComponent 有唯一 ID
2. **实例管理**：通过 ID 管理 PluginRender 实例
3. **生命周期绑定**：创建和销毁都通过 ID 绑定
4. **系统保证**：系统为每个 XComponent 创建一个 NativeWindow

### 5.3 代码体现

```cpp
// 创建时
OnSurfaceCreated(component, window) {
    id = GetXComponentId(component);  // ← 获取 ID
    render = GetInstance(id);         // ← 通过 ID 获取实例
    render->m_window = window;         // ← 保存 window（一一对应）
}

// 使用时
RegisterView(id) {
    render = GetInstance(id);         // ← 通过 ID 获取实例
    render->m_window                  // ← 使用同一个 window
}
```

---

**结论：NativeXComponent 和 NativeWindow 确实是一一对应的！**

**文档版本**: 1.0  
**最后更新**: 2025-01-XX  
**维护者**: AI Assistant
