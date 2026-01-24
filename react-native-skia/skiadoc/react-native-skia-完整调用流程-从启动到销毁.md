# react-native-skia 完整调用流程 - 从启动到销毁

本文档详细描述 `@react-native-ohos/react-native-skia` 在 HarmonyOS 平台上从启动到销毁的完整调用流程，包括每个接口的调用顺序、参数、线程和执行时机。

---

## 📋 目录

1. [模块初始化阶段](#1-模块初始化阶段)
2. [组件创建阶段](#2-组件创建阶段)
3. [Surface 创建与注册阶段](#3-surface-创建与注册阶段)
4. [渲染循环阶段](#4-渲染循环阶段)
5. [尺寸变化处理阶段](#5-尺寸变化处理阶段)
6. [组件销毁阶段](#6-组件销毁阶段)
7. [线程模型](#7-线程模型)
8. [关键数据结构](#8-关键数据结构)

---

## 1. 模块初始化阶段

### 1.1 NAPI 模块注册

**时机：** 应用启动时，系统加载 `librnoh_skia.so`

**调用链：**
```
系统加载动态库
  ↓
RegisterModule() (napi_init.cpp:52)
  ↓
napi_module_register(&rn_skiaModule)
  ↓
Init() (napi_init.cpp:18)
```

**关键代码：**
```cpp
// napi_init.cpp
extern "C" __attribute__((constructor)) void RegisterModule(void) {
    napi_module_register(&rn_skiaModule);
}

static napi_value Init(napi_env env, napi_value exports) {
    // 导出 NAPI 函数
    napi_property_descriptor desc[] = {
        {"registerView", nullptr, PluginRender::RegisterView, ...},
        {"unregisterView", nullptr, PluginRender::DropInstance, ...},
        {"setModeAndDebug", nullptr, PluginRender::SetModeAndDebug, ...},
        {"onSurfaceSizeChanged", nullptr, PluginRender::SurfaceSizeChanged, ...},
        {"TagGetView_s", nullptr, SkiaManager::TagGetView, ...},
    };
    napi_define_properties(env, exports, ...);
    
    // 注册 XComponent 回调
    PluginManager::GetInstance()->Export(env, exports);
}
```

**输出：**
- NAPI 模块 `rnoh_skia` 已注册
- 导出函数已可用
- `PluginManager` 单例已创建

---

## 2. 组件创建阶段

### 2.1 React Native 组件渲染

**时机：** React 组件树中包含 `<Canvas>` 组件

**调用链：**
```
React 渲染 <Canvas> 组件
  ↓
RNOH 创建 RNCSkiaDomView.ets
  ↓
RNCSkiaDomView 创建 SkiaDomView.ets
  ↓
SkiaDomView.aboutToAppear() (SkiaDomView.ets:25)
```

**关键代码：**
```typescript
// SkiaDomView.ets
aboutToAppear(): void {
    this.xComponentId = X_COMPONENT_ID + '_' + this.nativeID;
    // 获取显示信息
    display.getAllDisplays(...);
}
```

### 2.2 XComponent 创建

**时机：** `SkiaDomView.build()` 执行

**调用链：**
```
SkiaDomView.build()
  ↓
XComponent({id, type: SURFACE, libraryname: "rnoh_skia"})
  ↓
系统创建 XComponent 实例
```

**关键代码：**
```typescript
// SkiaDomView.ets:38
XComponent({
    id: X_COMPONENT_ID + "_" + this.nativeID,
    type: XComponentType.SURFACE,
    libraryname: "rnoh_skia",
    controller: this.xComponentController
})
```

**输出：**
- XComponent 实例已创建
- 等待 `onLoad` 回调

---

## 3. Surface 创建与注册阶段

### 3.1 XComponent.onLoad 回调

**时机：** XComponent 加载完成

**调用链：**
```
XComponent.onLoad() (SkiaDomView.ets:44)
  ↓
xComponentContext.registerView(xComponentId, nativeID)
  ↓
NAPI: PluginRender::RegisterView() (plugin_render.h:45)
```

**关键代码：**
```typescript
// SkiaDomView.ets:44
.onLoad((xComponentContext) => {
    this.xComponentContext = xComponentContext as XComponentContext;
    this.xComponentContext?.registerView(X_COMPONENT_ID + "_" + this.nativeID, this.nativeID);
    this.xComponentContext?.setModeAndDebug(this.xComponentId, this.mode, this.debug);
})
```

### 3.2 PluginManager.Export 注册回调

**时机：** NAPI Init 时或首次访问 XComponent

**调用链：**
```
PluginManager::Export() (plugin_manager.cpp:32)
  ↓
napi_unwrap() 获取 OH_NativeXComponent*
  ↓
PluginRender::GetInstance(id) (plugin_render.cpp:32)
  ↓
创建 PluginRender 实例
  ↓
PluginRender::RegisterCallback() (plugin_render.cpp:288)
```

**关键代码：**
```cpp
// plugin_render.cpp:288
void PluginRender::RegisterCallback(OH_NativeXComponent *nativeXComponent) {
    m_renderCallback.OnSurfaceCreated = OnSurfaceCreatedCB;
    m_renderCallback.OnSurfaceChanged = OnSurfaceChangedCB;
    m_renderCallback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    m_renderCallback.DispatchTouchEvent = DispatchTouchEventCB;
    OH_NativeXComponent_RegisterCallback(nativeXComponent, &m_renderCallback);
    
    m_mouseCallback.DispatchMouseEvent = DispatchMouseEventCB;
    m_mouseCallback.DispatchHoverEvent = DispatchHoverEventCB;
    OH_NativeXComponent_RegisterMouseEventCallback(nativeXComponent, &m_mouseCallback);
    
    OH_NativeXComponent_RegisterFocusEventCallback(nativeXComponent, OnFocusEventCB);
    OH_NativeXComponent_RegisterKeyEventCallback(nativeXComponent, OnKeyEventCB);
    OH_NativeXComponent_RegisterBlurEventCallback(nativeXComponent, OnBlurEventCB);
}
```

**输出：**
- XComponent 回调已注册
- `PluginRender` 实例已创建

### 3.3 OnSurfaceCreated 回调

**时机：** 系统创建 Surface 后

**调用链：**
```
系统创建 Surface
  ↓
OnSurfaceCreatedCB() (plugin_render.cpp:43)
  ↓
OH_NativeXComponent_GetXComponentId()
  ↓
OH_NativeXComponent_GetXComponentSize()
  ↓
PluginRender::GetInstance(id)
  ↓
保存 window, width, height 到 PluginRender
```

**关键代码：**
```cpp
// plugin_render.cpp:43
void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window) {
    // 获取 XComponent ID
    OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    std::string id(idStr);
    
    // 获取 PluginRender 实例
    auto render = PluginRender::GetInstance(id);
    
    // 获取尺寸
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    
    // 保存 window 和尺寸
    render->m_window = static_cast<OHNativeWindow *>(window);
    render->m_width = width;
    render->m_height = height;
}
```

**输出：**
- `OHNativeWindow*` 已获取
- 尺寸信息已保存

### 3.4 RegisterView NAPI 调用

**时机：** ArkTS `onLoad` 时调用

**调用链：**
```
PluginRender::RegisterView() (plugin_render.h:45)
  ↓
解析参数: xComponentId, nativeId
  ↓
_context->runOnMainThread() (plugin_render.h:70)
  ↓
获取 RNSkView
  ↓
SkiaManager::registerSkiaView(nativeId, rNSkView)
  ↓
view->surfaceAvailable(window, 1, 1) (plugin_render.h:76)
```

**关键代码：**
```cpp
// plugin_render.h:45
static napi_value RegisterView(napi_env env, napi_callback_info info) {
    // 解析参数
    NFuncArg funcArg(env, info);
    auto [v1Succ, xComponentId, nLength] = nValXcId.ToUTF8String();
    auto [v2Succ, nativeId] = nValNativeId.ToInt32();
    
    // 切换到主线程执行
    instance->_context->runOnMainThread([instance, nativeId, id]() {
        auto view = instance->_harmonyView;
        std::shared_ptr<RNSkView> rNSkView = view->getSkiaView();
        
        // 注册 Skia View
        SkiaManager::getInstance().getManager()->registerSkiaView(nId, rNSkView);
        
        // 通知 Surface 可用（如果 window 已存在）
        view->surfaceAvailable(instance->m_window, 1, 1);
    });
}
```

**输出：**
- `nativeID` 与 `RNSkView` 已绑定
- Surface 已 attach（如果 window 已存在）

### 3.5 surfaceAvailable 调用

**时机：** `RegisterView` 中或 `OnSurfaceCreated` 后

**调用链：**
```
RNSkHarmonyView::surfaceAvailable() (RNSkHarmonyView.h:45)
  ↓
RNSkOpenGLCanvasProvider::surfaceAvailable() (RNSkOpenGLCanvasProvider.h:400)
  ↓
SkiaOpenGLSurfaceFactory::makeWindowedSurface() (RNSkOpenGLCanvasProvider.h:351)
  ↓
创建 WindowSurfaceHolder
  ↓
RNSkView::renderImmediate() (RNSkHarmonyView.h:52)
```

**关键代码：**
```cpp
// RNSkHarmonyView.h:45
void surfaceAvailable(OHNativeWindow *surface, int width, int height) override {
    std::static_pointer_cast<RNSkOpenGLCanvasProvider>(T::getCanvasProvider())
        ->surfaceAvailable(surface, width, height);
    
    // 立即渲染，不等待 draw loop
    RNSkView::renderImmediate();
}

// RNSkOpenGLCanvasProvider.h:400
void surfaceAvailable(OHNativeWindow *surface, int width, int height) {
    _surfaceHolder = SkiaOpenGLSurfaceFactory::makeWindowedSurface(surface, width, height);
    _requestRedraw();
}
```

**输出：**
- `WindowSurfaceHolder` 已创建
- 触发首次渲染

---

## 4. 渲染循环阶段

### 4.1 首次渲染 - getSurface

**时机：** `renderToCanvas` 首次调用时

**调用链：**
```
RNSkView::renderImmediate()
  ↓
RNSkOpenGLCanvasProvider::renderToCanvas()
  ↓
WindowSurfaceHolder::getSurface() (RNSkOpenGLCanvasProvider.h:83)
  ↓
SkiaOpenGLHelper::createSkiaDirectContextIfNecessary()
  ↓
SkiaOpenGLHelper::createWindowedSurface()
  ↓
SkiaOpenGLHelper::makeCurrent()
  ↓
GrBackendRenderTargets::MakeGL()
  ↓
SkSurfaces::WrapBackendRenderTarget()
  ↓
canvas->resetMatrix() (修复：重置变换矩阵)
```

**关键代码：**
```cpp
// RNSkOpenGLCanvasProvider.h:83
sk_sp<SkSurface> getSurface() {
    if (_skSurface == nullptr) {
        // 1. 创建/确保 Skia DirectContext
        SkiaOpenGLHelper::createSkiaDirectContextIfNecessary(
            &ThreadContextHarmonyHolder::ThreadSkiaOpenGLContext);
        
        // 2. 创建 EGLSurface
        _glSurface = SkiaOpenGLHelper::createWindowedSurface(_window);
        SkiaOpenGLHelper::makeCurrent(..., _glSurface);
        
        // 3. 创建 GrBackendRenderTarget
        GrBackendRenderTarget renderTarget = GrBackendRenderTargets::MakeGL(
            _width, _height, samples, stencil, fboInfo);
        
        // 4. Wrap 成 SkSurface
        _skSurface = SkSurfaces::WrapBackendRenderTarget(
            directContext, renderTarget,
            kBottomLeft_GrSurfaceOrigin, colorType, nullptr, &props, ...);
        
        // 🔧 修复：重置 Canvas 变换矩阵
        if (_skSurface) {
            SkCanvas* canvas = _skSurface->getCanvas();
            if (canvas) {
                canvas->resetMatrix();
                canvas->clipRect(SkRect::MakeWH(_width, _height));
            }
        }
    }
    return _skSurface;
}
```

**输出：**
- `EGLSurface` 已创建
- `SkSurface` 已创建
- Canvas 矩阵已重置

### 4.2 renderToCanvas 执行绘制

**时机：** 每次渲染循环

**调用链：**
```
HarmonyPlatformContext::notifyDrawLoop()
  ↓
RNSkView::draw()
  ↓
RNSkOpenGLCanvasProvider::renderToCanvas(cb)
  ↓
WindowSurfaceHolder::getSurface()
  ↓
WindowSurfaceHolder::makeCurrent()
  ↓
WindowSurfaceHolder::updateTexImage()
  ↓
canvas->save()
  ↓
canvas->resetMatrix() (修复：重置变换矩阵)
  ↓
canvas->clipRect() (修复：重置裁剪区域)
  ↓
cb(canvas) (执行 JS 层绘制回调)
  ↓
canvas->restore()
  ↓
WindowSurfaceHolder::present()
```

**关键代码：**
```cpp
// RNSkOpenGLCanvasProvider.h:371
bool renderToCanvas(const std::function<void(SkCanvas *)> &cb) {
    if (_surfaceHolder != nullptr && cb != nullptr) {
        auto surface = _surfaceHolder->getSurface();
        if (surface) {
            // 确保当前线程绑定到 EGLSurface
            if (!_surfaceHolder->makeCurrent()) {
                return false;
            }
            
            _surfaceHolder->updateTexImage();
            
            // 🔧 修复：重置 Canvas 状态
            SkCanvas* canvas = surface->getCanvas();
            if (canvas) {
                canvas->save();
                canvas->resetMatrix();  // 重置变换矩阵
                canvas->clipRect(SkRect::MakeWH(width, height));  // 重置裁剪区域
                
                // 执行绘制回调
                cb(canvas);
                
                canvas->restore();
            }
            
            // 提交并交换缓冲区
            return _surfaceHolder->present();
        }
    }
    return false;
}
```

### 4.3 present 提交渲染

**时机：** 绘制完成后

**调用链：**
```
WindowSurfaceHolder::present() (RNSkOpenGLCanvasProvider.h:226)
  ↓
directContext->flushAndSubmit()
  ↓
SkiaOpenGLHelper::swapBuffers()
```

**关键代码：**
```cpp
// RNSkOpenGLCanvasProvider.h:226
bool present() {
    // 刷新并提交 GPU 命令
    ThreadContextHarmonyHolder::ThreadSkiaOpenGLContext.directContext->flushAndSubmit();
    
    // 交换前后缓冲区
    return SkiaOpenGLHelper::swapBuffers(&ThreadContextHarmonyHolder::ThreadSkiaOpenGLContext, _glSurface);
}
```

**输出：**
- GPU 命令已提交
- 缓冲区已交换
- 帧已上屏

### 4.4 持续渲染循环

**时机：** `PlayLink` 定时触发

**调用链：**
```
HarmonyPlatformContext 构造函数
  ↓
创建 PlayLink 实例
  ↓
PlayLink::startDrawLoop()
  ↓
定时触发回调
  ↓
HarmonyPlatformContext::notifyDrawLoop()
  ↓
_context->runOnMainThread([this](){ notifyDrawLoop(false); })
  ↓
RNSkView::draw()
  ↓
重复 4.2 和 4.3
```

**关键代码：**
```cpp
// HarmonyPlatformContext.cpp:39
HarmonyPlatformContext::HarmonyPlatformContext(...)
    : playLink(std::make_unique<PlayLink>([this](double deltaTime) {
        runOnMainThread([this](){
            notifyDrawLoop(false);
        });
    })) {
    mainThread = std::thread(&HarmonyPlatformContext::runTaskOnMainThread, this);
}

void HarmonyPlatformContext::startDrawLoop() {
    if (playLink) {
        playLink->startDrawLoop();
    }
}
```

---

## 5. 尺寸变化处理阶段

### 5.1 onSizeChange 回调

**时机：** XComponent 尺寸变化

**调用链：**
```
XComponent.onSizeChange() (SkiaDomView.ets:50)
  ↓
xComponentContext.onSurfaceSizeChanged(xComponentId, nativeID, width, height)
  ↓
NAPI: PluginRender::SurfaceSizeChanged() (plugin_render.h:172)
```

**关键代码：**
```typescript
// SkiaDomView.ets:50
.onSizeChange((oldValue: SizeOptions, newValue: SizeOptions) => {
    let width = newValue.width?.valueOf() as number;
    let height = newValue.height?.valueOf() as number;
    this.xComponentContext?.onSurfaceSizeChanged(
        X_COMPONENT_ID + "_" + this.nativeID, 
        this.nativeID, 
        width, 
        height
    );
})
```

### 5.2 OnSurfaceChanged 系统回调

**时机：** 系统检测到 Surface 尺寸变化

**调用链：**
```
系统检测 Surface 尺寸变化
  ↓
OnSurfaceChangedCB() (plugin_render.cpp:77)
  ↓
PluginRender::OnSurfaceChanged() (plugin_render.cpp:235)
  ↓
OH_NativeXComponent_GetXComponentSize()
  ↓
harmonyView->surfaceSizeChanged(width, height)
```

**关键代码：**
```cpp
// plugin_render.cpp:77
void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window) {
    OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        render->OnSurfaceChanged(component, window);
    }
}

// plugin_render.cpp:235
void PluginRender::OnSurfaceChanged(OH_NativeXComponent *component, void *window) {
    uint64_t width, height;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    if (render != nullptr) {
        render->_harmonyView->surfaceSizeChanged(width, height);
    }
}
```

### 5.3 surfaceSizeChanged 处理

**时机：** 收到尺寸变化通知

**调用链：**
```
RNSkHarmonyView::surfaceSizeChanged() (RNSkHarmonyView.h:60)
  ↓
RNSkOpenGLCanvasProvider::surfaceSizeChanged() (RNSkOpenGLCanvasProvider.h:445)
  ↓
WindowSurfaceHolder::resize() (RNSkOpenGLCanvasProvider.h:216)
  ↓
_skSurface = nullptr (标记需要重建)
  ↓
RNSkView::renderImmediate() (RNSkHarmonyView.h:64)
```

**关键代码：**
```cpp
// RNSkHarmonyView.h:60
void surfaceSizeChanged(int width, int height) override {
    std::static_pointer_cast<RNSkOpenGLCanvasProvider>(T::getCanvasProvider())
        ->surfaceSizeChanged(width, height);
    RNSkView::renderImmediate();
}

// RNSkOpenGLCanvasProvider.h:445
void surfaceSizeChanged(int width, int height) {
    if (width == 0 && height == 0) {
        return;
    }
    _surfaceHolder->resize(width, height);
    
    // 🔧 修复：高度增加时，强制重置 Canvas 状态
    if (_surfaceHolder) {
        auto surface = _surfaceHolder->getSurface();
        if (surface) {
            SkCanvas* canvas = surface->getCanvas();
            if (canvas) {
                canvas->resetMatrix();
                canvas->clipRect(SkRect::MakeWH(width, height));
            }
        }
    }
    
    _requestRedraw();
}

// RNSkOpenGLCanvasProvider.h:216
void resize(int width, int height) {
    _width = width;
    _height = height;
    _skSurface = nullptr;  // 标记需要重建
}
```

**输出：**
- Surface 尺寸已更新
- `_skSurface` 已标记为无效
- 下次渲染时会重建 Surface

---

## 6. 组件销毁阶段

### 6.1 aboutToDisappear 回调

**时机：** ArkTS 组件即将销毁

**调用链：**
```
SkiaDomView.aboutToDisappear() (SkiaDomView.ets:32)
  ↓
xComponentContext.unregisterView(xComponentId, nativeID)
  ↓
NAPI: PluginRender::DropInstance() (plugin_render.h:84)
```

**关键代码：**
```typescript
// SkiaDomView.ets:32
aboutToDisappear(): void {
    this.xComponentContext?.unregisterView(
        X_COMPONENT_ID + "_" + this.nativeID, 
        this.nativeID
    );
}
```

### 6.2 DropInstance 处理

**时机：** `unregisterView` NAPI 调用

**调用链：**
```
PluginRender::DropInstance() (plugin_render.h:84)
  ↓
解析参数: xComponentId, nativeId
  ↓
SkiaManager::setSkiaView(nId, nullptr)
  ↓
SkiaManager::unregisterSkiaView(nId)
  ↓
harmonyView->viewDidUnmount()
```

**关键代码：**
```cpp
// plugin_render.h:84
static napi_value DropInstance(napi_env env, napi_callback_info info) {
    // 解析参数
    NFuncArg funcArg(env, info);
    auto [v1Succ, xComponentId, nLength] = nValXcId.ToUTF8String();
    auto [v2Succ, nativeId] = nValNativeId.ToInt32();
    
    std::string id(xComponentId.get());
    if (m_instance.find(id) != m_instance.end()) {
        auto instance = m_instance[id];
        size_t nId = static_cast<size_t>(nativeId);
        
        // 注销 Skia View
        SkiaManager::getInstance().getManager()->setSkiaView(nId, nullptr);
        SkiaManager::getInstance().getManager()->unregisterSkiaView(nId);
        
        // 通知 View 卸载
        instance->_harmonyView->viewDidUnmount();
    }
}
```

### 6.3 viewDidUnmount 处理

**时机：** `DropInstance` 中调用

**调用链：**
```
RNSkHarmonyView::viewDidUnmount() (RNSkHarmonyView.h:79)
  ↓
RNSkView::endDrawingLoop()
  ↓
HarmonyPlatformContext::stopDrawLoop()
  ↓
PlayLink::stopDrawLoop()
```

**关键代码：**
```cpp
// RNSkHarmonyView.h:79
void viewDidUnmount() override {
    T::endDrawingLoop();
}

// HarmonyPlatformContext.cpp:66
void HarmonyPlatformContext::stopDrawLoop() {
    if (drawLoopActive) {
        drawLoopActive = false;
    }
    if (playLink) {
        playLink->stopDrawLoop();
    }
}
```

### 6.4 OnSurfaceDestroyed 系统回调

**时机：** 系统销毁 Surface

**调用链：**
```
系统销毁 Surface
  ↓
OnSurfaceDestroyedCB() (plugin_render.cpp:99)
  ↓
SkiaManager::setReleaseVideo(true)
  ↓
harmonyView->surfaceDestroyed()
  ↓
PluginRender::Release(id)
```

**关键代码：**
```cpp
// plugin_render.cpp:99
void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window) {
    OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        SkiaManager::getInstance().setReleaseVideo(true);
        render->_harmonyView->surfaceDestroyed();
        PluginRender::Release(id);
    }
}
```

### 6.5 surfaceDestroyed 处理

**时机：** `OnSurfaceDestroyed` 回调

**调用链：**
```
RNSkHarmonyView::surfaceDestroyed() (RNSkHarmonyView.h:56)
  ↓
RNSkOpenGLCanvasProvider::surfaceDestroyed() (RNSkOpenGLCanvasProvider.h:427)
  ↓
WindowSurfaceHolder::dispose() (RNSkOpenGLCanvasProvider.h:55)
  ↓
_skSurface.reset()
  ↓
OH_NativeWindow_DestroyNativeWindow()
```

**关键代码：**
```cpp
// RNSkHarmonyView.h:56
void surfaceDestroyed() override {
    std::static_pointer_cast<RNSkOpenGLCanvasProvider>(T::getCanvasProvider())
        ->surfaceDestroyed();
}

// RNSkOpenGLCanvasProvider.h:427
void surfaceDestroyed() {
    auto holder = std::move(_surfaceHolder);
    if(!holder) return;
    auto sharedHolder = std::shared_ptr<WindowSurfaceHolder>(holder.release());
    _platformContext->runOnMainThread([sharedHolder](){
        sharedHolder->dispose();
    });
}

// RNSkOpenGLCanvasProvider.h:55
void dispose() {
    if(_skSurface) {
        _skSurface.reset();
        _glSurface = EGL_NO_SURFACE;
    }
    if(_window) {
        OH_NativeWindow_DestroyNativeWindow(_window);
        _window = nullptr;
    }
}
```

### 6.6 PluginRender::Release 清理

**时机：** `OnSurfaceDestroyed` 中调用

**调用链：**
```
PluginRender::Release(id) (plugin_render.cpp:127)
  ↓
PluginRender::GetInstance(id)
  ↓
m_instance.erase(id)
```

**关键代码：**
```cpp
// plugin_render.cpp:127
void PluginRender::Release(std::string &id) {
    auto render = PluginRender::GetInstance(id);
    if (render != nullptr) {
        m_instance.erase(m_instance.find(id));
    }
}
```

---

## 7. 线程模型

### 7.1 线程列表

| 线程 | 职责 | 关键操作 |
|------|------|----------|
| **JS 线程** | React 渲染、JSI 调用 | 组件更新、绘制指令生成 |
| **ArkUI UI 线程** | ArkTS 组件生命周期 | `onLoad`、`onSizeChange`、`aboutToDisappear` |
| **NAPI 回调线程** | XComponent 系统回调 | `OnSurfaceCreated`、`OnSurfaceChanged`、`OnSurfaceDestroyed` |
| **Skia 平台线程** | HarmonyPlatformContext 任务队列 | `runOnMainThread`、`notifyDrawLoop` |
| **PlayLink 线程** | 定时触发渲染循环 | `startDrawLoop`、`stopDrawLoop` |
| **GPU 驱动线程** | OpenGL 命令执行 | `flushAndSubmit`、`swapBuffers` |

### 7.2 线程切换点

1. **ArkTS → C++**: NAPI 调用（同步）
2. **系统回调 → C++**: XComponent 回调（可能在系统线程）
3. **C++ → Skia 平台线程**: `runOnMainThread()`（异步）
4. **Skia 平台线程 → 渲染**: `notifyDrawLoop()`（同步）
5. **渲染 → GPU**: `flushAndSubmit()`（异步，由驱动执行）

---

## 8. 关键数据结构

### 8.1 PluginRender

```cpp
class PluginRender {
    OHNativeWindow *m_window;           // Native Window 句柄
    uint64_t m_width;                   // Surface 宽度
    uint64_t m_height;                  // Surface 高度
    std::shared_ptr<RNSkBaseHarmonyView> _harmonyView;  // Harmony View 实例
    std::shared_ptr<RNSkPlatformContext> _context;      // 平台上下文
};
```

### 8.2 WindowSurfaceHolder

```cpp
class WindowSurfaceHolder {
    OHNativeWindow *_window;            // Native Window
    sk_sp<SkSurface> _skSurface;        // Skia Surface
    EGLSurface _glSurface;               // EGL Surface
    int _width;                         // 宽度
    int _height;                        // 高度
    float _widthPercent;                // 宽高比
};
```

### 8.3 RNSkOpenGLCanvasProvider

```cpp
class RNSkOpenGLCanvasProvider {
    std::unique_ptr<WindowSurfaceHolder> _surfaceHolder;  // Surface 持有者
    std::shared_ptr<RNSkPlatformContext> _platformContext;  // 平台上下文
};
```

---

## 9. 关键修复点

### 9.1 Canvas 居中问题修复

**问题：** 从 5 行切到 6 行时，Canvas 内容被垂直居中

**修复位置：**

1. **getSurface() 中** (RNSkOpenGLCanvasProvider.h:157-163)
   ```cpp
   if (_skSurface) {
       SkCanvas* canvas = _skSurface->getCanvas();
       if (canvas) {
           canvas->resetMatrix();
           canvas->clipRect(SkRect::MakeWH(_width, _height));
       }
   }
   ```

2. **renderToCanvas() 中** (RNSkOpenGLCanvasProvider.h:393-402)
   ```cpp
   SkCanvas* canvas = surface->getCanvas();
   if (canvas) {
       canvas->save();
       canvas->resetMatrix();
       canvas->clipRect(SkRect::MakeWH(width, height));
       cb(canvas);
       canvas->restore();
   }
   ```

3. **surfaceSizeChanged() 中** (RNSkOpenGLCanvasProvider.h:456-465)
   ```cpp
   if (_surfaceHolder) {
       auto surface = _surfaceHolder->getSurface();
       if (surface) {
           SkCanvas* canvas = surface->getCanvas();
           if (canvas) {
               canvas->resetMatrix();
               canvas->clipRect(SkRect::MakeWH(width, height));
           }
       }
   }
   ```

**修复原理：**
- `resetMatrix()` 清除可能影响坐标的变换矩阵
- `clipRect()` 重置裁剪区域为整个 Canvas
- 确保每次渲染时坐标从 (0,0) 开始，避免居中问题

---

## 10. 总结

### 10.1 完整生命周期流程

```
1. 模块初始化
   └─ NAPI 模块注册 → 导出函数 → 注册回调

2. 组件创建
   └─ React 渲染 → ArkTS 创建 XComponent → aboutToAppear

3. Surface 创建
   └─ onLoad → registerView → OnSurfaceCreated → surfaceAvailable

4. 首次渲染
   └─ getSurface → 创建 EGLSurface/SkSurface → renderToCanvas → present

5. 持续渲染
   └─ PlayLink 定时触发 → renderToCanvas → present

6. 尺寸变化
   └─ onSizeChange → OnSurfaceChanged → surfaceSizeChanged → resize → 重建 Surface

7. 组件销毁
   └─ aboutToDisappear → unregisterView → OnSurfaceDestroyed → surfaceDestroyed → dispose
```

### 10.2 关键时序点

- **Surface 创建时机**: `OnSurfaceCreated` 可能在 `registerView` 之前或之后
- **首次渲染时机**: `surfaceAvailable` 后立即调用 `renderImmediate()`
- **尺寸变化处理**: `resize()` 立即失效 `_skSurface`，下次渲染时重建
- **销毁顺序**: `viewDidUnmount` → `surfaceDestroyed` → `dispose`

---

## 附录：文件索引

### ArkTS 层
- `src/main/ets/view/SkiaDomView.ets` - XComponent 包装组件
- `src/main/ets/RNCSkiaDomView.ets` - RN 组件包装层

### C++ 层
- `src/main/cpp/rnskia/napi_init.cpp` - NAPI 模块入口
- `src/main/cpp/rnskia/plugin_manager.cpp` - 插件管理器
- `src/main/cpp/rnskia/plugin_render.cpp` - 渲染插件实现
- `src/main/cpp/rnskia/RNSkHarmonyView.h` - Harmony View 包装
- `src/main/cpp/rnskia/RNSkOpenGLCanvasProvider.h` - Canvas Provider
- `src/main/cpp/rnskia/HarmonyPlatformContext.cpp` - 平台上下文

---

**文档版本**: 1.0  
**最后更新**: 2025-01-XX  
**维护者**: AI Assistant
