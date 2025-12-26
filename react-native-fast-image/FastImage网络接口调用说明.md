# FastImage 网络模块接口调用说明

## 📋 概述

本文档列出 `react-native-fast-image` 在 HarmonyOS 平台上调用的所有网络相关接口，供网络模块同事排查折叠屏冷启动延迟问题。

---

## 🔍 调用的网络接口清单

### 1. **fetchDataFromUrl** ⭐ 主要接口

**位置**：`RemoteImageLoader.ts` 第 33 行

**来源**：
```typescript
import { fetchDataFromUrl, FetchOptions, FetchResult } from '@rnoh/react-native-openharmony/src/main/ets/RNOH/HttpRequestHelper';
```

**调用代码**：
```typescript
private async fetchImage(url: string, headers?: Record<string, any>): Promise<FetchResult> {
  let options: FetchOptions = {
    usingCache: true,
    headers: headers,
  };
  const promise = fetchDataFromUrl(url, options);  // ← 这里调用
  this.activeRequestByUrl.set(url, promise);
  promise.finally(() => {
    this.activeRequestByUrl.delete(url);
  });
  return promise;
}
```

**用途**：
- 用于**正常图片加载**（非预加载场景）
- 发起 HTTP/HTTPS 请求获取图片数据
- 返回 `ArrayBuffer` 格式的图片数据

**调用时机**：
- 图片不在内存缓存中
- 图片不在磁盘缓存中
- 需要从网络下载图片时

**参数**：
- `url`: 图片的 HTTP/HTTPS URL
- `options`: 
  - `usingCache: true` - 使用缓存
  - `headers`: 自定义请求头（可选）

**返回**：
- `FetchResult` 包含：
  - `result: ArrayBuffer` - 图片二进制数据
  - `headers: Record<string, string>` - 响应头（包含 `location` 用于重定向）

---

### 2. **request.downloadFile** ⭐ 预加载接口

**位置**：`RemoteImageLoader.ts` 第 168 行

**来源**：
```typescript
import request from '@ohos.request';
```

**调用代码**：
```typescript
private async performDownload(config: request.DownloadConfig): Promise<boolean> {
  return await new Promise(async (resolve, reject) => {
    try {
      if(config.header === undefined)
        delete config.header
      const downloadTask = await request.downloadFile(this.context, config);  // ← 这里调用
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

**用途**：
- 用于**预加载（prefetch）**场景
- 下载图片到本地磁盘缓存
- 支持下载进度和失败回调

**调用时机**：
- 调用 `FastImage.prefetch()` 时
- 图片需要提前下载到磁盘缓存

**参数**：
- `context: common.UIAbilityContext` - 应用上下文
- `config: request.DownloadConfig`:
  - `url: string` - 图片 URL
  - `filePath: string` - 保存路径（临时文件路径）
  - `header?: Record<string, string>` - 自定义请求头（可选）

**事件监听**：
- `downloadTask.on("complete")` - 下载完成
- `downloadTask.on("fail")` - 下载失败

---

### 3. **http.createHttp().request()** ⚠️ 备用接口

**位置**：`RNFastImage.ets` 第 94 行

**来源**：
```typescript
import http from '@ohos.net.http';
```

**调用代码**：
```typescript
requestWithHeaders() {
  http.createHttp()  // ← 这里调用
    .request(this.descriptorWrapper.props.source?.uri, 
      { header: this.descriptorWrapper.props.source?.headers },
      (error, data) => {
        if (!error) {
          let code = data.responseCode;
          if (ResponseCode.ResponseCode.OK === code) {
            let res: ArrayBuffer = data.result as ArrayBuffer
            let imageSource = image.createImageSource(res);
            // ... 处理图片
          }
        } else {
          Logger.error(`RNOH in RNFastImage http reqeust failed with. Code: ${error.code}, message: ${error.message}`);
          this.onError();
        }
      }
    )
}
```

**用途**：
- **备用方案**：当图片有自定义 headers 时使用
- 直接使用 HarmonyOS 的 HTTP 模块

**调用时机**：
- 图片 URI 有自定义 headers（`source.headers` 不为空）
- 不走 `RemoteImageLoader` 的正常流程

**参数**：
- `uri: string` - 图片 URL
- `options: { header: Record<string, string> }` - 请求头
- `callback: (error, data) => void` - 回调函数

---

## 📊 调用流程图

### 正常图片加载流程

```
FastImage 组件
    ↓
