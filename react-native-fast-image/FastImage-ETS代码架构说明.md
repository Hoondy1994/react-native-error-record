# FastImage HarmonyOS ETS 代码架构说明

## 📁 目录结构

```
ets/
├── FastImagePackage.ts          # 包入口，注册 TurboModule 和组件
├── FastImageLoaderTurboModule.ts # TurboModule：提供图片加载 API
├── RNCFastImageViewTurboModule.ts # TurboModule：提供缓存清理 API
├── RNFastImage.ets              # 组件：FastImage 的 ArkUI 实现
├── RNCSpecs.ts                  # 组件规范：定义 Props、Events、Commands
├── TMSpecs.ts                   # TurboModule 规范
├── Logger.ts                    # 日志工具
└── FastRemoteImageLoader/       # 图片加载核心逻辑
    ├── RemoteImageLoader.ts     # 图片加载器（网络请求、缓存管理）
    ├── RemoteImageSource.ts     # 图片源封装
    ├── RemoteImageCache.ts      # 内存缓存
    ├── RemoteImageDiskCache.ts  # 磁盘缓存
    └── RemoteImageLoaderError.ts # 错误处理
```

---

## 🏗️ 架构层次

### 1. **包注册层**（`FastImagePackage.ts`）

**作用**：注册 FastImage 模块到 React Native

**关键代码**：
```typescript
export class FastImagePackage extends RNPackage {
  createTurboModulesFactory(ctx: TurboModuleContext): TurboModulesFactory {
    // 设置图片缓存大小
    app.setImageCacheCount(200);
    app.setImageRawDataCacheSize(100*1024*1024);
    app.setImageFileCacheSize(100*1024*1024);
    
    // 创建 TurboModule 工厂
    return new FastImageTurboModulesFactory(ctx);
  }
  
  createDescriptorWrapperFactoryByDescriptorType(ctx): DescriptorWrapperFactoryByDescriptorType {
    return {
      [FastImageView.NAME]: (ctx) => new FastImageView.DescriptorWrapper(ctx.descriptor)
    }
  }
}
```

**功能**：
- 注册两个 TurboModule：`FastImageLoader` 和 `RNCFastImageView`
- 注册组件：`FastImageView`
- 初始化系统级图片缓存配置

---

### 2. **TurboModule 层**

#### 2.1 `FastImageLoaderTurboModule.ts`（主要 TurboModule）

**作用**：提供图片加载相关的 API

**核心方法**：

| 方法 | 功能 | 调用链 |
|------|------|--------|
| `getRemoteImageSource(uri)` | 获取远程图片源 | → `RemoteImageLoader.getImageSource()` |
| `getPixelMap(uri)` | 获取 PixelMap | → `RemoteImageLoader.getPixelMap()` |
| `prefetchImage(uri, headers)` | 预加载图片 | → `RemoteImageLoader.prefetch()` |
| `queryCache(uris)` | 查询缓存状态 | → `RemoteImageLoader.queryCache()` |
| `getSize(uri)` | 获取图片尺寸 | → `RemoteImageLoader.getImageSource()` → `getImageInfo()` |
| `diskCacheClear()` | 清理磁盘缓存 | → `RemoteImageLoader.diskCacheClear()` |
| `memoryCacheClear()` | 清理内存缓存 | → `RemoteImageLoader.memoryCacheClear()` |

**初始化**：
```typescript
constructor(protected ctx: TurboModuleContext) {
  super(ctx)
  this.imageLoader = new RemoteImageLoader(
    new RemoteImageMemoryCache(128),      // 内存缓存：128 个图片
    new RemoteImageDiskCache(128, ...),   // 磁盘缓存：128 个图片
    ctx.uiAbilityContext,
    onDiskCacheUpdate,                     // 缓存更新回调
    onDownloadFileFail                     // 下载失败回调
  )
}
```

---

#### 2.2 `RNCFastImageViewTurboModule.ts`（缓存管理 TurboModule）

**作用**：提供缓存清理 API

**方法**：
- `clearMemoryCache()` - 清理内存缓存
- `clearDiskCache()` - 清理磁盘缓存

---

### 3. **组件层**（`RNFastImage.ets`）

**作用**：FastImage 组件的 ArkUI 实现

**核心流程**：

