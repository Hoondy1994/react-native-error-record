# Index.ets 导入组件详解

本文档详细解释 `Index.ets` 中从 `@rnoh/react-native-openharmony` 导入的所有组件，包括它们在框架中的用法、职责和相互关系。

## 组件分类

### 📦 Bundle 相关

#### 1. `ResourceJSBundleProvider`
**作用**: 从资源文件中加载 JS Bundle

**用途**: 加载打包好的 `.harmony.bundle` 文件或 `.hbc` (Hermes Bytecode) 文件

**框架内部实现**:
- 继承自 `JSBundleProvider` 基类
- 支持从 `rawfile` 资源目录加载 bundle
- 支持内存映射（mmap）优化，当 RNOHCoreContext 可用时自动使用
- 如果 mmap 不可用，则使用 `getRawFileContent()` 加载到内存

**关键方法**:
```typescript
// 获取 Bundle 内容
async getBundle(
  onProgress?: (progress: number) => void,
  onProviderSwitch?: (currentProvider: JSBundleProvider) => void
): Promise<RawFileJSBundle | ArrayBuffer>

// 获取 Bundle 路径
getURL(): string

// 获取 App Keys（用于多 Bundle 场景）
getAppKeys(): string[]
```

**使用示例**:
```typescript
// 从 rawfile 资源中加载 bundle
const provider = new ResourceJSBundleProvider(
  getContext().resourceManager,
  'bundle/basic/basic.harmony.bundle'
);
await instance.runJSBundle(provider);

// 加载 Hermes Bytecode（推荐生产环境）
const hbcProvider = new ResourceJSBundleProvider(
  rnohCoreContext.uiAbilityContext.resourceManager,
  'hermes_bundle.hbc'
);
```

**在代码中的使用**:
```161:162:b/c/d/e/entry/src/main/ets/pages/Index.ets
    await cpInstance.runJSBundle(new ResourceJSBundleProvider(getContext()
      .resourceManager, 'bundle/basic/basic.harmony.bundle'));
```

**框架中的其他 Bundle Provider**:
- `FileJSBundleProvider`: 从文件系统加载
- `MetroJSBundleProvider`: 从 Metro 开发服务器加载
- `AnyJSBundleProvider`: 支持多个 Provider 的链式加载

---

### 🏗️ Context 相关（核心架构组件）

#### 2. `RNOHCoreContext`
**作用**: React Native 的**核心上下文**，管理所有 RNInstance 的共享资源

**职责**:
- 创建和销毁 RNInstance
- 管理全局配置（调试模式、UIAbilityContext 等）
- 提供开发工具控制器（DevTools）
- 管理错误事件发射器
- 管理显示指标（DisplayMetrics）
- 管理 UIAbility 状态（前台/后台）
- 提供安全区域信息（SafeAreaInsets）

**框架内部架构**:
- 在 `RNInstancesCoordinator` 中创建
- 通过 `AppStorage` 全局共享
- 包含 `rnInstanceRegistry` 用于管理所有 Instance
- 提供 `devToolsController` 用于开发工具集成

**关键属性和方法**:
```typescript
// 属性
reactNativeVersion: string
uiAbilityContext: common.UIAbilityContext
isDebugModeEnabled: boolean
devToolsController: DevToolsController
safeAreaInsetsProvider: SafeAreaInsetsProvider
launchUri?: string

// 方法
async createAndRegisterRNInstance(options: RNInstanceOptions): Promise<RNInstance>
async destroyAndUnregisterRNInstance(rnInstance: RNInstance): Promise<void>
reportRNOHError(rnohError: RNOHError): void
subscribeToRNOHErrors(listener: (err: RNOHError) => void): () => void
getDisplayMetrics(): DisplayMetrics
getUIAbilityState(): UIAbilityState
```

**特点**:
- **全局唯一**: 整个应用只有一个 RNOHCoreContext
- **跨 Instance 共享**: 所有 RNInstance 共享这个 Context
- **生命周期**: 在 EntryAbility 中创建，存储在 AppStorage
- **线程安全**: 标记为 `@thread: MAIN`，必须在主线程使用

**使用示例**:
```typescript
// 从 AppStorage 获取（在 EntryAbility 中已创建）
@StorageLink('RNOHCoreContext') rnohCoreContext: RNOHCoreContext | undefined = undefined;

// 创建 RNInstance
const instance = await rnohCoreContext.createAndRegisterRNInstance({
  createRNPackages: createRNPackages,
  enableNDKTextMeasuring: true,
  enableBackgroundExecutor: false,
  enableCAPIArchitecture: true,
  arkTsComponentNames: []
});

// 监听框架错误
rnohCoreContext.subscribeToRNOHErrors((error) => {
  console.error('RNOH Error:', error);
});
```

