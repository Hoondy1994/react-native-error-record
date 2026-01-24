import type { Context } from "@ohos:abilityAccessCtrl";
import display from "@ohos:display";
import sdl from "@app:com.example.myapplication/entry/SDL2";
export function setResourceManager(manager: Context) {
    sdl.setResourceManager(manager.cacheDir, manager.resourceManager);
}
export function setScreenResolution() {
    let mDisplay: display.Display;
    try {
        mDisplay = display.getDefaultDisplaySync();
        let nDeviceWidth: number = mDisplay.width;
        let nDeviceHeight: number = mDisplay.height;
        let nsurfaceWidth: number = mDisplay.width;
        let nsurfaceHeight: number = mDisplay.height;
        let format: number = mDisplay.densityPixels;
        let rate: number = mDisplay.refreshRate;
        let scaledDensity = mDisplay.scaledDensity;
        sdl.nativeSetScreenResolution(nDeviceWidth, nDeviceHeight, nsurfaceWidth, nsurfaceHeight, format, rate);
        sdl.setScaledDensity(scaledDensity);
    }
    catch (exception) {
        console.error('Failed to obtain the default display object. Code: ' + JSON.stringify(exception));
    }
}
export function nativeSendQuit() {
    sdl.nativeSendQuit();
}
export function nativePause() {
    sdl.nativePause();
}
export function nativeResume() {
    sdl.nativeResume();
}
export function onNativeOrientationChanged(orientation: number) {
    sdl.onNativeOrientationChanged(orientation);
}
export function onNativePermissionResult(result: Boolean) {
    sdl.nativePermissionResult(result);
}
