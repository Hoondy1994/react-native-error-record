# FastImage 三个 Trace 完整分析说明

## 📋 目录

1. [渲染问题 Trace](#渲染问题-trace)
   - [Trace 1: setAttribute 的时间（应用层到 HarmonyOS）](#trace-1-setattribute-的时间应用层到-harmonyos)
   - [Trace 2: 前端应用到 FastImage 的时间](#trace-2-前端应用到-fastimage-的时间)
2. [网络问题 Trace](#网络问题-trace)
   - [Trace 3: 应用层传递到下载完成的时间](#trace-3-应用层传递到下载完成的时间)
3. [总结](#总结)

---

## 🎨 渲染问题 Trace

### Trace 1: setAttribute 的时间（应用层到 HarmonyOS）

#### 📍 位置

**文件**：`FastImageNode.cpp`

**位置 1**：`setSources()` 方法（第 84-96 行）
```cpp
FastImageNode& FastImageNode::setSources(std::string const& uri, std::string prefix) {
    // ... URI 处理逻辑 ...
    
#ifdef WITH_HITRACE_SYSTRACE
    // ⭐⭐⭐ Trace: 测量应用层到 HarmonyOS 系统层 setAttribute 的调用时间
    react::SystraceSection s("FastImageNode::setSources::setAttribute");
#endif
    maybeThrow(NativeNodeApi::getInstance()->setAttribute(m_nodeHandle, NODE_IMAGE_SRC, &item));
    return *this;
}
```

**位置 2**：`setResizeMode()` 方法（第 98-117 行）
```cpp
FastImageNode& FastImageNode::setResizeMode(
    facebook::react::ImageResizeMode const& mode) {
  // ... 模式转换逻辑 ...
  
#ifdef WITH_HITRACE_SYSTRACE
  react::SystraceSection s("FastImageNode::setResizeMode::setAttribute");
#endif
  maybeThrow(NativeNodeApi::getInstance()->setAttribute(
      m_nodeHandle, NODE_IMAGE_OBJECT_FIT, &item));
  return *this;
}
```

#### 🎯 测量内容

**测量范围**：
- **从**：应用层（FastImage C++ 代码）调用 `setAttribute()` 开始
- **到**：系统 API `setAttribute()` 返回
- **含义**：应用层 → HarmonyOS 系统层的同步调用时间

#### 📊 调用链

```
应用层（FastImage C++ 代码）
  ↓
SystraceSection s("FastImageNode::setSources::setAttribute")  ← ⭐ Trace 开始
  ↓
NativeNodeApi::setAttribute(...)  ← 调用系统 API
  ↓
HarmonyOS 系统层（ArkUI 框架）
  ├─ 处理属性设置
  ├─ 更新 UI 节点
  └─ 处理完成
  ↓
setAttribute 返回  ← ⭐ Trace 结束
```

#### 💡 实际意义

**如果 Trace 耗时很长（> 10ms）**：
- 说明 HarmonyOS 系统层（ArkUI 框架）处理 `setAttribute` 很慢
- 可能是系统层的性能问题
- 需要联系 HarmonyOS 系统团队

**如果 Trace 耗时很短（< 1ms）**：
- 说明系统层调用很快
- 如果前端到这里的总时间很长，问题在调用链路上：
  - React Native Fabric 渲染层耗时
  - JSI 调用耗时
  - Props 更新处理耗时

#### ⚠️ 注意事项

- **只测量同步调用时间**：`setAttribute` 是同步调用，Trace 只测量函数执行时间
- **不包含异步操作**：如果 ArkUI 内部有异步处理（如网络下载、图片解码），这些不在 Trace 范围内
- **不包含渲染时间**：实际渲染到屏幕的时间不在这个 Trace 中
- **适用于所有图片类型**：包括网络图片、Base64 图片、本地资源图片

---

### Trace 2: 前端应用到 FastImage 的时间

#### 📍 位置

**文件**：`FastImageViewComponentInstance.cpp`（C-API 路径）

**位置**：`onPropsChanged()` 方法开始处
```cpp
void FastImageViewComponentInstance::onPropsChanged(SharedConcreteProps const &props) {
    CppComponentInstance::onPropsChanged(props);
    
#ifdef WITH_HITRACE_SYSTRACE
    // ⭐⭐⭐ Trace 2 开始：测量从前端应用到 FastImage 的时间
    // 这个 Trace 测量的是从 React Native Fabric 渲染层调用 onPropsChanged 到方法执行完成的时间
    // 适用于所有图片类型：网络图片、Base64 图片、本地资源图片
    facebook::react::SystraceSection s("FastImage::onPropsChanged");
#endif
    
    // ... 处理 Props ...
    // 对于 Base64 图片（无 headers），会调用：
    // this->getLocalRootArkUINode().setSources(uri, ...);
    // 这会触发 Trace 1（FastImageNode::setSources::setAttribute）
    
    // ... 其他 Props 处理（resizeMode、tintColor 等）...
    
    // ⭐⭐⭐ Trace 2 结束：当 SystraceSection 对象离开作用域时（方法返回时）
    // SystraceSection 是 RAII 对象，析构时自动结束 trace
}
```

**Trace 2 的结束位置**：
- **自动结束**：`SystraceSection` 是 RAII（Resource Acquisition Is Initialization）对象
- **开始时机**：创建 `SystraceSection` 对象时（进入作用域）
- **结束时机**：`SystraceSection` 对象析构时（离开作用域），即 `onPropsChanged()` 方法返回时
- **测量范围**：从方法开始到方法返回的**整个方法执行时间**

**⚠️ 重要说明**：
- **Base64 图片走 C-API 路径**：Base64 图片（无 headers）会经过 `FastImageViewComponentInstance::onPropsChanged` → `FastImageNode::setSources` → `setAttribute`
- **Base64 图片不走 ETS 路径**：不会经过 `RNFastImage.ets` 的 `build()` 方法，所以 ETS 路径的 trace 不会触发
- **Trace 2 是第一个入口点**：对于 Base64 图片，这是从前端到 FastImage 的第一个 trace 点

#### 🎯 测量内容

**测量范围**：
- **从**：React Native Fabric 渲染层调用 `onPropsChanged()`（Trace 开始）
- **到**：`onPropsChanged()` 方法返回（Trace 结束，`SystraceSection` 对象析构）
- **含义**：React Native Fabric 渲染层 → FastImage 组件接收并处理 Props 的完整时间

**⚠️ 重要说明**：
- **"前端"指的是 React Native Fabric 渲染层**：Trace 2 测量的是从 Fabric 调用 `onPropsChanged()` 到方法返回的时间
- **不包括 JS 层到 Fabric 层的传递时间**：JS 层 → Fabric 层的传递在 React Native 内部完成，不在 FastImage 代码中，无法直接测量
- **`onPropsChanged()` 方法结束确实代表了"前端应用到 FastImage"的时间**：这里的"前端"指的是 Fabric 渲染层

**包含的内容**：
- ✅ Props 接收和处理（从 Fabric 层接收）
- ✅ URI 解析和验证
- ✅ 调用 `setSources()`、`setResizeMode()` 等（这些调用会触发 Trace 1）
- ✅ 其他 Props 处理（tintColor、resizeMode 等）

**不包含的内容**：
- ❌ JS 层到 Fabric 层的传递时间（React Native 内部，不在 FastImage 代码中）
- ❌ 系统层的异步处理（网络下载、图片解码、渲染）
- ❌ `onComplete()` 回调的执行时间

#### 📊 调用链

**对于 Base64 图片（C-API 路径）**：
```
前端 JS/TS
  ↓
<FastImage source={{ uri: "data:image/png;base64,xxx" }} />
  ↓
React Native Fabric 渲染层（JS → Fabric 传递，不在 Trace 2 范围内）
  ↓
FastImageViewComponentInstance::onPropsChanged()  ← ⭐ Trace 2 开始（Fabric 调用）
  ├─ 处理 Props（source、resizeMode、tintColor 等）
  ├─ 检测到无 headers，走 C-API 路径
  ├─ 调用 this->getLocalRootArkUINode().setSources(uri, ...)
  │   └─ 触发 FastImageNode::setSources()
  │       └─ 触发 Trace 1（FastImageNode::setSources::setAttribute）
  └─ 更新组件状态
  ↓
onPropsChanged 返回  ← ⭐ Trace 2 结束（Fabric → FastImage 完成）
```

**Trace 2 测量的是**：从 React Native Fabric 渲染层调用 `onPropsChanged()` 到方法返回的时间
- ✅ 这确实代表了"前端应用到 FastImage"的时间
- ✅ 这里的"前端"指的是 React Native Fabric 渲染层
- ✅ `onPropsChanged()` 方法结束 = Fabric 层到 FastImage 的传递和处理完成

**对于网络图片（有 headers，ETS 路径）**：
```
前端 JS/TS
  ↓
<FastImage source={{ uri, headers: {...} }} />
  ↓
React Native Fabric 渲染层
  ↓
FastImageViewComponentInstance::onPropsChanged()  ← ⭐ Trace 2 开始
  ├─ 检测到有 headers，走 ETS 路径
  ├─ 调用 GetHeaderUri() → prefetchImage()
  └─ 更新组件状态
  ↓
onPropsChanged 返回  ← ⭐ Trace 2 结束
  ↓
ETS 层：RNFastImage.ets → build() → Image(...)  ← ETS 路径的 trace
```

#### 💡 实际意义

**如果 Trace 耗时很长（> 5ms）**：
- 说明 React Native Fabric 渲染层或 Props 处理很慢
- 可能是 Fabric 渲染层的性能问题
- 需要检查 Props 处理的逻辑

**如果 Trace 耗时很短（< 1ms）**：
- 说明 Props 处理很快
- 如果总时间很长，问题可能在：
  - 前端渲染时间
  - React Native 桥接层（JSI/Bridge）
  - 其他组件实例的处理

#### ⚠️ 注意事项

- **包含所有 Props 处理**：不仅包括图片源设置，还包括 resizeMode、tintColor 等所有属性的处理
- **不包含系统层处理**：不包含 `setAttribute` 调用后的系统层处理时间
- **需要配合其他 Trace**：需要配合 `setAttribute` Trace 才能完整分析性能问题

---

## 🌐 网络问题 Trace

### Trace 3: 应用层传递到下载完成的时间

#### 📍 位置

**文件**：`FastImageViewComponentInstance.cpp`

**位置 1**：`onPropsChanged()` 方法（开始时间记录）
```cpp
void FastImageViewComponentInstance::onPropsChanged(SharedConcreteProps const &props) {
    // ...
    if (!props->source.headers.empty()) {
        GetHeaderUri(props->source.uri, props->source.headers);
    } else {
        // ⭐⭐⭐ Trace: 网络下载开始（只测量下载时间，不包含解码渲染）
        // ⚠️ 注意：无headers时，网络下载由ArkUI Image组件在系统层完成
        auto startTime = std::chrono::high_resolution_clock::now();
        m_imageLoadStartTime = startTime;
        
        if(!uri.empty()){
           this->getLocalRootArkUINode().setSources(uri, ...); 
        }
    }
}
```

**位置 2**：`onComplete()` 方法（结束时间记录）
```cpp
void FastImageViewComponentInstance::onComplete(float width, float height) {
    // ⭐⭐⭐ Trace: 记录结束时间并计算耗时（应用层传递到下载完成的时间）
    auto endTime = std::chrono::high_resolution_clock::now();
    
    if (m_imageLoadStartTime != defaultTimePoint) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - m_imageLoadStartTime);
        DLOG(INFO) << "[FastImage::网络下载Trace] 网络图片加载完成（通过ArkUI Image）: URI=" << uriShort 
                   << ", 总耗时（下载+解码+渲染）=" << duration.count() << "ms"
                   << ", ⚠️ 注意：无法单独测量纯下载时间，因为下载在系统层完成";
    }
}
```

#### 🎯 测量内容

**测量范围**：
- **从**：应用层调用 `setSources()` 开始
- **到**：`onComplete()` 回调触发
- **含义**：应用层传递图片源 → 下载完成（包含解码和渲染）

#### 📊 调用链

```
应用层（FastImage C++ 代码）
  ↓
m_imageLoadStartTime = now()  ← ⭐ Trace 开始
  ↓
setSources(uri, ...)  ← 设置图片源
  ↓
HarmonyOS 系统层（ArkUI Image 组件）
  ├─ 网络下载（如果是网络 URI）
  ├─ Base64 解码（如果是 Base64 URI）
  ├─ 图片格式解码（PNG/JPEG → PixelMap）
  ├─ GPU 纹理上传
  └─ 渲染到屏幕
  ↓
onComplete() 回调触发  ← ⭐ Trace 结束
  ↓
计算耗时 = endTime - m_imageLoadStartTime
```

#### 💡 实际意义

**对于网络图片**：
- 测量从应用层传递 URI 到图片下载完成的时间
- 包含网络下载、解码、渲染的完整时间
- 如果耗时很长，可能是：
  - 网络延迟
  - 图片解码慢
  - 渲染慢

**对于 Base64 图片**：
- 测量从应用层传递 Base64 URI 到图片渲染完成的时间
- 不包含网络下载，只包含解码和渲染
- 如果耗时很长，可能是：
  - Base64 解码慢
  - 图片格式解码慢
  - 渲染慢

#### ⚠️ 注意事项

- **包含完整流程**：包含下载、解码、渲染的完整时间
- **不区分各阶段**：无法单独测量网络下载时间、解码时间、渲染时间
- **需要配合其他 Trace**：需要配合 `setAttribute` Trace 才能区分各阶段的耗时
- **系统层限制**：对于无 headers 的网络图片，下载在系统层完成，无法单独测量纯下载时间

---

## 📊 三个 Trace 的对比

| Trace | 位置 | 测量内容 | 测量范围 | 用途 |
|-------|------|---------|---------|------|
| **Trace 1** | `setSources::setAttribute`<br>`setResizeMode::setAttribute` | 应用层 → 系统层 | 同步调用时间 | 定位系统层性能问题 |
| **Trace 2** | `onPropsChanged` | 前端应用 → FastImage | Props 处理时间 | 定位 Fabric 渲染层问题 |
| **Trace 3** | `onPropsChanged` → `onComplete` | 应用层 → 下载完成 | 完整流程时间 | 定位整体性能问题 |

---

## 🎯 使用场景

### 场景 1: 渲染延迟问题

**问题**：Base64 图片渲染有延迟

**分析步骤**：
1. 查看 **Trace 1**（setAttribute）：如果耗时很长，说明系统层慢
2. 查看 **Trace 2**（onPropsChanged）：如果耗时很长，说明 Fabric 渲染层慢
3. 查看 **Trace 3**（onComplete）：如果耗时很长，说明整体流程慢

**结论**：
- 如果 Trace 1 很快，Trace 2 很快，但 Trace 3 很慢 → 问题在解码/渲染阶段
- 如果 Trace 1 很慢 → 问题在系统层
- 如果 Trace 2 很慢 → 问题在 Fabric 渲染层

### 场景 2: 网络下载延迟问题

**问题**：网络图片下载慢

**分析步骤**：
1. 查看 **Trace 3**（onComplete）：测量总时间
2. 查看 ETS 层的网络下载 Trace（`fetchImage`、`performDownload`）：测量纯下载时间（仅当有 headers 时）
3. 对比两者差异：差异部分就是解码+渲染时间

**结论**：
- 如果 Trace 3 很长，但网络下载 Trace 很短 → 问题在解码/渲染阶段
- 如果 Trace 3 很长，网络下载 Trace 也很长 → 问题在网络下载阶段
- 如果无 headers，无法单独测量纯下载时间，只能通过 Trace 3 了解总时间

---

## 📝 代码位置总结

### Trace 1: setAttribute

**文件**：`FastImageNode.cpp`
- `setSources()`：第 84-96 行
- `setResizeMode()`：第 98-117 行

**Trace 名称**：
- `FastImageNode::setSources::setAttribute`
- `FastImageNode::setResizeMode::setAttribute`

**启用条件**：需要定义 `WITH_HITRACE_SYSTRACE` 宏

### Trace 2: onPropsChanged

**文件**：`FastImageViewComponentInstance.cpp`（C-API 路径）
- `onPropsChanged()`：方法开始处

**Trace 名称**：
- `FastImage::onPropsChanged`

**⚠️ 重要说明**：
- **Base64 图片会触发此 Trace**：Base64 图片走 C-API 路径，会经过 `onPropsChanged`
- **Base64 图片不会触发 ETS 路径的 trace**：不会经过 `RNFastImage.ets` 的 `build()` 方法
- **这是 Base64 图片从前端到 FastImage 的第一个 trace 点**

**启用条件**：需要定义 `WITH_HITRACE_SYSTRACE` 宏

### Trace 3: 应用层到下载完成

**文件**：`FastImageViewComponentInstance.cpp`
- 开始时间：`onPropsChanged()` 中调用 `setSources()` 前
- 结束时间：`onComplete()` 回调中

**日志输出**：
- `[FastImage::网络下载Trace] 网络图片加载完成（通过ArkUI Image）: URI=..., 总耗时（下载+解码+渲染）=...ms`

**启用条件**：始终启用（使用 `std::chrono` 测量）

---

## ✅ 总结

### 三个 Trace 的作用

1. **Trace 1（setAttribute）**：
   - 测量应用层到系统层的同步调用时间
   - 用于定位系统层性能问题
   - 适用于所有图片类型（网络、Base64、本地资源）

2. **Trace 2（onPropsChanged）**：
   - 测量前端应用到 FastImage 的 Props 处理时间
   - 用于定位 Fabric 渲染层性能问题

3. **Trace 3（应用层到下载完成）**：
   - 测量从应用层传递图片源到下载/渲染完成的完整时间
   - 用于定位整体性能问题
   - 包含下载、解码、渲染的完整流程

### 配合使用

三个 Trace 需要配合使用才能完整分析性能问题：
- **Trace 1** + **Trace 2**：区分应用层和系统层的问题
- **Trace 3**：了解整体性能情况
- **Trace 1** + **Trace 3**：区分同步调用和异步处理的时间

### 关键限制

- **无 headers 网络图片**：无法单独测量纯下载时间，因为下载在系统层完成
- **有 headers 网络图片**：可以通过 ETS 层的 `fetchImage` 和 `performDownload` 测量纯下载时间
- **Base64 图片**：不涉及网络下载，只包含解码和渲染时间

---

## 📅 创建时间

2025-01-06

## 📝 相关文件

- `FastImageNode.cpp`：Trace 1 的位置
- `FastImageViewComponentInstance.cpp`：Trace 2 和 Trace 3 的位置
