import type common from "@ohos:app.ability.common";
import { requestPermissionFromUsers, Request_SetWindowStyle, setOrientationBis } from "@bundle:com.example.myapplication/entry/ets/service/Applicant";
export function setWindowStyle(mContext: common.UIAbilityContext, fullscree: boolean) {
    console.log("OHOS_NAPI_SetWindowStyle");
    Request_SetWindowStyle(mContext, fullscree);
}
export function setOrientation(mContext: common.UIAbilityContext, w: number, h: number, resizable: number, hint: string) {
    console.log("OHOS_NAPI_SetOrientation");
    setOrientationBis(mContext, w, h, resizable, hint);
}
export function requestPermission(mContext: common.UIAbilityContext, permission: string) {
    console.log("OHOS_NAPI_RequestPermission");
    requestPermissionFromUsers(mContext, permission);
}
