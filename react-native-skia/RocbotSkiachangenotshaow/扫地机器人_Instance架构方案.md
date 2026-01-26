# 扫地机器人 - 一个 Instance 对应一个设备架构方案

## 架构设计

### 核心思路
- **一个 RNInstance = 一个扫地机器人设备**
- 每个设备有独立的 JS 执行环境
- 设备之间完全隔离，互不影响

## 实现方案

### 1. 设备管理器（DeviceInstanceManager.ets）

```typescript
import { RNInstance, RNOHCoreContext } from '@rnoh/react-native-openharmony';
import { createRNPackages } from './RNPackagesFactory';
import { arkTsComponentNames } from './LoadBundle';

// 设备信息
interface RobotDevice {
  deviceId: string;        // 设备唯一标识
  deviceName: string;     // 设备名称
  instance: RNInstance;    // 对应的 RNInstance
  status: 'online' | 'offline' | 'cleaning';
}

export class DeviceInstanceManager {
  // 设备实例映射表：deviceId -> RobotDevice
  private static deviceMap: Map<string, RobotDevice> = new Map();
  
  /**
   * 为设备创建 RNInstance
   */
  public static async createDeviceInstance(
    rnohCoreContext: RNOHCoreContext,
    deviceId: string,
    deviceName: string
  ): Promise<RNInstance> {
    // 如果已存在，直接返回
    if (this.deviceMap.has(deviceId)) {
      return this.deviceMap.get(deviceId)!.instance;
    }
    
    // 创建新的 RNInstance
    const instance: RNInstance = await rnohCoreContext.createAndRegisterRNInstance({
      createRNPackages: createRNPackages,
      enableNDKTextMeasuring: true,
      enableBackgroundExecutor: false,
      enableCAPIArchitecture: true,
      arkTsComponentNames: arkTsComponentNames
    });
    
    // 为设备加载专属 bundle
    await instance.runJSBundle(
      new ResourceJSBundleProvider(
        getContext().resourceManager,
        'bundle/robot/robot.harmony.bundle'
      )
    );
    
    // 保存设备信息
    const device: RobotDevice = {
      deviceId,
      deviceName,
      instance,
      status: 'online'
    };
    this.deviceMap.set(deviceId, device);
    
    console.log(`[DeviceInstanceManager] 为设备 ${deviceName}(${deviceId}) 创建 Instance`);
    return instance;
  }
  
  /**
   * 获取设备的 RNInstance
   */
  public static getDeviceInstance(deviceId: string): RNInstance | undefined {
    return this.deviceMap.get(deviceId)?.instance;
  }
  
  /**
   * 获取所有在线设备
   */
  public static getAllDevices(): RobotDevice[] {
    return Array.from(this.deviceMap.values());
  }
  
  /**
   * 销毁设备 Instance
   */
  public static async destroyDeviceInstance(deviceId: string): Promise<void> {
    const device = this.deviceMap.get(deviceId);
    if (device) {
      // 销毁 Instance（如果有销毁方法）
      // await device.instance.destroy();
      this.deviceMap.delete(deviceId);
      console.log(`[DeviceInstanceManager] 销毁设备 ${deviceId} 的 Instance`);
    }
  }
  
  /**
   * 更新设备状态
   */
  public static updateDeviceStatus(
    deviceId: string, 
    status: 'online' | 'offline' | 'cleaning'
  ): void {
    const device = this.deviceMap.get(deviceId);
    if (device) {
      device.status = status;
    }
  }
}
```

### 2. 设备列表页面（RobotListPage.ets）

```typescript
@Component
export default struct RobotListPage {
  @State devices: RobotDevice[] = [];
  
  aboutToAppear() {
    // 模拟发现设备
    this.discoverDevices();
  }
  
  async discoverDevices() {
    // 从服务器获取设备列表
    const deviceList = [
      { deviceId: 'robot_001', deviceName: '客厅扫地机器人' },
      { deviceId: 'robot_002', deviceName: '卧室扫地机器人' },
      { deviceId: 'robot_003', deviceName: '厨房扫地机器人' }
    ];
    
    const rnohCoreContext = AppStorage.get<RNOHCoreContext>('RNOHCoreContext');
    if (!rnohCoreContext) return;
    
    // 为每个设备创建 Instance
    for (const deviceInfo of deviceList) {
      await DeviceInstanceManager.createDeviceInstance(
        rnohCoreContext,
        deviceInfo.deviceId,
        deviceInfo.deviceName
      );
    }
    
    this.devices = DeviceInstanceManager.getAllDevices();
  }
  
  build() {
    Column() {
      List() {
        ForEach(this.devices, (device: RobotDevice) => {
          ListItem() {
            NavigationLink({
              target: `pages/RobotControlPage?deviceId=${device.deviceId}`
            }) {
              Text(device.deviceName)
                .fontSize(18)
            }
          }
        })
      }
    }
  }
}
```

