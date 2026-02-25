# FastImage Trace 分析说明

## 🎯 为什么在 `setResizeMode` 的 `setAttribute` 调用处加 Trace？

### 核心目的：测量从前端应用到 FastImage 渲染的时间

**问题背景**：
- 用户报告 FastImage 渲染有延迟
- 需要定位延迟发生在哪个阶段
- 需要测量从 React Native 前端应用（JavaScript）到 FastImage 实际设置属性的时间

### Trace 的作用

**1. 性能分析（Performance Profiling）**
- 记录关键函数的执行时间
- 识别性能瓶颈
- 定位延迟发生的具体位置

**2. 调用链追踪（Call Chain Tracking）**
- 追踪从前端到 Native 层的完整调用路径
- 测量各个阶段的耗时
- 识别哪个阶段耗时最长

**3. 问题定位（Issue Identification）**
- 如果 `setAttribute` 调用耗时很长，说明问题在 HarmonyOS 系统层
- 如果 `setAttribute` 调用很快，但前端到这里的路径耗时很长，说明问题在 React Native 桥接层或 JSI 层

---

## 📊 完整的调用链路

### 从前端到 FastImage 的完整路径

```
┌─────────────────────────────────────────────────────────────┐
│ 1. React Native 前端（JavaScript/TypeScript）                │
│    <FastImage resizeMode="cover" />                         │
│    ↓ 时间点 T1: 前端应用开始                                 │
│                                                             │
│ 2. React Native Codegen 生成的组件                          │
│    FastImageViewComponentDescriptor                         │
│    ↓ 时间点 T2: Codegen 处理                                │
│                                                             │
│ 3. Fabric 渲染层（C++）                                     │
│    FastImageViewComponentInstance::updateProps()            │
│    ↓ 时间点 T3: Fabric 更新 Props                           │
│                                                             │
│ 4. FastImage C++ 实现层                                     │
│    FastImageViewComponentInstance::onPropsChanged()         │
│    ↓ 时间点 T4: Props 变化回调                              │
│                                                             │
│ 5. FastImageNode 设置属性                                   │
│    FastImageNode::setResizeMode()                           │
│    ↓ 时间点 T5: 开始设置 resizeMode                         │
│                                                             │
│ 6. HarmonyOS ArkUI Native API 调用                          │
│    NativeNodeApi::setAttribute(..., NODE_IMAGE_OBJECT_FIT) │
│    ↓ 时间点 T6: 调用系统 API（这里加 Trace）⭐⭐⭐          │
│                                                             │
│ 7. HarmonyOS 系统层                                         │
│    ArkUI 框架处理 setAttribute                              │
│    ↓ 时间点 T7: 系统层处理完成                              │
│                                                             │
│ 8. 图片实际渲染到屏幕                                        │
│    GPU 渲染、合成、上屏                                      │
│    ↓ 时间点 T8: 渲染完成                                    │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔍 添加 Trace 的具体位置

### 文件位置
`example/harmony/entry/oh_modules/@react-native-oh-tpl/react-native-fast-image/src/main/cpp/FastImageNode.cpp`

### 方法位置
`FastImageNode::setResizeMode()` 方法（第74-94行）

### 需要修改的代码

**修改前**（第91-92行）：
```cpp
maybeThrow(NativeNodeApi::getInstance()->setAttribute(
    m_nodeHandle, NODE_IMAGE_OBJECT_FIT, &item));
```

**修改后**（添加 Trace）：
```cpp
#ifdef WITH_HITRACE_SYSTRACE
#include <react/renderer/debug/SystraceSection.h>
#endif

