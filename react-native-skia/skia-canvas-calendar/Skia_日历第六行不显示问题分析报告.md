# React Native Skia 日历第六行不显示问题 - 完整分析报告

## 一、问题描述

### 1.1 现象

在 HarmonyOS 上使用 React Native Skia 绘制日历组件时：

- **5 行月份**（如 2025 年 8 月）：日历正常显示 5 行日期
- **6 行月份**（如 2025 年 11 月）：切换后**第六行不显示**，内容被裁切或压扁
- **切换异常**：在 5 行与 6 行月份之间切换时，有时显示正常 6 行，有时显示异常（第六行缺失或布局错乱）

### 1.2 正常与异常对比

| 状态 | 描述 |
|------|------|
| **正常 6 行** | 第一行从 26 开始（上月补全），共 6 行完整显示 |
| **异常 6 行** | 第六行被裁切，或首行起始日期错误（如从 2 开始而非 26） |

### 1.3 技术栈

- React Native + React Native HarmonyOS
- @shopify/react-native-skia（Skia 绘制）
- HarmonyOS XComponent（原生 Surface 容器）
- 日历行数由 `rowCount = ceil((startDay + daysInMonth) / 7)` 动态计算

---

## 二、排查过程

### 2.1 添加日志定位

在以下三处添加宽高日志，用于追踪尺寸传递：

| 位置 | 日志内容 | 作用 |
|------|----------|------|
| `plugin_render.h` napi SurfaceSizeChanged | `[实际调用 surfaceSizeChanged 宽高] width/height` | 看 napi 传给 Skia 的尺寸 |
| `plugin_render.cpp` OnSurfaceChanged | `[宽高] width/height` | 看 native 从 XComponent 拿到的尺寸 |
| `RNSkOpenGLCanvasProvider.h` renderToCanvas | `[宽高] width/height` | 看实际绘制时 Surface 的尺寸 |

### 2.2 日志现象

```
napi SurfaceSizeChanged [实际调用 surfaceSizeChanged 宽高] width: 1224 height: 1154   ← 6 行时正确
napi SurfaceSizeChanged [实际调用 surfaceSizeChanged 宽高] width: 1224 height: 958    ← 5 行时
napi SurfaceSizeChanged [实际调用 surfaceSizeChanged 宽高] width: 1224 height: 1154
napi SurfaceSizeChanged [实际调用 surfaceSizeChanged 宽高] width: 1224 height: 958
...（1154 与 958 交替）

renderToCanvas [宽高] width: 1224 height: 958   ← 始终 958！
renderToCanvas [宽高] width: 1224 height: 958
renderToCanvas [宽高] width: 1224 height: 958
```

**关键发现**：

1. napi 有时正确传入 **1154**（6 行物理高度）
2. 但 **renderToCanvas 始终为 958**（5 行高度）
3. 说明 1154 被某处**覆盖**回 958

---

## 三、根因分析

### 3.1 两条并行路径

宽高有两条来源，都会调用 `surfaceSizeChanged`：