```
组件创建 (aboutToAppear)
    ↓
onDescriptorWrapperChange()
    ↓
检测 URI 变化？
    ├─ 是 → onLoadStart() + updateImageSource()
    └─ 否 → 不处理
    ↓
updateImageSource()
    ↓
判断 URI 类型：
    ├─ asset:// → 本地资源
    ├─ data: → Base64 图片
    ├─ 有 headers → requestWithHeaders() (http.createHttp)
    └─ 普通 URL → FastImageLoaderTurboModule.getRemoteImageSource()
```

**关键方法**：

1. **`updateImageSource()`** - 更新图片源
   - 检测 URI 类型（asset://、data:、http://）
   - 调用相应的加载方法
   - 处理缓存查询

2. **`requestWithHeaders()`** - 带自定义 headers 的请求
   - 使用 `http.createHttp().request()`
   - 直接处理响应，不经过 `RemoteImageLoader`

3. **`onDescriptorWrapperChange()`** - Props 变化处理
   - 检测 URI 是否变化
   - 变化时触发 `onLoadStart()` 和 `updateImageSource()`

---

### 4. **图片加载核心层**（`FastRemoteImageLoader/`）

#### 4.1 `RemoteImageLoader.ts`（核心加载器）

**作用**：统一管理图片加载、缓存、网络请求

**核心方法**：

1. **`getImageSource(uri, headers)`** - 获取图片源
   ```
   检查 Base64 → 检查内存缓存 → 检查磁盘缓存 → 网络请求
   ```

2. **`fetchImage(url, headers)`** - 网络请求
   - 使用 `fetchDataFromUrl()` 发起请求
   - 请求去重（相同 URL 复用 Promise）
   - 返回 `ArrayBuffer`

3. **`prefetch(uri, headers)`** - 预加载
   - 使用 `request.downloadFile()` 下载到磁盘
   - 不加载到内存

4. **`getPixelMap(uri)`** - 获取 PixelMap
   - 先获取 `ImageSource`
   - 再转换为 `PixelMap`

**缓存策略**：
- **内存缓存**：`RemoteImageMemoryCache`（最多 128 个）
- **磁盘缓存**：`RemoteImageDiskCache`（最多 128 个文件）
- **请求去重**：`activeRequestByUrl` Map

---

#### 4.2 `RemoteImageSource.ts`（图片源封装）

**作用**：封装 HarmonyOS 的 `ImageSource`

**功能**：
- 包装 `image.ImageSource`
- 提供 `getPixelMapPromise()` 方法
- 处理动画图片（GIF）的特殊情况

---

#### 4.3 `RemoteImageCache.ts`（内存缓存）

**作用**：内存中的图片缓存

**实现**：
- 使用 `Map<string, RemoteImageSource>` 存储
- LRU 策略：超过 `maxSize` 时删除最旧的
- 提供 `memoryCacheClear()` 清理所有缓存

---

#### 4.4 `RemoteImageDiskCache.ts`（磁盘缓存）

**作用**：磁盘文件缓存

**实现**：
- 使用 `Map<string, boolean>` 记录缓存文件
- 文件存储在 `cacheDir` 目录
- 缓存键：URI 去除特殊字符
- 提供 `diskCacheClear()` 删除所有缓存文件

---

## 🔄 完整调用流程

### 场景 1: 网络图片加载

```
index.tsx
  <FastImage source={{ uri: "https://..." }} />
    ↓
RNFastImage.ets
  aboutToAppear() → onDescriptorWrapperChange()
    ↓
  updateImageSource()
    ↓
  imageLoader.getRemoteImageSource(uri)
    ↓
FastImageLoaderTurboModule.ts
  getRemoteImageSource(uri)
    ↓
RemoteImageLoader.ts
  getImageSource(uri)
    ↓
  检查缓存？
    ├─ 内存缓存 → 直接返回
    ├─ 磁盘缓存 → 从文件加载
    └─ 无缓存 → fetchImage(uri)
        ↓
      fetchDataFromUrl(url, options)  ⭐ 网络请求
        ↓
      返回 ArrayBuffer
        ↓
      image.createImageSource(response.result)
        ↓
      保存到内存缓存 + 磁盘缓存
        ↓
  返回 RemoteImageSource
    ↓
RNFastImage.ets
  imageSource = new ImageSourceHolder(pixelMap)
    ↓
  ArkUI Image 组件渲染
```

---

### 场景 2: Base64 图片加载

