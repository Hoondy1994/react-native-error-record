/**
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE-MIT file in the root directory of this source tree.
 */

import {
  RNOHCoreContext,
  RNSurface,
  RNInstance,
  RNOHContext,
  RNComponentContext,
  buildRNComponentForTag
} from "@rnoh/react-native-openharmony";
import { LoadManager, buildCustomComponent } from './LoadBundle';

const wrappedCustomRNComponentBuilder = wrapBuilder(buildCustomComponent);

@Component
export struct BaseRN {
  @StorageLink('RNOHCoreContext') rnohCoreContext: RNOHCoreContext | undefined = undefined;
  useBundleCache: boolean = true;
  rnInstance!: RNInstance;
  moduleName: string = '';
  initProps: Record<string, string> = {};
  bundlePath: string = '';
  private appearCount: number = 0;
  @State isBundleReady: boolean = false;

  aboutToAppear() {
    this.appearCount++;
    console.log('[BaseRN] aboutToAppear module=' + this.moduleName + ', count=' + this.appearCount);
    LoadManager.loadBundle(this.rnInstance, this.bundlePath, this.useBundleCache).then(() => {
      this.isBundleReady = true;
      console.log('[BaseRN] load success module=' + this.moduleName + ', count=' + this.appearCount + ', bundle=' + this.bundlePath);
    }).catch((err: Error) => {
      console.log('[BaseRN] load failed module=' + this.moduleName + ', bundle=' + this.bundlePath + ', error=' + err);
    });
  }

  aboutToDisappear() {
    console.log('[BaseRN] aboutToDisappear module=' + this.moduleName + ', count=' + this.appearCount);
  }

  build() {
    Column() {
      if (this.rnohCoreContext && this.isBundleReady) {
        RNSurface({
          surfaceConfig: {
            appKey: this.moduleName,
            initialProps: this.initProps,
          },
          ctx: new RNComponentContext(
            RNOHContext.fromCoreContext(this.rnohCoreContext!, this.rnInstance),
            wrappedCustomRNComponentBuilder,
            wrapBuilder(buildRNComponentForTag),
            new Map()
          ),
        })
      }
    }
    .height('100%')
    .width('100%')
  }
}
