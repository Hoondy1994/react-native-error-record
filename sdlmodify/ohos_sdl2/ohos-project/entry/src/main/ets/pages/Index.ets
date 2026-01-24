/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import {
  notifySdlPageShow,
  notifySdlAboutToAppear,
  notifySdlAboutToDisappear,
  notifySdlPageHide,
} from '../service/SdlModule'

import display from '@ohos.display'
import { window } from '@kit.ArkUI'
import qt from 'libentry.so'

let storage = LocalStorage.getShared()
let rootNode: object

@Entry(storage)
@Component
struct Index {
  private mWidth: number = 100
  private mHeight: number = 100
  private x: number = 0
  private y: number = 0
  aboutToAppear(): void {
    let windowStage = storage.get<window.WindowStage>('windowStage') as window.WindowStage
    let windowClass = windowStage.getMainWindowSync()
    let windowProperties = windowClass.getWindowProperties()
    let windowRect = windowProperties.windowRect
    let windowAvoidArea = windowClass.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM)
    let topRect = windowAvoidArea.topRect
    let bottomRect = windowAvoidArea.bottomRect

    this.mWidth = windowRect.width
    this.mHeight = windowRect.height - topRect.height - bottomRect.height
  }

  aboutToDisappear(): void {
    qt.removeNode('RootXC');
    notifySdlAboutToDisappear()
  }

  onPageShow(): void {
    notifySdlPageShow()
  }

  onPageHide(): void {
    notifySdlPageHide()
  }

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      XComponent({
        id: 'RootXC',
        type: XComponentType.NODE,
        libraryname: 'nativerender'
      })
        .onAppear(() => {
          let scaledDensity = display.getDefaultDisplaySync().scaledDensity;
          display.on('change', (id: number) => {
            const densityDPI = display.getDefaultDisplaySync().densityDPI.toString()
            const scaledDensity = display.getDefaultDisplaySync().scaledDensity.toString()
            const densityPixels = display.getDefaultDisplaySync().densityPixels.toString()
            const xDPI = display.getDefaultDisplaySync().xDPI.toString()
            const yDPI = display.getDefaultDisplaySync().yDPI.toString()
            const widthStr = display.getDefaultDisplaySync().width.toString()
            const heightStr = display.getDefaultDisplaySync().height.toString()
            console.log('densityDPI(显示设备屏幕的物理像素密度，表示每英寸上的像素点数):' + densityDPI
              + '\nscaledDensity(显示设备的显示字体的缩放因子):' + scaledDensity
              + '\ndensityPixels(显示设备逻辑像素的密度，代表物理像素与逻辑像素的缩放系数):' + densityPixels
              + '\nxDPI(x方向中每英寸屏幕的确切物理像素值):' + xDPI
              + '\nyDPI(y方向中每英寸屏幕的确切物理像素值):' + yDPI
              + '\nwidth:' + widthStr
              + '\nheight:' + heightStr)

          })
          qt.setScaledDensity(scaledDensity)
          rootNode = qt.createNativeNode('RootXC', this.mWidth, this.mHeight, this.x, this.y)
          notifySdlAboutToAppear(getContext(this), rootNode)
        })
        .width('100%')
        .height('100%')
        .id('RootXComponent')
        .backgroundColor(Color.Yellow)
    }
    .gesture(PanGesture())
    .onClick(() => {
    })
    .width('100%')
    .height('100%')
  }
}