**在代码中的使用**:
```28:28:b/c/d/e/entry/src/main/ets/pages/Index.ets
  @StorageLink('RNOHCoreContext') rnohCoreContext: RNOHCoreContext | undefined = undefined;
```

---

#### 3. `RNOHContext`
**作用**: 特定 RNInstance 的上下文，继承自 RNOHCoreContext

**职责**:
- 提供特定 RNInstance 相关的功能
- 包含对特定 RNInstance 的引用
- 用于创建 RNComponentContext
- 提供 Instance 特定的工具方法

**框架内部架构**:
- 继承自 `RNOHCoreContext`，拥有所有 CoreContext 的功能
- 内部包含 `_rnohContextDeps`，其中包含 `rnInstance` 引用
- 通过静态方法 `fromCoreContext()` 创建，避免直接构造

**关键属性和方法**:
```typescript
// 属性（继承自 RNOHCoreContext）
rnInstance: RNInstance  // 关联的 RNInstance

// 方法（继承自 RNOHCoreContext）
// 所有 RNOHCoreContext 的方法都可用
```

**特点**:
- **每个 Instance 一个**: 每个 RNInstance 有对应的 RNOHContext
- **从 CoreContext 创建**: 通过 `fromCoreContext()` 静态方法创建
- **不可直接构造**: 必须使用静态工厂方法

**使用示例**:
```typescript
// 从 RNOHCoreContext 和 RNInstance 创建
const rnohContext = RNOHContext.fromCoreContext(
  rnohCoreContext,
  rnInstance
);

// 然后用于创建 RNComponentContext
const componentContext = new RNComponentContext(
  rnohContext,
  wrapBuilder(buildCustomComponent),
  wrapBuilder(buildRNComponentForTag),
  new Map()
);
```

**在代码中的使用**:
```125:130:b/c/d/e/entry/src/main/ets/pages/Index.ets
    const ctxCp: RNComponentContext = new RNComponentContext(
      RNOHContext.fromCoreContext(this.rnohCoreContext!, cpInstance),
      wrapBuilder(buildCustomComponent),
      wrapBuilder(buildRNComponentForTag),
      new Map()
    );
```

---

#### 4. `RNComponentContext`
**作用**: 组件渲染上下文，用于渲染 React Native 组件

**职责**:
- 连接 RNInstance 和 ArkTS UI 组件
- 提供组件构建器（buildCustomComponent、buildRNComponentForTag）
- 管理自定义组件的映射
- 管理组件数据源（ComponentDataSource）
- 管理组件内容（Content）与标签（Tag）的映射

**框架内部架构**:
- 继承自 `RNOHContext`
- 包含 `descriptorRegistry` 用于组件描述符管理
- 维护 `contentByTag` Map 用于存储组件内容
- 提供 `wrappedCustomRNComponentBuilder` 和 `wrappedRNComponentBuilder`

**关键属性和方法**:
```typescript
// 属性
wrappedCustomRNComponentBuilder: WrappedCustomRNComponentBuilder
customWrappedCustomRNComponentBuilder?: WrappedCustomRNComponentBuilder
wrappedRNChildrenBuilder: WrappedRNChildrenBuilder
wrappedRNComponentBuilder: WrappedRNComponentBuilder
rnInstance: RNInstance  // 继承自 RNOHContext

// 方法
createComponentDataSource(ctx: RNComponentDataSourceFactoryContext): RNComponentDataSource
getContentForTag(tag: Tag): Content | undefined
__setContentForTag(tag: Tag, content: Content): void  // @internal
__deleteContentForTag(tag: Tag): void  // @internal
runOnWorkerThread<TParams, TResult, TRunnable>(...): Promise<TResult>
```

**构造函数参数**:
```typescript
constructor(
  rnohContext: RNOHContext,  // 从 RNOHContext.fromCoreContext() 创建
  wrappedCustomRNComponentBuilder: WrappedCustomRNComponentBuilder,  // 自定义组件构建器
  wrappedRNComponentBuilder: WrappedRNComponentBuilder,  // RN 组件构建器
  rnComponentDataSourceFactoriesByDescriptorType: Map<string, RNComponentDataSourceFactory>,  // 数据源工厂映射
  customWrappedCustomRNComponentBuilder?: WrappedCustomRNComponentBuilder  // 可选的自定义构建器
)
```

**特点**:
- **每个 Instance 一个**: 每个 RNInstance 需要创建对应的 RNComponentContext
- **用于渲染**: 在 RNSurface 或自定义组件中使用
- **管理组件树**: 通过 Content 和 Tag 管理组件树结构

