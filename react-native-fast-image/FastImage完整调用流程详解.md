# FastImage 完整调用流程详解

## 📋 目录

1. [整体架构](#整体架构)
2. [JavaScript 层调用](#javascript-层调用)
3. [组件层处理](#组件层处理)
4. [TurboModule 层](#turbomodule-层)
5. [图片加载核心层](#图片加载核心层)
6. [网络请求层](#网络请求层)
7. [缓存处理](#缓存处理)
8. [图片解码与渲染](#图片解码与渲染)
9. [完整流程图](#完整流程图)

---

## 🏗️ 整体架构

```
JavaScript (index.tsx)
    ↓
React Native Bridge
    ↓
ArkUI 组件层 (RNFastImage.ets)
    ↓
TurboModule 层 (FastImageLoaderTurboModule.ts)
    ↓
图片加载核心层 (RemoteImageLoader.ts)
    ↓
网络请求层 (fetchDataFromUrl / request.downloadFile / http.createHttp)
    ↓
HarmonyOS 系统 API
```

---

## 📱 JavaScript 层调用

### 文件位置
`example/src/skia/index.tsx`

### 调用代码
```typescript
<FastImage
  source={{ uri: "https://picsum.photos/800/600?random=1" }}
  onLoadStart={() => handleLoadStart(0)}
  onLoad={(e) => handleLoad(0, e)}
  onLoadEnd={() => {}}
  resizeMode="cover"
/>
```

### 执行流程

1. **React 渲染阶段**
   - React 创建虚拟 DOM
   - 调用 `FastImage` 组件（来自 `react-native-fast-image` 库）

2. **FastImage 组件处理**（JavaScript 层）
   - 接收 `source`、`onLoadStart`、`onLoad` 等 props
   - 将 props 传递给原生组件

3. **Bridge 通信**
   - React Native Bridge 将 props 序列化
   - 通过 JSI/TurboModule 传递给原生层

---

## 🎨 组件层处理

### 文件位置
`example/harmony/entry/oh_modules/@react-native-oh-tpl/react-native-fast-image/src/main/ets/RNFastImage.ets`

### 关键方法

#### 1. `aboutToAppear()` - 组件初始化

```typescript
aboutToAppear() {
  // 第64-72行
  this.eventEmitter = new FastImageView.EventEmitter(this.ctx.rnInstance, this.tag)
  this.onDescriptorWrapperChange(this.ctx.descriptorRegistry.findDescriptorWrapperByTag<FastImageView.DescriptorWrapper>(this.tag)!)
  this.cleanUpCallbacks.push(this.ctx.descriptorRegistry.subscribeToDescriptorChanges(this.tag,
    (_descriptor, newDescriptorWrapper) => {
      this.onDescriptorWrapperChange(newDescriptorWrapper! as FastImageView.DescriptorWrapper)
    }
  ))
}
```

**作用**：
- 创建事件发射器（用于向 JavaScript 发送事件）
- 订阅 props 变化
- 初始化时调用 `onDescriptorWrapperChange()`

---

#### 2. `onDescriptorWrapperChange()` - Props 变化处理

```typescript
private onDescriptorWrapperChange(descriptorWrapper: FastImageView.DescriptorWrapper) {
  // 第75-87行
  let uriChanged = false;
  if (this.descriptorWrapper.props === undefined) {
    uriChanged = true;
  } else {
    uriChanged = descriptorWrapper.props.source?.uri !== this.descriptorWrapper.props.source?.uri;
  }
  this.descriptorWrapper = descriptorWrapper
  if (uriChanged) {
    this.onLoadStart();  // 触发 onLoadStart 事件
    this.updateImageSource();  // 更新图片源
  }
}
```

**执行时机**：
- 组件首次创建
- `source.uri` 发生变化

**关键步骤**：
1. 检测 URI 是否变化
2. 触发 `onLoadStart()` 事件（JavaScript 层收到）
3. 调用 `updateImageSource()` 开始加载

---

#### 3. `updateImageSource()` - 图片源更新 ⭐ 核心方法

```typescript
async updateImageSource() {
  // 第135-178行
  const uri = this.descriptorWrapper.props.source?.uri ?? '';
  
  // 1. 处理 asset:// 本地资源
  if (uri.startsWith("asset://")) {
    this.imageSource = new ImageSourceHolder($rawfile(uri.replace("asset://", this.ctx.rnInstance.getAssetsDest())));
    return;
  }
  
  // 2. 处理 data: Base64 图片
  if (uri.startsWith("data:")) {
    this.imageSource = new ImageSourceHolder(uri);
    return;
  }
  
  // 3. 处理带自定义 headers 的请求
  if (this.descriptorWrapper.props.source?.headers && this.descriptorWrapper.props.source?.headers.length > 0) {
    this.requestWithHeaders()  // 使用 http.createHttp()
    return;
  }
  
  // 4. 普通网络图片（主要流程）
  const imageLoader = this.ctx.rnInstance.getTurboModule<ImageLoaderTurboModule>("FastImageLoader");
  const queryCacheResult = await imageLoader.queryCache([uri]) as Record<string, string>;
  const isNotCached = !queryCacheResult[uri];
  
  // 5. 如果禁用 ImageLoader 且无缓存，直接使用 URI
  const skipImageLoader = true && !this.ctx.rnInstance.isFeatureFlagEnabled("IMAGE_LOADER");
  if (skipImageLoader && isNotCached) {
    this.imageSource = new ImageSourceHolder(uri);
    return;
  }
  
  // 6. 通过 TurboModule 加载图片
  this.imageSource = undefined;
  imageLoader.getRemoteImageSource(uri).then(async (remoteImage) => {
    try {
      const imageSource = remoteImage.getImageSource();
      const frameCounter = await imageSource.getFrameCount();
      if (frameCounter === 1) {
        // 静态图片：转换为 PixelMap
        this.imageSource = new ImageSourceHolder(await imageLoader.getPixelMap(uri));
      } else {
        // 动画 GIF：使用文件路径
        this.imageSource = new ImageSourceHolder(remoteImage.getLocation())
      }
    } catch (error) {
      this.onError();
      this.onLoadEnd()
    }
  }).catch((error: RemoteImageLoaderError) => {
    this.onError()
    this.onLoadEnd()
  })
}
```

**URI 类型判断流程**：

```
updateImageSource()
    ↓
检查 URI 前缀
    ├─ asset:// → 本地资源，直接使用 $rawfile()
    ├─ data: → Base64，直接使用 URI
    ├─ 有 headers → requestWithHeaders() (http.createHttp)
    └─ 普通 URL → FastImageLoaderTurboModule.getRemoteImageSource()
```

---

#### 4. `requestWithHeaders()` - 自定义 Headers 请求

```typescript
requestWithHeaders() {
  // 第93-133行
  http.createHttp()
    .request(this.descriptorWrapper.props.source?.uri, 
      { header: this.descriptorWrapper.props.source?.headers },
      (error, data) => {
        if (!error) {
          let code = data.responseCode;
          if (ResponseCode.ResponseCode.OK === code) {
            let res: ArrayBuffer = data.result as ArrayBuffer
            let imageSource = image.createImageSource(res);
            // 创建 PixelMap
            imageSource.createPixelMap(options, (err, pixelMap) => {
              if (err) {
                this.onError();
              } else {
                this.imageSource = new ImageSourceHolder(pixelMap)
              }
            })
          }
        } else {
          this.onError();
        }
      }
    )
}
```

**特点**：
- 不走 `RemoteImageLoader` 流程
- 直接使用 `http.createHttp().request()`
- 适用于需要自定义 headers 的场景

---

## 🔌 TurboModule 层

### 文件位置
`example/harmony/entry/oh_modules/@react-native-oh-tpl/react-native-fast-image/src/main/ets/FastImageLoaderTurboModule.ts`

### 关键方法

#### 1. `getRemoteImageSource(uri)` - 获取图片源

```typescript
public async getRemoteImageSource(uri: string): Promise<RemoteImageSource> {
  // 第85-99行
  try {
    const imageSource = await this.imageLoader.getImageSource(uri);
    return imageSource;
  }
  catch (e) {
    // 错误处理
    if (!(e instanceof RemoteImageLoaderError) && e instanceof Object && e.message) {
      throw new RemoteImageLoaderError(`Failed to load the image: ${e.message}`);
    }
    if (typeof e === 'string') {
      throw new RemoteImageLoaderError(e);
    }
    throw e;
  }
}
```

**调用链**：
```
getRemoteImageSource(uri)
    ↓
this.imageLoader.getImageSource(uri)  // RemoteImageLoader
```

---

#### 2. `getPixelMap(uri)` - 获取 PixelMap

```typescript
public async getPixelMap(uri: string): Promise<image.PixelMap> {
  // 第105-118行
  try {
    return await this.imageLoader.getPixelMap(uri);
  }
  catch (e) {
    // 错误处理
  }
}
```

**调用链**：
```
getPixelMap(uri)
    ↓
this.imageLoader.getPixelMap(uri)  // RemoteImageLoader
    ↓
getImageSource(uri) → getPixelMapPromise()
```

---

#### 3. `prefetchImage(uri, headers)` - 预加载

```typescript
public async prefetchImage(uri: string, headers?: object): Promise<boolean> {
  // 第64-66行
  return this.imageLoader.prefetch(uri, headers);
}
```

**调用链**：
```
prefetchImage(uri)
    ↓
this.imageLoader.prefetch(uri)  // RemoteImageLoader
    ↓
downloadFile(uri)  // 下载到磁盘
```

---

#### 4. `queryCache(uris)` - 查询缓存

```typescript
public queryCache(uris: Array<string>): Promise<Object> {
  // 第77-83行
  const cachedUriEntries = uris.map(uri =>
    [uri, this.imageLoader.queryCache(uri)]
  ).filter(([_uri, value]) => value !== undefined);
  const cachedUriMap = Object.fromEntries(cachedUriEntries)
  return Promise.resolve(cachedUriMap)
}
```

**返回格式**：
```typescript
{
  "https://example.com/image.jpg": "memory" | "disk" | undefined
}
```

---

## 🎯 图片加载核心层

### 文件位置
`example/harmony/entry/oh_modules/@react-native-oh-tpl/react-native-fast-image/src/main/ets/FastRemoteImageLoader/RemoteImageLoader.ts`

### 核心数据结构

```typescript
export class RemoteImageLoader {
  // 请求去重：相同 URL 的并发请求复用同一个 Promise
  private activeRequestByUrl: Map<string, Promise<FetchResult>> = new Map();
  
  // 预加载去重：相同 URL 的并发预加载复用同一个 Promise
  private activePrefetchByUrl: Map<string, Promise<boolean>> = new Map();
  
  // 内存缓存
  private memoryCache: RemoteImageMemoryCache;
  
  // 磁盘缓存
  private diskCache: RemoteImageDiskCache;
  
  // UIAbilityContext（用于下载文件）
  private context: common.UIAbilityContext;
}
```

---

### 关键方法详解

#### 1. `getImageSource(uri, headers)` ⭐ 核心方法

```typescript
public async getImageSource(uri: string, headers?: Record<string, any>): Promise<RemoteImageSource> {
  // 第46-112行
  
  // 步骤1: 检查 Base64
  if (uri.startsWith("data:")) {
    const imageSource = image.createImageSource(uri);
    return new RemoteImageSource(imageSource, '');
  }
  
  // 步骤2: 检查内存缓存
  if (this.memoryCache.has(uri)) {
    return this.memoryCache.get(uri);  // ✅ 直接返回，最快
  }
  
  // 步骤3: 等待预加载完成（如果正在预加载）
  if (this.activePrefetchByUrl.has(uri)) {
    await this.activePrefetchByUrl.get(uri).catch(() => {});
  }
  
  // 步骤4: 检查磁盘缓存
  if (this.diskCache.has(uri)) {
    const location = `file://${this.diskCache.getLocation(uri)}`;
    const imageSource = image.createImageSource(location);
    const remoteImageSource = new RemoteImageSource(imageSource, location);
    this.memoryCache.set(uri, remoteImageSource);  // 加载到内存缓存
    return remoteImageSource;  // ✅ 从磁盘加载
  }
  
  // 步骤5: 网络请求（无缓存时）
  let response: FetchResult;
  try {
    response = await this.fetchImage(uri, headers);  // ⚠️ 网络请求
  } catch (e) {
    throw new RemoteImageLoaderError(e.message ?? 'Failed to fetch the image');
  }
  
  // 步骤6: 再次检查内存缓存（并发请求可能已缓存）
  if (this.memoryCache.has(uri)) {
    return this.memoryCache.get(uri);
  }
  
  // 步骤7: 创建 ImageSource
  const imageSource = image.createImageSource(response.result);  // ArrayBuffer → ImageSource
  const location = response.headers['location'] ?? uri;  // 处理重定向
  const remoteImageSource = new RemoteImageSource(imageSource, location);
  this.memoryCache.set(uri, remoteImageSource);  // 保存到内存缓存
  
  // 步骤8: 异步保存到磁盘缓存
  if (!this.activePrefetchByUrl.has(uri) && !this.diskCache.has(uri)) {
    const promise = this.saveFile(uri, response.result);
    this.activePrefetchByUrl.set(uri, promise);
    promise.finally(() => {
      this.activePrefetchByUrl.delete(uri);
    });
  }
  
  return remoteImageSource;
}
```

**完整流程**：

```
getImageSource(uri)
    ↓
检查 Base64? → 是 → 直接创建 ImageSource
    ↓ 否
检查内存缓存? → 是 → 直接返回 ✅
    ↓ 否
等待预加载完成（如果正在预加载）
    ↓
检查磁盘缓存? → 是 → 从文件加载 → 保存到内存缓存 → 返回 ✅
    ↓ 否
网络请求 fetchImage(uri) ⚠️
    ↓
再次检查内存缓存（并发请求可能已缓存）? → 是 → 返回 ✅
    ↓ 否
ArrayBuffer → ImageSource
    ↓
保存到内存缓存
    ↓
异步保存到磁盘缓存
    ↓
返回 RemoteImageSource
```

---

#### 2. `fetchImage(url, headers)` - 网络请求 ⚠️ 关键方法

```typescript
private async fetchImage(url: string, headers?: Record<string, any>): Promise<FetchResult> {
  // 第24-39行
  
  // 请求去重：相同 URL 的并发请求复用同一个 Promise
  if (this.activeRequestByUrl.has(url)) {
    return this.activeRequestByUrl.get(url);  // ✅ 复用请求
  }
  
  // 创建请求选项
  let options: FetchOptions = {
    usingCache: true,  // 使用缓存
    headers: headers,   // 自定义请求头
  };
  
  // ⚠️ 发起网络请求（这里是网络延迟的关键点）
  const promise = fetchDataFromUrl(url, options);
  
  // 记录活跃请求
  this.activeRequestByUrl.set(url, promise);
  
  // 请求完成后清理
  promise.finally(() => {
    this.activeRequestByUrl.delete(url);
  });
  
  return promise;
}
```

**关键点**：
- **请求去重**：相同 URL 的并发请求只发起一次网络请求
- **网络接口**：`fetchDataFromUrl()` 来自 `@rnoh/react-native-openharmony`
- **返回类型**：`FetchResult` 包含 `result: ArrayBuffer` 和 `headers: Record<string, string>`

---

#### 3. `prefetch(uri, headers)` - 预加载

```typescript
public async prefetch(uri: string, headers?: object): Promise<boolean> {
  // 第133-161行
  
  // 检查磁盘缓存
  if (this.diskCache.has(uri)) {
    return true;  // ✅ 已缓存
  }
  
  // 检查是否正在预加载
  if (this.activePrefetchByUrl.has(uri)) {
    return await this.activePrefetchByUrl.get(uri);  // ✅ 复用预加载
  }
  
  // 从内存缓存中移除（预加载只下载到磁盘，不加载到内存）
  if (this.memoryCache.has(uri)) {
    this.memoryCache.remove(uri);
  }
  
  // 下载文件
  const promise = this.downloadFile(uri, headers);
  this.activePrefetchByUrl.set(uri, promise);
  
  // 错误处理
  promise.catch((e) => {
    this.onDownloadFileFail({ remoteUri: uri });
  })
  
  // 下载完成后通知
  promise.finally(() => {
    this.activePrefetchByUrl.delete(uri);
    const fileUri = `file://${this.diskCache.getLocation(uri)}`;
    this.onDiskCacheUpdate({ remoteUri: uri, fileUri })
  });
  
  return await promise;
}
```

**特点**：
- 只下载到磁盘，不加载到内存
- 使用 `request.downloadFile()` 下载
- 下载完成后通知 C++ 层更新映射

---

#### 4. `downloadFile(uri, headers)` - 文件下载

```typescript
private async downloadFile(uri: string, headers?: object): Promise<boolean> {
  // 第181-199行
  
  const path = this.diskCache.getLocation(uri);
  const tempPath = path + '_tmp';  // 临时文件路径
  
  try {
    // 删除已存在的临时文件
    if (fs.accessSync(tempPath)) {
      await fs.unlink(tempPath);
    }
    
    // ⚠️ 下载到临时文件
    await this.performDownload({ url: uri, filePath: tempPath, header: headers });
    
    // 移动到最终位置（原子操作，避免损坏文件）
    await fs.moveFile(tempPath, path);
    
    // 更新磁盘缓存记录
    this.diskCache.set(uri);
  } catch (e) {
    return Promise.reject(e);
  }
  
  return true;
}
```

**安全机制**：
- 先下载到临时文件（`_tmp` 后缀）
- 下载完成后移动到最终位置（原子操作）
- 避免下载中断导致文件损坏

---

#### 5. `performDownload(config)` - 执行下载

```typescript
private async performDownload(config: request.DownloadConfig): Promise<boolean> {
  // 第163-179行
  
  return await new Promise(async (resolve, reject) => {
    try {
      if(config.header === undefined)
        delete config.header
      
      // ⚠️ 使用 HarmonyOS 的下载 API
      const downloadTask = await request.downloadFile(this.context, config);
      
      downloadTask.on("complete", () => {
        resolve(true);
      });
      
      downloadTask.on("fail", (err: number) => {
        reject(`Failed to download the task. Code: ${err}`)
      });
    } catch (e) {
      reject(e);
    }
  });
}
```

**网络接口**：`request.downloadFile()` 来自 `@ohos.request`

---

#### 6. `saveFile(uri, arrayBuffer)` - 保存到磁盘

```typescript
private async saveFile(uri: string, arrayBuffer: ArrayBuffer) {
  // 第114-131行
  
  try {
    const path = this.diskCache.getLocation(uri);
    const file = await fs.open(
      path,
      fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE,
    );
    await fs.write(file.fd, arrayBuffer);
    fs.close(file);
    this.diskCache.set(uri);  // 更新缓存记录
  } catch (error) {
    console.error('Error occurred when storing file for disk cache ' + error.message);
  }
  return true;
}
```

**用途**：
- 在 `getImageSource()` 中，网络请求成功后异步保存到磁盘
- 下次访问时可以从磁盘缓存快速加载

---

## 🌐 网络请求层

### 三个网络接口

#### 1. `fetchDataFromUrl()` ⭐ 主要接口

**位置**：`@rnoh/react-native-openharmony/src/main/ets/RNOH/HttpRequestHelper.ts`

**调用位置**：`RemoteImageLoader.ts` 第 33 行

**用途**：正常图片加载（非预加载）

**参数**：
```typescript
fetchDataFromUrl(url: string, options: FetchOptions): Promise<FetchResult>

interface FetchOptions {
  usingCache: boolean;
  headers?: Record<string, any>;
}

interface FetchResult {
  result: ArrayBuffer;  // 图片二进制数据
  headers: Record<string, string>;  // 响应头（包含 location 用于重定向）
}
```

**特点**：
- 返回 `ArrayBuffer`，可直接用于创建 `ImageSource`
- 支持自定义 headers
- 支持缓存

---

#### 2. `request.downloadFile()` ⭐ 预加载接口

**位置**：`@ohos.request`（HarmonyOS 系统 API）

**调用位置**：`RemoteImageLoader.ts` 第 168 行

**用途**：预加载图片到磁盘

**参数**：
```typescript
request.downloadFile(
  context: common.UIAbilityContext,
  config: request.DownloadConfig
): Promise<request.DownloadTask>

interface DownloadConfig {
  url: string;
  filePath: string;
  header?: Record<string, string>;
}
```

**特点**：
- 直接下载到文件系统
- 支持进度回调
- 适用于预加载场景

---

#### 3. `http.createHttp().request()` ⭐ 自定义 Headers 接口

**位置**：`@ohos.net.http`（HarmonyOS 系统 API）

**调用位置**：`RNFastImage.ets` 第 94 行

**用途**：带自定义 headers 的请求

**参数**：
```typescript
http.createHttp().request(
  url: string,
  options: { header?: Record<string, string> },
  callback: (error, data) => void
)
```

**特点**：
- 不走 `RemoteImageLoader` 流程
- 直接处理响应
- 适用于需要自定义 headers 的场景

---

## 💾 缓存处理

### 三层缓存策略

#### 1. 系统缓存（HarmonyOS 系统级）

**配置位置**：`FastImagePackage.ts` 第 62-66 行

```typescript
app.setImageCacheCount(200);  // 最多缓存 200 个图片
app.setImageRawDataCacheSize(100*1024*1024);  // 原始数据缓存 100MB
app.setImageFileCacheSize(100*1024*1024);  // 文件缓存 100MB
```

**特点**：
- HarmonyOS 系统级缓存
- 自动管理
- 应用退出后可能保留

---

#### 2. 内存缓存（应用级）

**文件位置**：`FastRemoteImageLoader/RemoteImageCache.ts`

**实现**：
```typescript
export class RemoteImageMemoryCache extends RemoteImageCache<RemoteImageSource> {
  protected data: Map<string, RemoteImageSource>;  // URI → RemoteImageSource
  protected maxSize: number;  // 默认 128
}
```

**LRU 策略**：
- 超过 `maxSize` 时删除最旧的条目
- 使用 `Map` 保持插入顺序

**清理**：
```typescript
public memoryCacheClear(): boolean {
  // 释放所有 ImageSource
  for (const entry of this.data.entries()) {
    PromiseArr.push(entry[1].release().then(() => {
      this.data.delete(entry[0])
    }))
  }
  return true;
}
```

---

#### 3. 磁盘缓存（应用级）

**文件位置**：`FastRemoteImageLoader/RemoteImageDiskCache.ts`

**实现**：
```typescript
export class RemoteImageDiskCache extends RemoteImageCache<boolean> {
  private cacheDir: string;  // 缓存目录
  protected data: Map<string, boolean>;  // 文件名 → 是否存在
  protected maxSize: number;  // 默认 128
}
```

**缓存键生成**：
```typescript
private getCacheKey(uri: string): string {
  // 去除特殊字符
  return uri.replace(/[^a-zA-Z0-9 -]/g, '');
}
```

**文件路径**：
```typescript
private getFilePath(key: string): string {
  return this.cacheDir + '/' + key;
}
```

**清理**：
```typescript
public diskCacheClear() {
  for (const key of this.data.keys()) {
    fs.unlink(this.cacheDir + '/' + key);
    this.data.delete(key);
  }
}
```

---

## 🖼️ 图片解码与渲染

### 解码流程

#### 1. 从 ArrayBuffer 创建 ImageSource

```typescript
// RemoteImageLoader.ts 第 89 行
const imageSource = image.createImageSource(response.result);  // ArrayBuffer → ImageSource
```

**系统 API**：`@ohos.multimedia.image.createImageSource()`

---

#### 2. 创建 PixelMap（静态图片）

```typescript
// RNFastImage.ets 第 165 行
this.imageSource = new ImageSourceHolder(await imageLoader.getPixelMap(uri));

// RemoteImageLoader.ts 第 41-44 行
public async getPixelMap(uri: string): Promise<image.PixelMap> {
  const imageSource = await this.getImageSource(uri);
  return await imageSource.getPixelMapPromise();
}

// RemoteImageSource.ts 第 17-37 行
async getPixelMapPromise(): Promise<image.PixelMap> {
  if (this.pixelMapPromise === undefined) {
    return this.createPixelMap();
  }
  return this.pixelMapPromise;
}

private createPixelMap(): Promise<image.PixelMap> {
  const pixelMapPromise = (async () => {
    if (await this.imageSource.getFrameCount() === 1) {
      return this.imageSource.createPixelMap()  // 单帧图片
    }
    throw Error("Cannot create a PixelMap for an animated image");  // GIF 不支持
  })()
  this.pixelMapPromise = pixelMapPromise;
  return pixelMapPromise;
}
```

**特点**：
- 静态图片（JPEG、PNG）：转换为 `PixelMap`
- 动画图片（GIF）：使用文件路径，不转换为 `PixelMap`

---

#### 3. 渲染到 ArkUI Image 组件

```typescript
// RNFastImage.ets 第 195-236 行
build() {
  RNViewBase({ ctx: this.ctx, tag: this.tag }) {
    if (this.imageSource?.source) {
      Image(this.imageSource.source)  // PixelMap | string | Resource
        .interpolation(ImageInterpolation.High)
        .objectFit(this.getResizeMode(this.descriptorWrapper.props.resizeMode))
        .onComplete((event) => {
          if (event.loadingStatus) {
            this.onLoadEnd();
            this.onProgress(1, 1);
          } else {
            this.onLoad(event.width, event.height);  // ⚠️ 触发 onLoad 事件
          }
        })
        .onError((event) => {
          this.onError();
        })
    }
  }
}
```

**事件触发**：
- `onComplete` → `onLoad(width, height)` → JavaScript 层收到 `onLoad` 事件
- `onComplete` → `onLoadEnd()` → JavaScript 层收到 `onLoadEnd` 事件

---

## 📊 完整流程图

### 场景 1: 网络图片加载（无缓存）

```
┌─────────────────────────────────────────────────────────────────┐
│ JavaScript 层 (index.tsx)                                       │
└─────────────────────────────────────────────────────────────────┘
                    ↓
    <FastImage source={{ uri: "https://..." }} />
                    ↓
┌─────────────────────────────────────────────────────────────────┐
│ React Native Bridge                                             │
│ - 序列化 props                                                   │
│ - 通过 JSI/TurboModule 传递                                      │
└─────────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────────┐
│ 组件层 (RNFastImage.ets)                                       │
│                                                                 │
│ aboutToAppear()                                                 │
│   ↓                                                             │
│ onDescriptorWrapperChange()                                     │
│   ↓                                                             │
│ onLoadStart() → emit("fastImageLoadStart") → JS 层收到          │
│   ↓                                                             │
│ updateImageSource()                                             │
│   ├─ asset:// → 直接使用 $rawfile()                            │
│   ├─ data: → 直接使用 URI                                       │
│   ├─ 有 headers → requestWithHeaders() (http.createHttp)       │
│   └─ 普通 URL → FastImageLoaderTurboModule.getRemoteImageSource() ⭐
└─────────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────────┐
│ TurboModule 层 (FastImageLoaderTurboModule.ts)                 │
│                                                                 │
│ getRemoteImageSource(uri)                                       │
│   ↓                                                             │
│ this.imageLoader.getImageSource(uri)                            │
└─────────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────────┐
│ 图片加载核心层 (RemoteImageLoader.ts)                          │
│                                                                 │
│ getImageSource(uri)                                             │
│   ├─ 检查 Base64? → 否                                          │
│   ├─ 检查内存缓存? → 否                                          │
│   ├─ 检查磁盘缓存? → 否                                          │
│   └─ 网络请求 fetchImage(uri) ⚠️                                │
│       ↓                                                         │
│   fetchImage(uri)                                               │
│     ├─ 请求去重检查? → 否                                        │
│     └─ fetchDataFromUrl(url, options) ⚠️ 网络请求               │
│         ↓                                                       │
│     返回 FetchResult { result: ArrayBuffer, headers: {...} }    │
│       ↓                                                         │
│   image.createImageSource(response.result)                      │
│       ↓                                                         │
│   创建 RemoteImageSource                                        │
│       ↓                                                         │
│   保存到内存缓存                                                │
│       ↓                                                         │
│   异步保存到磁盘缓存                                            │
│       ↓                                                         │
│   返回 RemoteImageSource                                        │
└─────────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────────┐
│ 组件层 (RNFastImage.ets)                                       │
│                                                                 │
│ imageLoader.getRemoteImageSource(uri).then(async (remoteImage) │
│   ↓                                                             │
│ const imageSource = remoteImage.getImageSource()                │
│ const frameCounter = await imageSource.getFrameCount()          │
│   ├─ frameCounter === 1 → 静态图片                             │
│   │   ↓                                                         │
│   │ await imageLoader.getPixelMap(uri)                         │
│   │   ↓                                                         │
│   │ remoteImage.getPixelMapPromise()                           │
│   │   ↓                                                         │
│   │ imageSource.createPixelMap()                               │
│   │   ↓                                                         │
│   │ this.imageSource = new ImageSourceHolder(pixelMap)         │
│   │                                                             │
│   └─ frameCounter > 1 → 动画 GIF                                │
│       ↓                                                         │
│     this.imageSource = new ImageSourceHolder(remoteImage.getLocation())
└─────────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────────┐
│ ArkUI 渲染层                                                    │
│                                                                 │
│ build() {                                                       │
│   Image(this.imageSource.source)                                │
│     .onComplete((event) => {                                   │
│       if (!event.loadingStatus) {                               │
│         this.onLoad(event.width, event.height)                 │
│           ↓                                                     │
│         emit("fastImageLoad", { width, height })                │
│       }                                                         │
│     })                                                          │
│ }                                                               │
└─────────────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────────────┐
│ JavaScript 层 (index.tsx)                                       │
│                                                                 │
│ onLoad={(e) => handleLoad(0, e)}                               │
│   ↓                                                             │
│ 收到 onLoad 事件，图片显示完成 ✅                                │
└─────────────────────────────────────────────────────────────────┘
```

---

### 场景 2: 有内存缓存

```
getImageSource(uri)
    ↓
检查内存缓存? → 是 ✅
    ↓
return this.memoryCache.get(uri)  // 直接返回，最快
```

**耗时**：< 1ms

---

### 场景 3: 有磁盘缓存

```
getImageSource(uri)
    ↓
检查内存缓存? → 否
    ↓
检查磁盘缓存? → 是 ✅
    ↓
const location = `file://${this.diskCache.getLocation(uri)}`
    ↓
image.createImageSource(location)  // 从文件加载
    ↓
保存到内存缓存
    ↓
return remoteImageSource
```

**耗时**：5-20ms（取决于文件大小）

---

### 场景 4: 预加载流程

```
FastImage.prefetch(uri)
    ↓
FastImageLoaderTurboModule.prefetchImage(uri)
    ↓
RemoteImageLoader.prefetch(uri)
    ↓
检查磁盘缓存? → 否
    ↓
downloadFile(uri)
    ↓
performDownload({ url: uri, filePath: tempPath })
    ↓
request.downloadFile(context, config) ⚠️ 下载接口
    ↓
下载到临时文件
    ↓
fs.moveFile(tempPath, path)  // 移动到最终位置
    ↓
this.diskCache.set(uri)  // 更新缓存记录
    ↓
onDiskCacheUpdate({ remoteUri: uri, fileUri })  // 通知 C++ 层
```

**特点**：
- 只下载到磁盘，不加载到内存
- 下次访问时从磁盘缓存快速加载

---

## 🎯 关键时间点

### 正常加载流程（无缓存）

| 时间点 | 操作 | 代码位置 | 耗时 |
|--------|------|----------|------|
| T0 | JavaScript 渲染开始 | `index.tsx` | 0ms |
| T1 | `onLoadStart` 事件 | `RNFastImage.ets:84` | ~1ms |
| T2 | 调用 `getRemoteImageSource()` | `RNFastImage.ets:159` | ~2ms |
| T3 | 检查缓存（无） | `RemoteImageLoader.ts:51-62` | ~3ms |
| T4 | 调用 `fetchImage()` | `RemoteImageLoader.ts:75` | ~4ms |
| T5 | 调用 `fetchDataFromUrl()` ⚠️ | `RemoteImageLoader.ts:33` | **~178ms**（折叠屏冷启动延迟） |
| T6 | 网络请求完成 | `fetchDataFromUrl` 返回 | ~180ms |
| T7 | 创建 `ImageSource` | `RemoteImageLoader.ts:89` | ~185ms |
| T8 | 创建 `PixelMap` | `RemoteImageSource.ts:28` | ~200ms |
| T9 | `onLoad` 事件 | `RNFastImage.ets:225` | ~205ms |

**关键延迟点**：T4 → T5（网络栈初始化延迟，折叠屏上约 178ms）

---

## 📝 总结

### 核心流程

1. **JavaScript 层**：调用 `<FastImage source={{ uri }} />`
2. **组件层**：`RNFastImage.ets` 处理 props 变化，调用 TurboModule
3. **TurboModule 层**：`FastImageLoaderTurboModule` 提供 API
4. **加载核心层**：`RemoteImageLoader` 管理缓存和网络请求
5. **网络请求层**：`fetchDataFromUrl()` / `request.downloadFile()` / `http.createHttp()`
6. **解码渲染层**：`ImageSource` → `PixelMap` → ArkUI `Image` 组件

### 关键设计

- ✅ **三层缓存**：系统缓存 + 内存缓存 + 磁盘缓存
- ✅ **请求去重**：相同 URL 的并发请求复用
- ✅ **异步加载**：不阻塞 UI 线程
- ✅ **多种 URI 支持**：网络、Base64、本地资源
- ✅ **错误处理**：网络失败、解码失败都有处理

### 性能优化点

1. **内存缓存**：最快，< 1ms
2. **磁盘缓存**：较快，5-20ms
3. **网络请求**：最慢，取决于网络和系统初始化
4. **请求去重**：避免重复网络请求

### 折叠屏延迟问题

**问题定位**：延迟发生在 `fetchDataFromUrl()` 调用之前（网络栈初始化延迟）

**解决方案**：
- 网络栈预热
- 预加载关键图片
- 使用本地资源或 Base64（绕过网络）

