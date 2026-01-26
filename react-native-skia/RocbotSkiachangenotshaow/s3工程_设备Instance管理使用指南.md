# s3 工程 - 设备 Instance 管理使用指南

## 概述

在 s3 工程中，我们已经创建了 `DeviceInstanceManager` 来管理**一个设备对应一个 RNInstance** 的场景。

## 文件位置

- **管理器文件**: `b/c/d/e/entry/src/main/ets/rn/DeviceInstanceManager.ets`
- **导出位置**: 需要在 `b/c/d/e/entry/src/main/ets/rn/index.ets` 中导出

## 快速开始

### 1. 导出 DeviceInstanceManager

在 `b/c/d/e/entry/src/main/ets/rn/index.ets` 中添加：

```typescript
export { DeviceInstanceManager, DeviceInfo } from './DeviceInstanceManager';
```

### 2. 在页面中使用

#### 示例 1: 设备列表页面

```typescript
// b/c/d/e/entry/src/main/ets/pages/RobotListPage.ets
import { DeviceInstanceManager } from '../rn';
import { RNOHCoreContext } from '@rnoh/react-native-openharmony';

@Component
export default struct RobotListPage {
  @StorageLink('RNOHCoreContext') rnohCoreContext: RNOHCoreContext | undefined = undefined;
  @State devices: DeviceInfo[] = [];
  
  aboutToAppear() {
    this.discoverDevices();
  }
  
  async discoverDevices() {
    // 模拟从服务器获取设备列表
    const deviceList = [
      { deviceId: 'robot_001', deviceName: '客厅扫地机器人' },
      { deviceId: 'robot_002', deviceName: '卧室扫地机器人' },
      { deviceId: 'robot_003', deviceName: '厨房扫地机器人' }
    ];
    
    if (!this.rnohCoreContext) {
      console.error('RNOHCoreContext 未初始化');
      return;
    }
    
    // 为每个设备创建 Instance
    for (const deviceInfo of deviceList) {
      await DeviceInstanceManager.getOrCreateDeviceInstance(
        this.rnohCoreContext,
        deviceInfo.deviceId,
        deviceInfo.deviceName,
        'bundle/robot/robot.harmony.bundle'  // 设备专属 bundle 路径
      );
    }
    
    // 获取所有设备
    this.devices = DeviceInstanceManager.getAllDevices();
  }
  
  build() {
    Column() {
      List() {
        ForEach(this.devices, (device: DeviceInfo) => {
          ListItem() {
            NavigationLink({
              target: `pages/RobotControlPage?deviceId=${device.deviceId}`
            }) {
              Column() {
                Text(device.deviceName)
                  .fontSize(18)
                Text(`状态: ${device.status}`)
                  .fontSize(14)
                  .fontColor(Color.Gray)
              }
            }
          }
        })
      }
    }
  }
}
```

#### 示例 2: 设备控制页面

```typescript
// b/c/d/e/entry/src/main/ets/pages/RobotControlPage.ets
import { DeviceInstanceManager } from '../rn';
import { RNSurface } from '@rnoh/react-native-openharmony';

@Component
export default struct RobotControlPage {
  @Param deviceId: string = '';
  private instance: RNInstance | undefined = undefined;
  private ctx: RNComponentContext | undefined = undefined;
  
  aboutToAppear() {
    // 获取设备对应的 Instance
    this.instance = DeviceInstanceManager.getDeviceInstance(this.deviceId);
    this.ctx = DeviceInstanceManager.getDeviceContext(this.deviceId);
    
    if (!this.instance) {
      console.error(`设备 ${this.deviceId} 的 Instance 不存在`);
      return;
    }
    
    // 更新设备状态为使用中
    DeviceInstanceManager.updateDeviceStatus(this.deviceId, 'online');
  }
  
  aboutToDisappear() {
    // 页面消失时，可以更新设备状态
    // DeviceInstanceManager.updateDeviceStatus(this.deviceId, 'offline');
  }
  
  build() {
    Column() {
      if (this.instance && this.ctx) {
        // 使用该设备的 Instance 渲染控制界面
        RNSurface({
          rnInstance: this.instance,
          appKey: 'RobotControl',
          initialProps: {
            deviceId: this.deviceId,
            // 传递设备专属参数
          }
        })
      } else {
        Text('设备未连接')
          .fontSize(16)
          .fontColor(Color.Red)
      }
    }
    .width('100%')
    .height('100%')
  }
}
```

#### 示例 3: 在 BasePrecreateView 中使用（可选）

如果你想在现有的 `BasePrecreateView` 中支持设备 Instance，可以这样修改：

```typescript
// 在 BasePrecreateView.ets 中添加设备支持
@Component
export struct BasePrecreateView {
  // ... 现有属性
  public deviceId?: string;  // 新增：设备 ID（可选）
  
  private async getOrCreateRNInstance(): Promise<CachedInstance> {
    // 如果指定了 deviceId，使用设备 Instance 管理器
    if (this.deviceId) {
      const rnohCoreContext = this.rnohCoreContext;
      if (rnohCoreContext) {
        const instance = await DeviceInstanceManager.getOrCreateDeviceInstance(
          rnohCoreContext,
          this.deviceId,
          `设备_${this.deviceId}`,
          this.bundlePath
        );
        if (instance) {
          return { rnInstance: instance };
        }
      }
    }
    
    // 否则使用原有的逻辑
    // ... 原有代码
  }
}
```

## API 说明

### 核心方法

#### 1. `getOrCreateDeviceInstance()`
创建或获取设备的 RNInstance