**使用示例**:
```typescript
// 创建 RNComponentContext
const ctx = new RNComponentContext(
  RNOHContext.fromCoreContext(rnohCoreContext, instance),
  wrapBuilder(buildCustomComponent),  // 自定义组件构建器
  wrapBuilder(buildRNComponentForTag), // RN 组件构建器
  new Map()  // 数据源工厂映射（通常为空 Map）
);

// 在 RNSurface 中使用
RNSurface({
  ctx: ctx,
  surfaceConfig: {
    appKey: "MyApp",
    initialProps: {}
  }
});
```

**在代码中的使用**:
```125:130:b/c/d/e/entry/src/main/ets/pages/Index.ets
    const ctxCp: RNComponentContext = new RNComponentContext(
      RNOHContext.fromCoreContext(this.rnohCoreContext!, cpInstance),
      wrapBuilder(buildCustomComponent),
      wrapBuilder(buildRNComponentForTag),
      new Map()
    );
```

---

### 🎯 Instance 相关

#### 5. `RNInstance`
**作用**: React Native 运行实例，独立的 JS 执行环境

**职责**:
- 管理 JS 引擎（Hermes/JSVM）
- 运行 JS Bundle
- 管理组件树和状态
- 提供 TurboModule 访问
- 管理生命周期事件
- 管理 Native 模块注册

**框架内部架构**:
- 每个 RNInstance 运行在独立的 JS 线程
- 包含独立的 JS 引擎实例（Hermes 或 JSVM）
- 维护独立的组件描述符注册表（DescriptorRegistry）
- 管理 Native 模块和 TurboModule 的注册

**关键属性和方法**:
```typescript
// 方法
getId(): number  // 获取 Instance ID
async runJSBundle(provider: JSBundleProvider): Promise<void>  // 加载并运行 Bundle
getTurboModule<T>(name: string): T  // 获取 TurboModule
subscribeToLifecycleEvents(event: string, callback: Function): () => void  // 订阅生命周期事件
enableFeatureFlag(flag: string): void  // 启用特性标志
getPackages(): RNOHPackage[]  // 获取已注册的包
```

**RNInstanceOptions 配置**:
```typescript
interface RNInstanceOptions {
  createRNPackages: () => RNOHPackage[]  // 创建 RN 包列表
  enableNDKTextMeasuring?: boolean  // 启用 NDK 文本测量
  enableBackgroundExecutor?: boolean  // 启用后台执行器
  enableCAPIArchitecture?: boolean  // 启用 C-API 架构
  arkTsComponentNames?: string[]  // ArkTS 组件名称列表
}
```

**特点**:
- **独立环境**: 每个 RNInstance 有独立的 JS 线程和引擎
- **可创建多个**: 一个应用可以有多个 RNInstance（如 cpInstance、bpInstance）
- **生命周期**: 通过 RNOHCoreContext 创建和销毁
- **隔离性**: 不同 Instance 之间完全隔离，互不影响

**使用示例**:
```typescript
// 创建 RNInstance
const instance: RNInstance = await rnohCoreContext.createAndRegisterRNInstance({
  createRNPackages: createRNPackages,
  enableNDKTextMeasuring: true,
  enableBackgroundExecutor: false,
  enableCAPIArchitecture: true,
  arkTsComponentNames: ['MyCustomComponent']
});

// 加载 Bundle
await instance.runJSBundle(
  new ResourceJSBundleProvider(
    getContext().resourceManager,
    'bundle.harmony.js'
  )
);

// 获取 TurboModule
const logBoxModule = instance.getTurboModule<LogBoxTurboModule>(
  LogBoxTurboModule.NAME
);

// 订阅生命周期事件
instance.subscribeToLifecycleEvents('BUNDLE_LOADED', () => {
  console.log('Bundle loaded!');
});
```

**在代码中的使用**:
```118:124:b/c/d/e/entry/src/main/ets/pages/Index.ets
    const cpInstance: RNInstance = await this.rnohCoreContext.createAndRegisterRNInstance({
      createRNPackages: createRNPackages,
      enableNDKTextMeasuring: true,
      enableBackgroundExecutor: false,
      enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
      arkTsComponentNames: arkTsComponentNames
    });
```

---

### 🐛 调试和错误处理

#### 6. `LogBoxDialog`
**作用**: 显示 React Native 的警告和错误日志对话框

**用途**: 开发模式下显示 JS 错误、警告信息，帮助开发者快速定位问题

**框架内部实现**:
- 是一个 `@CustomDialog` 装饰的 ArkTS 组件
- 内部使用 `RNSurface` 渲染 React Native 的 LogBox 组件
- 通过 `appKey: "LogBox"` 注册的 React Native 应用

**关键属性**:
```typescript
ctx: RNComponentContext  // 组件上下文（必须）
controller: CustomDialogController  // 对话框控制器
rnInstance: RNInstance  // RN 实例（必须）
initialProps: Record<string, string>  // 初始属性
buildCustomComponent: CustomComponentBuilder  // 自定义组件构建器（可选）
```

