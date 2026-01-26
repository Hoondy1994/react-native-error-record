# RNInstance 机制详解

## 目录
1. [什么是 RNInstance](#什么是-rninstance)
2. [核心文件解析](#核心文件解析)
3. [Instance 创建流程](#instance-创建流程)
4. [Instance 使用场景](#instance-使用场景)
5. [预创建机制详解](#预创建机制详解)
6. [Instance 生命周期](#instance-生命周期)
7. [常见问题分析](#常见问题分析)

---

## 什么是 RNInstance

### 核心概念

**RNInstance** 是 React Native 在鸿蒙系统中的**运行实例**，可以理解为：

- **一个独立的 JS 执行环境**：每个 RNInstance 都有自己的 JS 引擎（Hermes/JSVM）
- **一个独立的 JS 线程**：每个 RNInstance 运行在独立的 JS 线程上
- **一个独立的组件树**：每个 RNInstance 可以渲染多个 RNSurface（页面）

### 类比理解

```
RNInstance ≈ 一个独立的浏览器标签页
├── 有自己的 JS 执行环境
├── 可以加载不同的 bundle（网页）
└── 可以渲染多个页面（Surface）
```

### Instance 的作用

1. **隔离性**：不同的 Instance 之间相互独立，互不影响
2. **性能**：每个 Instance 有独立的线程，避免阻塞
3. **灵活性**：可以为不同的业务模块创建不同的 Instance

---

## 核心文件解析

### 1. LoadBundle.ets - Instance 管理器

**文件路径：** `b/c/d/e/entry/src/main/ets/rn/LoadBundle.ets`

**核心作用：** 管理全局的 RNInstance 实例

```typescript
export class LoadManager {
  // 全局静态变量：存储不同的 Instance
  public static metroInstance: RNInstance;    // Metro 开发模式用的 Instance
  public static cpInstance: RNInstance;       // CP 业务用的 Instance
  public static bpInstance: RNInstance;       // BP 业务用的 Instance 1
  public static bpInstance2: RNInstance;      // BP 业务用的 Instance 2
  
  // Bundle 缓存：记录已加载的 bundle，避免重复加载
  private static loadedBundle: Set<string> = new Set();
  
  // 加载 bundle 到指定的 Instance
  public static loadBundle(
    instance: RNInstance,        // 要加载到的 Instance
    bundlePath: string,         // bundle 文件路径
    useBundleCache: boolean      // 是否使用缓存
  ): Promise<void> {
    // 如果已经加载过，直接返回
    if (LoadManager.loadedBundle.has(bundlePath) && useBundleCache) {
      return Promise.resolve();
    }
    // 记录已加载
    if (useBundleCache) {
      LoadManager.loadedBundle.add(bundlePath);
    }
    // 执行加载
    return instance.runJSBundle(
      new ResourceJSBundleProvider(getContext().resourceManager, bundlePath)
    );
  }
}
```

**关键点：**
- `LoadManager` 是**单例模式**，所有 Instance 都在这里管理
- 不同的 Instance 用于不同的业务场景
- Bundle 缓存机制避免重复加载

### 2. Index.ets - Instance 创建入口

**文件路径：** `b/c/d/e/entry/src/main/ets/pages/Index.ets`

**核心作用：** 应用启动时创建全局 Instance

```typescript
@Entry()
@Component
struct Index {
  async register(): Promise<Map<string, RNInstance>> {
    // 创建 cpInstance（用于 CP 业务）
    const cpInstance: RNInstance = await this.rnohCoreContext.createAndRegisterRNInstance({
      createRNPackages: createRNPackages,        // 注册 TurboModule 包
      enableNDKTextMeasuring: true,              // 启用文本测量
      enableBackgroundExecutor: false,           // 禁用后台执行器
      enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,  // 启用 C-API
      arkTsComponentNames: arkTsComponentNames    // 自定义组件名称列表
    });
    
    // 创建 bpInstance（用于 BP 业务）
    const bpInstance: RNInstance = await this.rnohCoreContext.createAndRegisterRNInstance({
      // ... 相同配置
    });
    
    // 创建 bpInstance2（用于 BP 业务，另一个实例）
    const bpInstance2: RNInstance = await this.rnohCoreContext.createAndRegisterRNInstance({
      // ... 相同配置
    });
    
    // 保存到 LoadManager
    LoadManager.cpInstance = cpInstance;
    LoadManager.bpInstance = bpInstance;
    LoadManager.bpInstance2 = bpInstance2;
    
    // 为 cpInstance 加载基础 bundle
    await cpInstance.runJSBundle(
      new ResourceJSBundleProvider(
        getContext().resourceManager, 
        'bundle/basic/basic.harmony.bundle'
      )
    );
    
    return instanceMap;
  }
}
```

**关键点：**
- 应用启动时创建**全局共享的 Instance**
- 不同的 Instance 可以加载不同的 bundle
- Instance 创建后需要注册到 `LoadManager` 供其他页面使用

### 3. BaseRN.ets - 使用共享 Instance

**文件路径：** `b/c/d/e/entry/src/main/ets/rn/BaseRN.ets`

**核心作用：** 使用已创建的共享 Instance 渲染 RN 页面

```typescript
@Component
export struct BaseRN {
  rnInstance!: RNInstance;        // 从外部传入的 Instance（共享的）
  moduleName: string = '';        // RN 模块名称
  bundlePath: string = '';        // bundle 文件路径
  initProps: Record<string, string> = {};  // 初始属性
  
  aboutToAppear() {
    // 加载 bundle 到共享的 Instance
    LoadManager.loadBundle(this.rnInstance, this.bundlePath, this.useBundleCache)
      .then(() => {
        this.isBundleReady = true;  // bundle 加载完成
      });
  }
  
  build() {
    Column() {
      if (this.rnohCoreContext && this.isBundleReady) {
        // 使用共享的 Instance 创建 RNSurface
        RNSurface({
          surfaceConfig: {
            appKey: this.moduleName,      // 页面标识
            initialProps: this.initProps, // 初始属性
          },
          ctx: new RNComponentContext(
            RNOHContext.fromCoreContext(this.rnohCoreContext!, this.rnInstance),
            // ...
          ),
        })
      }
    }
  }
}
```

**关键点：**
- **使用共享的 Instance**：不创建新 Instance，使用 `LoadManager` 中的
- **延迟渲染**：等 bundle 加载完成（`isBundleReady = true`）才渲染
- **一个 Instance 可以渲染多个 Surface**：通过不同的 `appKey` 区分

**使用示例：**
```typescript
// Details.ets
aboutToAppear() {
  // 根据条件选择不同的 Instance
  if (this.name.endsWith("1")) {
    this.instance = LoadManager.bpInstance2;  // 使用 bpInstance2
  } else {
    this.instance = LoadManager.bpInstance;   // 使用 bpInstance
  }
}

build() {
  // 使用选中的 Instance
  BaseRN({
    rnInstance: this.instance,      // 传入共享的 Instance
    moduleName: this.moduleName,
    bundlePath: this.bundlePath,
    initProps: this.initProps
  })
}
```

### 4. BasePrecreateView.ets - 预创建 Instance

**文件路径：** `b/c/d/e/entry/src/main/ets/rn/BasePrecreateView.ets`

**核心作用：** 为每个页面创建独立的 Instance，并支持预创建机制

```typescript
@Component
export struct BasePrecreateView {
  @StorageLink('rnInstances') rnInstances: ArrayList<CachedInstance>;  // 预创建实例池
  private rnInstance!: RNInstance;  // 当前使用的 Instance
  
  aboutToAppear() {
    this.init();              // 初始化：获取或创建 Instance
    this.precreateInstance(); // 预创建：为下次进入准备 Instance
  }
  
  private async init() {
    // 1. 获取或创建 Instance
    const instance = await this.getOrCreateRNInstance();
    this.rnInstance = instance.rnInstance;
    
    // 2. 检查 bundle 状态
    const jsBundleExecutionStatus = instance.status;
    
    // 3. 如果 Instance 还没运行过 bundle，或者需要强制重新加载
    if (this.jsBundleProvider && (jsBundleExecutionStatus === undefined || shouldForceReload)) {
      await this.rnInstance.runJSBundle(this.jsBundleProvider);  // 运行 bundle
      this.shouldShow = true;  // 可以显示了
      return;
    }
    
    // 4. 如果 bundle 还在加载中，等待完成
    if (jsBundleExecutionStatus !== "DONE") {
      this.rnInstance.subscribeToLifecycleEvents("JS_BUNDLE_EXECUTION_FINISH", () => {
        this.shouldShow = true;
      });
    } else {
      // 5. bundle 已经加载完成，直接显示
      this.shouldShow = true;
    }
  }
  
  private async getOrCreateRNInstance(): Promise<CachedInstance> {
    // 尝试从预创建池中获取
    if (!shouldSkipCache && this.rnInstances.length > 0) {
      const instance = this.rnInstances.removeByIndex(0);  // 取出预创建的 Instance
      return instance;
    }
    
    // 池中没有，创建新的 Instance
    const instance = await this.rnohCoreContext!.createAndRegisterRNInstance({
      // ... 配置
    });
    return { rnInstance: instance };
  }
  
  private async precreateInstance() {
    // 如果池是空的，且不需要强制重新加载，就预创建一个
    if (instances.length === 0 && !this.shouldForceReload()) {
      // 创建新的 Instance
      const rnInstance = await rnohCoreContext.createAndRegisterRNInstance({
        // ... 配置
      });
      
      // 在后台运行 bundle
      await rnInstance.runJSBundle(provider);
      
      // 标记为已完成，存入池中
      instance.status = "DONE";
      instances.add(instance);
    }
  }
}
```

**关键点：**
- **每个页面有独立的 Instance**：不共享，每次进入可能创建新的
- **预创建机制**：在后台提前创建 Instance 并运行 bundle，下次进入时直接使用
- **Instance 池**：使用 `@StorageLink('rnInstances')` 在页面间共享预创建的 Instance

**预创建流程：**
```
第一次进入页面：
1. init() → 创建新 Instance → 运行 bundle → 显示
2. precreateInstance() → 在后台创建另一个 Instance → 运行 bundle → 存入池

第二次进入页面：
1. init() → 从池中取出预创建的 Instance（bundle 已运行）→ 直接显示
2. precreateInstance() → 池是空的，再创建一个新的 → 运行 bundle → 存入池
```

### 5. MetroBaseRN.ets - Metro 开发模式

**文件路径：** `b/c/d/e/entry/src/main/ets/rn/MetroBaseRN.ets`

**核心作用：** 使用 Metro 开发服务器的 Instance

```typescript
@Component
export struct MetroBaseRN {
  build() {
    // 直接使用 LoadManager.metroInstance（开发模式专用）
    RNSurface({
      surfaceConfig: {
        appKey: this.moduleName,
        initialProps: this.initProps,
      },
      ctx: new RNComponentContext(
        RNOHContext.fromCoreContext(this.rnohCoreContext!, LoadManager.metroInstance),
        // ...
      ),
    })
  }
}
```

**关键点：**
- **开发模式专用**：连接 Metro 服务器，支持热重载
- **使用全局 metroInstance**：所有页面共享同一个 Instance
- **无需加载 bundle**：bundle 从 Metro 服务器动态加载

---

## Instance 创建流程

### 完整流程图

```
应用启动（Index.ets）
  ↓
创建 RNOHCoreContext（全局上下文）
  ↓
创建全局 Instance（cpInstance, bpInstance, bpInstance2）
  ├── 创建 JS 引擎（Hermes/JSVM）
  ├── 创建 JS 线程
  ├── 注册 TurboModule 包
  └── 注册自定义组件
  ↓
保存到 LoadManager（全局管理）
  ↓
加载基础 bundle（basic.harmony.bundle）到 cpInstance
  ↓
页面使用（BaseRN 或 BasePrecreateView）
  ├── BaseRN：使用共享的 Instance
  └── BasePrecreateView：创建独立的 Instance
```

### 创建 Instance 的配置参数

```typescript
await rnohCoreContext.createAndRegisterRNInstance({
  // 1. 注册 TurboModule 包
  createRNPackages: createRNPackages,
  // 包含：
  // - SampleTurboModulePackage（自定义 TurboModule）
  // - ViewPagerPackage（第三方组件包）
  // - RNSkiaPackage（Skia 组件包）
  
  // 2. 启用文本测量（使用 NDK）
  enableNDKTextMeasuring: true,
  
  // 3. 禁用后台执行器（实验特性，不稳定）
  enableBackgroundExecutor: false,
  
  // 4. 启用 C-API 架构（性能优化）
  enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
  
  // 5. 自定义组件名称列表
  arkTsComponentNames: [MarqueeView.NAME, SKIA_DOM_VIEW_TYPE]
  // 这些组件会在 buildCustomComponent 中处理
})
```

---

## Instance 使用场景

### 场景 1：共享 Instance（BaseRN）

**适用场景：**
- 多个页面使用同一个 bundle
- 需要共享状态
- 节省内存

**代码示例：**
```typescript
// Index.ets - 创建全局 Instance
const bpInstance = await rnohCoreContext.createAndRegisterRNInstance({...});
LoadManager.bpInstance = bpInstance;

// Details.ets - 使用共享 Instance
BaseRN({
  rnInstance: LoadManager.bpInstance,  // 使用共享的
  moduleName: 'Details',
  bundlePath: 'bundle/bp/details.harmony.bundle'
})
```

**特点：**
- ✅ 多个页面共享同一个 Instance
- ✅ 一个 Instance 可以运行多个 bundle（通过 `runJSBundle`）
- ✅ 节省内存，但可能相互影响

### 场景 2：独立 Instance（BasePrecreateView）

**适用场景：**
- 每个页面需要完全隔离
- 需要预创建优化首屏速度
- 页面间不共享状态

**代码示例：**
```typescript
// PrecreateRN.ets
BasePrecreateView({
  appKey: 'Details',
  bundlePath: 'bundle/bp/details.harmony.bundle',
  forceReloadOnAppear: true  // 强制重新加载
})
```

**特点：**
- ✅ 每个页面有独立的 Instance
- ✅ 完全隔离，互不影响
- ✅ 支持预创建，优化性能
- ⚠️ 内存占用较大

### 场景 3：Metro 开发模式（MetroBaseRN）

**适用场景：**
- 开发调试
- 热重载
- 动态加载 bundle

**代码示例：**
```typescript
// Goods.ets
if (this.isMetroAvailable) {
  MetroBaseRN({
    moduleName: this.moduleName,
    initProps: this.initProps
  })
}
```

**特点：**
- ✅ 连接 Metro 服务器
- ✅ 支持热重载
- ✅ 无需预打包 bundle

---

## 预创建机制详解

### 什么是预创建

**预创建**：在当前页面显示时，在后台提前创建下一个页面需要的 Instance，并提前运行 bundle，这样下次进入页面时可以直接使用，无需等待。

### 预创建流程

#### 第一次进入页面

```
1. aboutToAppear() 被调用
   ↓
2. init() 执行
   ├── getOrCreateRNInstance()
   │   └── 池是空的，创建新 Instance A
   ├── 运行 bundle 到 Instance A
   └── shouldShow = true（显示页面）
   ↓
3. precreateInstance() 执行（异步，不阻塞显示）
   ├── 池是空的，创建新 Instance B
   ├── 在后台运行 bundle 到 Instance B
   └── 标记 status = "DONE"，存入池中
```

**时间线：**
```
t0: 进入页面
t1: 创建 Instance A，运行 bundle（耗时）
t2: bundle 运行完成，显示页面 ✅
t3: （后台）创建 Instance B，运行 bundle
t4: Instance B 准备完成，存入池中
```

#### 第二次进入页面

```
1. aboutToAppear() 被调用
   ↓
2. init() 执行
   ├── getOrCreateRNInstance()
   │   └── 从池中取出 Instance B（bundle 已运行）
   ├── status = "DONE"，跳过运行 bundle
   └── shouldShow = true（立即显示）✅
   ↓
3. precreateInstance() 执行
   └── 池是空的，再创建一个 Instance C 存入池中
```

**时间线：**
```
t0: 进入页面
t1: 从池中取出 Instance B（已准备好）
t2: 立即显示页面 ✅（无等待）
t3: （后台）创建 Instance C，运行 bundle
```

### 预创建的优势

- ✅ **首屏速度快**：第二次进入无需等待 bundle 加载
- ✅ **用户体验好**：页面切换流畅

### 预创建的问题（Skia 圆圈消失的原因）

**问题：**
- 预创建的 Instance 在**后台**运行 bundle
- 此时**没有实际的 UI surface**
- Skia 的 Canvas 组件需要 attach 到 surface 才能创建 GPU context
- 第二次进入时，虽然 Instance 的 bundle 已运行，但 Skia 组件**没有重新 attach**

**解决方案：**
- 添加 `forceReloadOnAppear` 开关
- 当 `forceReloadOnAppear = true` 时，强制重新运行 bundle
- 重新运行 bundle → React 组件重新 mount → Skia 组件重新 attach

---

## Instance 生命周期

### 生命周期阶段

```
创建阶段
  ├── createAndRegisterRNInstance()
  │   ├── 创建 JS 引擎
  │   ├── 创建 JS 线程
  │   ├── 注册 TurboModule
  │   └── 注册自定义组件
  │
运行阶段
  ├── runJSBundle()
  │   ├── 读取 bundle 文件
  │   ├── 解析模块
  │   └── 执行 JS 代码
  │
使用阶段
  ├── createSurface() - 创建页面
  ├── RNSurface 渲染
  └── 处理用户交互
  │
销毁阶段
  └── destroyAndUnregisterRNInstance()
      ├── 销毁 JS 引擎
      ├── 停止 JS 线程
      └── 清理资源
```

### 生命周期管理

#### BaseRN 的生命周期

```typescript
aboutToAppear() {
  // 1. 加载 bundle 到共享的 Instance
  LoadManager.loadBundle(this.rnInstance, this.bundlePath)
    .then(() => {
      this.isBundleReady = true;  // bundle 准备完成
    });
}

aboutToDisappear() {
  // 页面消失，但不销毁 Instance（因为是共享的）
  // Instance 继续存在，供其他页面使用
}

build() {
  // 2. bundle 准备好后才渲染
  if (this.isBundleReady) {
    RNSurface({ ... })
  }
}
```

#### BasePrecreateView 的生命周期

```typescript
aboutToAppear() {
  this.init();              // 获取或创建 Instance
  this.precreateInstance(); // 预创建下一个 Instance
}

aboutToDisappear() {
  this.cleanUp();
  // 如果 shouldDestroyRNInstance = true，销毁当前 Instance
  // 否则 Instance 可能被预创建池复用
}

private async cleanUp() {
  this.shouldShow = false;  // 隐藏页面
  if (this.shouldDestroyRNInstance) {
    // 销毁 Instance（释放资源）
    this.rnohCoreContext!.destroyAndUnregisterRNInstance(this.rnInstance);
  }
}
```

---

## 常见问题分析

### 问题 1：Skia Canvas 圆圈消失

**现象：**
- 第一次进入：圆圈正常显示
- 第二次进入：圆圈消失

**原因：**
- 预创建的 Instance 在后台运行 bundle
- Skia 组件需要 attach 到 surface 才能创建 GPU context
- 后台运行时没有 surface，GPU context 未创建
- 第二次进入时，虽然 bundle 已运行，但 Skia 组件没有重新 attach

**解决方案：**
```typescript
// 添加 forceReloadOnAppear 开关
BasePrecreateView({
  forceReloadOnAppear: true  // 强制重新运行 bundle
})

// BasePrecreateView.ets
if (shouldForceReload) {
  // 即使有预创建的 Instance，也强制重新运行 bundle
  await this.rnInstance.runJSBundle(this.jsBundleProvider);
  // 重新运行 → React 组件重新 mount → Skia 重新 attach
}
```

### 问题 2：Instance 内存泄漏

**现象：**
- 页面退出后，Instance 没有被销毁
- 内存持续增长

**原因：**
- 预创建池中的 Instance 没有被清理
- 共享的 Instance 一直存在

**解决方案：**
```typescript
aboutToDisappear() {
  if (this.shouldDestroyRNInstance) {
    // 明确销毁 Instance
    this.rnohCoreContext!.destroyAndUnregisterRNInstance(this.rnInstance);
  }
}
```

### 问题 3：Bundle 重复加载

**现象：**
- 同一个 bundle 被加载多次
- 性能浪费

**原因：**
- 没有使用 bundle 缓存机制

**解决方案：**
```typescript
// LoadManager.loadBundle 中有缓存机制
if (LoadManager.loadedBundle.has(bundlePath) && useBundleCache) {
  return Promise.resolve();  // 已加载过，直接返回
}
```

---

## Instance 机制总结

### 核心概念

1. **RNInstance = JS 执行环境**
   - 每个 Instance 有独立的 JS 引擎和线程
   - 可以运行多个 bundle
   - 可以渲染多个 Surface（页面）

2. **两种使用模式**
   - **共享模式**（BaseRN）：多个页面共享同一个 Instance
   - **独立模式**（BasePrecreateView）：每个页面有独立的 Instance

3. **预创建机制**
   - 在后台提前创建 Instance 并运行 bundle
   - 下次进入时直接使用，无需等待
   - 但可能导致某些组件（如 Skia）无法正确 attach

### 选择建议

- **使用 BaseRN（共享 Instance）**：
  - 多个页面使用同一个 bundle
  - 需要共享状态
  - 内存敏感的场景

- **使用 BasePrecreateView（独立 Instance）**：
  - 每个页面需要完全隔离
  - 需要预创建优化性能
  - 包含 Skia 等需要重新 attach 的组件时，使用 `forceReloadOnAppear: true`

---

*文档生成时间：2024年*
*基于 React Native for OpenHarmony Instance 机制分析*


