# Skia Canvas 圆圈消失问题 - 完整对话记录

## 对话时间线

---

## 第一阶段：问题发现与分析

### 用户问题 1
> 进入details页面，再切回details，圆圈不显示了，你给我分析这个原因，然后再把这一串流程给我讲下，能不能快点别那么慢

**问题现象：**
- 第一次进入 Details 页面：Skia Canvas 绘制的圆圈正常显示 ✅
- 第二次进入 Details 页面：圆圈消失 ❌

**初步分析：**
- 可能是 Instance 复用导致的问题
- Skia 组件需要 attach 到 surface 才能创建 GPU context
- 预创建机制可能在后台运行 bundle 时没有 UI surface

---

### 用户问题 2
> 你先帮我把旧的日志加一下，然后我去看一下效果

**操作：** 添加日志验证 Skia 组件是否重新创建

**尝试 1：在 buildCustomComponent 中直接加日志**
- ❌ 编译错误：`'console.log("1111111111");' does not meet UI component syntax.`

**尝试 2：在 Builder 外部添加日志函数**
- ✅ 编译通过
- ✅ 日志可以正常输出

**验证结果：**
- 第一次进入：✅ 打印 `[RNCSkiaDomView] build component, tag=2`
- 第二次进入：❌ 没有打印日志

**结论：** 第二次进入时，Skia 组件没有被重新创建

---

### 用户问题 3
> 你可以在 RNCSkiaDomView 层加日志验证：第一次进入会触发 create，第二次进入直接复用旧 surface，Skia native 部分没有 OnAttach，因此无法绘制。你说的这个

**分析：**
- 第一次进入：会触发 create，Skia 组件正常 attach
- 第二次进入：直接复用旧 surface，Skia native 部分没有 OnAttach，无法绘制

**根因确认：** 预创建机制导致的问题

---

### 用户问题 4
> 这个地方加不了日志，你给我找个地方加日志 我来确认问题

**解决方案：** 在 Builder 外部添加日志函数

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

---

### 用户问题 5
> 我加了这个日志，第二次切回details的时候，确实没有打印了

**确认：** 第二次进入时，`buildCustomComponent` 没有被调用，说明 Skia 组件没有被重新创建

**下一步：** 分析 BasePrecreateView 的执行流程

---

## 第二阶段：问题定位与根因确认

### 分析 BasePrecreateView 的执行流程

**发现：**
```typescript
// BasePrecreateView.ets
private async init() {
  const jsBundleExecutionStatus = instance.status;
  
  if (this.jsBundleProvider && jsBundleExecutionStatus === undefined) {
    // 新 Instance，需要运行 bundle
    await this.rnInstance.runJSBundle(this.jsBundleProvider);
    this.shouldShow = true;
    return;
  }
  
  if (jsBundleExecutionStatus === "DONE") {
    // bundle 已经加载完成（预创建的 Instance）
    this.shouldShow = true;  // ❌ 直接显示，没有重新运行 bundle
  }
}
```

**问题确认：**
- 第一次进入：`jsBundleExecutionStatus = undefined` → 运行 bundle → 显示 ✅
- 第二次进入：`jsBundleExecutionStatus = "DONE"` → **跳过运行 bundle** → 直接显示 ❌

---

### 理解预创建机制

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

### 根因确认

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

---

## 第三阶段：解决方案探索

### 用户问题 6
> 怎么改，你帮我改下

**解决方案：** 添加 `forceReloadOnAppear` 开关，强制每次进入都重新运行 bundle

**修改 BasePrecreateView.ets：**
```typescript
export struct BasePrecreateView {
  public forceReloadOnAppear: boolean = false;  // 新增属性
  
  private async init() {
    const shouldForceReload = this.shouldForceReload();
    
    // 即使 status = "DONE"，如果 forceReloadOnAppear = true，也强制重新运行
    if (this.jsBundleProvider && (jsBundleExecutionStatus === undefined || shouldForceReload)) {
      if (shouldForceReload) {
        console.log("BasePrecreateView force reload enabled for appKey =", this.appKey);
      }
      await this.rnInstance.runJSBundle(this.jsBundleProvider);  // 强制重新运行
      this.shouldShow = true;
      return;
    }
  }
  
  private shouldForceReload(): boolean {
    return this.forceReloadOnAppear;
  }
}
```