```typescript
DeviceInstanceManager.getOrCreateDeviceInstance(
  rnohCoreContext: RNOHCoreContext,
  deviceId: string,
  deviceName: string,
  bundlePath?: string
): Promise<RNInstance | undefined>
```

**参数：**
- `rnohCoreContext`: RNOHCoreContext 实例
- `deviceId`: 设备唯一标识
- `deviceName`: 设备名称
- `bundlePath`: bundle 路径（可选，不指定则使用默认路径）

**返回：** RNInstance 或 undefined

#### 2. `getDeviceInstance()`
获取设备的 RNInstance

```typescript
DeviceInstanceManager.getDeviceInstance(deviceId: string): RNInstance | undefined
```

#### 3. `getDeviceContext()`
获取设备的 RNComponentContext

```typescript
DeviceInstanceManager.getDeviceContext(deviceId: string): RNComponentContext | undefined
```

#### 4. `updateDeviceStatus()`
更新设备状态

```typescript
DeviceInstanceManager.updateDeviceStatus(
  deviceId: string,
  status: 'online' | 'offline' | 'cleaning'
): void
```

#### 5. `destroyDeviceInstance()`
销毁设备 Instance

```typescript
DeviceInstanceManager.destroyDeviceInstance(
  deviceId: string,
  rnohCoreContext?: RNOHCoreContext
): Promise<void>
```

#### 6. `getAllDevices()`
获取所有设备信息

```typescript
DeviceInstanceManager.getAllDevices(): DeviceInfo[]
```

#### 7. `getOnlineDevices()`
获取在线设备列表

```typescript
DeviceInstanceManager.getOnlineDevices(): DeviceInfo[]
```

#### 8. `cleanupOfflineDevices()`
清理所有离线设备

```typescript
DeviceInstanceManager.cleanupOfflineDevices(
  rnohCoreContext?: RNOHCoreContext
): Promise<void>
```

### 内存管理

#### 设置最大实例数

```typescript
// 限制最多同时存在 5 个设备 Instance
DeviceInstanceManager.setMaxInstances(5);
```

当超过最大实例数时，会自动销毁最久未使用的 Instance。

#### 手动清理离线设备

```typescript
const rnohCoreContext = AppStorage.get<RNOHCoreContext>('RNOHCoreContext');
await DeviceInstanceManager.cleanupOfflineDevices(rnohCoreContext);
```

## 与现有 LoadManager 的关系

### 兼容性

- `DeviceInstanceManager` 是**独立的管理器**，不影响现有的 `LoadManager`
- `LoadManager` 继续管理 `cpInstance`、`bpInstance` 等业务 Instance
- `DeviceInstanceManager` 专门管理设备相关的 Instance

### 使用场景区分

| 场景 | 使用管理器 | 说明 |
|------|-----------|------|
| 业务模块（CP、BP） | `LoadManager` | 固定的业务 Instance |
| 设备管理（扫地机器人） | `DeviceInstanceManager` | 动态的设备 Instance |
| 预创建 Instance | `BasePrecreateView` | 页面预创建机制 |

## 最佳实践

### 1. 设备生命周期管理

```typescript
// 设备上线
aboutToAppear() {
  await DeviceInstanceManager.getOrCreateDeviceInstance(
    this.rnohCoreContext,
    this.deviceId,
    this.deviceName
  );
  DeviceInstanceManager.updateDeviceStatus(this.deviceId, 'online');
}

// 设备离线
aboutToDisappear() {
  DeviceInstanceManager.updateDeviceStatus(this.deviceId, 'offline');
  // 可选：如果确定不再使用，可以销毁
  // await DeviceInstanceManager.destroyDeviceInstance(this.deviceId, this.rnohCoreContext);
}
```

### 2. 定期清理

```typescript
// 在应用启动时清理离线设备
onCreate() {
  // ... 其他初始化
  DeviceInstanceManager.cleanupOfflineDevices(this.rnohCoreContext);
}
```

### 3. Bundle 路径管理

```typescript
// 方式 1: 所有设备共享同一个 bundle
await DeviceInstanceManager.getOrCreateDeviceInstance(
  rnohCoreContext,
  deviceId,
  deviceName,
  'bundle/robot/robot.harmony.bundle'  // 统一 bundle
);

// 方式 2: 每个设备有专属 bundle（需要提前打包）
await DeviceInstanceManager.getOrCreateDeviceInstance(
  rnohCoreContext,
  deviceId,
  deviceName,
  `bundle/robot/${deviceId}.harmony.bundle`  // 设备专属 bundle
);
```

## 注意事项

### ⚠️ 1. 内存占用
- 每个 Instance 占用约 10-20MB 内存
- 建议设置合理的 `MAX_INSTANCES` 限制

### ⚠️ 2. Bundle 加载
- 每个 Instance 都需要加载 bundle
- 如果 bundle 很大，首次加载可能较慢

### ⚠️ 3. 线程资源
- 每个 Instance 占用一个 JS 线程
- 设备数量过多可能影响性能

### ⚠️ 4. 生命周期
- 设备离线时及时更新状态
- 长时间不用的设备建议销毁 Instance

## 总结

`DeviceInstanceManager` 为 s3 工程提供了**设备级别的 Instance 管理能力**，与现有的 `LoadManager` 和 `BasePrecreateView` 完全兼容，可以同时使用。

**核心优势：**
- ✅ 设备隔离：每个设备有独立的 JS 环境
- ✅ 自动管理：支持实例数量限制和自动清理
- ✅ 易于使用：简单的 API，开箱即用
- ✅ 完全兼容：不影响现有代码





