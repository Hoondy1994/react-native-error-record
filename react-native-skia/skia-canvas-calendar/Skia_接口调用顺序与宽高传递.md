# React Native Skia (HarmonyOS) 接口调用顺序与宽高传递说明

本文档基于日历 5↔6 行切换问题的排查经验，详细说明 HarmonyOS 上 React Native Skia 的接口调用顺序、宽高传递路径及各接口功能。

---

## 一、整体架构与调用链路

```
React Native (layoutMetrics)
        ↓
RNCSkiaDomView (viewWidth, viewHeight)
        ↓
SkiaDomView (XComponent + onSizeChange)
        ↓
┌─────────────────────────────────────────────────────────────────┐
│  两条并行路径（宽高来源）                                          │
├─────────────────────────────────────────────────────────────────┤
│  【路径 A】ArkTS onSizeChange  →  napi SurfaceSizeChanged  →  surfaceSizeChanged  │
│  【路径 B】Native XComponent   →  OnSurfaceChanged         →  (仅更新 m_width/m_height) │
└─────────────────────────────────────────────────────────────────┘
        ↓
RNSkOpenGLCanvasProvider (surfaceAvailable / surfaceSizeChanged)
        ↓
WindowSurfaceHolder (resize / getSurface)
        ↓
renderToCanvas (实际绘制)
```

---

## 二、接口调用顺序（典型生命周期）

### 1. 初始化阶段

| 顺序 | 接口/回调 | 触发方 | 宽高来源 | 说明 |
|------|-----------|--------|----------|------|
| 1 | XComponent `onLoad` | ArkTS | - | XComponent 加载完成 |
| 2 | `registerView` (napi) | ArkTS | - | 注册视图，建立 id 映射 |
| 3 | `OnSurfaceCreatedCB` (native) | XComponent 系统 | `OH_NativeXComponent_GetXComponentSize` | Native 层 Surface 创建，拿到物理尺寸 |
| 4 | `PluginRender::m_width/m_height` 赋值 | plugin_render.cpp | GetXComponentSize 返回值 | 存储到 instance，供后续 napi 使用 |
| 5 | `surfaceAvailable(window, 1, 1)` | napi RegisterView | **写死 1, 1** | 创建 WindowSurfaceHolder，初始尺寸为 1×1 |
| 6 | XComponent `onSizeChange` | ArkTS 布局 | `newValue.width/height`（逻辑尺寸，如 vp） | 布局完成，首次尺寸回调 |
| 7 | `SurfaceSizeChanged` (napi) | ArkTS | 入参 width/height + `instance->m_width/m_height` | **实际调用 surfaceSizeChanged 时使用 instance->m_width/m_height**（物理 px） |
| 8 | `surfaceSizeChanged(w, h)` | RNSkOpenGLCanvasProvider | napi 传入 | 调用 `_surfaceHolder->resize(w, h)`，下一帧 getSurface 时按新尺寸创建 SkSurface |
| 9 | `renderToCanvas` | Skia 渲染循环 | `_surfaceHolder->getWidth()/getHeight()` | 实际绘制，使用当前 holder 的宽高 |

### 2. 尺寸变更阶段（如 5 行 → 6 行）

| 顺序 | 接口/回调 | 触发方 | 宽高 | 说明 |
|------|-----------|--------|------|------|
| 1 | RN layout 更新 | React Native | 逻辑尺寸变化 | 如 5 行 283 → 6 行 341 |
| 2 | `viewWidth/viewHeight` 更新 | RNCSkiaDomView | `layoutMetrics.frame.size` | 从 descriptor 取新布局 |
| 3 | XComponent `onSizeChange` | ArkTS | newValue（逻辑） | 布局变化触发 |
| 4 | `SurfaceSizeChanged` (napi) | ArkTS | 入参 + instance | **使用 instance->m_width/m_height** 调用 surfaceSizeChanged |
| 5 | `OnSurfaceChangedCB` (native) | XComponent 系统 | GetXComponentSize | **仅更新 m_width/m_height，不调用 surfaceSizeChanged**（修复后） |
| 6 | `renderToCanvas` | 渲染循环 | holder 当前尺寸 | 使用上次 surfaceSizeChanged 传入的尺寸 |