**修改 PrecreateRN.ets：**
```typescript
@Component
export default struct PrecreateRN {
  @Prop forceReloadOnAppear: boolean = false;  // 新增属性
  
  build() {
    BasePrecreateView({
      appKey: this.moduleName,
      bundlePath: this.bundlePath,
      initialProps: this.initProps,
      forceReloadOnAppear: this.forceReloadOnAppear  // 透传
    })
  }
}
```

**修改 Goods.ets：**
```typescript
@Builder
PageMap(name: string, param: string) {
  if (name === 'Details' || name === 'Details2') {
    PrecreateRN({ forceReloadOnAppear: true })  // 启用强制重新加载
  }
}
```

---

### 用户问题 7
> 还是不显示第二个details2

**问题发现：** Detail2 页面完全不显示

**分析：**
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

**同时修复 Goods.ets：**
```typescript
// Goods.ets
} else if (path === "pages/Details2") {
  // 修复前：this.pagePathsGoods.pushPath({ name: "Details", param: "detail2" });
  // 修复后：
  this.pagePathsGoods.pushPath({ name: "Details2", param: "detail2" });
}
```

---

### 用户问题 8
> [RNCSkiaDomView] build component, tag=2 现在进入details2不会打印这个了

**问题：** Detail2 页面仍然不显示，且没有打印 Skia 组件日志

**进一步分析：**
- Detail2 的跳转路径已修复
- 但 `PageMap` 可能没有正确匹配
- 需要检查 `aboutToAppear` 是否被调用

**添加日志验证：**
```typescript
// PrecreateRN.ets
aboutToAppear() {
  console.log('[PrecreateRN] aboutToAppear, forceReloadOnAppear=' + this.forceReloadOnAppear.toString());
}
```

**发现：** Detail2 的 `aboutToAppear` 没有被调用，说明路由有问题

---

### 用户问题 9
> 点击Details2会打印未预加载

**日志输出：**
```
BasePrecreateView status = DONE
点击Details2会打印这个
```

**分析：**
- Detail2 确实进入了 `BasePrecreateView`
- 但 `shouldForceReload()` 返回 `false`，说明 `forceReloadOnAppear` 没有正确传递

**检查：** `PageMap` 中的 `forceReloadOnAppear: true` 是否正确传递

---

### 用户问题 10
> 切换deails2未进入

**问题：** `PrecreateRN.aboutToAppear()` 没有被调用

**原因：** `PageMap` 中的条件判断可能有问题

**修复：** 确保 `PageMap` 正确匹配 `Details2`

---

### 用户问题 11
> deatil2还是不显示

**最终修复：** 确保所有路径都正确

1. **GoodsMainPage.tsx**：跳转路径改为 `'pages/Details2'`
2. **Goods.ets**：`pushPath` 使用正确的 name `"Details2"`
3. **Goods.ets**：`PageMap` 匹配 `name === 'Details2'`

---

## 第四阶段：修复验证

### 验证结果

**第一次进入 Details：**
- ✅ 圆圈正常显示
- ✅ 日志显示：`[RNCSkiaDomView] build component, tag=2`
- ✅ 日志显示：`BasePrecreateView RNInstance 未预加载`

**第二次进入 Details：**
- ✅ 圆圈正常显示（修复后）
- ✅ 日志显示：`[RNCSkiaDomView] build component, tag=2`（修复后）
- ✅ 日志显示：`BasePrecreateView force reload enabled for appKey = Details`
- ✅ 日志显示：`runJSBundle1111111111111 RNInstance`（确认重新运行了 bundle）

**Detail2 页面：**
- ✅ 页面正常显示（修复跳转路径后）
- ✅ 圆圈正常显示

---

## 第五阶段：架构理解

### 用户问题 12
> 你说的这个Instance是什么啊，类似鸿蒙化的react-native-svg,react-native-skia是不是也是这个instance机制呢

**回答：**

**RNInstance 是什么：**
- RNInstance 是 React Native 在鸿蒙上的运行实例
- 包含：独立的 JS 引擎（Hermes/JSVM）、独立的 JS 线程、可运行多个 bundle、可渲染多个 Surface（页面）

**react-native-skia 和 react-native-svg 的机制：**

**react-native-skia：**
- 通过 `RNSkiaPackage` 注册到 `RNInstance`（通过 `createRNPackages`）
- `SKIA_DOM_VIEW_TYPE` 注册到 `arkTsComponentNames`
- `RNCSkiaDomView` 在 `buildCustomComponent` 中渲染

