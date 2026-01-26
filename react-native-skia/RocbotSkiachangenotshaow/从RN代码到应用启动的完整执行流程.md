# 从 React Native 代码到应用启动的完整执行流程

## 目录
1. [开发阶段：代码打包](#开发阶段代码打包)
2. [应用启动阶段](#应用启动阶段)
3. [Instance 创建阶段](#instance-创建阶段)
4. [Bundle 加载阶段](#bundle-加载阶段)
5. [页面渲染阶段](#页面渲染阶段)
6. [完整时序图](#完整时序图)

---

## 开发阶段：代码打包

### 1.1 JS/TS 代码编写

```
开发者编写 React Native 代码
  ├── React 组件 (View, Text, Image 等)
  ├── 业务逻辑代码
  ├── TurboModule 调用
  └── 样式定义
```

**示例代码**:
```typescript
// DetailsMainPage.tsx
import { View, Text } from 'react-native';
import { Canvas, Circle } from '@shopify/react-native-skia';

function AppDetails() {
  return (
    <View>
      <Text>Hello Details</Text>
      <Canvas>
        <Circle cx={50} cy={50} r={30} color="cyan" />
      </Canvas>
    </View>
  );
}

AppRegistry.registerComponent('Details', () => AppDetails);
```

---

### 1.2 Metro Bundler 打包

```
Metro Bundler 处理
  ├── 解析依赖关系
  ├── Babel/TypeScript 转换
  ├── 代码压缩和优化
  └── 生成 bundle 文件
```

**打包流程**:
```
JS/TS 源代码
  ↓
Metro Bundler
  ├── 依赖解析
  ├── 代码转换 (Babel)
  ├── 模块打包
  └── 代码压缩
  ↓
bundle.harmony.bundle
  └── 存储在 rawfile/bundle/ 目录
```

**生成的文件**:
- `bundle/basic/basic.harmony.bundle` - 基础库
- `bundle/cp/homepage.harmony.bundle` - CP 业务
- `bundle/bp/details.harmony.bundle` - BP 业务

---

## 应用启动阶段

### 2.1 系统启动应用

```
系统调用应用
  ↓
EntryAbility.onCreate()
```

**执行位置**: `EntryAbility.ets`

**关键代码**:
```typescript
export default class EntryAbility extends RNAbility {
  onCreate(want: Want) {
    // RNAbility 内部会创建 RNInstancesCoordinator
    // 并创建 RNOHCoreContext
  }
}
```

---

### 2.2 RNAbility.onCreate() 内部执行

```
RNAbility.onCreate()
  ├── 创建 RNInstancesCoordinator
  │   ├── 初始化配置
  │   ├── 创建 RNOHCoreContext
  │   └── 设置全局状态
  └── 存储到 AppStorage
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. RNAbility.onCreate(want: Want)                            │
│    - 系统调用应用入口                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. 创建 RNInstancesCoordinator                               │
│    RNInstancesCoordinator.create({                          │
│      fontSizeScale: config.fontSizeScale,                   │
│      logger: this.createLogger(),                            │
│      uiAbilityContext: this.context,                        │
│      defaultBackPressHandler: () => {...},                   │
│    }, {                                                      │
│      launchURI: want.uri,                                   │
│      onGetPackagerClientConfig: (buildMode) => {...},       │
│      defaultHttpClient: this.onCreateDefaultHttpClient()    │
│    })                                                        │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. RNInstancesCoordinator 内部创建 RNOHCoreContext           │
│    - 初始化依赖项                                            │
│    - 创建 rnInstanceRegistry                                 │
│    - 创建 displayMetricsProvider                            │
│    - 创建 uiAbilityStateProvider                             │
│    - 创建 rnohErrorEventEmitter                              │
│    - 创建 logger                                             │
│    - 创建 devToolsController                                 │
│    - 创建 devMenu                                            │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. 存储 RNOHCoreContext 到 AppStorage                        │
│    AppStorage.setOrCreate(                                   │
│      'RNOHCoreContext',                                      │
│      rnInstancesCoordinator.getRNOHCoreContext()             │
│    )                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. 调用 onWindowStageCreate()                                │
│    - 创建窗口阶段                                            │
│    - 加载入口页面 (pages/Index)                             │
└─────────────────────────────────────────────────────────────┘
```

---

### 2.3 Index.ets 页面加载

```
Index.aboutToAppear()
  ├── 从 AppStorage 获取 RNOHCoreContext
  ├── 调用 loadMetroBundle()
  └── 等待 Bundle 就绪
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Index.aboutToAppear()                                    │
│    - 页面显示时系统调用                                       │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. 检查 RNOHCoreContext                                     │
│    if (!this.rnohCoreContext) {                             │
│      return;  // 如果未初始化，直接返回                      │
│    }                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. 调用 loadMetroBundle()                                   │
│    this.loadMetroBundle()                                   │
└─────────────────────────────────────────────────────────────┘
```

**代码位置**: `Index.ets:38-43`

```typescript
aboutToAppear() {
  if (!this.rnohCoreContext) {
    return;
  }
  this.loadMetroBundle()
}
```

---

## Instance 创建阶段

### 3.1 Metro Instance 创建（开发模式）

```
LoadManager.loadMetroBundle()
  ├── 检查是否已创建
  ├── 创建 metroInstance
  ├── 创建 RNComponentContext
  └── 加载 Metro bundle
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. LoadManager.loadMetroBundle(uiContext)                   │
│    - 从 AppStorage 获取 RNOHCoreContext                      │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. 检查是否需要创建                                          │
│    if (LoadManager.shouldResetMetroInstance &&              │
│        rnohCoreContext) {                                   │
│      // 创建 metroInstance                                  │
│    }                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. 创建 metroInstance                                        │
│    LoadManager.metroInstance =                              │
│      await rnohCoreContext.createAndRegisterRNInstance({    │
│        createRNPackages: createRNPackages,                  │
│        enableNDKTextMeasuring: true,                        │
│        enableBackgroundExecutor: false,                      │
│        enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,    │
│        arkTsComponentNames: arkTsComponentNames              │
│      })                                                      │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. 创建 RNComponentContext                                   │
│    LoadManager.ctx = new RNComponentContext(                │
│      RNOHContext.fromCoreContext(                           │
│        rnohCoreContext,                                      │
│        LoadManager.metroInstance                             │
│      ),                                                      │
│      wrapBuilder(buildCustomComponent),                     │
│      wrapBuilder(buildRNComponentForTag),                    │
│      new Map()                                               │
│    )                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. 加载 Metro bundle                                         │
│    const provider = new MetroJSBundleProvider();            │
│    await LoadManager.metroInstance.runJSBundle(provider);   │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. 检查 Bundle 执行状态                                      │
│    const status =                                            │
│      LoadManager.metroInstance                               │
│        .getBundleExecutionStatus(provider.getURL());         │
│    if (status === "DONE") {                                 │
│      return true;  // Metro 可用                             │
│    }                                                         │
└─────────────────────────────────────────────────────────────┘
```

**代码位置**: `LoadBundle.ets:71-102`

---

### 3.2 createAndRegisterRNInstance 内部执行（C++ 层）

```
ArkTS: createAndRegisterRNInstance()
  ↓ NAPI
C++: onCreateRNInstance()
  ├── 获取 RNInstance ID
  ├── 创建 RNInstanceInternal
  ├── 初始化 JS 引擎
  ├── 注册 TurboModule
  └── 注册 Fabric
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. ArkTS 调用 createAndRegisterRNInstance()                  │
│    const instance = await rnohCoreContext                    │
│      .createAndRegisterRNInstance(options)                   │
└─────────────────────────────────────────────────────────────┘
                    ↓ NAPI
┌─────────────────────────────────────────────────────────────┐
│ 2. NAPI Bridge: onCreateRNInstance()                        │
│    - 类型转换 (ArkTS → C++)                                 │
│    - 调用 C++ 函数                                          │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. C++: RNInstanceRegistry.getNextRNInstanceId()            │
│    - 生成唯一的 Instance ID                                  │
│    - 返回 ID (例如: 1, 2, 3...)                             │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. C++: RNInstanceFactory.createInstance()                   │
│    - 创建 RNInstanceInternal 对象                            │
│    - 初始化配置                                              │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. C++: 注册 TurboModule                                    │
│    - 处理 createRNPackages 中的包                           │
│    - 注册系统 TurboModule                                    │
│    - 注册自定义 TurboModule                                  │
│    - 创建 __turboModuleProxy (JSI 函数)                     │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. C++: 注册字体                                             │
│    - 注册系统字体                                            │
│    - 注册自定义字体                                          │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 7. C++: 注册 ArkTS 组件                                     │
│    - 处理 arkTsComponentNames                               │
│    - 注册自定义组件 (如 RNCSkiaDomView)                     │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 8. C++: 初始化 JS 引擎                                      │
│    - 创建 Hermes Runtime 或 JSVM Runtime                    │
│    - 初始化 JSI Runtime                                     │
│    - 设置全局对象                                            │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 9. C++: 注册 JSI 通道                                       │
│    - 注册 __turboModuleProxy                                │
│    - 注册 nativeFabricUIManager                            │
│    - 注册其他 JSI 函数                                      │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 10. C++: 创建 JS 线程                                        │
│     - 创建独立的 JS 执行线程                                  │
│     - 设置消息队列                                           │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 11. 返回 RNInstance 对象到 ArkTS                             │
│     - 通过 NAPI 返回                                         │
│     - 类型转换 (C++ → ArkTS)                                │
└─────────────────────────────────────────────────────────────┘
```

---

### 3.3 业务 Instance 创建（生产模式）

```
Index.register()
  ├── 检查 Metro 是否可用
  ├── 如果不可用，创建业务 Instance
  │   ├── cpInstance
  │   ├── bpInstance
  │   └── bpInstance2
  └── 加载基础 bundle
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Index.register()                                          │
│    - loadMetroBundle() 完成后调用                            │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. 检查 Metro 是否可用                                       │
│    if (this.isMetroAvailable) {                             │
│      // Metro 模式：不创建业务 Instance                      │
│      this.subscribeReload();                                 │
│      this.subscribeLogBox();                                  │
│      return;                                                  │
│    }                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. 创建 cpInstance                                           │
│    const cpInstance: RNInstance =                           │
│      await this.rnohCoreContext                             │
│        .createAndRegisterRNInstance({                        │
│          createRNPackages: createRNPackages,                │
│          enableNDKTextMeasuring: true,                      │
│          enableBackgroundExecutor: false,                    │
│          enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,  │
│          arkTsComponentNames: arkTsComponentNames            │
│        })                                                     │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. 创建 bpInstance                                           │
│    const bpInstance: RNInstance =                           │
│      await this.rnohCoreContext                             │
│        .createAndRegisterRNInstance({...})                    │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. 创建 bpInstance2                                         │
│    const bpInstance2: RNInstance =                           │
│      await this.rnohCoreContext                             │
│        .createAndRegisterRNInstance({...})                    │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. 保存到 LoadManager                                        │
│    LoadManager.cpInstance = cpInstance;                     │
│    LoadManager.bpInstance = bpInstance;                     │
│    LoadManager.bpInstance2 = bpInstance2;                    │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 7. 加载基础 bundle 到 cpInstance                             │
│    await cpInstance.runJSBundle(                             │
│      new ResourceJSBundleProvider(                           │
│        getContext().resourceManager,                         │
│        'bundle/basic/basic.harmony.bundle'                   │
│      )                                                       │
│    )                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 8. 设置 isBundleReady = true                                 │
│    this.isBundleReady = true;                                │
│    - 触发 UI 更新                                            │
│    - 显示 MultiHome                                          │
└─────────────────────────────────────────────────────────────┘
```

**代码位置**: `Index.ets:105-169`

---

## Bundle 加载阶段

### 4.1 Bundle 加载流程

```
runJSBundle(provider)
  ├── 读取 bundle 文件
  ├── 传递到 C++ 层
  ├── 加载到 JS 引擎
  └── 执行 JS 代码
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. ArkTS: instance.runJSBundle(provider)                     │
│    await cpInstance.runJSBundle(                             │
│      new ResourceJSBundleProvider(                           │
│        getContext().resourceManager,                         │
│        'bundle/basic/basic.harmony.bundle'                   │
│      )                                                       │
│    )                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. ResourceJSBundleProvider 读取文件                         │
│    - 从 rawfile 资源中读取 bundle 文件                       │
│    - 转换为字节流                                            │
└─────────────────────────────────────────────────────────────┘
                    ↓ NAPI
┌─────────────────────────────────────────────────────────────┐
│ 3. NAPI: loadScript()                                        │
│    - 类型转换 (ArkTS → C++)                                 │
│    - 传递 bundle 数据到 C++                                 │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. C++: RNInstanceInternal.loadScript()                     │
│    - 接收 bundle 字节流                                      │
│    - 准备加载到 JS 引擎                                      │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. C++: 在 JS 线程中执行                                     │
│    - 切换到 JS 线程                                          │
│    - 调用 JS 引擎的 evaluateJavaScript()                   │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. JS 引擎执行 bundle 代码                                   │
│    - Hermes/JSVM 解析和执行 JS 代码                         │
│    - 执行 AppRegistry.registerComponent()                   │
│    - 注册组件到全局注册表                                    │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 7. Bundle 执行完成                                           │
│    - 触发 JS_BUNDLE_EXECUTION_FINISH 事件                   │
│    - 更新状态为 "DONE"                                       │
└─────────────────────────────────────────────────────────────┘
```

---

### 4.2 JS 代码执行

```
JS 引擎执行 bundle
  ├── 执行全局代码
  ├── 注册组件 (AppRegistry.registerComponent)
  ├── 注册 TurboModule
  └── 初始化 React Native 环境
```

**示例 bundle 代码执行**:

```javascript
// bundle 中的代码
import { AppRegistry } from 'react-native';
import AppDetails from './DetailsMainPage';

// 执行注册
AppRegistry.registerComponent('Details', () => AppDetails);

// 注册 TurboModule（通过 JSI）
// __turboModuleProxy 已在 C++ 层注册
```

---

## 页面渲染阶段

### 5.1 页面导航

```
用户点击按钮
  ↓
Navigation 跳转
  ↓
Details.aboutToAppear()
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. 用户操作触发导航                                          │
│    - 点击按钮跳转到 Details 页面                             │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. Navigation 组件处理                                       │
│    - 推入新的 NavDestination                                │
│    - 触发 Details.aboutToAppear()                           │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. Details.aboutToAppear()                                  │
│    - 选择对应的 RNInstance                                   │
│    - 准备 initialProps                                       │
└─────────────────────────────────────────────────────────────┘
```

**代码位置**: `Details.ets:42-53`

```typescript
aboutToAppear() {
  if (this.name.endsWith("1")) {
    this.instance = LoadManager.bpInstance2;
  } else {
    this.instance = LoadManager.bpInstance;
  }
  this.initProps = {
    'stringParam': 'ArkTS传递给RN的参数：' + this.name,
    'styles': this.styles
  };
}
```

---

### 5.2 BaseRN 初始化

```
BaseRN.aboutToAppear()
  ├── 加载 bundle
  ├── 等待 bundle 就绪
  └── 渲染 RNSurface
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. BaseRN.aboutToAppear()                                   │
│    - 页面显示时调用                                           │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. 加载 bundle                                               │
│    LoadManager.loadBundle(                                   │
│      this.rnInstance,                                        │
│      this.bundlePath,                                        │
│      this.useBundleCache                                     │
│    )                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. 检查 bundle 缓存                                          │
│    if (LoadManager.loadedBundle.has(bundlePath)) {          │
│      return Promise.resolve();  // 已加载，直接返回          │
│    }                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. 加载 bundle                                               │
│    await instance.runJSBundle(                                │
│      new ResourceJSBundleProvider(                           │
│        getContext().resourceManager,                         │
│        bundlePath                                            │
│      )                                                       │
│    )                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. Bundle 加载完成                                           │
│    this.isBundleReady = true;                                │
│    - 触发 UI 更新                                            │
└─────────────────────────────────────────────────────────────┘
```

**代码位置**: `BaseRN.ets:31-40`

---

### 5.3 RNSurface 渲染

```
RNSurface 渲染
  ├── 创建 XComponent
  ├── 调用 C++ startSurface
  ├── 创建 RootView
  └── 注册到 XComponent
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. RNSurface.build()                                         │
│    - ArkTS UI 构建                                           │
│    - 创建 XComponent                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. 创建 XComponent                                           │
│    XComponent({                                              │
│      id: instanceId + "_" + surfaceId,                      │
│      type: "node",                                           │
│      libraryname: 'rnoh_app'                                │
│    })                                                        │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. C++: registerNativeXComponent()                          │
│    - Init 函数自动调用                                        │
│    - 解析 XComponent ID                                      │
│    - 获取对应的 Instance 和 Surface                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. C++: startSurface()                                       │
│    - 创建 RootView                                           │
│    - 创建 Surface                                            │
│    - 初始化渲染环境                                          │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. C++: 附加 RootView 到 XComponent                         │
│    OH_NativeXComponent_AttachNativeRootNode(                │
│      nativeXComponent,                                       │
│      rootView.getLocalRootArkUINode()                       │
│        .getArkUINodeHandle()                                │
│    )                                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. 调用 JS: AppRegistry.runApplication()                    │
│    - 在 JS 线程中执行                                         │
│    - 启动 React Native 应用                                 │
└─────────────────────────────────────────────────────────────┘
```

---

### 5.4 React 渲染流程

```
AppRegistry.runApplication()
  ├── 创建 React 根节点
  ├── 开始渲染
  ├── 创建 ShadowTree
  ├── Yoga 布局计算
  └── 生成 Mutations
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. JS: AppRegistry.runApplication('Details', initialProps) │
│    - 获取注册的组件                                          │
│    - 创建 React 根节点                                       │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. React 开始渲染                                            │
│    - 调用组件函数                                            │
│    - 执行 JSX 代码                                          │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. 创建 ShadowTree                                           │
│    - 为每个组件创建 ShadowNode                               │
│    - 构建虚拟 DOM 树                                         │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. Yoga 布局计算                                             │
│    - 计算每个节点的位置和尺寸                                │
│    - 处理 Flexbox 布局                                       │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. 生成 Mutations                                            │
│    - Create: 创建新组件                                      │
│    - Update: 更新组件属性                                    │
│    - Insert: 插入组件到树                                   │
│    - Remove: 从树中移除组件                                  │
│    - Delete: 删除组件                                        │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. Fabric 发送 Mutations 到 C++                              │
│    - 通过 JSI 调用 nativeFabricUIManager                    │
│    - 传递 Mutations 数据                                    │
└─────────────────────────────────────────────────────────────┘
```

---

### 5.5 C++ 处理 Mutations

```
SchedulerDelegate 接收 Mutations
  ├── 解析 Mutations
  ├── 创建/更新 ComponentInstance
  └── 调用 ArkUI C-API
```

**详细流程**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. C++: SchedulerDelegate.schedulerDidFinishTransaction() │
│    - 接收 Fabric 传来的 Mutations                           │
│    - 获取 MountingCoordinator                                │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. 解析 Mutations                                            │
│    - 遍历所有 Mutations                                      │
│    - 识别操作类型 (Create/Update/Insert/Remove/Delete)     │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. MountingManagerCAPI 处理                                  │
│    - 根据操作类型执行相应逻辑                                │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. Create 操作                                               │
│    - 创建 ComponentInstance                                 │
│    - 调用 ArkUI C-API 创建节点                              │
│    - 设置初始属性                                            │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. Update 操作                                               │
│    - 更新 ComponentInstance 的 Props                        │
│    - 调用 ArkUI C-API 更新属性                              │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. Insert 操作                                               │
│    - 将组件插入到父组件中                                    │
│    - 调用 ArkUI C-API 插入节点                              │
└─────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────┐
│ 7. ArkUI 渲染                                                │
│    - 系统级渲染                                              │
│    - 显示到屏幕                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 完整时序图

```
时间轴 →
│
├─ [系统启动应用]
│  └─ EntryAbility.onCreate()
│     └─ 创建 RNInstancesCoordinator
│        └─ 创建 RNOHCoreContext
│           └─ 存储到 AppStorage
│
├─ [页面加载]
│  └─ Index.aboutToAppear()
│     └─ loadMetroBundle()
│        └─ 创建 metroInstance (开发模式)
│           └─ 加载 Metro bundle
│
├─ [业务 Instance 创建]
│  └─ Index.register()
│     └─ 创建 cpInstance
│        └─ 创建 bpInstance
│           └─ 创建 bpInstance2
│              └─ 加载 basic bundle
│
├─ [用户导航]
│  └─ Details.aboutToAppear()
│     └─ BaseRN.aboutToAppear()
│        └─ 加载 details bundle
│
├─ [Surface 创建]
│  └─ RNSurface 渲染
│     └─ 创建 XComponent
│        └─ C++ startSurface()
│           └─ 创建 RootView
│
├─ [React 渲染]
│  └─ AppRegistry.runApplication()
│     └─ React 渲染
│        └─ 创建 ShadowTree
│           └─ Yoga 布局计算
│              └─ 生成 Mutations
│
├─ [原生渲染]
│  └─ SchedulerDelegate 接收 Mutations
│     └─ MountingManagerCAPI 处理
│        └─ 创建 ComponentInstance
│           └─ 调用 ArkUI C-API
│              └─ 渲染到屏幕
│
└─ [完成]
   └─ 页面显示
```

---

## 关键时间点总结

| 阶段 | 时间点 | 关键操作 | 涉及组件 |
|------|--------|---------|---------|
| **启动** | EntryAbility.onCreate() | 创建 RNOHCoreContext | RNAbility, RNInstancesCoordinator |
| **初始化** | Index.aboutToAppear() | 获取 RNOHCoreContext | Index |
| **Instance 创建** | loadMetroBundle() / register() | 创建 RNInstance | LoadManager, RNOHCoreContext |
| **C++ 初始化** | createAndRegisterRNInstance() | 创建 JS 引擎、注册 JSI | RNInstanceInternal |
| **Bundle 加载** | runJSBundle() | 加载 bundle 到 JS 引擎 | ResourceJSBundleProvider |
| **JS 执行** | JS 引擎执行 | 执行 bundle 代码、注册组件 | Hermes/JSVM |
| **页面渲染** | RNSurface 渲染 | 创建 Surface、启动应用 | RNSurface, XComponent |
| **React 渲染** | AppRegistry.runApplication() | React 渲染、创建 ShadowTree | React, Fabric |
| **原生渲染** | SchedulerDelegate | 处理 Mutations、调用 ArkUI | MountingManagerCAPI, ComponentInstance |

---

## 总结

从 React Native 代码到应用启动的完整流程：

1. **开发阶段**: JS 代码 → Metro Bundler → bundle 文件
2. **启动阶段**: EntryAbility → RNOHCoreContext → RNInstance
3. **加载阶段**: Bundle 加载 → JS 引擎执行 → 组件注册
4. **渲染阶段**: RNSurface → React 渲染 → Mutations → ArkUI 渲染

整个流程涉及 **6 层架构**（JS → JSI → C++ → NAPI → ArkTS → ArkUI）和 **多个线程**（MAIN、JS、WORKER、UI），通过 **3 种通信机制**（JSI、NAPI、C-API）实现跨语言通信。





