# RNInstance 创建时机和关联关系详解

## 核心概念

### RNInstance 是什么？

**RNInstance** 是 React Native 在鸿蒙系统中的**运行实例**，每个 RNInstance 包含：
- 独立的 JS 引擎（Hermes/JSVM）
- 独立的 JS 线程
- 独立的组件树和状态管理

### cpInstance、bpInstance 是什么？

`cpInstance`、`bpInstance`、`bpInstance2` 都是 **RNInstance 类型的变量**，它们只是给不同的 RNInstance 起的**别名**，用于区分不同的业务场景：

- `cpInstance`: CP 业务模块的 RNInstance
- `bpInstance`: BP 业务模块的 RNInstance 1
- `bpInstance2`: BP 业务模块的 RNInstance 2
- `metroInstance`: Metro 开发模式下的 RNInstance

**它们本质上都是 RNInstance，只是用途不同。**

---

## 创建时机详解

### 1. 应用启动流程

```
应用启动
  ↓
EntryAbility.onCreate()  // 创建 RNOHCoreContext
  ↓
Index.aboutToAppear()     // 入口页面显示
  ↓
loadMetroBundle()         // 尝试连接 Metro
  ↓
register()                // 创建业务 Instance
```

### 2. metroInstance 的创建

**位置**: `LoadManager.loadMetroBundle()`

**时机**: 应用启动时，在 `Index.aboutToAppear()` 中调用

**代码流程**:

```typescript
// Index.ets - aboutToAppear()
aboutToAppear() {
  if (!this.rnohCoreContext) {
    return;
  }
  this.loadMetroBundle()  // ← 这里开始
}

// Index.ets - loadMetroBundle()
loadMetroBundle() {
  LoadManager.loadMetroBundle(this.getUIContext()).then((flag: boolean) => {
    // ...
    this.register();  // ← Metro 加载完成后，创建业务 Instance
  })
}

// LoadBundle.ets - loadMetroBundle()
public static async loadMetroBundle(uiContext: UIContext): Promise<boolean> {
  const rnohCoreContext: RNOHCoreContext | undefined = AppStorage.get('RNOHCoreContext');
  if (LoadManager.shouldResetMetroInstance && rnohCoreContext) {
    // ← 创建 metroInstance
    LoadManager.metroInstance = await rnohCoreContext.createAndRegisterRNInstance({
      createRNPackages: createRNPackages,
      enableNDKTextMeasuring: true,
      enableBackgroundExecutor: false,
      enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
      arkTsComponentNames: arkTsComponentNames
    });
    
    // 加载 Metro bundle
    const provider = new MetroJSBundleProvider();
    await LoadManager.metroInstance.runJSBundle(provider);
    
    LoadManager.shouldResetMetroInstance = false;
    return true;
  }
  return true;
}
```

**创建条件**:
- `shouldResetMetroInstance === true`（首次创建或重置后）
- `rnohCoreContext` 存在

**用途**: 开发模式下连接 Metro 服务器，支持热更新

---

### 3. cpInstance、bpInstance、bpInstance2 的创建

**位置**: `Index.register()`

**时机**: `loadMetroBundle()` 完成后调用

**代码流程**:

```typescript
// Index.ets - register()
async register(): Promise<Map<string, RNInstance>> {
  if (!this.rnohCoreContext) {
    return new Map();
  }

  // 如果 Metro 可用，不创建业务 Instance
  if (this.isMetroAvailable) {
    this.isBundleReady = true;
    this.subscribeReload();
    this.subscribeLogBox();
    return new Map();
  }

  // ← 创建 cpInstance
  const cpInstance: RNInstance = await this.rnohCoreContext.createAndRegisterRNInstance({
    createRNPackages: createRNPackages,
    enableNDKTextMeasuring: true,
    enableBackgroundExecutor: false,
    enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
    arkTsComponentNames: arkTsComponentNames
  });
  
  // ← 创建 bpInstance
  const bpInstance: RNInstance = await this.rnohCoreContext.createAndRegisterRNInstance({
    createRNPackages: createRNPackages,
    enableNDKTextMeasuring: true,
    enableBackgroundExecutor: false,
    enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
    arkTsComponentNames: arkTsComponentNames
  });
  
  // ← 创建 bpInstance2
  const bpInstance2: RNInstance = await this.rnohCoreContext.createAndRegisterRNInstance({
    createRNPackages: createRNPackages,
    enableNDKTextMeasuring: true,
    enableBackgroundExecutor: false,
    enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
    arkTsComponentNames: arkTsComponentNames
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
```

