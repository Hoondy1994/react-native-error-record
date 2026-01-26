import { ArrayList } from '@kit.ArkTS';
import {
  buildRNComponentForTag,
  RNComponentContext,
  RNInstance,
  RNOHContext,
  RNOHCoreContext
} from '@rnoh/react-native-openharmony';
import { buildCustomComponent, createRNPackages, ENABLE_CAPI_ARCHITECTURE } from '../rn';
import { arkTsComponentNames } from './LoadBundle';

export class InstanceManager {
  private static rnInstances: ArrayList<RNInstance> = new ArrayList()

  public static async addInstance(instacne:RNInstance){
    InstanceManager.rnInstances.add(instacne)
  }
  public static async loadPlugin(rnohCoreContext: RNOHCoreContext | undefined): Promise<RNInstance> {
    // if (this.rnInstances.length > 0) {
    //   console.log("BasePrecreateView getOrCreateRNInstance 获取预创建的 RNInstance：", this.rnInstances[0]);
    //   const instance = this.rnInstances.removeByIndex(0);
    //   console.log("BasePrecreateView status = ", instance.status);
    //   return Promise.resolve(instance);
    // } else {
    const instance: RNInstance = await rnohCoreContext!.createAndRegisterRNInstance({
      createRNPackages: createRNPackages,
      enableNDKTextMeasuring: true,
      enableBackgroundExecutor: false,
      enableCAPIArchitecture: ENABLE_CAPI_ARCHITECTURE,
      arkTsComponentNames: arkTsComponentNames
    });
    const ctxInstance: RNComponentContext = new RNComponentContext(
      RNOHContext.fromCoreContext(rnohCoreContext!, instance),
      wrapBuilder(buildCustomComponent),
      wrapBuilder(buildRNComponentForTag),
      new Map()
    );
    InstanceManager.rnInstances.add(instance)
    return instance;
  }
}