**react-native-svg：**
- 在 C++ 层处理，通过标准的 `ComponentInstance` 机制
- 不需要单独注册 ArkTS 组件

**总结：**
- 所有 React Native 库（包括 react-native-skia、react-native-svg）都运行在同一个 `RNInstance` 中
- 区别在于：
  - react-native-skia：需要注册 ArkTS 原生组件（`RNCSkiaDomView`）
  - react-native-svg：在 C++ 层处理，不需要 ArkTS 组件

---

### 用户问题 13
> 我的伙伴是扫地机器人，他想一个实例对应一个设备，可以嘛

**回答：** 可以，一个 RNInstance 对应一个设备是可行的架构

**实现方案：**
- 创建 `DeviceInstanceManager` 管理设备 Instance
- 每个设备有独立的 RNInstance
- 设备之间完全隔离

**优势：**
- ✅ 完全隔离：设备 A 和 B 互不影响
- ✅ 独立状态：每个设备有独立的 JS 环境
- ✅ 并发控制：多设备同时操作不阻塞

**注意事项：**
- ⚠️ 内存占用：每个 Instance 占用内存，设备多时需要管理
- ⚠️ 生命周期：设备离线时及时销毁 Instance

---

### 用户问题 14
> 那这个instance怎么可以在s3这个工程里面管理

**回答：** 基于 s3 工程现有结构，设计设备 Instance 管理方案

**实现：**
- 扩展现有的 `LoadManager` 或创建新的 `DeviceInstanceManager`
- 使用 Map 管理 deviceId -> RNInstance 的映射
- 提供创建、获取、销毁设备 instance 的方法
- 与现有的代码结构兼容

**注意：** 用户要求不要改代码，所以只提供方案文档，不实际修改代码

---

### 用户问题 15
> 那RNINSTANCE 是什么时候创建的，然后rninstance跟这些cpInstance，bpInstance有什么关联，这些又是什么时候创建的

**回答：**

**RNInstance 和 cpInstance、bpInstance 的关系：**
- `cpInstance`、`bpInstance` 都是 `RNInstance` 类型的变量
- 它们只是**变量名**，用于区分不同的业务场景
- 每个都是通过 `createAndRegisterRNInstance()` 创建的**独立实例**

**创建时机：**

| Instance | 创建时机 | 创建位置 | 条件 |
|----------|---------|---------|------|
| `metroInstance` | 应用启动 | `LoadManager.loadMetroBundle()` | Metro 可用且首次创建 |
| `cpInstance` | 应用启动 | `Index.register()` | Metro 不可用 |
| `bpInstance` | 应用启动 | `Index.register()` | Metro 不可用 |
| `bpInstance2` | 应用启动 | `Index.register()` | Metro 不可用 |
| `BasePrecreateView` 的 Instance | 页面显示 | `BasePrecreateView.getOrCreateRNInstance()` | 页面首次显示或缓存为空 |

**完整流程：**
```
应用启动
  ↓
Index.aboutToAppear()
  ↓
loadMetroBundle() → 创建 metroInstance（如果 Metro 可用）
  ↓
register() → 创建 cpInstance、bpInstance、bpInstance2（如果 Metro 不可用）
  ↓
页面显示 → BasePrecreateView 动态创建 Instance（如果需要）
```

---

### 用户问题 16
> 给我讲一下这个里面的组件都是干嘛的

**回答：** 详细解释 Index.ets 中导入的所有组件

**核心架构组件：**
1. **RNOHCoreContext** - 核心上下文，全局唯一，管理所有 RNInstance
2. **RNInstance** - 运行实例，独立的 JS 执行环境
3. **RNOHContext** - Instance 上下文，从 CoreContext + Instance 创建
4. **RNComponentContext** - 组件渲染上下文，用于渲染 React Native 组件

**Bundle 加载：**
5. **ResourceJSBundleProvider** - Bundle 加载器，从资源文件加载 `.harmony.bundle`

**调试工具：**
6. **LogBoxDialog** - 错误日志对话框，显示 JS 错误和警告
7. **RNOHErrorDialog** - 框架错误对话框，显示框架级别的错误
8. **LogBoxTurboModule** - LogBox 控制模块，控制 LogBox 的显示/隐藏

**组件构建：**
9. **ComponentBuilderContext** - 组件构建上下文，在 `@Builder` 中传递组件信息
10. **buildRNComponentForTag** - RN 组件构建器，根据 tag 构建 RN 组件