**创建条件**:
- `isMetroAvailable === false`（生产模式或 Metro 不可用）
- `rnohCoreContext` 存在

**用途**:
- `cpInstance`: 用于 CP 业务模块
- `bpInstance`: 用于 BP 业务模块（Details 页面等）
- `bpInstance2`: 用于 BP 业务模块的另一个实例（Details2 页面等）

---

### 4. BasePrecreateView 中的动态创建

**位置**: `BasePrecreateView.getOrCreateRNInstance()`

**时机**: 页面显示时（`aboutToAppear()`）

**代码流程**:

```typescript
// BasePrecreateView.ets - aboutToAppear()
aboutToAppear() {
  this.init();           // ← 获取或创建 RNInstance
  this.precreateInstance();  // ← 预创建下一个 RNInstance
}

// BasePrecreateView.ets - getOrCreateRNInstance()
private async getOrCreateRNInstance(): Promise<CachedInstance> {
  // 尝试从缓存中获取预创建的 RNInstance
  if (!shouldSkipCache && this.rnInstances.length > 0) {
    const instance = this.rnInstances.removeByIndex(0);
    return Promise.resolve(instance);
  } else {
    // ← 如果没有缓存，创建新的 RNInstance
    const instance = await this.rnohCoreContext!.createAndRegisterRNInstance({
      createRNPackages: createRNPackages,
      enableNDKTextMeasuring: true,
      enableBackgroundExecutor: false,
      enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
      arkTsComponentNames: arkTsComponentNames
    });
    return { rnInstance: instance };
  }
}

// BasePrecreateView.ets - precreateInstance()
private async precreateInstance() {
  // 预创建下一个 RNInstance（用于性能优化）
  if (instances.length === 0 && !this.shouldForceReload()) {
    const rnInstance = await rnohCoreContext.createAndRegisterRNInstance({
      createRNPackages: createRNPackages,
      enableNDKTextMeasuring: true,
      enableBackgroundExecutor: false,
      enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
      arkTsComponentNames: arkTsComponentNames
    });
    instances.add({ rnInstance: rnInstance, status: undefined });
    await rnInstance.runJSBundle(provider);
    instance.status = "DONE";
  }
}
```

**创建条件**:
- 页面首次显示，且缓存中没有可用的 RNInstance
- 预创建机制：页面消失时预创建下一个 RNInstance

**用途**: 页面级别的 RNInstance 管理，支持预创建优化

---

## 关联关系图

```
RNInstance (基类/接口)
  │
  ├── metroInstance (LoadManager.metroInstance)
  │   └── 用途: Metro 开发模式
  │   └── 创建时机: Index.loadMetroBundle()
  │
  ├── cpInstance (LoadManager.cpInstance)
  │   └── 用途: CP 业务模块
  │   └── 创建时机: Index.register()
  │
  ├── bpInstance (LoadManager.bpInstance)
  │   └── 用途: BP 业务模块（Details）
  │   └── 创建时机: Index.register()
  │
  ├── bpInstance2 (LoadManager.bpInstance2)
  │   └── 用途: BP 业务模块（Details2）
  │   └── 创建时机: Index.register()
  │
  └── BasePrecreateView 中的动态 Instance
      └── 用途: 页面级别的 Instance
      └── 创建时机: BasePrecreateView.aboutToAppear()
```