**特点**:
- **开发工具**: 仅在开发模式下使用
- **自动显示**: 通过 LogBoxTurboModule 的事件触发
- **全屏覆盖**: 通常设置为全屏透明背景，覆盖整个应用
- **React Native 渲染**: 内部使用 RNSurface 渲染 RN 的 LogBox 组件

**完整使用示例**:
```typescript
// 1. 创建 LogBoxDialog 的 CustomDialogController
this.logBoxDialogController = new CustomDialogController({
  cornerRadius: 0,  // 无圆角
  customStyle: true,  // 自定义样式
  alignment: DialogAlignment.TopStart,  // 顶部对齐
  backgroundColor: Color.Transparent,  // 透明背景
  builder: LogBoxDialog({
    // 创建专门的 RNComponentContext（通常使用 metroInstance）
    ctx: new RNComponentContext(
      RNOHContext.fromCoreContext(rnohCoreContext, metroInstance),
      wrapBuilder(buildCustomComponent),
      wrapBuilder(buildRNComponentForTag),
      new Map()
    ),
    rnInstance: metroInstance,  // 使用 metroInstance
    initialProps: {},
    buildCustomComponent: this.logBoxBuilder,  // 可选的自定义构建器
  })
});

// 2. 订阅 LogBoxTurboModule 事件
const logBoxModule = metroInstance.getTurboModule<LogBoxTurboModule>(
  LogBoxTurboModule.NAME
);

// 监听显示事件
logBoxModule.eventEmitter.subscribe("SHOW", () => {
  this.logBoxDialogController.open();
});

// 监听隐藏事件
logBoxModule.eventEmitter.subscribe("HIDE", () => {
  this.logBoxDialogController.close();
});
```

**在代码中的使用**:
```63:90:b/c/d/e/entry/src/main/ets/pages/Index.ets
  subscribeLogBox() {
    this.logBoxDialogController = new CustomDialogController({
      cornerRadius: 0,
      customStyle: true,
      alignment: DialogAlignment.TopStart,
      backgroundColor: Color.Transparent,
      builder: LogBoxDialog({
        ctx: new RNComponentContext(
          RNOHContext.fromCoreContext(this.rnohCoreContext!, LoadManager.metroInstance),
          wrapBuilder(buildCustomComponent),
          wrapBuilder(buildRNComponentForTag),
          new Map()
        ),
        rnInstance: LoadManager.metroInstance,
        initialProps: {},
        buildCustomComponent: this.logBoxBuilder,
      })
    })

    this.cleanUpCallbacks.push(LoadManager.metroInstance.getTurboModule<LogBoxTurboModule>(LogBoxTurboModule.NAME).eventEmitter.subscribe("SHOW",
      () => {
        this.logBoxDialogController.open();
      }))
    this.cleanUpCallbacks.push(LoadManager.metroInstance.getTurboModule<LogBoxTurboModule>(LogBoxTurboModule.NAME).eventEmitter.subscribe("HIDE",
      () => {
        this.logBoxDialogController.close();
      }))
  }
```

**注意事项**:
- 必须使用 `metroInstance`（开发模式下的 Instance）
- 记得在组件销毁时取消事件订阅
- LogBox 是一个完整的 React Native 应用，需要注册 `appKey: "LogBox"`

---

#### 7. `RNOHErrorDialog`
**作用**: 显示 React Native 框架级别的错误对话框

**用途**: 显示框架错误（如 Bundle 加载失败、Native 模块错误、Instance 创建失败等）

**框架内部实现**:
- 是一个 `@Component` 装饰的 ArkTS 组件
- 内部监听 `devToolsController.eventEmitter` 的 "NEW_ERROR" 事件
- 自动创建 `CustomDialogController` 并在错误发生时打开
- 显示 `RNOHError` 或 `FatalRNOHError` 的详细信息

**关键属性**:
```typescript
controller: CustomDialogController  // 对话框控制器（内部创建）
ctx: RNOHCoreContext  // 核心上下文（必须）
```

**特点**:
- **框架错误**: 显示底层框架错误，不是 JS 错误
- **自动显示**: 错误发生时自动弹出
- **开发模式**: 通常只在调试模式下使用
- **全局监听**: 监听所有框架级别的错误

**错误类型**:
- `RNOHError`: 一般框架错误
- `FatalRNOHError`: 致命错误，可能导致应用崩溃
- `RNInstanceError`: Instance 相关错误

**使用示例**:
```typescript
build() {
  Stack() {
    // 仅在调试模式且 Metro 可用时显示
    if (this.rnohCoreContext?.isDebugModeEnabled && this.isMetroAvailable) {
      RNOHErrorDialog({ ctx: this.rnohCoreContext });
    }
    // 或者不传参数（从 AppStorage 自动获取）
    if (this.rnohCoreContext?.isDebugModeEnabled) {
      RNOHErrorDialog();
    }
    // ...
  }
}
```