### 3. 设备控制页面（RobotControlPage.ets）

```typescript
@Component
export default struct RobotControlPage {
  @Param deviceId: string = '';
  private instance: RNInstance | undefined = undefined;
  
  aboutToAppear() {
    // 获取设备对应的 Instance
    this.instance = DeviceInstanceManager.getDeviceInstance(this.deviceId);
    if (!this.instance) {
      console.error(`设备 ${this.deviceId} 的 Instance 不存在`);
      return;
    }
  }
  
  build() {
    Column() {
      if (this.instance) {
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
      }
    }
  }
}
```

### 4. React Native 侧（RobotControl.tsx）

```typescript
import React from 'react';
import { View, Text, Button } from 'react-native';

interface Props {
  deviceId: string;
}

function RobotControl({ deviceId }: Props) {
  // 每个设备有独立的 JS 环境
  // 可以维护设备专属的状态、数据
  
  const handleStartCleaning = () => {
    // 调用设备专属的 TurboModule
    RobotTurboModule.startCleaning(deviceId);
  };
  
  return (
    <View>
      <Text>控制设备: {deviceId}</Text>
      <Button title="开始清扫" onPress={handleStartCleaning} />
    </View>
  );
}

AppRegistry.registerComponent('RobotControl', () => RobotControl);
```

## 架构优势

### ✅ 1. 完全隔离
- 每个设备有独立的 JS 执行环境
- 设备 A 的状态不会影响设备 B
- 设备 A 崩溃不会影响设备 B

### ✅ 2. 独立状态管理
- 每个设备可以维护自己的 Redux/Zustand store
- 设备专属的数据、配置互不干扰

### ✅ 3. 性能优化
- 每个设备有独立的 JS 线程
- 多设备并发操作不会相互阻塞

### ✅ 4. 灵活扩展
- 可以动态添加/删除设备
- 可以为不同类型的设备创建不同的 Instance

## 注意事项

### ⚠️ 1. 内存占用
- 每个 Instance 会占用一定内存（JS 引擎、线程等）
- 如果设备数量很多（>10个），需要考虑内存限制

### ⚠️ 2. 资源管理
- 设备离线时需要及时销毁 Instance
- 避免内存泄漏

### ⚠️ 3. Bundle 加载
- 每个 Instance 都需要加载 bundle
- 可以考虑预加载或懒加载策略

## 优化建议

### 1. 按需创建
```typescript
// 只在需要控制设备时才创建 Instance
public static async getOrCreateDeviceInstance(
  deviceId: string
): Promise<RNInstance> {
  let instance = this.deviceMap.get(deviceId)?.instance;
  if (!instance) {
    instance = await this.createDeviceInstance(deviceId);
  }
  return instance;
}
```

### 2. 设备池管理
```typescript
// 限制同时存在的 Instance 数量
private static MAX_INSTANCES = 5;

public static async createDeviceInstance(deviceId: string) {
  // 如果超过限制，销毁最久未使用的
  if (this.deviceMap.size >= this.MAX_INSTANCES) {
    await this.destroyOldestInstance();
  }
  // ... 创建新 Instance
}
```

### 3. 共享基础 Bundle
```typescript
// 所有设备共享基础 bundle，只加载设备专属 bundle
await instance.runJSBundle(basicBundleProvider);  // 共享
await instance.runJSBundle(deviceSpecificBundle); // 设备专属
```

## 总结

**一个 Instance 对应一个设备**是完全可行的架构，特别适合：
- 多设备管理场景
- 需要设备隔离的场景
- 需要独立状态管理的场景

关键是要做好 Instance 的生命周期管理，避免内存泄漏。





