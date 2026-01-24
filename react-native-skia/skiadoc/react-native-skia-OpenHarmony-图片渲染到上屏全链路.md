## 背景与范围

本文描述 **OpenHarmony（鸿蒙）+ RNOH** 环境下，`@react-native-oh-tpl/react-native-skia` 中 **“一张图片从渲染到最终上屏”** 的完整链路。

目标是让你能：

- 从 **JS/React 层** 一路追到 **C++ Skia 绘制**、**EGL swapBuffers**，再到 **系统合成上屏**
- 在每个阶段明确：**输入是什么、输出是什么、在哪个线程、关键代码在哪个文件**
- 方便你在“折叠屏/首帧延迟/不出帧”等问题里快速定位瓶颈点

> 说明：本文基于本仓库路径：
>
> `example/harmony/entry/oh_modules/@react-native-oh-tpl/react-native-skia`

---

## 一句话总览（图片到上屏的最短路径）

**React/JS 更新图片节点 → RNOH 把 `nativeID/布局尺寸` 下发到 ArkTS → ArkTS 用 `XComponent(type: SURFACE)` 承载 `librnoh_skia.so` → C++ 通过 `OH_NativeXComponent` 拿到 `OHNativeWindow` → `RNSkOpenGLCanvasProvider` 为该 window 创建 `EGLSurface` 并 wrap 成 Skia 的 `SkSurface` → Skia 在 `SkCanvas` 上 drawImage → `flushAndSubmit()` → `eglSwapBuffers()` → 系统合成 → 上屏**

---

## 参与者与文件索引（从上到下）

### ArkTS（ArkUI / RNOH）

- **承载 Skia Surface 的 ArkUI 组件**
  - `src/main/ets/view/SkiaDomView.ets`
    - 创建 `XComponent(SURFACE)`，`libraryname: "rnoh_skia"`
    - `onLoad()` 时调用 NAPI：`registerView(...)`
    - `onSizeChange()` 时调用 NAPI：`onSurfaceSizeChanged(...)`
- **RNOH 的 RN 组件包装层（把 RN descriptor 转成 ArkTS props）**
  - `src/main/ets/RNCSkiaDomView.ets`

### C++（NAPI + Skia + EGL/OpenGL）

- **NAPI 模块入口（把 ArkTS 调用映射到 C++ 实现）**
  - `src/main/cpp/rnskia/napi_init.cpp`
- **XComponent 回调注册与分发**
  - `src/main/cpp/rnskia/plugin_manager.cpp`
  - `src/main/cpp/rnskia/plugin_render.cpp`
  - `src/main/cpp/rnskia/plugin_render.h`
- **Skia 视图与 CanvasProvider（真正把 Skia 画到 window 并 swap 上屏）**
  - `src/main/cpp/rnskia/RNSkHarmonyView.h`
  - `src/main/cpp/rnskia/RNSkOpenGLCanvasProvider.h`
  - `src/main/cpp/rnskia/egl_core.*` / `HarmonyOpenGLHelper.*`（EGL 初始化、createSurface、swapBuffers 等）
- **帧驱动/平台上下文（绘制循环、任务队列、截图/资源加载等）**
  - `src/main/cpp/rnskia/HarmonyPlatformContext.cpp`
  - `src/main/cpp/rnskia/HarmonyPlayLink.cpp`

---

## 线程模型（理解“哪里会卡住”的基础）

在这份实现里，至少会涉及到这些执行域：

- **JS 线程**：React 计算 props / 生成 Skia 节点（JSI 侧），更新触发
- **ArkUI UI 线程**：ArkTS 组件生命周期（`aboutToAppear/onLoad/onSizeChange` 等）
- **NAPI/Native 回调线程**：`OH_NativeXComponent` 的 surface 回调可能来自系统线程（实现相关）
- **Skia/平台线程（该实现自建）**：`HarmonyPlatformContext` 内部维护一个任务队列线程（`runTaskOnMainThread`），并用 `PlayLink` 线程定时触发 draw loop
- **GPU 驱动线程**：OpenGL 命令的真正执行由驱动/内核完成（你无法直接控制，但能通过 flush/swap 的耗时判断压力）

> 重要：这里的 `HarmonyPlatformContext::runOnMainThread()` 语义更像“Skia 平台线程/事件线程”，并不等同于 ArkUI 的 UI 线程。

---

## 图片从“声明”到“上屏”的时序（一步一步）

下面按时间顺序描述：**一个包含图片的 Skia 画面**是如何最终显示到屏幕上的。