---

### 用户问题 17
> 再讲一下RN的架构和所有组件，及其执行流程

**回答：** 创建完整的架构文档

**整体架构（6层）：**
```
JS 层 (React Native)
  ↓ JSI
C++ 适配层 (React Common + OpenHarmony 适配)
  ↓ NAPI
ArkTS 层 (应用层)
  ↓ C-API
ArkUI 层 (原生渲染)
```

**核心组件：**
- **应用层**：RNAbility、RNOHCoreContext、RNInstance、RNSurface
- **C++ 层**：RNInstanceInternal、SchedulerDelegate、MountingManagerCAPI
- **React 层**：Scheduler、ShadowTree、Yoga、Fabric
- **通信层**：JSI、NAPI、C-API

**完整执行流程：**
1. **应用启动**：EntryAbility → RNOHCoreContext → RNInstance
2. **页面渲染**：RNSurface → React 渲染 → Mutations → ArkUI 渲染
3. **用户交互**：ArkUI 事件 → C++ → JSI → JS → 业务逻辑 → UI 更新

---

### 用户问题 18
> 再给我整理一个从rn到启动，这些架构及其组建的执行流程

**回答：** 创建从 React Native 代码到应用启动的完整执行流程文档

**完整流程：**
```
开发阶段
  JS 代码 → Metro Bundler → bundle 文件
    ↓
应用启动
  EntryAbility.onCreate()
    → RNInstancesCoordinator
    → RNOHCoreContext
    ↓
Instance 创建
  Index.aboutToAppear()
    → loadMetroBundle()
    → register()
    → 创建 cpInstance/bpInstance
    ↓
Bundle 加载
  runJSBundle()
    → NAPI → C++
    → JS 引擎执行
    → 组件注册
    ↓
页面渲染
  RNSurface
    → React 渲染
    → Mutations
    → ArkUI 渲染
```

---

## 第六阶段：问题总结

### 用户问题 19
> 这个问题解决完了，你把从头到尾的分析尝试，到最后写出skia_crash.patch这个正确patch全部详细总结一遍

**回答：** 创建完整的问题解决过程总结文档

**总结内容：**
1. 问题发现与初步分析
2. 问题定位过程（包括日志添加的尝试和错误）
3. 根因确认
4. 解决方案探索与尝试
5. 最终修复实现
6. 修复验证
7. 完整 Patch 文件说明
8. 经验总结

**关键时间线：**
```
问题发现 → 初步分析 → 添加日志验证 → 分析执行流程 
→ 理解预创建机制 → 确认根因 → 探索解决方案 
→ 实现修复 → 修复 Detail2 问题 → 验证修复 → 完成
```

---

## 最终解决方案总结

### 核心修复

**问题：** Skia Canvas 圆圈在第二次进入 Details 页面时消失

**根因：** 预创建机制在后台运行 bundle 时没有 UI surface，导致 Skia 组件无法 attach，GPU context 未创建

**解决方案：** 添加 `forceReloadOnAppear` 开关，强制每次进入都重新运行 bundle

### 修改的文件

1. **BasePrecreateView.ets** - 添加 `forceReloadOnAppear` 开关和相关逻辑
2. **PrecreateRN.ets** - 添加 `@Prop forceReloadOnAppear` 并透传
3. **Goods.ets** - 在 `PageMap` 中为 Details/Details2 传入 `forceReloadOnAppear: true`
4. **GoodsMainPage.tsx** - 修复 Detail2 的跳转路径
5. **LoadBundle.ets** - 添加日志支持（调试用）

### Patch 文件

- **skia_crash.patch** - 包含所有修改的完整 patch 文件
- **skia_circle_fix.patch** - 核心修复的 patch 文件（forceReloadOnAppear 相关）

---

## 关键经验总结

1. **日志的重要性**：通过添加日志，快速定位问题根源
2. **理解组件生命周期**：某些组件需要在 surface 存在时才能正确初始化
3. **预创建机制的局限性**：需要 attach 到 surface 的组件不适合预创建
4. **性能与正确性的权衡**：在保证功能正确性的前提下，可以牺牲部分性能优化
5. **系统化分析**：从现象到根因，从根因到解决方案

---

*对话记录时间：2024年*
*问题状态：已修复 ✅*
*最终 Patch：skia_crash.patch*