```
┌──────────────────────────────────────────────────────────────────────────┐
│ 路径 A（ArkTS / NAPI）                                                     │
│   RN 布局更新 → viewWidth/viewHeight 变化 → XComponent onSizeChange       │
│   → onSurfaceSizeChanged(xComponentId, nativeId, width, height)          │
│   → napi SurfaceSizeChanged → surfaceSizeChanged(instance->m_width,       │
│     instance->m_height)                                                   │
│   使用：instance->m_width/m_height（由 native 更新）                       │
├──────────────────────────────────────────────────────────────────────────┤
│ 路径 B（Native XComponent）                                                │
│   XComponent 系统检测到 Surface 尺寸变化                                   │
│   → OnSurfaceChangedCB → OnSurfaceChanged(component, window)             │
│   → OH_NativeXComponent_GetXComponentSize(&width, &height)                 │
│   → surfaceSizeChanged(width, height)   ← 原逻辑会调用                     │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.2 时序问题

当用户从 5 行切换到 6 行时：

| 时间线 | 路径 A（NAPI） | 路径 B（Native） | 结果 |
|--------|----------------|------------------|------|
| T1 | RN 布局更新为 6 行 | - | - |
| T2 | ArkTS onSizeChange 触发 | - | - |
| T3 | napi SurfaceSizeChanged 执行 | - | - |
| T4 | instance->m_height 可能已为 1154（若 native 先跑过）| - | - |
| T5 | surfaceSizeChanged(1224, 1154) 被调用 | - | ✅ 正确 |
| T6 | - | OnSurfaceChanged 被系统回调 | - |
| T7 | - | GetXComponentSize 可能仍返回 **958**（系统滞后） | - |
| T8 | - | surfaceSizeChanged(1224, **958**) 被调用 | ❌ **覆盖** 1154 |

**结论**：Native 的 OnSurfaceChanged 在部分场景下**滞后**于布局，仍拿到 5 行高度 958，若继续调用 surfaceSizeChanged(958)，会覆盖 napi 已正确传入的 1154。

### 3.3 为何 renderToCanvas 一直是 958

- `surfaceSizeChanged` 会调用 `_surfaceHolder->resize(width, height)`
- 后调用者会覆盖先调用者的结果
- 当 native 在 napi 之后调用 surfaceSizeChanged(958) 时，holder 被 resize 回 958
- 后续 `renderToCanvas` 使用 holder 的 getWidth/getHeight，因此始终为 958

---

## 四、解决方案

### 4.1 修复思路

**让 napi 成为 surfaceSizeChanged 的唯一调用方**，native 只负责更新 `m_width/m_height`，不再调用 surfaceSizeChanged。

### 4.2 具体修改

**文件**：`plugin_render.cpp`  
**位置**：`PluginRender::OnSurfaceChanged`

**修改前**：
```cpp
OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
if (render != nullptr) {
    render->m_width = width;
    render->m_height = height;
    render->_harmonyView->surfaceSizeChanged(width, height);  // 会覆盖 napi 的 1154
}
```

**修改后**：
```cpp
OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
DLOG(INFO) << "OnSurfaceChanged [宽高] width: " << width << " height: " << height;
if (render != nullptr) {
    render->m_width = width;
    render->m_height = height;
    // 不在此调用 surfaceSizeChanged，仅由 napi 驱动，避免 native 滞后报 958 覆盖 napi 已传的 1154
}
```

### 4.3 修改后的数据流

```
Native OnSurfaceChanged
    → 仅更新 render->m_width, render->m_height
    → 不调用 surfaceSizeChanged

NAPI SurfaceSizeChanged
    → 使用 instance->m_width, instance->m_height 调用 surfaceSizeChanged
    → instance->m_width/m_height 由 native 更新，napi 不覆盖
    → 当 native 已拿到 1154 时，napi 会传入 1154
    → 当 native 仍为 958 时，napi 传入 958（5 行场景正确）
```

**关键点**：napi 的调用时机在 ArkTS onSizeChange 之后，此时 native 的 OnSurfaceChanged 通常已执行过至少一次，`instance->m_width/m_height` 会逐步更新到正确值。由于 native 不再调用 surfaceSizeChanged，napi 传入的值不会被覆盖。

---

## 五、验证与影响

### 5.1 验证结果

- 5 行 ↔ 6 行切换正常
- renderToCanvas 在 6 行时显示 1224×1154
- 第六行完整显示

### 5.2 影响范围

- **仅修改**：`plugin_render.cpp` 中 OnSurfaceChanged 一处
- **不影响**：OnSurfaceCreated、napi、RNSkOpenGLCanvasProvider 等逻辑
- **初始尺寸**：仍由 napi RegisterView → surfaceAvailable(1,1)，再由首次 SurfaceSizeChanged 传入真实尺寸完成

---

## 六、经验总结

### 6.1 设计教训

1. **单一职责**：surfaceSizeChanged 应由**单一来源**驱动，避免多路径竞态
2. **时序敏感**：布局（ArkTS）与系统回调（Native）的时序不确定，不能假设谁先谁后
3. **后写覆盖**：后调用的 surfaceSizeChanged 会覆盖先调用的结果，需要明确“以谁为准”

### 6.2 排查方法

1. **加日志**：在尺寸传递的关键节点打日志（napi 传入、native 传入、实际绘制）
2. **对比数值**：对比 napi、native、renderToCanvas 三处的宽高，定位覆盖点
3. **理解时序**：结合两条路径的触发时机，分析谁可能覆盖谁

### 6.3 适用场景

本修复适用于：**ArkTS 布局驱动 + Native XComponent 系统回调** 并存，且存在时序差异导致错误覆盖的场景。若未来架构调整（如仅用 native 或仅用 napi 驱动），需重新评估调用关系。

---

## 七、相关文档

- [Skia 接口调用顺序与宽高传递](./Skia_接口调用顺序与宽高传递.md)：接口说明与宽高传递路径