**在代码中的使用**:
```171:179:b/c/d/e/entry/src/main/ets/pages/Index.ets
  build() {
    Stack() {
      if (this.rnohCoreContext?.isDebugModeEnabled && this.isMetroAvailable) {
        RNOHErrorDialog();
      }
      if (this.isBundleReady) {
        MultiHome();
      }
    }
  }
```

**内部工作流程**:
```
框架错误发生
  ↓
RNOHCoreContext.reportRNOHError()
  ↓
devToolsController.eventEmitter.emit("NEW_ERROR")
  ↓
RNOHErrorDialog 订阅者收到事件
  ↓
自动打开对话框显示错误信息
```

**与 LogBoxDialog 的区别**:
- `RNOHErrorDialog`: 显示框架/Native 层错误（如 Bundle 加载失败）
- `LogBoxDialog`: 显示 JS 层错误和警告（如 JS 运行时错误）

**注意事项**:
- 通常只在开发/调试模式下使用
- 如果传入了 `ctx` 参数，必须确保是有效的 `RNOHCoreContext`
- 如果不传参数，会尝试从全局获取（可能失败）

---

#### 8. `LogBoxTurboModule`
**作用**: 控制 LogBox 显示/隐藏的 TurboModule

**用途**: 
- 监听 JS 错误/警告事件
- 控制 LogBoxDialog 的显示和隐藏
- 提供 JS 端调用 Native 端显示/隐藏 LogBox 的接口

**框架内部实现**:
- 继承自 `TurboModule` 基类
- 提供 `show()` 和 `hide()` 方法供 JS 端调用
- 使用 `EventEmitter` 发射 "SHOW" 和 "HIDE" 事件
- 在 `RNOHCorePackage` 中自动注册

**关键属性和方法**:
```typescript
// 静态属性
static NAME: Readonly<string> = "LogBox"  // TurboModule 名称

// 属性
eventEmitter: EventEmitter<{
  "SHOW": [],
  "HIDE": []
}>

// 方法
show(): void  // 显示 LogBox（JS 端调用）
hide(): void  // 隐藏 LogBox（JS 端调用）
```

**工作流程**:
```
JS 端发生错误
  ↓
React Native LogBox 调用 LogBoxTurboModule.show()
  ↓
Native 端 LogBoxTurboModule.show() 被调用
  ↓
eventEmitter.emit("SHOW")
  ↓
ArkTS 端订阅者收到事件
  ↓
打开 LogBoxDialog
```

**使用示例**:
```typescript
// 获取 LogBoxTurboModule
const logBoxModule = instance.getTurboModule<LogBoxTurboModule>(
  LogBoxTurboModule.NAME
);

// 监听显示事件
const unsubscribeShow = logBoxModule.eventEmitter.subscribe("SHOW", () => {
  dialogController.open();
});

// 监听隐藏事件
const unsubscribeHide = logBoxModule.eventEmitter.subscribe("HIDE", () => {
  dialogController.close();
});

// 清理订阅（重要！）
// 在组件销毁时调用
unsubscribeShow();
unsubscribeHide();
```

**在代码中的使用**:
```82:89:b/c/d/e/entry/src/main/ets/pages/Index.ets
    this.cleanUpCallbacks.push(LoadManager.metroInstance.getTurboModule<LogBoxTurboModule>(LogBoxTurboModule.NAME).eventEmitter.subscribe("SHOW",
      () => {
        this.logBoxDialogController.open();
      }))
    this.cleanUpCallbacks.push(LoadManager.metroInstance.getTurboModule<LogBoxTurboModule>(LogBoxTurboModule.NAME).eventEmitter.subscribe("HIDE",
      () => {
        this.logBoxDialogController.close();
      }))
```

**注意事项**:
- 必须在 `metroInstance` 上获取（开发模式）
- 记得在组件销毁时取消订阅，避免内存泄漏
- 通常只在开发模式下使用

---

### 🧩 组件构建相关

#### 9. `ComponentBuilderContext`
**作用**: 组件构建时的上下文信息

**用途**: 在 `@Builder` 函数中传递组件构建信息，用于构建自定义 ArkTS 组件

**框架内部架构**:
- 在 `CustomRNComponentFrameNodeFactory` 中创建
- 传递给自定义组件的 `@Builder` 函数
- 包含构建组件所需的所有信息

**关键属性**:
```typescript
// 主要属性
rnComponentContext: RNComponentContext  // RN 组件上下文（推荐使用）
rnohContext: RNOHContext  // @deprecated，使用 rnComponentContext 代替
tag: Tag  // 组件的唯一标签
componentName: string  // 组件名称（如 "MyCustomComponent"）
descriptor: DescriptorEssence  // @deprecated，使用 tag 和 componentName 代替

// 内部属性
customRNComponentWrappedBuilderByName: Map<string, WrappedBuilder<[ComponentBuilderContext]>>
  // 从 RNOHPackage 中注册的自定义组件构建器映射
```