RNFastImage.ets.updateImageSource()
    ↓
检查是否有自定义 headers？
    ├─ 有 → http.createHttp().request() (接口3)
    └─ 无 → FastImageLoaderTurboModule.getRemoteImageSource()
            ↓
        RemoteImageLoader.getImageSource()
            ↓
        检查缓存？
            ├─ 内存缓存 → 直接返回
            ├─ 磁盘缓存 → 从文件加载
            └─ 无缓存 → fetchDataFromUrl() (接口1) ⚠️ 主要接口
```

### 预加载流程

```
FastImage.prefetch(uri)
    ↓
FastImageLoaderTurboModule.prefetchImage()
    ↓
RemoteImageLoader.prefetch()
    ↓
request.downloadFile() (接口2) ⚠️ 预加载接口
    ↓
下载到磁盘缓存
```

---

## 🎯 关键问题定位

### 折叠屏冷启动延迟问题

**问题现象**：
- 从开始渲染到第一张图片开始加载：**178ms** ⚠️
- 实际下载时间：2-7ms（正常）

**问题定位**：
延迟发生在调用 `fetchDataFromUrl()` 之前，即：
- **网络栈初始化延迟**（178ms）
- 不是网络传输慢
- 不是接口本身的问题

**需要排查的点**：
1. `fetchDataFromUrl()` 内部实现
   - 首次调用时是否有初始化延迟？
   - 是否在折叠屏上有特殊处理？
   - 是否有网络栈预热机制？

2. `request.downloadFile()` 首次调用
   - 是否有初始化延迟？
   - 折叠屏上是否有性能差异？

3. `http.createHttp()` 首次调用
   - 创建 HTTP 客户端是否有延迟？
   - 折叠屏上是否有性能差异？

---

## 📝 接口调用统计

### 正常图片加载（5张图片）

| 接口 | 调用次数 | 调用时机 | 延迟位置 |
|------|---------|---------|---------|
| `fetchDataFromUrl()` | 5次 | 图片加载时 | ⚠️ 首次调用延迟 178ms |
| `request.downloadFile()` | 0次 | 预加载时 | - |
| `http.createHttp().request()` | 0次 | 有自定义 headers 时 | - |

### 预加载场景

| 接口 | 调用次数 | 调用时机 | 延迟位置 |
|------|---------|---------|---------|
| `fetchDataFromUrl()` | 0次 | - | - |
| `request.downloadFile()` | N次 | prefetch() 调用时 | ⚠️ 需要测试 |
| `http.createHttp().request()` | 0次 | - | - |

---

## 🔧 建议排查方向

### 1. 检查 `fetchDataFromUrl` 实现

**文件位置**：
```
@rnoh/react-native-openharmony/src/main/ets/RNOH/HttpRequestHelper.ts
```

**排查点**：
- 首次调用时是否有网络栈初始化？
- 是否有懒加载机制？
- 折叠屏上是否有特殊处理？

### 2. 检查 `request.downloadFile` 实现

**排查点**：
- HarmonyOS 系统 API
- 首次调用时是否有初始化延迟？
- 折叠屏上是否有性能差异？

### 3. 检查 `http.createHttp` 实现

**排查点**：
- HarmonyOS 系统 API
- 创建 HTTP 客户端是否有延迟？
- 折叠屏上是否有性能差异？

### 4. 网络栈初始化时机

**问题**：
- 为什么折叠屏冷启动时网络栈初始化需要 178ms？
- 非折叠屏为什么没有这个延迟？
- 是否可以提前初始化网络栈？

---

## 📞 联系方式

如有疑问，请联系：
- FastImage 模块：xxx
- 网络模块：xxx

---

## 📅 更新记录

- 2025-12-20: 初版文档，列出所有网络接口调用