为了让链路更清晰，我们把过程分为 8 个阶段：

- 阶段 A：RN/JS 侧决定要画哪张图
- 阶段 B：ArkTS 侧创建 `XComponent(SURFACE)` 容器
- 阶段 C：NAPI 模块加载与 XComponent 回调注册
- 阶段 D：系统创建 Surface → C++ 拿到 `OHNativeWindow`
- 阶段 E：ArkTS `registerView()` → 绑定 `nativeID ↔ RNSkView` 并 attach surface
- 阶段 F：创建 EGLSurface / 创建 SkSurface
- 阶段 G：执行 Skia drawImage（真正“画”）
- 阶段 H：flush + swap → 合成 → 上屏

---

## 阶段 A：RN/JS 侧决定要画哪张图（“内容来源”）

你在 RN 中使用 `react-native-skia` 时，图片通常来自：

- `useImage(...)`/`Skia.Image.MakeImageFromEncoded(...)`（不同版本 API 略有差异）
- 或者通过 `Canvas` 节点树声明 `<Image image={...} x={...} y={...} ... />`

这里的核心点不是 API 细节，而是：**JS 侧最终会形成一个“Skia 场景（scene graph）”或“绘制指令集合”，并通过 JSI 直接交给 C++**。

> 说明：JSI binder 的细节在这个 oh_module 里也能看到（例如 `SkiaViewJSIBinder.h`、`RNSkiaModule.cpp`），但本文聚焦“图片如何变成屏幕上的像素”，所以把 JSI 视为“内容输入通道”。

输出：

- C++ 侧 Skia view 在下一帧渲染时能拿到“要绘制的图片对象/纹理句柄/解码数据”等

---

## 阶段 B：ArkTS 创建承载容器 `XComponent(SURFACE)`

当 RN 视图树包含 Skia 组件时，RNOH 的 descriptor 会下发给 ArkTS 侧包装组件 `RNCSkiaDomView.ets`。

`RNCSkiaDomView.ets` 的责任是：

- 从 `descriptor.rawProps` 取出 `nativeID`（Skia view 的唯一标识）
- 从 `descriptor.layoutMetrics` 取出 `viewWidth/viewHeight`
- 把这三个值传给真正的 `SkiaDomView.ets`

关键点：真正承载 native surface 的是 `SkiaDomView.ets` 里的 `XComponent`：

- `type: XComponentType.SURFACE`
- `libraryname: "rnoh_skia"`
- `id: "SkiaDomView_<nativeID>"`

这三项决定了：

- 系统会为这个 `XComponent` 创建一个可被 native 绘制的 Surface
- 并把 native 侧实现绑定到 `librnoh_skia.so`（NAPI 模块 `rnoh_skia`）

输出：

- ArkUI 侧已经出现一个“可被 native 绘制的洞（surface）”
- 后续会触发 `onLoad` / `onSizeChange` / native surface callbacks

---

## 阶段 C：NAPI 模块加载与回调注册（把 ArkTS 与 C++ 接起来）

NAPI 模块名是 `rnoh_skia`（与 ArkTS `libraryname: "rnoh_skia"` 对应）。

在 `rnskia/napi_init.cpp` 中导出了 ArkTS 会调用的函数：

- `registerView(xComponentId, nativeId)`
- `onSurfaceSizeChanged(xComponentId, nativeId, width, height)`
- `unregisterView(...)`
- 以及一些辅助能力（`setModeAndDebug`、`TagGetView_s`）

同时在 `Init()` 中调用 `PluginManager::Export(env, exports)`：

这一步会从 `exports` 中 unwrap 出 `OH_NativeXComponent*`，并注册 surface/touch 等回调：

- `OnSurfaceCreated`
- `OnSurfaceChanged`
- `OnSurfaceDestroyed`

输出：

- C++ 侧能收到 “surface created/changed/destroyed” 的系统回调
- ArkTS 侧 `xComponentContext.registerView()` 等调用也能落到 C++ 实现

---

## 阶段 D：系统创建 Surface → C++ 拿到 `OHNativeWindow`（“上屏目标”出现）

当 `XComponent(SURFACE)` 的 surface 真正可用时，系统会回调到 C++：

- `plugin_render.cpp` 的 `OnSurfaceCreatedCB(...)`

此时 C++ 会：

- 通过 `OH_NativeXComponent_GetXComponentId(...)` 得到 `idStr`（例如 `SkiaDomView_123`）
- 通过 `OH_NativeXComponent_GetXComponentSize(...)` 得到 `width/height`
- 拿到 `window` 并转为 `OHNativeWindow*`
- 缓存在 `PluginRender` 实例的 `m_window/m_width/m_height`