**使用场景**:
1. **C-API 组件混合方案**: 在 C-API 架构中渲染 ArkTS 组件
2. **自定义组件**: 创建自定义的 React Native 组件
3. **LogBox 自定义渲染**: 自定义 LogBox 的渲染方式

**使用示例**:
```typescript
@Builder
function buildCustomComponent(ctx: ComponentBuilderContext) {
  // 必须用 Stack 包裹，并设置 position 为 (0, 0)
  Stack() {
    if (ctx.componentName === 'MyCustomComponent') {
      MyCustomComponent({
        ctx: ctx.rnComponentContext,
        tag: ctx.tag
      })
    } else if (ctx.componentName === 'AnotherComponent') {
      AnotherComponent({
        ctx: ctx.rnComponentContext,
        tag: ctx.tag
      })
    }
  }
  .position({ x: 0, y: 0 })  // 重要！必须设置
}

// 在创建 RNComponentContext 时使用
const ctx = new RNComponentContext(
  rnohContext,
  wrapBuilder(buildCustomComponent),  // 这里传入
  wrapBuilder(buildRNComponentForTag),
  new Map()
);
```

**在代码中的使用**:
```34:36:b/c/d/e/entry/src/main/ets/pages/Index.ets
  @Builder
  logBoxBuilder(_: ComponentBuilderContext) {
  }
```

**注意事项**:
- 自定义组件构建器必须用 `Stack` 包裹
- `Stack` 的 `position` 必须设置为 `{ x: 0, y: 0 }`
- 使用 `ctx.rnComponentContext` 而不是已废弃的 `ctx.rnohContext`
- 使用 `ctx.componentName` 和 `ctx.tag` 而不是已废弃的 `ctx.descriptor`

---

#### 10. `buildRNComponentForTag`
**作用**: 根据 tag 构建 React Native 组件的构建器函数

**用途**: 在 RNComponentContext 中使用，用于渲染标准的 React Native 组件（如 View、Text、Image 等）

**框架内部实现**:
```typescript
@Builder
export function buildRNComponentForTag(ctx: RNComponentContext, tag: Tag) {
  buildRNComponent(ctx, ctx.descriptorRegistry.findDescriptorWrapperByTag(tag)!)
}
```

**工作流程**:
```
RN 组件需要渲染
  ↓
通过 tag 查找组件描述符（DescriptorWrapper）
  ↓
调用 buildRNComponent() 构建对应的 ArkTS 组件
  ↓
渲染到屏幕上
```

**使用示例**:
```typescript
// 从 @rnoh/react-native-openharmony 导入
import { buildRNComponentForTag } from '@rnoh/react-native-openharmony';

// 在创建 RNComponentContext 时使用
const ctx = new RNComponentContext(
  RNOHContext.fromCoreContext(rnohCoreContext, instance),
  wrapBuilder(buildCustomComponent),  // 自定义组件构建器
  wrapBuilder(buildRNComponentForTag),  // ← RN 组件构建器
  new Map()
);

// 在 RNSurface 中使用
RNSurface({
  ctx: ctx,  // ctx 包含 buildRNComponentForTag
  surfaceConfig: {
    appKey: "MyApp",
    initialProps: {}
  }
});
```

**在代码中的使用**:
```125:130:b/c/d/e/entry/src/main/ets/pages/Index.ets
    const ctxCp: RNComponentContext = new RNComponentContext(
      RNOHContext.fromCoreContext(this.rnohCoreContext!, cpInstance),
      wrapBuilder(buildCustomComponent),
      wrapBuilder(buildRNComponentForTag),
      new Map()
    );
```

**支持的组件类型**:
- 标准 RN 组件：View、Text、Image、ScrollView 等
- 通过 `RNOHPackage` 注册的自定义组件
- 不支持需要 `ComponentBuilderContext` 的自定义 ArkTS 组件（那些用 `buildCustomComponent`）

**与 buildCustomComponent 的区别**:
- `buildRNComponentForTag`: 用于标准 RN 组件，通过 tag 查找描述符
- `buildCustomComponent`: 用于自定义 ArkTS 组件，通过 componentName 匹配

---

## 组件关系图

```
RNOHCoreContext (全局，唯一)
  │
  ├─ createAndRegisterRNInstance()
  │   └─ 创建 RNInstance
  │
  └─ RNOHContext (每个 Instance 一个)
      │
      └─ RNComponentContext (每个 Instance 一个)
          │
          ├─ buildCustomComponent (自定义组件构建器)
          ├─ buildRNComponentForTag (RN 组件构建器)
          └─ ComponentBuilderContext (构建上下文)

RNInstance (运行实例)
  │
  ├─ runJSBundle(ResourceJSBundleProvider)
  ├─ getTurboModule<LogBoxTurboModule>()
  └─ subscribeToLifecycleEvents()

LogBoxTurboModule
  │
  └─ eventEmitter
      ├─ "SHOW" → LogBoxDialog.open()
      └─ "HIDE" → LogBoxDialog.close()

RNOHErrorDialog (自动显示框架错误)
```

