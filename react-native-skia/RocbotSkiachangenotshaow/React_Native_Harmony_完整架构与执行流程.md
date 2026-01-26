# React Native for OpenHarmony 完整架构与执行流程

## 目录
1. [整体架构概览](#整体架构概览)
2. [核心组件详解](#核心组件详解)
3. [完整执行流程](#完整执行流程)
4. [数据流向](#数据流向)
5. [线程模型](#线程模型)
6. [生命周期管理](#生命周期管理)

---

## 整体架构概览

### 分层架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                    应用层（ArkTS）                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │  RNAbility   │  │   Index.ets  │  │ BaseRN.ets   │         │
│  │  EntryAbility│  │  MultiHome   │  │ BasePrecreate│         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
│         │                  │                  │                  │
│         └──────────────────┼──────────────────┘                  │
│                            │                                     │
│                    ┌───────▼────────┐                          │
│                    │ RNOHCoreContext │                          │
│                    │  (全局上下文)    │                          │
│                    └───────┬─────────┘                          │
│                            │                                     │
│                    ┌───────▼────────┐                          │
│                    │   RNInstance   │                          │
│                    │  (运行实例)     │                          │
│                    └───────┬─────────┘                          │
│                            │                                     │
│                    ┌───────▼────────┐                          │
│                    │   RNSurface    │                          │
│                    │  (渲染容器)     │                          │
│                    └─────────────────┘                          │
└─────────────────────────────────────────────────────────────────┘
                            │
                            │ NAPI (Native API)
                            │ ArkTS ↔ C++
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    C++ 适配层                                    │
│  ┌──────────────────┐  ┌──────────────────┐                   │
│  │ RNInstanceInternal│  │ SchedulerDelegate│                   │
│  │  - JS 引擎管理    │  │  - 接收 Mutations│                   │
│  │  - JSI Runtime   │  │  - 调度渲染      │                   │
│  └──────────────────┘  └──────────────────┘                   │
│         │                       │                               │
│         └───────────┬────────────┘                               │
│                     │                                            │
│         ┌───────────▼───────────┐                                │
│         │  MountingManagerCAPI  │                                │
│         │  - 处理 Mutations     │                                │
│         │  - 创建/更新组件      │                                │
│         └───────────┬───────────┘                                │
│                     │                                            │
│         ┌───────────▼───────────┐                                │
│         │ ComponentInstance     │                                │
│         │  - View/Text/Image    │                                │
│         │  - 调用 ArkUI C-API   │                                │
│         └───────────┬───────────┘                                │
└─────────────────────────────────────────────────────────────────┘
                            │
                            │ C-API
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    React Common 层                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │  Scheduler   │  │ ShadowTree  │  │    Yoga      │         │
│  │  - 调度渲染   │  │  - 虚拟树    │  │  - 布局计算   │         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
│         │                  │                  │                  │
│         └──────────────────┼──────────────────┘                  │
│                            │                                     │
│                    ┌───────▼────────┐                          │
│                    │   Fabric       │                          │
│                    │  - 渲染系统    │                          │
│                    └───────┬─────────┘                          │
└─────────────────────────────────────────────────────────────────┘
                            │
                            │ JSI (JavaScript Interface)
                            │ JS ↔ C++
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    JS 层（React Native）                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │  React 组件  │  │ TurboModule  │  │   Bundle     │         │
│  │  - View/Text │  │  - 原生能力   │  │  - JS 代码    │         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
│         │                  │                  │                  │
│         └──────────────────┼──────────────────┘                  │
│                            │                                     │
│                    ┌───────▼────────┐                          │
│                    │  JS 引擎       │                          │
│                    │  (Hermes/JSVM) │                          │
│                    └─────────────────┘                          │
└─────────────────────────────────────────────────────────────────┘
                            │
                            │ 渲染指令
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ArkUI 原生层                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │  XComponent  │  │  ContentSlot  │  │  NodeContent  │         │
│  │  - 占位组件   │  │  - 容器组件    │  │  - C-API 组件 │         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
│                            │                                     │
│                    ┌───────▼────────┐                          │
│                    │  原生组件渲染   │                          │
│                    │  (系统级渲染)    │                          │
│                    └─────────────────┘                          │
└─────────────────────────────────────────────────────────────────┘
```

---

## 核心组件详解

### 1. 应用层组件（ArkTS）

#### 1.1 RNAbility / EntryAbility
**作用**: 应用入口，管理应用生命周期

**职责**:
- 创建 `RNInstancesCoordinator`
- 创建 `RNOHCoreContext`
- 管理窗口生命周期

**代码位置**: `b/c/d/e/entry/src/main/ets/entryability/EntryAbility.ets`

**关键方法**:
```typescript
onCreate(want: Want) {
  // 创建 RNInstancesCoordinator
  // 创建 RNOHCoreContext
  // 存储到 AppStorage
}
```

---

#### 1.2 RNOHCoreContext
**作用**: 全局核心上下文，管理所有 RNInstance

**职责**:
- 创建和销毁 RNInstance
- 管理全局配置（调试模式、UIAbilityContext 等）
- 提供开发工具控制器

**特点**:
- **全局唯一**: 整个应用只有一个
- **跨 Instance 共享**: 所有 RNInstance 共享

**关键方法**:
```typescript
createAndRegisterRNInstance(options: RNInstanceOptions): Promise<RNInstance>
destroyAndUnregisterRNInstance(instance: RNInstance): void
```

---

#### 1.3 RNInstance
**作用**: React Native 运行实例

**职责**:
- 管理 JS 引擎（Hermes/JSVM）
- 运行 JS Bundle
- 管理组件树和状态
- 提供 TurboModule 访问

**特点**:
- **独立环境**: 每个 RNInstance 有独立的 JS 线程和引擎
- **可创建多个**: 一个应用可以有多个 RNInstance

**关键方法**:
```typescript
runJSBundle(provider: JSBundleProvider): Promise<void>
getTurboModule<T>(name: string): T
subscribeToLifecycleEvents(event: string, callback: Function): void
```

**在代码中的使用**:
```typescript
// 创建 Instance
const cpInstance: RNInstance = await rnohCoreContext.createAndRegisterRNInstance({
  createRNPackages: createRNPackages,
  enableNDKTextMeasuring: true,
  enableBackgroundExecutor: false,
  enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
  arkTsComponentNames: arkTsComponentNames
});

// 加载 Bundle
await cpInstance.runJSBundle(
  new ResourceJSBundleProvider(
    getContext().resourceManager,
    'bundle/basic/basic.harmony.bundle'
  )
);
```

---

#### 1.4 RNOHContext
**作用**: 特定 RNInstance 的上下文

**职责**:
- 提供特定 RNInstance 相关的功能
- 包含对特定 RNInstance 的引用

**创建方式**:
```typescript
const rnohContext = RNOHContext.fromCoreContext(
  rnohCoreContext,
  rnInstance
);
```

---

#### 1.5 RNComponentContext
**作用**: 组件渲染上下文

**职责**:
- 连接 RNInstance 和 ArkTS UI 组件
- 提供组件构建器（buildCustomComponent、buildRNComponentForTag）

**创建方式**:
```typescript
const ctx = new RNComponentContext(
  RNOHContext.fromCoreContext(rnohCoreContext, instance),
  wrapBuilder(buildCustomComponent),
  wrapBuilder(buildRNComponentForTag),
  new Map()
);
```

---

#### 1.6 RNSurface
**作用**: React Native 渲染容器

**职责**:
- 承载 React Native 组件的渲染
- 管理 Surface 生命周期
- 处理用户交互事件

**使用示例**:
```typescript
RNSurface({
  rnInstance: this.instance,
  appKey: 'Details',
  initialProps: {
    stringParam: 'Hello'
  }
})
```

---

#### 1.7 BaseRN / BasePrecreateView
**作用**: 封装 RNSurface 的组件

**职责**:
- 简化 RNSurface 的使用
- 管理 Instance 生命周期
- 支持预创建优化

**BaseRN**: 使用共享 Instance
**BasePrecreateView**: 支持 Instance 预创建

---

### 2. C++ 适配层组件

#### 2.1 RNInstanceInternal
**作用**: RNInstance 的 C++ 实现

**职责**:
- 创建和管理 JS 引擎（Hermes/JSVM）
- 初始化 JSI Runtime
- 加载和执行 Bundle
- 注册 JSI 通道（__turboModuleProxy、nativeFabricUIManager）

**关键流程**:
```
创建 JS 引擎
  ↓
初始化 JSI Runtime
  ↓
注册 JSI 函数
  ↓
加载 Bundle
  ↓
执行 JS 代码
```

---

#### 2.2 SchedulerDelegate
**作用**: 接收 React 的渲染指令

**职责**:
- 接收 Fabric 传来的 Mutations
- 调度渲染任务
- 管理渲染队列

**关键方法**:
```cpp
void schedulerDidFinishTransaction(
  MountingCoordinator::Shared const& mountingCoordinator
) {
  // 接收 Mutations
  // 调度渲染
}
```

---

#### 2.3 MountingManagerCAPI
**作用**: 处理 Mutations，创建/更新 ArkUI 组件

**职责**:
- 解析 Mutations（Create/Update/Delete/Insert/Remove）
- 创建对应的 ComponentInstance
- 调用 ArkUI C-API 更新组件

**关键流程**:
```
接收 Mutations
  ↓
解析操作类型
  ↓
创建/更新 ComponentInstance
  ↓
调用 ArkUI C-API
```

---

#### 2.4 ComponentInstance
**作用**: React Native 组件的 C++ 实现

**类型**:
- `ViewComponentInstance`: View 组件
- `TextComponentInstance`: Text 组件
- `ImageComponentInstance`: Image 组件
- `ScrollViewComponentInstance`: ScrollView 组件
- `CustomNodeComponentInstance`: 自定义组件

**职责**:
- 管理组件的生命周期
- 处理 Props 更新
- 调用 ArkUI C-API 渲染

---

### 3. React Common 层组件

#### 3.1 Scheduler
**作用**: React 渲染调度器

**职责**:
- 调度 React 渲染任务
- 管理渲染优先级
- 协调 JS 线程和 UI 线程

---

#### 3.2 ShadowTree
**作用**: React 虚拟 DOM 树

**职责**:
- 存储组件的虚拟节点
- 执行 Diff 算法
- 生成 Mutations

**结构**:
```
ShadowRoot
  └── ShadowNode (View)
      ├── ShadowNode (Text)
      └── ShadowNode (Image)
```

---

#### 3.3 Yoga
**作用**: 布局引擎

**职责**:
- 计算组件布局
- 处理 Flexbox 布局
- 计算组件尺寸和位置

---

#### 3.4 Fabric
**作用**: React Native 的渲染系统

**职责**:
- 处理组件渲染
- 生成 Mutations
- 发送渲染指令到原生层

**关键流程**:
```
React 渲染
  ↓
创建 ShadowTree
  ↓
Yoga 布局计算
  ↓
生成 Mutations
  ↓
发送到 SchedulerDelegate
```

---

### 4. JS 层组件

#### 4.1 React 组件
**作用**: 业务逻辑和 UI 定义

**类型**:
- 函数组件
- 类组件
- Hooks

**示例**:
```typescript
function App() {
  return (
    <View>
      <Text>Hello</Text>
    </View>
  );
}
```

---

#### 4.2 TurboModule
**作用**: JS 访问原生能力的桥梁

**特点**:
- 同步调用（通过 JSI）
- 类型安全
- 高性能

**使用示例**:
```typescript
import { NativeModules } from 'react-native';
const { SampleTurboModule } = NativeModules;

SampleTurboModule.doSomething();
```

---

#### 4.3 Bundle
**作用**: 打包后的 JS 代码

**格式**: `.harmony.bundle`

**内容**:
- React 组件代码
- React Native 库代码
- 业务逻辑代码

---

### 5. 通信桥梁

#### 5.1 JSI (JavaScript Interface)
**作用**: JS 与 C++ 的通信桥梁

**特点**:
- **同步调用**: 无需异步等待
- **高性能**: 直接内存访问
- **类型安全**: 自动类型转换

**工作原理**:
```javascript
// JS 侧
TurboModule.someMethod();
  ↓
JSI Runtime
  ↓
// C++ 侧
jsi::Value someMethod(jsi::Runtime& rt, ...) {
  // 执行逻辑
}
```

---

#### 5.2 NAPI (Native API)
**作用**: ArkTS 与 C++ 的通信桥梁

**特点**:
- **类型转换**: ArkTS 类型 ↔ C++ 类型
- **异步支持**: 支持异步调用
- **双向通信**: ArkTS ↔ C++

**工作原理**:
```typescript
// ArkTS 侧
this.rnInstance.postMessageToCpp("MESSAGE", payload);
  ↓
NAPI Bridge
  ↓
// C++ 侧
void onMessageReceived(ArkTSMessage const& message) {
  // 处理消息
}
```

---

#### 5.3 C-API
**作用**: C++ 直接调用 ArkUI 原生接口

**特点**:
- **性能优化**: 绕过中间层
- **直接调用**: C++ → ArkUI

**使用示例**:
```cpp
// C++ 侧
NativeNodeApi::getInstance()->createNode(ARKUI_NODE_STACK);
NativeNodeApi::getInstance()->setNodeAttribute(nodeHandle, ...);
```

---

## 完整执行流程

### 1. 应用启动流程

```
┌─────────────────────────────────────────────────────────────┐
│ 1. 应用启动                                                  │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. EntryAbility.onCreate()                                  │
│    - 创建 RNInstancesCoordinator                            │
│    - 创建 RNOHCoreContext                                    │
│    - 存储到 AppStorage                                       │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. Index.aboutToAppear()                                    │
│    - 从 AppStorage 获取 RNOHCoreContext                      │
│    - 调用 loadMetroBundle()                                 │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. LoadManager.loadMetroBundle()                            │
│    - 尝试连接 Metro 服务器                                  │
│    - 如果可用，创建 metroInstance                           │
│    - 加载 Metro bundle                                       │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. Index.register()                                         │
│    - 如果 Metro 不可用，创建业务 Instance                    │
│    - 创建 cpInstance, bpInstance, bpInstance2               │
│    - 加载 basic bundle 到 cpInstance                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. 应用就绪                                                  │
│    - isBundleReady = true                                    │
│    - 显示 MultiHome                                          │
└─────────────────────────────────────────────────────────────┘
```

---

### 2. 页面渲染流程

```
┌─────────────────────────────────────────────────────────────┐
│ 1. 用户导航到页面                                            │
│    - 例如：点击按钮跳转到 Details 页面                       │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. Details.aboutToAppear()                                  │
│    - 选择对应的 RNInstance (bpInstance 或 bpInstance2)      │
│    - 准备 initialProps                                       │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. BasePrecreateView.init()                                 │
│    - 获取或创建 RNInstance                                   │
│    - 加载 bundle（如果未加载）                               │
│    - 设置 shouldShow = true                                  │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┘
│ 4. RNSurface 渲染                                            │
│    - 创建 XComponent                                         │
│    - 调用 C++ 的 startSurface                                │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. C++ 层创建 Surface                                        │
│    - RNInstanceInternal.startSurface()                       │
│    - 创建 RootView                                          │
│    - 注册到 XComponent                                      │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. JS 层执行                                                 │
│    - AppRegistry.runApplication('Details', initialProps)     │
│    - React 开始渲染                                          │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 7. React 渲染流程                                           │
│    - 创建 ShadowTree                                        │
│    - Yoga 布局计算                                          │
│    - 生成 Mutations                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 8. Fabric 发送 Mutations                                     │
│    - 通过 JSI 发送到 C++                                    │
│    - SchedulerDelegate 接收                                 │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 9. C++ 处理 Mutations                                        │
│    - MountingManagerCAPI 解析 Mutations                     │
│    - 创建/更新 ComponentInstance                            │
│    - 调用 ArkUI C-API                                       │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 10. ArkUI 渲染                                               │
│     - 创建原生组件                                           │
│     - 渲染到屏幕                                             │
└─────────────────────────────────────────────────────────────┘
```

---

### 3. 用户交互流程

```
┌─────────────────────────────────────────────────────────────┐
│ 1. 用户点击按钮                                              │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. ArkUI 捕获事件                                            │
│    - 通过 C-API 传递事件                                     │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. ComponentInstance 处理事件                                │
│    - 转换为 React Native 事件格式                            │
│    - 通过 JSI 发送到 JS                                      │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. JS 层处理事件                                             │
│    - React 事件系统处理                                       │
│    - 调用 onPress 回调                                       │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. 业务逻辑执行                                              │
│    - 更新状态                                                │
│    - 调用 TurboModule                                        │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. TurboModule 调用（如果需要）                              │
│    - 通过 JSI 调用 C++                                       │
│    - C++ 执行原生操作                                        │
│    - 通过 NAPI 通知 ArkTS                                    │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 7. UI 更新（如果状态改变）                                   │
│    - React 重新渲染                                          │
│    - 生成新的 Mutations                                      │
│    - 更新 UI                                                 │
└─────────────────────────────────────────────────────────────┘
```

---

## 数据流向

### 1. JS → C++ 数据流

```
JS 代码
  ↓
JSI Runtime
  ↓ (类型转换)
C++ 函数参数
  ↓
C++ 执行逻辑
```

**示例**:
```javascript
// JS
TurboModule.doSomething({ key: 'value' });
  ↓
// JSI
jsi::Value doSomething(jsi::Runtime& rt, jsi::Object& params) {
  // params 是 JS 对象，自动转换为 C++ 对象
}
```

---

### 2. C++ → JS 数据流

```
C++ 执行结果
  ↓
JSI Runtime
  ↓ (类型转换)
JS 返回值
```

**示例**:
```cpp
// C++
return jsi::String::createFromUtf8(rt, "result");
  ↓
// JS
const result = TurboModule.doSomething(); // "result"
```

---

### 3. ArkTS → C++ 数据流

```
ArkTS 调用
  ↓
NAPI Bridge
  ↓ (类型转换)
C++ 函数参数
  ↓
C++ 执行逻辑
```

**示例**:
```typescript
// ArkTS
this.rnInstance.postMessageToCpp("EVENT", { data: "value" });
  ↓
// NAPI
void onMessageReceived(ArkTSMessage const& message) {
  // message 包含 ArkTS 传递的数据
}
```

---

### 4. C++ → ArkTS 数据流

```
C++ 执行结果
  ↓
NAPI Bridge
  ↓ (类型转换)
ArkTS 回调/事件
```

**示例**:
```cpp
// C++
emitComponentEvent(tag, "onPress", payload);
  ↓
// ArkTS
// 通过事件系统接收
```

---

## 线程模型

### 线程架构

```
┌─────────────────────────────────────────────────────────────┐
│ MAIN 线程（ArkTS UI 线程）                                   │
│  - ArkTS 组件渲染                                            │
│  - 生命周期管理                                              │
│  - NAPI 调用                                                │
└─────────────────────────────────────────────────────────────┘
                    ↓ NAPI
┌─────────────────────────────────────────────────────────────┐
│ JS 线程（每个 RNInstance 一个）                              │
│  - JS 代码执行                                              │
│  - React 渲染                                               │
│  - JSI 调用                                                 │
└─────────────────────────────────────────────────────────────┘
                    ↓ JSI
┌─────────────────────────────────────────────────────────────┐
│ C++ 线程（WORKER 线程）                                      │
│  - TurboModule 执行                                         │
│  - 原生能力调用                                              │
└─────────────────────────────────────────────────────────────┘
                    ↓ C-API
┌─────────────────────────────────────────────────────────────┐
│ UI 渲染线程（系统线程）                                       │
│  - ArkUI 原生渲染                                            │
└─────────────────────────────────────────────────────────────┘
```

### 线程通信

1. **MAIN ↔ JS**: 通过 NAPI（异步）
2. **JS ↔ C++**: 通过 JSI（同步）
3. **C++ ↔ UI**: 通过 C-API（同步）

---

## 生命周期管理

### RNInstance 生命周期

```
创建
  ↓
初始化 JS 引擎
  ↓
加载 Bundle
  ↓
运行中
  ↓
销毁
```

### Surface 生命周期

```
创建 Surface
  ↓
startSurface
  ↓
渲染中
  ↓
stopSurface
  ↓
销毁 Surface
```

### 组件生命周期

```
创建组件 (Create)
  ↓
更新 Props (Update)
  ↓
插入到树 (Insert)
  ↓
渲染中
  ↓
从树移除 (Remove)
  ↓
删除组件 (Delete)
```

---

## 总结

### 架构特点

1. **分层清晰**: JS → JSI → C++ → NAPI → ArkTS → ArkUI
2. **跨语言通信**: JSI（JS↔C++）、NAPI（ArkTS↔C++）
3. **多线程**: MAIN、JS、WORKER、UI 渲染线程
4. **按需加载**: 多 Bundle 架构，支持按需加载
5. **性能优化**: C-API 直接调用，减少跨语言开销

### 核心组件

- **应用层**: RNAbility、RNOHCoreContext、RNInstance、RNSurface
- **C++ 层**: RNInstanceInternal、SchedulerDelegate、MountingManagerCAPI
- **React 层**: Scheduler、ShadowTree、Yoga、Fabric
- **通信层**: JSI、NAPI、C-API

### 执行流程

1. **启动**: EntryAbility → RNOHCoreContext → RNInstance → Bundle 加载
2. **渲染**: RNSurface → C++ Surface → JS 执行 → React 渲染 → Mutations → ArkUI
3. **交互**: ArkUI 事件 → C++ → JSI → JS → 业务逻辑 → UI 更新