FastImageNode& FastImageNode::setResizeMode(
    facebook::react::ImageResizeMode const& mode) {
  int32_t val = ARKUI_OBJECT_FIT_COVER;
  if (mode == facebook::react::ImageResizeMode::Cover) {
    val = ARKUI_OBJECT_FIT_COVER;
  } else if (mode == facebook::react::ImageResizeMode::Contain) {
    val = ARKUI_OBJECT_FIT_CONTAIN;
  } else if (mode == facebook::react::ImageResizeMode::Stretch) {
    val = ARKUI_OBJECT_FIT_FILL;
  } else if (
      mode == facebook::react::ImageResizeMode::Center ||
      mode == facebook::react::ImageResizeMode::Repeat) {
    val = ARKUI_OBJECT_FIT_NONE;
  }

  ArkUI_NumberValue value[] = {{.i32 = val}};
  ArkUI_AttributeItem item = {value, sizeof(value) / sizeof(ArkUI_NumberValue)};
  
  // ⭐⭐⭐ 添加 Trace：测量从前端应用到 FastImage 设置属性的时间
#ifdef WITH_HITRACE_SYSTRACE
  react::SystraceSection s("FastImageNode::setResizeMode::setAttribute");
#endif
  
  maybeThrow(NativeNodeApi::getInstance()->setAttribute(
      m_nodeHandle, NODE_IMAGE_OBJECT_FIT, &item));
  return *this;
}
```

---

## 📝 完整修改代码

需要在文件头部添加头文件，在 `setResizeMode` 方法中添加 Trace：

```cpp
// FastImageNode.cpp

#include "FastImageNode.h"

#include <string_view>
#include "RNOH/arkui/NativeNodeApi.h"

// ⭐⭐⭐ 添加 Trace 头文件
#ifdef WITH_HITRACE_SYSTRACE
#include <react/renderer/debug/SystraceSection.h>
#endif

// ... 其他代码 ...

FastImageNode& FastImageNode::setResizeMode(
    facebook::react::ImageResizeMode const& mode) {
  int32_t val = ARKUI_OBJECT_FIT_COVER;
  if (mode == facebook::react::ImageResizeMode::Cover) {
    val = ARKUI_OBJECT_FIT_COVER;
  } else if (mode == facebook::react::ImageResizeMode::Contain) {
    val = ARKUI_OBJECT_FIT_CONTAIN;
  } else if (mode == facebook::react::ImageResizeMode::Stretch) {
    val = ARKUI_OBJECT_FIT_FILL;
  } else if (
      mode == facebook::react::ImageResizeMode::Center ||
      mode == facebook::react::ImageResizeMode::Repeat) {
    val = ARKUI_OBJECT_FIT_NONE;
  }

  ArkUI_NumberValue value[] = {{.i32 = val}};
  ArkUI_AttributeItem item = {value, sizeof(value) / sizeof(ArkUI_NumberValue)};
  
  // ⭐⭐⭐ 添加 Trace：测量 setAttribute 调用的执行时间
  // 这个 Trace 会记录从调用 setAttribute 到返回的时间
  // 如果这个时间很长，说明问题在 HarmonyOS 系统层（ArkUI 框架）
  // 如果这个时间很短，说明问题在前端到这里的调用路径上
#ifdef WITH_HITRACE_SYSTRACE
  react::SystraceSection s("FastImageNode::setResizeMode::setAttribute");
#endif
  
  maybeThrow(NativeNodeApi::getInstance()->setAttribute(
      m_nodeHandle, NODE_IMAGE_OBJECT_FIT, &item));
  return *this;
}
```

---

## 🎯 Trace 能帮我们定位什么？

### 1. 如果 `setAttribute` 调用耗时很长（> 10ms）

**说明问题在 HarmonyOS 系统层**：
- ArkUI 框架处理 `setAttribute` 很慢
- 可能是系统层的性能问题
- 需要联系 HarmonyOS 系统团队

### 2. 如果 `setAttribute` 调用很快（< 1ms），但前端到这里的时间很长

**说明问题在调用链路上**：
- React Native Fabric 渲染层耗时
- JSI 调用耗时
- Props 更新处理耗时
- 需要在前面的步骤也添加 Trace

### 3. 测量从前端应用到 FastImage 的总时间

**需要在前端也添加时间点**：
```typescript
// 在前端代码中
const startTime = performance.now();
<FastImage resizeMode="cover" />
const endTime = performance.now();
console.log(`前端到 FastImage 总时间: ${endTime - startTime}ms`);
```

然后对比：
- 前端时间戳 vs Trace 记录的时间戳
- 差值就是从前端到 `setAttribute` 的总时间

---

## 📊 如何查看 Trace 结果？

### 方法 1: 使用 DevEco Studio 的性能分析器

1. 打开 DevEco Studio
2. 运行应用
3. 打开 Profiler / Performance Analyzer
4. 查看 `FastImageNode::setResizeMode::setAttribute` 的执行时间

### 方法 2: 使用 HiTrace 工具

```bash
# 在设备上启用 HiTrace
hdc shell hitrace -t 10 -b 32768