```
index.tsx
  <FastImage source={{ uri: "data:image/png;base64,xxx" }} />
    ↓
RNFastImage.ets
  updateImageSource()
    ↓
  if (uri.startsWith("data:"))
    ↓
  imageSource = new ImageSourceHolder(uri)  ⭐ 直接使用 Base64 字符串
    ↓
  ArkUI Image 组件渲染
    ↓
  HarmonyOS image.createImageSource(uri)  ⭐ 系统自动解码
```

---

### 场景 3: 带自定义 headers 的图片

```
index.tsx
  <FastImage source={{ uri: "https://...", headers: {...} }} />
    ↓
RNFastImage.ets
  updateImageSource()
    ↓
  if (has headers)
    ↓
  requestWithHeaders()
    ↓
  http.createHttp().request(uri, { header: headers })  ⭐ 直接使用 HTTP 模块
    ↓
  返回 ArrayBuffer
    ↓
  image.createImageSource(res)
    ↓
  imageSource.createPixelMap()
    ↓
  imageSource = new ImageSourceHolder(pixelMap)
```

---

## 📊 关键数据结构

### 1. **缓存结构**

```typescript
// 内存缓存
RemoteImageMemoryCache {
  data: Map<string, RemoteImageSource>  // URI → ImageSource
  maxSize: 128
}

// 磁盘缓存
RemoteImageDiskCache {
  data: Map<string, boolean>  // 文件名 → 是否存在
  cacheDir: string
  maxSize: 128
}
```

### 2. **请求去重**

```typescript
RemoteImageLoader {
  activeRequestByUrl: Map<string, Promise<FetchResult>>  // URL → Promise
  activePrefetchByUrl: Map<string, Promise<boolean>>     // URL → Promise
}
```

### 3. **组件状态**

```typescript
RNFastImage {
  imageSource: ImageSourceHolder | undefined  // 当前图片源
  descriptorWrapper: FastImageView.DescriptorWrapper  // Props 包装器
  eventEmitter: FastImageView.EventEmitter  // 事件发射器
}
```

---

## 🎯 关键设计点

### 1. **三层缓存策略**

1. **系统缓存**：HarmonyOS 系统级图片缓存（`app.setImageCacheCount()`）
2. **内存缓存**：应用级内存缓存（`RemoteImageMemoryCache`）
3. **磁盘缓存**：应用级磁盘缓存（`RemoteImageDiskCache`）

### 2. **请求去重机制**

- 相同 URL 的并发请求会复用同一个 Promise
- 避免重复网络请求

### 3. **多种加载路径**

- **网络图片**：`fetchDataFromUrl()` → `RemoteImageLoader`
- **Base64 图片**：直接使用，不经过网络
- **本地资源**：`asset://` → `$rawfile()`
- **自定义 headers**：`http.createHttp().request()`

### 4. **错误处理**

- `RemoteImageLoaderError` 统一错误类型
- 网络失败、解码失败都有相应处理

---

## 🔍 关键代码位置

### 网络请求入口

```typescript
// RemoteImageLoader.ts 第33行
const promise = fetchDataFromUrl(url, options);
```

### Base64 处理

```typescript
// RNFastImage.ets 第141行
if (uri.startsWith("data:")) {
  this.imageSource = new ImageSourceHolder(uri);
  return;
}
```

### 缓存查询

```typescript
// RemoteImageLoader.ts 第51-70行
if (this.memoryCache.has(uri)) { return ... }
if (this.diskCache.has(uri)) { return ... }
```

### 组件更新

```typescript
// RNFastImage.ets 第75-86行
onDescriptorWrapperChange() {
  if (uriChanged) {
    this.onLoadStart();
    this.updateImageSource();
  }
}
```

---

## 💡 总结

FastImage 的 ETS 实现采用**分层架构**：

1. **包注册层**：注册模块和组件
2. **TurboModule 层**：提供 JavaScript 可调用的 API
3. **组件层**：ArkUI 组件实现，处理 UI 渲染
4. **加载核心层**：网络请求、缓存管理、图片解码

**核心特点**：
- ✅ 三层缓存策略（系统 + 内存 + 磁盘）
- ✅ 请求去重机制
- ✅ 多种图片源支持（网络、Base64、本地）
- ✅ 异步加载，不阻塞 UI

**网络请求流程**：
`fetchDataFromUrl()` → `RemoteImageLoader` → 缓存检查 → 网络请求 → 解码 → 渲染