---

## 使用流程

### 1. 初始化流程

```
应用启动
  ↓
EntryAbility.onCreate()
  ↓
创建 RNOHCoreContext
  ↓
存储到 AppStorage
  ↓
Index.aboutToAppear()
  ↓
从 AppStorage 获取 RNOHCoreContext
  ↓
创建 RNInstance
  ↓
创建 RNOHContext (fromCoreContext)
  ↓
创建 RNComponentContext
  ↓
加载 Bundle (ResourceJSBundleProvider)
```

### 2. 调试工具流程

```
Metro 模式启用
  ↓
创建 metroInstance
  ↓
订阅 LogBoxTurboModule 事件
  ↓
创建 LogBoxDialog
  ↓
JS 错误发生
  ↓
LogBoxTurboModule 发射 "SHOW" 事件
  ↓
打开 LogBoxDialog
```

---

## 框架中的完整使用流程

### 1. 应用启动流程

```
应用启动
  ↓
EntryAbility.onCreate()
  ↓
创建 RNInstancesCoordinator
  ↓
获取 RNOHCoreContext
  ↓
存储到 AppStorage.setOrCreate('RNOHCoreContext', ...)
  ↓
Index.aboutToAppear()
  ↓
从 AppStorage 获取 RNOHCoreContext (@StorageLink)
  ↓
loadMetroBundle()  // 尝试连接 Metro 开发服务器
  ↓
register()  // 创建业务 RNInstance
  ↓
创建 RNInstance (createAndRegisterRNInstance)
  ↓
创建 RNOHContext (fromCoreContext)
  ↓
创建 RNComponentContext
  ↓
加载 Bundle (runJSBundle with ResourceJSBundleProvider)
  ↓
应用就绪
```

### 2. 组件渲染流程

```
React Native 组件需要渲染
  ↓
RNSurface 接收渲染请求
  ↓
RNComponentContext.wrappedRNComponentBuilder
  ↓
buildRNComponentForTag(ctx, tag)
  ↓
查找组件描述符 (descriptorRegistry.findDescriptorWrapperByTag)
  ↓
buildRNComponent() 构建 ArkTS 组件
  ↓
渲染到屏幕
```

### 3. 自定义组件渲染流程

```
React Native 调用自定义组件
  ↓
CustomRNComponentFrameNodeFactory.create()
  ↓
创建 ComponentBuilderContext
  ↓
RNComponentContext.wrappedCustomRNComponentBuilder
  ↓
buildCustomComponent(ctx: ComponentBuilderContext)
  ↓
根据 componentName 匹配组件
  ↓
构建 ArkTS 自定义组件
  ↓
渲染到屏幕
```

### 4. 错误处理流程

#### JS 错误处理
```
JS 运行时发生错误
  ↓
React Native LogBox 捕获错误
  ↓
调用 LogBoxTurboModule.show()
  ↓
Native 端 eventEmitter.emit("SHOW")
  ↓
ArkTS 端订阅者收到事件
  ↓
打开 LogBoxDialog
  ↓
显示错误信息
```

#### 框架错误处理
```
框架层发生错误（如 Bundle 加载失败）
  ↓
RNOHCoreContext.reportRNOHError()
  ↓
devToolsController.eventEmitter.emit("NEW_ERROR")
  ↓
RNOHErrorDialog 订阅者收到事件
  ↓
自动打开错误对话框
  ↓
显示错误详情
```

## 最佳实践

### 1. Context 创建顺序

```typescript
// ✅ 正确顺序
const rnInstance = await rnohCoreContext.createAndRegisterRNInstance({...});
const rnohContext = RNOHContext.fromCoreContext(rnohCoreContext, rnInstance);
const componentContext = new RNComponentContext(
  rnohContext,
  wrapBuilder(buildCustomComponent),
  wrapBuilder(buildRNComponentForTag),
  new Map()
);

// ❌ 错误：不能直接创建 RNOHContext
// const rnohContext = new RNOHContext(...);  // 不支持！
```

### 2. Instance 管理

```typescript
// ✅ 正确：使用 Map 管理多个 Instance
const instanceMap = new Map<string, RNInstance>();
instanceMap.set('CPInstance', cpInstance);
instanceMap.set('BPInstance', bpInstance);

// ✅ 正确：在组件销毁时清理
aboutToDisappear() {
  instanceMap.forEach((instance) => {
    rnohCoreContext.destroyAndUnregisterRNInstance(instance);
  });
}
```