# 在应用中触发 FastImage 渲染
# 然后查看 trace 文件

# 使用 Python 脚本解析 trace 文件
python hitrace_parser.py trace_file.html
```

### 方法 3: 使用 Chrome DevTools（如果支持）

如果 React Native 支持 Chrome DevTools 的性能分析，可以在 Chrome DevTools 中查看 Flame Chart，找到 `FastImageNode::setResizeMode::setAttribute` 的执行时间。

---

## 🔧 还需要在哪里添加 Trace？

为了完整追踪从前端到 FastImage 的路径，建议在以下位置也添加 Trace：

### 1. 前端应用时间点（JavaScript）
```typescript
const renderStartTime = performance.now();
<FastImage resizeMode="cover" />
console.log(`前端渲染开始: ${renderStartTime}`);
```

### 2. ComponentInstance 更新时间点（C++）
```cpp
// FastImageViewComponentInstance.cpp
void FastImageViewComponentInstance::onPropsChanged(...) {
#ifdef WITH_HITRACE_SYSTRACE
  react::SystraceSection s("FastImageViewComponentInstance::onPropsChanged");
#endif
  // ... 更新逻辑
}
```

### 3. setResizeMode 开始时间点（C++）
```cpp
FastImageNode& FastImageNode::setResizeMode(...) {
#ifdef WITH_HITRACE_SYSTRACE
  react::SystraceSection s("FastImageNode::setResizeMode");
#endif
  // ... 设置逻辑
}
```

---

## 📈 预期结果分析

### 正常情况（无延迟）
- 前端渲染时间：< 1ms
- ComponentInstance 更新：< 1ms
- setResizeMode 执行：< 0.5ms
- setAttribute 调用：< 1ms
- **总时间：< 3.5ms**

### 异常情况（有延迟）
- 前端渲染时间：正常（< 1ms）
- ComponentInstance 更新：正常（< 1ms）
- setResizeMode 执行：正常（< 0.5ms）
- setAttribute 调用：**异常（> 10ms）** ← 说明问题在 HarmonyOS 系统层

或：

- 前端渲染时间：正常（< 1ms）
- ComponentInstance 更新：**异常（> 5ms）** ← 说明问题在 Fabric 渲染层
- setResizeMode 执行：正常
- setAttribute 调用：正常

---

## ✅ 总结

**为什么在这里加 Trace**：
1. **关键节点**：`setAttribute` 是前端应用和 HarmonyOS 系统层的分界点
2. **性能测量**：可以测量从 React Native 到系统层的调用时间
3. **问题定位**：如果这里耗时很长，说明问题在系统层；如果这里很快，说明问题在前面的调用路径
4. **完整追踪**：配合前端时间点，可以测量从前端应用到 FastImage 的完整时间

**下一步**：
1. 添加 Trace 代码
2. 运行应用并触发 FastImage 渲染
3. 查看 Trace 结果，分析各个阶段的耗时
4. 根据结果定位性能瓶颈