---

## 三、各接口功能详解

### 1. ArkTS 层

#### `SkiaDomView.ets`

- **viewWidth / viewHeight**：来自父组件 RNCSkiaDomView，对应 React Native 的 `layoutMetrics.frame.size`，单位为逻辑像素（vp）。
- **XComponent**：`libraryname: "rnoh_skia"`，绑定 native 插件。
- **onLoad**：XComponent 加载时调用 `registerView(xComponentId, nativeId)`。
- **onSizeChange**：XComponent 尺寸变化时调用 `onSurfaceSizeChanged(xComponentId, nativeId, width, height)`，其中 width/height 来自 `newValue`，通常为逻辑尺寸。

#### `RNCSkiaDomView.ets`

- **viewWidth / viewHeight**：`this.descriptor.layoutMetrics.frame.size.width/height`，来自 React Native 布局结果。
- **updatePropFromDesc**：descriptor 变化时更新 viewWidth/viewHeight，驱动 SkiaDomView 和 XComponent 的 `.width(this.viewWidth).height(this.viewHeight)`。

---

### 2. NAPI 层（plugin_render.h）

#### `RegisterView(env, info)`

- **功能**：注册 Skia 视图，建立 xComponentId ↔ PluginRender 的映射。
- **宽高**：调用 `view->surfaceAvailable(instance->m_window, 1, 1)`，**固定传入 1×1**。
- **说明**：此时 native 的 OnSurfaceCreated 可能尚未执行，真实尺寸来自后续 SurfaceSizeChanged。

#### `SurfaceSizeChanged(env, info)`

- **功能**：响应 ArkTS 的 onSurfaceSizeChanged，在 main 线程调用 `view->surfaceSizeChanged(instance->m_width, instance->m_height)`。
- **入参**：`(xComponentId, nativeId, width, height)`，其中 width/height 为 ArkTS 传入的**逻辑尺寸**。
- **实际使用**：**不**用入参 width/height 换算，而是用 `instance->m_width`、`instance->m_height`（由 native OnSurfaceChanged/OnSurfaceCreated 通过 GetXComponentSize 更新）作为传给 surfaceSizeChanged 的**物理尺寸**。
- **说明**：这是**唯一驱动 surfaceSizeChanged 的路径**（修复后 native 不再调用）。

---

### 3. Native 层（plugin_render.cpp）

#### `OnSurfaceCreatedCB(component, window)`

- **功能**：XComponent 的 Surface 创建完成时由系统回调。
- **宽高**：`OH_NativeXComponent_GetXComponentSize(component, window, &width, &height)`，得到**物理像素**。
- **动作**：`render->m_width = width; render->m_height = height; render->m_window = nativeWindow;`
- **说明**：不直接调用 surfaceSizeChanged，尺寸由 napi RegisterView 触发的 surfaceAvailable(1,1) 和后续 SurfaceSizeChanged 配合完成。

#### `OnSurfaceChanged(component, window)`（PluginRender 成员）

- **功能**：XComponent 尺寸变化时由系统回调。
- **宽高**：`OH_NativeXComponent_GetXComponentSize(component, window, &width, &height)`，得到当前**物理尺寸**。
- **动作**：`render->m_width = width; render->m_height = height;`，**不调用 surfaceSizeChanged**。
- **修复说明**：原先会调用 `render->_harmonyView->surfaceSizeChanged(width, height)`。当布局从 5 行切到 6 行时，napi 先传入 1154，但 native 可能滞后仍报 958，若再调用 surfaceSizeChanged(958) 会覆盖 1154。因此改为仅更新 m_width/m_height，由 napi 作为唯一调用方。

#### `OnSurfaceDestroyedCB`

- **功能**：Surface 销毁时调用 `surfaceDestroyed()`，释放资源。

---

### 4. RNSkOpenGLCanvasProvider（RNSkOpenGLCanvasProvider.h）

