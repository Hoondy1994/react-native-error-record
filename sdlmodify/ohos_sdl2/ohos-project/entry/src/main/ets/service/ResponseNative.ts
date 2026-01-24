/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License,Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import common from '@ohos.app.ability.common'
import { requestPermissionFromUsers, Request_SetWindowStyle, setOrientationBis } from './Applicant'

export function setWindowStyle(mContext: common.UIAbilityContext, fullscree: boolean) {
  console.log("OHOS_NAPI_SetWindowStyle")
  Request_SetWindowStyle(mContext, fullscree)
}

export function setOrientation(mContext: common.UIAbilityContext, w: number, h: number, resizable: number, hint: string) {
  console.log("OHOS_NAPI_SetOrientation")
  setOrientationBis(mContext, w, h, resizable, hint)
}

export function requestPermission(mContext: common.UIAbilityContext, permission: string) {
  console.log("OHOS_NAPI_RequestPermission")
  requestPermissionFromUsers(mContext, permission)
}
