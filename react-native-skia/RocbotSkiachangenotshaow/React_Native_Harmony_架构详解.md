# React Native for OpenHarmony 架构详解

## 目录
1. [整体架构流程](#整体架构流程)
2. [分层架构详解](#分层架构详解)
3. [跨语言通信机制](#跨语言通信机制)
4. [多线程模型](#多线程模型)
5. [多Bundle按需加载](#多bundle按需加载)
6. [性能优化策略](#性能优化策略)

---

## 整体架构流程

### 从JS打包到C++使用的完整流程

```
┌─────────────────────────────────────────────────────────────────┐
│                        开发阶段                                   │
├─────────────────────────────────────────────────────────────────┤
│  JS/TS/React 代码                                               │
│  ├── React 组件                                                 │
│  ├── React Native 组件                                          │
│  └── TurboModule 调用                                           │
│           ↓                                                     │
│  Metro Bundler 打包                                             │
│  ├── 解析依赖                                                    │
│  ├── 转换代码（Babel/TypeScript）                                │
│  └── 序列化成 bundle                                             │
│           ↓                                                     │
│  bundle.harmony.bundle 文件                                     │
│  └── 存储在 rawfile/bundle/ 目录                               │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                        运行时阶段                                 │
├─────────────────────────────────────────────────────────────────┤
│  ArkTS 层（MAIN 线程）                                          │
│  ├── RNInstance.createAndRegisterRNInstance()                  │
│  ├── ResourceJSBundleProvider 读取 bundle                      │
│  └── RNInstance.runJSBundle() 加载 bundle                      │
│           ↓                                                     │
│  NAPI 桥接层（ArkTS ↔ C++）                                     │
│  ├── RNOHAppNapiBridge.cpp                                     │
│  └── 类型转换、函数调用                                          │
│           ↓                                                     │
│  C++ 适配层                                                     │
│  ├── RNInstanceInternal.cpp                                    │
│  │   ├── 创建 JS 引擎（Hermes/JSVM）                           │
│  │   ├── 初始化 JSI Runtime                                    │
│  │   └── 加载 bundle 到 JS 引擎                               │
│  ├── Instance.cpp (ReactCommon)                                │
│  │   └── 执行 bundle 代码                                      │
│  └── 注册 JSI 通道                                              │
│       ├── __turboModuleProxy (TurboModule)                     │
│       └── nativeFabricUIManager (Fabric)                        │
│           ↓                                                     │
│  JS 线程（执行 React 代码）                                     │
│  ├── React Render 阶段                                         │
│  ├── 创建 ShadowTree                                            │
│  ├── Yoga 布局计算                                              │
│  └── 生成 Mutations（Create/Update/Delete/Insert/Remove）      │
│           ↓                                                     │
│  JSI 通信（JS ↔ C++）                                          │
│  ├── JS 调用 C++：通过 JSI 函数                                 │
│  └── C++ 调用 JS：通过 JSI Runtime                             │
│           ↓                                                     │
│  C++ 渲染层                                                     │
│  ├── SchedulerDelegate.cpp                                     │
│  │   └── 接收 Mutations                                        │
│  ├── MountingManagerCAPI.cpp                                   │
│  │   └── 处理 Mutations，创建/更新 CAPI 组件                   │
│  └── ComponentInstance (Image/Text/View 等)                    │
│       └── 调用 ArkUI C-API                                     │
│           ↓                                                     │
│  ArkUI 原生层                                                   │
│  ├── ContentSlot (占位组件)                                     │
│  ├── NodeContent (C-API 组件容器)                               │
│  └── ArkUI 原生组件渲染                                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 分层架构详解

### 架构特点：分层清晰

**JS → JSI → C++ → NAPI → ArkTS → ArkUI**

### 第一层：JS 层（React Native 应用代码）

**职责：**
- 业务逻辑实现
- React 组件定义
- 调用 TurboModule 访问原生能力

**代码示例：**
```javascript
// JS 代码示例
import { View, Text } from 'react-native';
import SampleTurboModule from './SampleTurboModule';

function App() {
  return (
    <View>
      <Text>Hello</Text>
    </View>
  );
}
```

### 第二层：JSI 层（JavaScript Interface）

**职责：**
- JS 与 C++ 的通信桥梁
- 同步调用（无需序列化）
- 类型转换（JS 对象 ↔ C++ 对象）

**工作原理：**
```javascript
// JS 侧调用
const TurboModule = require('NativeModules').SampleTurboModule;
TurboModule.someMethod();  // 直接调用，无异步开销
  ↓
JSI Runtime
  ↓
// C++ 侧执行
class SampleTurboModule : public TurboModule {
  jsi::Value someMethod(jsi::Runtime& rt, ...) { ... }
}
```

**C++ 侧注册：**
```cpp
// C++ 侧注册 JSI 函数
runtime->global().setProperty(
  runtime,
  "__turboModuleProxy",
  jsi::Function::createFromHostFunction(...)
);
```

### 第三层：C++ 层（React Common + 适配层）

**职责：**
- **React Common**：跨平台通用逻辑（ShadowTree、布局、Diff）
- **适配层**：对接鸿蒙系统（创建组件、处理事件）

**代码示例：**
```cpp
// React Common (平台无关)
class ShadowNode { ... };
class Scheduler { ... };

// OpenHarmony 适配层
class SchedulerDelegate : public SchedulerDelegate {
  void schedulerDidFinishTransaction(...) {
    // 处理 React 传来的 Mutations
  }
};
```

### 第四层：NAPI 层（Native API）

**职责：**
- ArkTS 与 C++ 的通信桥梁
- 类型转换（ArkTS 类型 ↔ C++ 类型）
- 异步调用支持

**工作原理：**
```typescript
// ArkTS 侧调用
this.rnInstance.postMessageToCpp("MESSAGE", payload);
  ↓
NAPI Bridge (RNOHAppNapiBridge.cpp)
  ↓
// C++ 侧接收
void onMessageReceived(ArkTSMessage const& message) { ... }
```

**C++ 侧注册：**
```cpp
// C++ 侧注册 NAPI 函数
static napi_value onCreateRNInstance(napi_env env, napi_callback_info info) {
  // 处理 ArkTS 调用
  return ...;
}

// 注册到 NAPI
napi_property_descriptor desc[] = {
  {"onCreateRNInstance", onCreateRNInstance, ...}
};
napi_define_properties(env, exports, 1, desc);
```

### 第五层：ArkTS 层（鸿蒙应用层）

**职责：**
- 应用生命周期管理
- UI 组件声明
- 调用 C++ 能力（通过 NAPI）

**代码示例：**
```typescript
// ArkTS 代码
@Component
export struct BaseRN {
  @StorageLink('RNOHCoreContext') rnohCoreContext: RNOHCoreContext;
  
  build() {
    RNSurface({
      surfaceConfig: { appKey: 'Details' }
    })
  }
}
```

### 第六层：ArkUI 层（原生渲染）

**职责：**
- 原生组件渲染
- 系统能力调用
- 硬件加速

**代码示例：**
```cpp
// C-API 调用
NativeNodeApi::getInstance()->createNode(ARKUI_NODE_STACK);
NativeNodeApi::getInstance()->setNodeAttribute(nodeHandle, ...);
```

### 数据流向示例

```
用户点击按钮
  ↓
JS: onPress={() => TurboModule.doSomething()}
  ↓ (JSI)
C++: TurboModule::doSomething()
  ↓ (NAPI)
ArkTS: rnInstance.postMessageToCpp("EVENT", data)
  ↓
ArkUI: 更新 UI
```

---

## 跨语言通信机制

### JSI（JavaScript Interface）

**特点：** 同步、高性能、类型安全

#### JS → C++ 调用

**JS 侧：**
```javascript
const TurboModule = require('NativeModules').SampleTurboModule;
const result = TurboModule.add(1, 2);  // 同步调用，立即返回
```

**C++ 侧实现：**
```cpp
class SampleTurboModule : public TurboModule {
public:
  jsi::Value add(jsi::Runtime& rt, jsi::Value const& a, jsi::Value const& b) {
    int numA = a.asNumber();
    int numB = b.asNumber();
    return jsi::Value(numA + numB);  // 直接返回，无序列化
  }
};
```

**优势：**
- ✅ 同步执行，无异步开销
- ✅ 无需 JSON 序列化
- ✅ 类型安全（编译期检查）

#### C++ → JS 调用

**C++ 侧：**
```cpp
runtime->global()
  .getPropertyAsFunction(runtime, "onEvent")
  .call(runtime, jsi::String::createFromUtf8(runtime, "click"));
```

**JS 侧：**
```javascript
global.onEvent = (eventName) => {
  console.log('Event:', eventName);
};
```

### NAPI（Native API）

**特点：** 异步支持、类型转换、错误处理

#### ArkTS → C++ 调用

**ArkTS 侧：**
```typescript
this.rnInstance.postMessageToCpp("SAMPLE_MESSAGE", {
  data: "hello"
});
```

**C++ 侧注册：**
```cpp
static napi_value onArkTSMessage(napi_env env, napi_callback_info info) {
  ArkJS arkJs(env);
  auto args = arkJs.getCallbackArgs(info, 3);
  auto instanceId = arkJs.getInteger(args[0]);
  auto messageName = arkJs.getString(args[1]);
  auto payload = arkJs.getDynamic(args[2]);
  
  // 处理消息
  auto rnInstance = maybeGetInstanceById(instanceId);
  rnInstance->handleMessage(messageName, payload);
  
  return arkJs.getUndefined();
}
```

#### C++ → ArkTS 调用

**C++ 侧：**
```cpp
m_deps->rnInstance.lock()->postMessageToArkTS(
  "SAMPLE_MESSAGE", 
  folly::dynamic::object("data", "hello")
);
```

**ArkTS 侧：**
```typescript
rnInstance.cppEventEmitter.subscribe("SAMPLE_MESSAGE", (value: object) => {
  console.log('Received:', value);
});
```

### 通信对比表

| 特性 | JSI | NAPI |
|------|-----|------|
| 通信方向 | JS ↔ C++ | ArkTS ↔ C++ |
| 调用方式 | 同步 | 同步/异步 |
| 性能 | 高（无序列化） | 中（有类型转换） |
| 使用场景 | TurboModule、Fabric | 组件通信、事件传递 |

---

## 多线程模型

### MAIN 线程（UI 线程）

**职责：**
- ArkTS 代码执行
- 组件生命周期管理（CREATE/UPDATE/DELETE）
- TurboModule 业务逻辑（部分）
- 事件处理

**代码示例：**
```typescript
// ArkTS 代码在 MAIN 线程执行
@Component
export struct BaseRN {
  aboutToAppear() {  // MAIN 线程
    // 创建 RNInstance
    // 加载 bundle
  }
  
  build() {  // MAIN 线程
    RNSurface({ ... })  // UI 渲染
  }
}
```

**特点：**
- ✅ 唯一实例
- ✅ 负责 UI 渲染
- ⚠️ 不能阻塞（否则 UI 卡顿）

### JS 线程

**职责：**
- 执行 bundle 代码
- React Render 阶段
- 创建 ShadowTree
- Yoga 布局计算
- 生成 Mutations

**代码示例：**
```javascript
// JS 代码在 JS 线程执行
function App() {
  // React Render 阶段
  const [count, setCount] = useState(0);
  
  // ShadowTree 创建
  return <View><Text>{count}</Text></View>;
}
```

**特点：**
- ✅ 每个 RNInstance 一个独立 JS 线程
- ✅ 与 MAIN 线程异步通信
- ✅ 通过消息队列传递 Mutations

**线程间通信：**
```
JS 线程
  ├── 生成 Mutations
  └── 提交到消息队列
        ↓
MAIN 线程
  ├── 接收 Mutations
  └── 执行 UI 更新
```

### BACKGROUND 线程（实验特性）

**职责：**
- 布局计算（Yoga）
- ShadowTree Diff
- 降低 JS 线程负荷

**代码示例：**
```cpp
// 部分布局任务迁移到 BACKGROUND 线程
if (enableBackgroundExecutor) {
  // 布局计算
  // ShadowTree 比较
}
```

**注意：**
- ⚠️ 实验特性，稳定性有风险
- ⚠️ 正式版本不建议开启

### WORKER 线程

**职责：**
- TurboModule 执行（避免阻塞主线程）
- Worker TurboModule 间通信
- 后台任务处理

**代码示例：**
```typescript
// WORKER 线程配置
class EntryAbility extends RNAbility {
  getRNOHWorkerScriptUrl(): string {
    return 'entry/ets/RNOH/RNOHWorker.ets';
  }
}
```

---

## 多Bundle按需加载

### 架构设计

```
基础包（basic.harmony.bundle）
├── React Native 核心库
├── React 核心
├── 公共依赖（如 crypto-js）
└── 基础工具函数

业务包（details.harmony.bundle, goods.harmony.bundle）
├── 业务逻辑代码
├── 页面组件
└── 业务相关依赖（排除基础包中已有的）
```

### 实现机制

#### 1. 模块 ID 管理

**代码示例：**
```javascript
// moduleId.js
function getModuleId(projectRootPath, modulePath) {
  let pathRelative = modulePath.substr(...);
  return String(SHA256(pathRelative));  // 使用 SHA256 生成唯一 ID
}
```

**作用：**
- ✅ 同一模块在不同 bundle 中保持相同 ID
- ✅ 避免重复打包
- ✅ 支持模块共享

#### 2. 模块过滤

**代码示例：**
```javascript
// postProcessModulesFilter
function postProcessModulesFilterWrap(projectRootPath) {
  return (module) => {
    const moduleId = getModuleId(projectRootPath, module.path);
    
    // 如果模块在基础包中，业务包不包含
    if (basicNameArray.includes(jsItem)) {
      return false;  // 不打包
    }
    
    return true;  // 打包
  };
}
```

**作用：**
- ✅ 基础包已包含的模块，业务包不再打包
- ✅ 减少 bundle 体积
- ✅ 避免重复代码

#### 3. 加载流程

**代码示例：**
```typescript
// ArkTS 侧
// 1. 先加载基础包
await cpInstance.runJSBundle(
  new ResourceJSBundleProvider(resourceManager, 'bundle/basic/basic.harmony.bundle')
);

// 2. 按需加载业务包
await bpInstance.runJSBundle(
  new ResourceJSBundleProvider(resourceManager, 'bundle/bp/details.harmony.bundle')
);
```

**优势：**
- ✅ 减少初始加载时间
- ✅ 按需加载，节省内存
- ✅ 独立更新业务包

---

## 性能优化策略

### C-API vs 声明式 API

#### 传统方式（声明式 API）

```
React Component
  ↓
ShadowNode
  ↓
ArkTS Component (声明式)
  ↓
ArkUI 后端
  ↓
原生渲染
```

**问题：**
- ❌ 跨语言调用多
- ❌ 数据格式转换开销
- ❌ 属性需要序列化

#### C-API 方式

```
React Component
  ↓
ShadowNode
  ↓
C++ ComponentInstance
  ↓
ArkUI C-API (直接调用)
  ↓
原生渲染
```

**优势：**
- ✅ 无跨语言调用（C++ 直接调用 C）
- ✅ 无数据格式转换
- ✅ 属性 Diff，避免重复设置

### 性能收益

#### 1. 无跨语言开销

**代码示例：**
```cpp
// C-API 直接调用
NativeNodeApi::getInstance()->createNode(ARKUI_NODE_STACK);
// 无需经过 ArkTS 层，直接调用原生接口
```

#### 2. 无类型转换

**代码示例：**
```cpp
// C-API：直接使用 C 类型
NativeNodeApi::getInstance()->setNodeAttribute(
  nodeHandle,
  NODE_WIDTH,
  ArkUI_NumberValue{100.0}  // 直接传数值，无需转换
);

// 声明式：需要类型转换
// ArkTS string → C++ string → C string
```

#### 3. 属性 Diff

**代码示例：**
```cpp
// C-API：可以比较属性，只更新变化的部分
if (oldProps.width != newProps.width) {
  setNodeAttribute(nodeHandle, NODE_WIDTH, newProps.width);
}
// 避免重复设置相同属性
```

### 实际性能数据

根据文档，C-API 的性能收益包括：
- ✅ C 端最小化、无跨语言的组件创建和属性设置
- ✅ 无跨语言前的数据格式转换
- ✅ 可以进行属性 Diff，避免重复设置

---

## Bundle 文件说明

### Bundle 文件位置

```
D:\s3\a\b\c\d\e\entry\src\main\resources\rawfile\bundle\
├── basic/
│   └── basic.harmony.bundle          # 基础包
├── cp/
│   ├── homepage.harmony.bundle      # 首页业务包
│   └── goods.harmony.bundle          # 商品页业务包
└── bp/
    ├── details.harmony.bundle        # 详情页业务包
    ├── test.harmony.bundle           # 测试页业务包
    └── sandbox.harmony.bundle        # 沙箱页业务包
```

### Bundle 文件作用

这些 `.harmony.bundle` 文件是：
1. **React Native JS 代码的打包产物**：将 React/React Native 代码打包成单个文件
2. **预编译的 JS 代码**：在构建时生成，运行时直接加载，无需 Metro 服务器
3. **按模块/页面拆分**：不同页面使用不同的 bundle，实现按需加载

### Bundle 打包结构

**打包工具：** Metro Bundler

**结构：**
```
Metro Bundler
├── Resolver（解析器）
│   └── 解析模块依赖，找到所有需要打包的文件
├── Transformer（转换器）
│   └── 将 ES6+、JSX、TypeScript 等转换为可执行的 JS
└── Serializer（序列化器）
    └── 将所有模块打包成一个 bundle 文件
```

**Bundle 文件内部结构：**
```javascript
// Metro 生成的 bundle 结构类似这样：
__d(function(g, r, i, a, m, e, d) {
  // g = global
  // r = require
  // i = module id
  // a = module exports
  // m = module
  // e = exports
  // d = dependencies
  
  var t = r(d[0]);  // 依赖模块 0
  var n = r(d[1]);  // 依赖模块 1
  // ... 模块代码
}, "moduleId", ["dependency1", "dependency2", ...]);
```

### 打包命令

```bash
npm run dev:basic      # 生成 basic.harmony.bundle
npm run dev:homepage   # 生成 homepage.harmony.bundle
npm run dev:goods      # 生成 goods.harmony.bundle
npm run dev:details    # 生成 details.harmony.bundle
npm run dev:all        # 生成所有 bundle
```

### 加载方式

**ArkTS 代码中加载：**
```typescript
// 例如在 Goods.ets 中
private bundlePath = 'bundle/cp/goods.harmony.bundle';
await this.rnInstance.runJSBundle(
  new ResourceJSBundleProvider(getContext().resourceManager, this.bundlePath)
);
```

---

## 架构总结

### 核心特点

1. **分层清晰**：JS → JSI → C++ → NAPI → ArkTS → ArkUI
   - 每层职责明确，便于维护和扩展

2. **跨语言通信**：JSI（JS↔C++）、NAPI（ArkTS↔C++）
   - JSI：同步、高性能，用于 TurboModule 和 Fabric
   - NAPI：支持异步，用于组件通信和事件传递

3. **多线程**：MAIN、JS、BACKGROUND、WORKER
   - MAIN：UI 线程，负责渲染和生命周期
   - JS：执行 React 代码，每个 RNInstance 独立线程
   - BACKGROUND：实验特性，分担布局任务
   - WORKER：执行 TurboModule，避免阻塞主线程

4. **按需加载**：多 Bundle 架构
   - 基础包 + 业务包分离
   - 模块 ID 管理，避免重复打包
   - 支持独立更新

5. **性能优化**：C-API 直接调用
   - 无跨语言开销
   - 无类型转换
   - 属性 Diff，避免重复设置

### 架构优势

- ✅ **清晰的职责划分**：每层专注自己的职责
- ✅ **高效的跨语言通信**：JSI 和 NAPI 提供高性能通信
- ✅ **合理的线程模型**：多线程协作，避免阻塞
- ✅ **灵活的按需加载**：多 Bundle 架构，支持按需加载
- ✅ **优秀的性能表现**：C-API 直接调用，减少开销

该架构让 React Native 代码能在鸿蒙系统上高效运行，并通过 C++ 层对接原生能力。

---

## 问题修复记录

### Skia Canvas 圆圈消失问题修复

**问题现象：**
- 第一次进入 Details 页面：圆圈正常显示
- 退出再进入 Details 页面：圆圈消失

**根本原因：**
- `BasePrecreateView` 使用预创建机制
- 预创建的 RNInstance 在后台运行 bundle，没有实际的 UI surface
- Skia 的 Canvas 组件需要 attach 到 surface 才能创建 GPU context
- 第二次进入时，虽然显示了页面，但 Skia 组件没有重新 attach

**解决方案：**
- 添加 `forceReloadOnAppear` 开关
- 当 `forceReloadOnAppear = true` 时，强制每次进入都重新运行 bundle
- 重新运行 bundle → React 组件重新 mount → Skia 组件重新 attach → 圆圈显示

**修改文件：**
1. `BasePrecreateView.ets` - 添加 `forceReloadOnAppear` 开关和相关逻辑
2. `PrecreateRN.ets` - 添加 `@Prop forceReloadOnAppear` 并透传
3. `Goods.ets` - 在 `PageMap` 中为 Details/Details2 传入 `forceReloadOnAppear: true`
4. `GoodsMainPage.tsx` - 修复 Detail2 的跳转路径

**详细说明见：** `skia_circle_fix.patch`

---

*文档生成时间：2024年*
*基于 React Native for OpenHarmony 架构分析*