输出：

- **有了 `OHNativeWindow*`**：这就是后续 EGL/Skia 最终要渲染并 swap 的目标

常见坑：

- 如果 `OnSurfaceCreated` 先到，但 `registerView` 还没来，native 侧暂时还不会 attach surface（要等阶段 E）
- 如果尺寸一开始是 0/1（例如首帧布局未完成），创建 EGLSurface/SkSurface 会导致首帧异常或延迟（你这份代码里通过 `w>=2 && h>=2` 在避免 1×1 首帧）

---

## 阶段 E：ArkTS `registerView()` → 绑定 `nativeID ↔ RNSkView` 并 attach surface

当 ArkTS 的 `XComponent.onLoad(...)` 触发后，它会调用：

- `xComponentContext.registerView("SkiaDomView_<nativeID>", nativeID)`

这会进入 C++ 的 `PluginRender::RegisterView(...)`（在 `plugin_render.h` 内实现）。

这一步做了两件非常关键的事：

### E1. 绑定 nativeID 与 Skia View

- 通过 `_harmonyView->getSkiaView()` 拿到 `std::shared_ptr<RNSkView>`
- 调用 `SkiaManager::getInstance().getManager()->registerSkiaView(nativeId, rNSkView)`

直观理解：

- **nativeID 是 RN 侧用来定位“这块 Skia 画布”的 key**
- **RNSkView 是 C++ 侧真正承载场景并渲染的对象**

绑定完成后，JSI/manager 侧才能通过 tag 找到对应 view，推动它渲染内容。

### E2. attach surface（把 window 交给 CanvasProvider）

如果此时 `m_window` 已经存在且尺寸有效，并且还没 attach 过：

- 调用 `view->surfaceAvailable(m_window, w, h)`
- 然后调用一次 `view->surfaceSizeChanged(w, h)` 强制触发 redraw

输出：

- `RNSkOpenGLCanvasProvider` 内部会创建 `WindowSurfaceHolder`
- 之后每次渲染都会走 `renderToCanvas(cb)` 并最终 swapBuffers 上屏

常见坑：

- `OnSurfaceCreated` 和 `registerView` 的先后顺序不稳定，可能导致第一次 attach 延迟
- `onSizeChange` 触发频繁时会反复 resize 导致 surface 重建/SkSurface 失效（阶段 F 会解释）

---

## 阶段 F：创建 EGLSurface / 创建 SkSurface（把“window”变成 Skia 可画的画布）

一旦 `surfaceAvailable(window,w,h)` 调到 `RNSkOpenGLCanvasProvider`：

- 它会构建 `WindowSurfaceHolder(window,w,h)`
- 之后首次渲染时 `WindowSurfaceHolder::getSurface()` 会完成真正的初始化：

### F1. 创建/确保 Skia DirectContext（OpenGL 后端）

- `SkiaOpenGLHelper::createSkiaDirectContextIfNecessary(...)`

这一步会：

- 初始化 EGL display/context（或复用）
- 创建 Skia 的 `GrDirectContext`（Skia GPU 上下文）

### F2. 用 `OHNativeWindow` 创建 EGLWindowSurface

在 `getSurface()` 中调用：

- `SkiaOpenGLHelper::createWindowedSurface(_window)`
- 得到 `_glSurface`

并调用：

- `SkiaOpenGLHelper::makeCurrent(..., _glSurface)`

### F3. wrap 成 Skia 的 `SkSurface`

这份实现使用 `GrBackendRenderTargets::MakeGL(...)` 创建 `GrBackendRenderTarget`，然后：

- `SkSurfaces::WrapBackendRenderTarget(...)` 得到 `_skSurface`

输出：

- 之后渲染阶段拿到的是：`SkSurface -> SkCanvas`
- **所有 drawImage 都是在这个 canvas 上执行**，最终 flush+swap 输出到 `_glSurface` 对应的 window

常见坑（延迟大户）：

- **首次创建 EGL/GrDirectContext 很重**：首帧卡顿经常就在这里
- `makeCurrent` 失败通常是 window/surface 生命周期或线程上下文问题

---

## 阶段 G：执行 Skia drawImage（真正把图片画进当前帧）

渲染发生在 `RNSkOpenGLCanvasProvider::renderToCanvas(cb)`：