### 3. 事件订阅清理

```typescript
// ✅ 正确：保存取消订阅函数并清理
private cleanUpCallbacks: (() => void)[] = [];

subscribeLogBox() {
  const unsubscribe = logBoxModule.eventEmitter.subscribe("SHOW", () => {
    // ...
  });
  this.cleanUpCallbacks.push(unsubscribe);
}

aboutToDisappear() {
  this.cleanUpCallbacks.forEach(cleanUp => cleanUp());
}
```

### 4. Bundle 加载

```typescript
// ✅ 推荐：使用 ResourceJSBundleProvider（生产环境）
await instance.runJSBundle(
  new ResourceJSBundleProvider(
    rnohCoreContext.uiAbilityContext.resourceManager,
    'bundle.harmony.js'
  )
);

// ✅ 推荐：使用 HBC 格式（Hermes Bytecode，性能更好）
await instance.runJSBundle(
  new ResourceJSBundleProvider(
    rnohCoreContext.uiAbilityContext.resourceManager,
    'hermes_bundle.hbc'
  )
);

// ✅ 开发模式：使用 MetroJSBundleProvider
await instance.runJSBundle(new MetroJSBundleProvider());
```

### 5. 自定义组件构建器

```typescript
// ✅ 正确：必须用 Stack 包裹并设置 position
@Builder
function buildCustomComponent(ctx: ComponentBuilderContext) {
  Stack() {
    if (ctx.componentName === 'MyComponent') {
      MyComponent({
        ctx: ctx.rnComponentContext,
        tag: ctx.tag
      })
    }
  }
  .position({ x: 0, y: 0 })  // 必须设置！
}
```

## 常见问题

### Q1: 为什么需要多个 Context？

**A**: 不同 Context 有不同的职责和生命周期：
- `RNOHCoreContext`: 全局共享，管理所有 Instance
- `RNOHContext`: Instance 级别，提供 Instance 特定功能
- `RNComponentContext`: 组件级别，用于渲染组件

### Q2: 什么时候使用 buildRNComponentForTag vs buildCustomComponent？

**A**: 
- `buildRNComponentForTag`: 用于标准 RN 组件（View、Text 等）
- `buildCustomComponent`: 用于自定义 ArkTS 组件（需要 C-API 架构）

### Q3: 为什么 LogBoxDialog 必须使用 metroInstance？

**A**: LogBox 是开发工具，只在开发模式下使用。Metro 是开发服务器，所以 LogBox 必须连接到 metroInstance。

### Q4: 如何判断应该创建几个 RNInstance？

**A**: 通常建议：
- **单 Instance**: 简单应用，所有页面共享一个 Instance
- **多 Instance**: 复杂应用，不同业务模块使用不同 Instance（如 cpInstance、bpInstance）

### Q5: ResourceJSBundleProvider 的 mmap 优化是什么？

**A**: 当 RNOHCoreContext 可用时，ResourceJSBundleProvider 会使用内存映射（mmap）直接读取文件，而不是加载到内存。这样可以：
- 减少内存占用
- 提高加载速度
- 支持更大的 Bundle 文件

## 总结

| 组件 | 作用 | 数量 | 生命周期 | 线程 |
|------|------|------|----------|------|
| `RNOHCoreContext` | 核心上下文，管理所有 Instance | 1个（全局） | 应用生命周期 | MAIN |
| `RNInstance` | JS 运行实例 | 多个（cpInstance、bpInstance等） | 可创建/销毁 | 独立 JS 线程 |
| `RNOHContext` | Instance 上下文 | 每个 Instance 一个 | 随 Instance | MAIN |
| `RNComponentContext` | 组件渲染上下文 | 每个 Instance 一个 | 随 Instance | MAIN |
| `ResourceJSBundleProvider` | Bundle 加载器 | 每次加载时创建 | 临时 | 任意 |
| `LogBoxDialog` | 错误日志对话框 | 1个（开发模式） | 开发模式 | MAIN |
| `RNOHErrorDialog` | 框架错误对话框 | 1个（开发模式） | 开发模式 | MAIN |
| `LogBoxTurboModule` | LogBox 控制模块 | 每个 Instance 一个 | 随 Instance | MAIN |
| `ComponentBuilderContext` | 组件构建上下文 | 每次构建时传递 | 临时 | MAIN |
| `buildRNComponentForTag` | RN 组件构建器 | 函数，可复用 | 全局 | MAIN |

## 相关文档

- [RNInstance 机制详解](./RNInstance机制详解.md)
- [RNInstance 创建时机和关联关系详解](./RNInstance创建时机和关联关系详解.md)
- [React Native Harmony 完整架构与执行流程](./React_Native_Harmony_完整架构与执行流程.md)