---

## 完整创建时序图

```
应用启动
  │
  ├─ EntryAbility.onCreate()
  │   └─ 创建 RNOHCoreContext
  │   └─ 存储到 AppStorage
  │
  ├─ Index.aboutToAppear()
  │   │
  │   ├─ loadMetroBundle()
  │   │   │
  │   │   └─ LoadManager.loadMetroBundle()
  │   │       └─ 创建 metroInstance ✅
  │   │       └─ 加载 Metro bundle
  │   │
  │   └─ register()
  │       │
  │       ├─ 如果 Metro 可用
  │       │   └─ 不创建业务 Instance
  │       │
  │       └─ 如果 Metro 不可用
  │           ├─ 创建 cpInstance ✅
  │           ├─ 创建 bpInstance ✅
  │           ├─ 创建 bpInstance2 ✅
  │           └─ 加载 basic bundle 到 cpInstance
  │
  └─ 页面显示（如 Details）
      │
      └─ BasePrecreateView.aboutToAppear()
          │
          ├─ init()
          │   └─ getOrCreateRNInstance()
          │       ├─ 从缓存获取（如果有）
          │       └─ 或创建新的 RNInstance ✅
          │
          └─ precreateInstance()
              └─ 预创建下一个 RNInstance ✅
```

---

## 关键点总结

### 1. RNInstance 和 cpInstance/bpInstance 的关系

- **cpInstance、bpInstance 都是 RNInstance 类型**
- 它们只是**变量名**，用于区分不同的业务场景
- 每个都是通过 `createAndRegisterRNInstance()` 创建的**独立实例**

### 2. 创建时机

| Instance | 创建时机 | 创建位置 | 条件 |
|----------|---------|---------|------|
| `metroInstance` | 应用启动 | `LoadManager.loadMetroBundle()` | Metro 可用且首次创建 |
| `cpInstance` | 应用启动 | `Index.register()` | Metro 不可用 |
| `bpInstance` | 应用启动 | `Index.register()` | Metro 不可用 |
| `bpInstance2` | 应用启动 | `Index.register()` | Metro 不可用 |
| `BasePrecreateView` 的 Instance | 页面显示 | `BasePrecreateView.getOrCreateRNInstance()` | 页面首次显示或缓存为空 |

### 3. 存储位置

```typescript
// LoadManager.ets
export class LoadManager {
  public static metroInstance: RNInstance;  // ← 存储 metroInstance
  public static cpInstance: RNInstance;      // ← 存储 cpInstance
  public static bpInstance: RNInstance;      // ← 存储 bpInstance
  public static bpInstance2: RNInstance;    // ← 存储 bpInstance2
}

// BasePrecreateView.ets
@StorageLink('rnInstances') rnInstances: ArrayList<CachedInstance> = new ArrayList();
// ← 存储页面级别的预创建 Instance
```

### 4. 使用场景

- **metroInstance**: 开发模式，支持热更新
- **cpInstance**: CP 业务模块（如 HomePage）
- **bpInstance**: BP 业务模块（如 Details）
- **bpInstance2**: BP 业务模块的另一个实例（如 Details2）
- **BasePrecreateView 的 Instance**: 页面级别的动态 Instance，支持预创建优化

---

## 注意事项

1. **Instance 是独立的**: 每个 RNInstance 都有独立的 JS 引擎和线程，互不影响

2. **创建顺序**: 
   - 先创建 `metroInstance`（如果 Metro 可用）
   - 再创建业务 Instance（`cpInstance`、`bpInstance`、`bpInstance2`）

3. **Metro 模式 vs 生产模式**:
   - Metro 模式：只创建 `metroInstance`，不创建业务 Instance
   - 生产模式：创建业务 Instance，不创建 `metroInstance`

4. **预创建机制**: `BasePrecreateView` 会在页面消失时预创建下一个 Instance，提升性能