1. `surface = _surfaceHolder->getSurface()` 获取 `SkSurface`
2. `_surfaceHolder->makeCurrent()` 让当前线程绑定到该 EGLSurface
3. `cb(surface->getCanvas())`：这里 cb 由 `RNSkView`/Skia manager 提供，内部会把场景中的图片绘制出来

把 “画图片” 抽象成最核心的 Skia 动作就是：

- `canvas->drawImage(...)` 或 `drawImageRect(...)`
- 背后可能触发：
  - 图片解码（如果还没解码）
  - GPU 纹理上传（如果还没上传）
  - 采样/缩放/滤镜等 GPU 操作

输出：

- 当前帧的像素结果已经写入 GPU render target（但还没“上屏”）

常见坑：

- **首帧图片首次解码/首次上传纹理** 会卡（尤其大图、网络图、首次字体/atlas 也类似）
- `drawImage` 伴随 blur/shadow/复杂 clip 可能显著增加 GPU cost

---

## 阶段 H：flush + swap → 系统合成 → 上屏（真正“到屏幕”）

当 cb 绘制完成后，`renderToCanvas` 调用 `_surfaceHolder->present()`。

`present()` 做两件事：

1. `directContext->flushAndSubmit()`
   - 把 Skia 录制的 GPU 命令 flush 到驱动队列并提交
2. `swapBuffers(display, glSurface)`
   - 交换前后缓冲
   - 让系统合成器在合适时机拿到最新帧内容进行合成并显示

这一步完成后，图片就完成了“上屏”。

常见坑：

- `flushAndSubmit` 慢：GPU 压力大或发生同步（例如纹理上传、readback、pipeline stall）
- `swapBuffers` 慢：可能被 vsync 节奏/合成器节流/缓冲队列卡住

---

## 尺寸变化与折叠屏（为什么经常导致“延迟上屏/卡首帧”）

折叠屏展开/折叠过程中会出现：

- view 宽高快速变化（短时间内多次 `onSizeChange`）
- surface 重建/重新 attach（系统侧可能触发 destroy/create）
- 像素密度/viewport 变化（需要重新配置 render target）

在本实现里，尺寸变化链路是：

1. ArkTS `SkiaDomView.ets` 的 `onSizeChange(...)` 调用 NAPI：`onSurfaceSizeChanged(...)`
2. C++ `PluginRender::SurfaceSizeChanged` 计算 `wpx/hpx`（乘以 pixelDensity），并：
   - 必要时补一次 `surfaceAvailable(window,wpx,hpx)`（首次 attach）
   - 调 `surfaceSizeChanged(wpx,hpx)` → `WindowSurfaceHolder::resize()` → `_skSurface = nullptr`（下次渲染重建 surface）

折叠屏典型延迟点：

- 多次 resize 让 `_skSurface` 反复失效 → 下次渲染不断走“重建 EGLSurface/WrapBackendRenderTarget”
- 若折叠瞬间出现 (w,h)=0 或非常小，可能触发失败/跳帧；等恢复到有效尺寸才重新渲染，表现为“延迟上屏”

---

## “图片到上屏”问题排查建议（按阶段打点）

如果你的目标是定位“为什么图片渲染了但晚才上屏”，建议按阶段加日志/耗时统计：

- **阶段 D（surface created）**
  - 记录 `OnSurfaceCreated` 的时间戳、拿到的 `width/height`
- **阶段 E（registerView/attach）**
  - 记录 `registerView` 到达时间、是否当场 attach（`m_window != nullptr && w>=2 && h>=2`）
- **阶段 F（getSurface 首次创建）**
  - 统计 `createSkiaDirectContextIfNecessary` 耗时
  - 统计 `createWindowedSurface`、`makeCurrent` 耗时
  - 统计 `WrapBackendRenderTarget` 耗时
- **阶段 G（draw）**
  - 统计 cb 绘制耗时（特别是首帧/首张图）
- **阶段 H（flush/swap）**
  - 统计 `flushAndSubmit` 与 `swapBuffers` 耗时（哪个慢哪个就是主要瓶颈方向）

---

## 结语

这份 `react-native-skia` 鸿蒙实现里，“上屏”的最硬核关键点只有两个：

- **把 `OHNativeWindow` 变成 `EGLSurface` + `SkSurface`**
- **每帧 `flushAndSubmit()` 后 `swapBuffers()` 把帧交给系统合成**

图片是否“及时上屏”，本质就是看：

- surface attach 时序是否顺畅（OnSurfaceCreated vs registerView vs sizeChange）
- 首次 EGL/SkSurface 创建是否过重
- 图片解码/纹理上传是否压住首帧
- flush/swap 是否被 GPU 或合成器节流