#### `surfaceAvailable(surface, width, height)`

- **功能**：创建 WindowSurfaceHolder，建立 EGL 与 Skia 的绑定。
- **宽高**：调用方传入，通常来自 RegisterView 的 (1, 1)。
- **动作**：`_surfaceHolder = SkiaOpenGLSurfaceFactory::makeWindowedSurface(surface, width, height)`，然后 `_requestRedraw()`。

#### `surfaceSizeChanged(width, height)`

- **功能**：通知 holder 尺寸变化，下次 getSurface 时按新尺寸创建 SkSurface。
- **宽高**：由 napi SurfaceSizeChanged 传入的 `instance->m_width/m_height`（物理 px）。
- **动作**：`_surfaceHolder->resize(width, height)`（置空 _skSurface），`_requestRedraw()`。
- **说明**：resize 后 holder 的 _width/_height 更新，下次 getSurface 时若 _skSurface 为 null 会按新尺寸创建。

#### `renderToCanvas(cb)`

- **功能**：执行实际绘制，将 Skia 绘制回调应用到 SkSurface。
- **宽高**：使用 `_surfaceHolder->getWidth()`、`_surfaceHolder->getHeight()`，即当前 holder 的尺寸。
- **流程**：getSurface → makeCurrent → updateTexImage → cb(surface->getCanvas()) → present。

---

### 5. WindowSurfaceHolder（RNSkOpenGLCanvasProvider.h 内）

#### `resize(width, height)`

- **功能**：更新 _width、_height，并置空 _skSurface，强制下次 getSurface 时重建。
- **说明**：避免在旧尺寸的 Surface 上继续绘制。

#### `getSurface()`

- **功能**：返回当前 SkSurface，若 _skSurface 为 null 则按 _width、_height 创建。
- **说明**：resize 后首次调用会按新尺寸创建。

---

## 四、宽高传递路径总结

| 阶段 | 宽高类型 | 来源 | 传递路径 |
|------|----------|------|----------|
| 布局 | 逻辑 (vp) | RN layoutMetrics | RNCSkiaDomView → SkiaDomView → XComponent |
| ArkTS onSizeChange | 逻辑 | newValue | SkiaDomView → onSurfaceSizeChanged |
| Native GetXComponentSize | 物理 (px) | XComponent 系统 | OnSurfaceCreated / OnSurfaceChanged |
| instance->m_width/m_height | 物理 | GetXComponentSize | 供 napi SurfaceSizeChanged 使用 |
| surfaceSizeChanged 入参 | 物理 | instance | napi → RNSkOpenGLCanvasProvider |
| renderToCanvas 使用 | 物理 | holder | _surfaceHolder->getWidth/Height |

---

## 五、时序问题与修复要点

**问题**：5 行 ↔ 6 行切换时，napi 已传入 1154，但 renderToCanvas 仍为 958。

**原因**：native OnSurfaceChanged 与 napi SurfaceSizeChanged 并行执行，native 可能滞后仍报 958，若再调用 surfaceSizeChanged(958) 会覆盖 napi 的 1154。

**修复**：native OnSurfaceChanged 仅更新 `m_width/m_height`，**不再调用 surfaceSizeChanged**，由 napi 作为唯一调用方。这样 napi 传入的 1154 不会被 native 的 958 覆盖。

---

## 六、相关文件

| 文件 | 职责 |
|------|------|
| `RNCSkiaDomView.ets` | 从 RN 取布局，提供 viewWidth/viewHeight |
| `SkiaDomView.ets` | XComponent 容器，onSizeChange 触发 onSurfaceSizeChanged |
| `XComponentContext.ts` | 接口定义，registerView / onSurfaceSizeChanged |
| `plugin_render.h` | napi RegisterView、SurfaceSizeChanged |
| `plugin_render.cpp` | OnSurfaceCreated、OnSurfaceChanged、RegisterCallback |
| `RNSkOpenGLCanvasProvider.h` | surfaceAvailable、surfaceSizeChanged、renderToCanvas、WindowSurfaceHolder |
