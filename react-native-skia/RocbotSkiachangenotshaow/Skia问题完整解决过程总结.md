# Skia Canvas 圆圈消失问题 - 完整解决过程总结

## 目录
1. [问题发现与初步分析](#问题发现与初步分析)
2. [问题定位过程](#问题定位过程)
3. [根因确认](#根因确认)
4. [解决方案探索与尝试](#解决方案探索与尝试)
5. [最终修复实现](#最终修复实现)
6. [修复验证](#修复验证)
7. [完整 Patch 文件说明](#完整-patch-文件说明)
8. [经验总结](#经验总结)

---

## 问题发现与初步分析

### 问题现象

**用户反馈：**
> "进入details页面，再切回details，圆圈不显示了"

**复现步骤：**
1. 打开应用，进入 Goods 页面
2. 点击"跳转 Details1"，进入 Details 页面
3. **第一次进入**：Skia Canvas 绘制的三个圆圈（cyan、magenta、yellow）**正常显示** ✅
4. 返回 Goods 页面
5. 再次点击"跳转 Details1"，进入 Details 页面
6. **第二次进入**：圆圈**消失**，页面其他内容（文字、按钮）正常显示 ❌

### 初步分析思路

**可能的原因：**
1. Skia 组件初始化失败
2. GPU Context 创建失败
3. Surface 绑定问题
4. Instance 复用导致的状态问题

**代码检查：**

```tsx
// DetailsMainPage.tsx - React Native 侧
function AppDetails() {
  return (
    <View style={styles.container}>
      <Canvas style={{ width, height }}>
        <Circle cx={r} cy={r} r={r} color="cyan" />
        <Circle cx={width - r} cy={r} r={r} color="magenta" />
        <Circle cx={width / 2} cy={width - r} r={r} color="yellow" />
      </Canvas>
      {/* 其他内容正常显示 */}
    </View>
  );
}
```

```typescript
// PrecreateRN.ets - ArkTS 侧
BasePrecreateView({
  appKey: 'Details',
  bundlePath: 'bundle/bp/details.harmony.bundle',
  initialProps: this.initProps
})
```

**初步判断：**
- JS 代码看起来正常
- 第一次进入能显示，说明代码逻辑没问题
- 第二次进入不显示，可能是 Instance 复用导致的问题

---

## 问题定位过程

### 第一步：添加日志验证 Skia 组件是否重新创建

**目标：** 确认第二次进入时，Skia 的原生组件（RNCSkiaDomView）是否被重新创建

**尝试 1：在 buildCustomComponent 中直接加日志**

```typescript
// LoadBundle.ets
@Builder
export function buildCustomComponent(ctx: ComponentBuilderContext) {
  if (ctx.componentName === SKIA_DOM_VIEW_TYPE) {
    console.log('[RNCSkiaDomView] build component, tag=' + ctx.tag.toString());
    RNCSkiaDomView({ ctx: ctx.rnComponentContext, tag: ctx.tag })
  }
}
```

**结果：** ❌ **编译错误**
```
Error: 'console.log("1111111111");' does not meet UI component syntax.
At File: D:/s3/a/b/c/d/e/entry/src/main/ets/rn/LoadBundle.ets:43:7
```

**原因：** ArkUI 的 `@Builder` 函数中不能直接使用 `console.log`，会被解析为 UI 组件

**尝试 2：在 Builder 外部添加日志函数**

```typescript
// LoadBundle.ets
function logCustomComponentLifecycle(ctx: ComponentBuilderContext) {
  if (ctx.componentName === SKIA_DOM_VIEW_TYPE) {
    console.log('[RNCSkiaDomView] build component, tag=' + ctx.tag.toString());
  }
}

@Builder
export function buildCustomComponent(ctx: ComponentBuilderContext) {
  logCustomComponentLifecycle(ctx);  // 在 Builder 外部调用
  Stack() {
    if (ctx.componentName === SKIA_DOM_VIEW_TYPE) {
      RNCSkiaDomView({ ctx: ctx.rnComponentContext, tag: ctx.tag })
    }
  }
}
```

**结果：** ✅ **编译通过，日志可以正常输出**

**验证结果：**
- **第一次进入 Details**：✅ 打印 `[RNCSkiaDomView] build component, tag=2`
- **第二次进入 Details**：❌ **没有打印日志**

**结论：** 第二次进入时，`buildCustomComponent` 根本没有被调用，说明 Skia 组件没有被重新创建

---

### 第二步：分析 BasePrecreateView 的执行流程

**查看 BasePrecreateView 的代码：**

```typescript
// BasePrecreateView.ets
private async init() {
  const instance = await this.getOrCreateRNInstance();
  this.rnInstance = instance.rnInstance;
  const jsBundleExecutionStatus = instance.status;
  
  if (this.jsBundleProvider && jsBundleExecutionStatus === undefined) {
    // 新 Instance，需要运行 bundle
    await this.rnInstance.runJSBundle(this.jsBundleProvider);
    this.shouldShow = true;
    return;
  }
  
  if (jsBundleExecutionStatus !== "DONE") {
    // bundle 还在加载中，等待完成
    this.rnInstance.subscribeToLifecycleEvents("JS_BUNDLE_EXECUTION_FINISH", () => {
      this.shouldShow = true;
    });
  } else {
    // bundle 已经加载完成（预创建的 Instance）
    this.shouldShow = true;  // ❌ 直接显示，没有重新运行 bundle
  }
}
```

**发现：**
- **第一次进入**：`jsBundleExecutionStatus = undefined` → 运行 bundle → 显示 ✅
- **第二次进入**：`jsBundleExecutionStatus = "DONE"` → **跳过运行 bundle** → 直接显示 ❌

**问题确认：** 预创建的 Instance 在后台已经运行了 bundle，第二次进入时直接复用，没有重新运行 bundle

---

### 第三步：理解预创建机制

**查看 precreateInstance 方法：**

```typescript
private async precreateInstance() {
  if (instances.length === 0) {
    // 创建新的 Instance
    const rnInstance = await rnohCoreContext.createAndRegisterRNInstance({...});
    
    // 在后台运行 bundle（此时没有 UI surface）
    await rnInstance.runJSBundle(provider);
    
    // 标记为已完成
    instance.status = "DONE";
    instances.add(instance);  // 存入预创建池
  }
}
```

**预创建流程：**
```
第一次进入：
1. init() → 创建 Instance A → 运行 bundle → 显示页面
2. precreateInstance() → （后台）创建 Instance B → 运行 bundle → 存入池

第二次进入：
1. init() → 从池中取出 Instance B（status = "DONE"）
2. 检查 status = "DONE" → 跳过运行 bundle → 直接显示
```

**问题根源：**
- 预创建的 Instance B 在**后台**运行 bundle 时，**没有 UI surface**
- Skia 的 Canvas 组件需要 attach 到 surface 才能创建 GPU context
- 后台运行时，Skia 组件无法 attach，GPU context 未创建
- 第二次进入时，虽然有了 surface，但**没有重新运行 bundle**，Skia 组件**没有重新 attach**

---

### 第四步：验证分析

**添加更多日志：**

```typescript
// BasePrecreateView.ets
private async init() {
  console.log("BasePrecreateView Begin ======================================================= ");
  const instance = await this.getOrCreateRNInstance();
  console.log("BasePrecreateView init RNInstance ID = ", this.rnInstance.getId());
  console.log("BasePrecreateView status = ", instance.status);
  
  if (jsBundleExecutionStatus === "DONE") {
    console.log("BasePrecreateView 预创建的 RNInstance 已完成 JSBundle 加载，直接展示页面");
    // ❌ 这里没有重新运行 bundle
  }
}
```

**日志输出：**
```
第一次进入：
BasePrecreateView Begin
BasePrecreateView init RNInstance ID = 5
BasePrecreateView status = undefined
BasePrecreateView RNInstance 未预加载
[RNCSkiaDomView] build component, tag=2  ✅

第二次进入：
BasePrecreateView Begin
BasePrecreateView init RNInstance ID = 6
BasePrecreateView status = DONE
BasePrecreateView 预创建的 RNInstance 已完成 JSBundle 加载，直接展示页面
（没有 [RNCSkiaDomView] 日志）❌
```

**确认：** 第二次进入时，确实跳过了 bundle 运行，导致 Skia 组件没有重新创建

---

## 根因确认

### 根本原因

**预创建机制与 Skia 组件的冲突：**

1. **预创建机制的设计**：
   - 在后台提前创建 Instance 并运行 bundle
   - 下次进入时直接使用，无需等待

2. **Skia 组件的特性**：
   - 需要在 surface attach 时创建 GPU context
   - 必须在有 UI surface 的情况下才能正确初始化

3. **冲突点**：
   - 预创建时：没有 UI surface → Skia 无法 attach → GPU context 未创建
   - 第二次进入：有 UI surface，但没有重新运行 bundle → Skia 没有重新 attach → GPU context 仍然不存在

### 时序图分析

```
第一次进入（正常）：
t0: 进入页面
t1: 创建 Instance A
t2: 运行 bundle
t3: 创建 RNSurface（有 UI surface）✅
t4: Skia Canvas attach 到 surface ✅
t5: GPU Context 创建 ✅
t6: 圆圈显示 ✅

预创建阶段（问题发生）：
t7: （后台）创建 Instance B
t8: 运行 bundle
t9: React 组件创建（包括 Canvas）
t10: 尝试 attach 到 surface ❌（没有 surface）
t11: GPU Context 创建失败 ❌
t12: Instance B 标记为 "DONE"，存入池

第二次进入（问题显现）：
t13: 从池中取出 Instance B
t14: 检查 status = "DONE"，跳过重新运行 bundle
t15: 创建 RNSurface（有 UI surface）
t16: 但 Skia Canvas 没有重新 attach ❌
t17: GPU Context 仍然不存在 ❌
t18: 圆圈不显示 ❌
```

---

## 解决方案探索与尝试

### 方案 1：强制重新运行 bundle（最终采用）

**思路：** 添加开关，强制每次进入都重新运行 bundle

**实现：**
```typescript
// BasePrecreateView.ets
public forceReloadOnAppear: boolean = false;

private async init() {
  const shouldForceReload = this.shouldForceReload();
  
  // 即使 status = "DONE"，如果 forceReloadOnAppear = true，也强制重新运行
  if (this.jsBundleProvider && (jsBundleExecutionStatus === undefined || shouldForceReload)) {
    if (shouldForceReload) {
      console.log("BasePrecreateView force reload enabled");
    }
    await this.rnInstance.runJSBundle(this.jsBundleProvider);  // 强制重新运行
    this.shouldShow = true;
    return;
  }
}
```

**优点：**
- ✅ 彻底解决问题
- ✅ 实现简单
- ✅ 适用于所有需要重新 attach 的组件

**缺点：**
- ⚠️ 牺牲了预创建的性能优势

### 方案 2：检测并重新 attach Skia 组件（未实施）

**思路：** 在 surface 创建时，检测 Skia 组件并重新 attach

**问题：**
- 需要深入 Skia 组件内部实现
- 可能影响其他组件
- 实现复杂度高

### 方案 3：禁用预创建（未采用）

**思路：** 对于包含 Skia 的页面，完全禁用预创建

**问题：**
- 失去了预创建的所有优势
- 不够灵活

**最终选择：** 方案 1（强制重新运行 bundle）

---

## 最终修复实现

### 修复步骤

#### 步骤 1：修改 BasePrecreateView.ets

**添加 forceReloadOnAppear 属性：**
```typescript
export struct BasePrecreateView {
  public appKey: string = 'app_name';
  public bundlePath: string = 'bundle.harmony.js';
  public initialProps: SurfaceProps = {};
  public forceReloadOnAppear: boolean = false;  // 新增属性
  
  private shouldForceReload(): boolean {
    return this.forceReloadOnAppear;
  }
}
```

**修改 init() 方法：**
```typescript
private async init() {
  const jsBundleExecutionStatus = instance.status;
  const shouldForceReload = this.shouldForceReload();  // 检查开关
  
  // 关键修改：即使 status = "DONE"，如果 forceReloadOnAppear = true，也强制重新运行
  if (this.jsBundleProvider && (jsBundleExecutionStatus === undefined || shouldForceReload)) {
    if (shouldForceReload) {
      console.log("BasePrecreateView force reload enabled for appKey =", this.appKey);
    } else {
      console.log("BasePrecreateView RNInstance 未预加载");
    }
    await this.rnInstance.runJSBundle(this.jsBundleProvider);  // 强制重新运行
    this.shouldShow = true;
    return;
  }
  // ... 其他逻辑保持不变
}
```

**修改 getOrCreateRNInstance() 方法：**
```typescript
private async getOrCreateRNInstance(): Promise<CachedInstance> {
  this.shouldDestroyRNInstance = true;
  const shouldSkipCache: boolean = this.shouldForceReload();  // 检查开关
  
  // 如果 forceReloadOnAppear = true，跳过预创建池
  if (!shouldSkipCache && this.rnInstances.length > 0) {
    const instance = this.rnInstances.removeByIndex(0);
    return Promise.resolve(instance);
  } else {
    // 创建新的 Instance
    const instance = await this.rnohCoreContext!.createAndRegisterRNInstance({...});
    return { rnInstance: instance };
  }
}
```

**修改 precreateInstance() 方法：**
```typescript
private async precreateInstance() {
  // 如果 forceReloadOnAppear = true，不预创建
  if (instances.length === 0 && !this.shouldForceReload()) {
    // 预创建逻辑
  }
}
```

#### 步骤 2：修改 PrecreateRN.ets

**添加 @Prop 并透传：**
```typescript
@Component
export default struct PrecreateRN {
  @Prop forceReloadOnAppear: boolean = false;  // 新增属性
  
  build() {
    BasePrecreateView({
      appKey: this.moduleName,
      bundlePath: this.bundlePath,
      initialProps: this.initProps,
      forceReloadOnAppear: this.forceReloadOnAppear  // 透传给 BasePrecreateView
    })
  }
}
```

#### 步骤 3：修改 Goods.ets

**在 PageMap 中启用强制重新加载：**
```typescript
@Builder
PageMap(name: string, param: string) {
  if (name === 'Details' || name === 'Details2') {
    PrecreateRN({ forceReloadOnAppear: true })  // 启用强制重新加载
  }
  // ...
}
```

**修复 Detail2 的 pushPath：**
```typescript
// Goods.ets
} else if (path === "pages/Details2") {
  // 修复前：this.pagePathsGoods.pushPath({ name: "Details", param: "detail2" });
  // 修复后：
  this.pagePathsGoods.pushPath({ name: "Details2", param: "detail2" });
}
```

#### 步骤 4：修复 Detail2 跳转路径问题

**问题发现：**
- Detail2 按钮跳转到 `'pages/PrecreateRN'`，而不是 `'pages/Details2'`
- 导致 `PageMap` 匹配到错误的分支

**修复：**
```typescript
// GoodsMainPage.tsx
<GoodsButton
  buttonText={'•跳转 Details2'}
  onPress={() => {
    // 修复前：SampleTurboModule.pushStringToHarmony('pages/PrecreateRN', 1);
    // 修复后：
    SampleTurboModule.pushStringToHarmony('pages/Details2', 1);
  }}
/>
```

### 修复后的流程

**第二次进入 Details（修复后）：**
```
1. aboutToAppear() 被调用
2. init() 执行
   ├── getOrCreateRNInstance() → 从池中取出预创建的 Instance
   ├── jsBundleExecutionStatus = "DONE"
   ├── shouldForceReload() = true（因为 forceReloadOnAppear = true）
   ├── 关键：即使 status = "DONE"，也执行 runJSBundle ✅
   ├── await this.rnInstance.runJSBundle(this.jsBundleProvider) ✅
   └── this.shouldShow = true
3. build() 执行
   ├── RNSurface 创建（有 UI surface）✅
   ├── bundle 重新运行 → React 组件重新 mount ✅
   ├── Skia Canvas 重新创建 ✅
   ├── Skia Canvas attach 到 surface ✅
   ├── GPU Context 重新创建 ✅
   └── 圆圈正常显示 ✅
```

---

## 修复验证

### 验证步骤

1. **第一次进入 Details**
   - ✅ 圆圈正常显示
   - ✅ 日志显示：`[RNCSkiaDomView] build component, tag=2`
   - ✅ 日志显示：`BasePrecreateView RNInstance 未预加载`

2. **返回 Goods 页面**
   - ✅ 正常返回

3. **第二次进入 Details**
   - ✅ 圆圈正常显示（修复后）
   - ✅ 日志显示：`[RNCSkiaDomView] build component, tag=2`（修复后）
   - ✅ 日志显示：`BasePrecreateView force reload enabled for appKey = Details`
   - ✅ 日志显示：`runJSBundle1111111111111 RNInstance`（确认重新运行了 bundle）

4. **Detail2 页面**
   - ✅ 页面正常显示（修复跳转路径后）
   - ✅ 圆圈正常显示

### 验证结果

| 场景 | 修复前 | 修复后 |
|------|--------|--------|
| 第一次进入 Details | ✅ 正常 | ✅ 正常 |
| 第二次进入 Details | ❌ 圆圈消失 | ✅ 正常 |
| Detail2 页面 | ❌ 不显示 | ✅ 正常 |

---

## 完整 Patch 文件说明

### skia_crash.patch 包含的修改

#### 1. 添加 bpInstance2 支持

**文件：** `Index.ets`

**修改内容：**
- 创建 `bpInstance2` Instance
- 创建对应的 `ctxBp2` RNComponentContext
- 保存到 `LoadManager.bpInstance2`

**目的：** 支持 Details 和 Details2 使用不同的 Instance

#### 2. 注册 RNSkiaPackage

**文件：** `RNPackagesFactory.ets`

**修改内容：**
```typescript
import { RNSkiaPackage } from '@react-native-oh-tpl/react-native-skia';

export function createRNPackages(ctx: RNPackageContext): RNPackage[] {
  return [
    new SampleTurboModulePackage(ctx),
    new ViewPagerPackage(ctx),
    new RNSkiaPackage(ctx)  // 新增
  ];
}
```

**文件：** `PackageProvider.cpp`

**修改内容：**
```cpp
#include "SkiaPackage.h"

std::vector<std::shared_ptr<RNPackage>> getPackages(RNPackageContext& ctx) {
    return {
        std::make_shared<SampleTurboModulePackage>(ctx),
        std::make_shared<ViewPagerPackage>(ctx),
        std::make_shared<SkiaPackage>(ctx),  // 新增
    };
}
```

**目的：** 注册 Skia 包，使 Skia 组件可用

#### 3. 修改 Details.ets 支持选择 Instance

**文件：** `Details.ets`

**修改内容：**
```typescript
aboutToAppear() {
  if (this.name.endsWith("1")) {
    this.instance = LoadManager.bpInstance2;  // Details1 使用 bpInstance2
  } else {
    this.instance = LoadManager.bpInstance;   // Details2 使用 bpInstance
  }
  // ...
}
```

**目的：** 根据页面名称选择不同的 Instance

#### 4. 添加 forceReloadOnAppear 开关（核心修复）

**文件：** `BasePrecreateView.ets`

**修改内容：**
- 添加 `public forceReloadOnAppear: boolean = false;` 属性
- 修改 `init()` 方法，支持强制重新加载
- 修改 `getOrCreateRNInstance()` 方法，支持跳过预创建池
- 修改 `precreateInstance()` 方法，支持禁用预创建
- 添加 `shouldForceReload()` 方法

**文件：** `PrecreateRN.ets`

**修改内容：**
- 添加 `@Prop forceReloadOnAppear: boolean = false;` 属性
- 透传给 `BasePrecreateView`

**文件：** `Goods.ets`

**修改内容：**
- 在 `PageMap` 中为 Details/Details2 传入 `forceReloadOnAppear: true`
- 修复 Detail2 的 `pushPath`，使用正确的 name

**文件：** `GoodsMainPage.tsx`

**修改内容：**
- 修复 Detail2 按钮的跳转路径，从 `'pages/PrecreateRN'` 改为 `'pages/Details2'`

#### 5. 添加日志支持（调试用）

**文件：** `LoadBundle.ets`

**修改内容：**
- 添加 `logCustomComponentLifecycle()` 函数
- 在 `buildCustomComponent` 中调用日志函数

**目的：** 用于调试，确认 Skia 组件是否被重新创建

### Patch 文件结构

```
skia_crash.patch
├── Index.ets (添加 bpInstance2)
├── RNPackagesFactory.ets (注册 RNSkiaPackage)
├── PackageProvider.cpp (注册 SkiaPackage)
├── Details.ets (支持选择 Instance)
├── BasePrecreateView.ets (添加 forceReloadOnAppear 开关) ⭐ 核心修复
├── PrecreateRN.ets (透传 forceReloadOnAppear)
├── Goods.ets (启用强制重新加载)
├── GoodsMainPage.tsx (修复跳转路径)
└── LoadBundle.ets (添加日志支持)
```

---

## 经验总结

### 问题分析的关键点

1. **日志的重要性**
   - 通过添加日志，确认了 Skia 组件没有被重新创建
   - 发现了预创建机制的执行流程
   - 快速定位问题根源

2. **理解组件生命周期**
   - Skia 组件需要在 surface attach 时创建 GPU context
   - 预创建时没有 surface，导致初始化失败
   - 某些组件必须在有 UI surface 的情况下才能正确初始化

3. **预创建机制的局限性**
   - 预创建机制不适用于所有组件
   - 需要 attach 到 surface 的组件不适合预创建
   - 性能优化需要考虑组件的特性

### 解决方案的设计思路

1. **可配置的开关**
   - 通过 `forceReloadOnAppear` 开关，灵活控制是否强制重新加载
   - 不影响其他不需要强制重新加载的页面
   - 保持向后兼容（默认 `forceReloadOnAppear = false`）

2. **最小化修改**
   - 只修改必要的代码
   - 保持代码结构清晰
   - 易于维护和扩展

3. **性能与正确性的权衡**
   - 牺牲了预创建的性能优势
   - 但保证了功能的正确性
   - 对于包含 Skia 的页面，这是必要的权衡

### 调试技巧

1. **分步骤验证**
   - 先验证组件是否被创建（日志）
   - 再验证 bundle 是否被运行（日志）
   - 最后验证组件是否正确 attach（功能验证）

2. **理解架构**
   - 理解预创建机制的工作原理
   - 理解 Skia 组件的初始化流程
   - 理解 Instance 和 Surface 的关系

3. **系统化分析**
   - 从现象到根因
   - 从根因到解决方案
   - 从解决方案到实现

### 后续优化建议

1. **组件级别的检测**
   - 自动检测 bundle 中是否包含需要重新 attach 的组件
   - 自动决定是否使用预创建机制
   - 减少手动配置

2. **Skia 组件的优化**
   - 在 surface 创建时，检查并重新 attach Skia 组件
   - 避免强制重新运行整个 bundle
   - 提升性能

3. **预创建机制的改进**
   - 区分"可以预创建"和"需要重新 attach"的组件
   - 只对可以预创建的组件使用预创建机制
   - 提供更细粒度的控制

---

## 问题分析时间线

```
1. 问题发现
   ↓ 用户反馈：第二次进入 Details，圆圈不显示
   
2. 初步分析
   ↓ 可能的原因：Instance 复用、GPU Context、Surface 绑定
   
3. 添加日志验证
   ↓ 确认：第二次进入时，Skia 组件没有被重新创建
   
4. 分析 BasePrecreateView 执行流程
   ↓ 发现：预创建的 Instance 跳过重新运行 bundle
   
5. 理解预创建机制
   ↓ 确认：预创建时没有 UI surface，Skia 无法 attach
   
6. 确认根因
   ↓ 预创建机制与 Skia 组件的冲突
   
7. 探索解决方案
   ↓ 方案 1：强制重新运行 bundle（最终采用）
   
8. 实现修复
   ↓ 添加 forceReloadOnAppear 开关
   
9. 修复 Detail2 跳转路径问题
   ↓ 确保 Detail2 页面正常显示
   
10. 验证修复
    ↓ 圆圈正常显示，问题解决
```

---

## 结论

### 问题本质

预创建机制在后台运行 bundle 时，**没有 UI surface**，导致 Skia 等需要 attach 到 surface 的组件无法正确初始化。第二次进入时，虽然有了 surface，但组件没有重新 attach，导致不显示。

### 解决方案

通过 `forceReloadOnAppear` 开关，强制每次进入都重新运行 bundle，确保组件在 surface 存在时重新 mount 和 attach。

### 关键经验

1. **预创建机制不适用于所有场景**：需要 attach 到 surface 的组件不适合预创建
2. **组件生命周期的重要性**：某些组件需要在 surface 存在时才能正确初始化
3. **性能与正确性的权衡**：在保证功能正确性的前提下，可以牺牲部分性能优化
4. **日志调试的重要性**：通过日志快速定位问题根源
5. **系统化分析**：从现象到根因，从根因到解决方案

---

## 修复文件清单

### 修改的文件

1. **b/c/d/e/entry/src/main/ets/rn/BasePrecreateView.ets**
   - 添加 `forceReloadOnAppear` 属性
   - 修改 `init()` 方法，支持强制重新加载
   - 修改 `getOrCreateRNInstance()` 方法，支持跳过预创建池
   - 修改 `precreateInstance()` 方法，支持禁用预创建
   - 添加 `shouldForceReload()` 方法

2. **b/c/d/e/entry/src/main/ets/pages/PrecreateRN.ets**
   - 添加 `@Prop forceReloadOnAppear` 属性
   - 透传给 `BasePrecreateView`

3. **b/c/d/e/entry/src/main/ets/pages/Goods.ets**
   - 修改 `PageMap`，为 Details/Details2 传入 `forceReloadOnAppear: true`
   - 修复 Detail2 的 `pushPath`，使用正确的 name

4. **b/c/d/SampleProject/MainProject/src/bundles/Goods/GoodsMainPage.tsx**
   - 修复 Detail2 按钮的跳转路径，从 `'pages/PrecreateRN'` 改为 `'pages/Details2'`

5. **b/c/d/e/entry/src/main/ets/rn/LoadBundle.ets**
   - 添加 `logCustomComponentLifecycle()` 函数（用于调试）
   - 在 `buildCustomComponent` 中调用日志函数

6. **b/c/d/e/entry/src/main/ets/pages/Index.ets**
   - 添加 `bpInstance2` 的创建和注册

7. **b/c/d/e/entry/src/main/ets/rn/RNPackagesFactory.ets**
   - 注册 `RNSkiaPackage`

8. **b/c/d/e/entry/src/main/cpp/PackageProvider.cpp**
   - 注册 `SkiaPackage`

9. **b/c/d/e/entry/src/main/ets/pages/Details.ets**
   - 支持根据页面名称选择不同的 Instance

### Patch 文件

- **skia_crash.patch** - 包含所有修改的完整 patch 文件
- **skia_circle_fix.patch** - 核心修复的 patch 文件（forceReloadOnAppear 相关）

---

*分析时间：2024年*
*问题状态：已修复 ✅*
*修复版本：包含 forceReloadOnAppear 开关的版本*
*Patch 文件：skia_crash.patch*
