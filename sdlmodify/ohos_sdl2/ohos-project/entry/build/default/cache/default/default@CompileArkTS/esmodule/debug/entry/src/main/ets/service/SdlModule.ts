import { setOrientation, requestPermission } from "@bundle:com.example.myapplication/entry/ets/service/ResponseNative";
import { setResourceManager, nativePause, nativeSendQuit, setScreenResolution } from "@bundle:com.example.myapplication/entry/ets/service/InvokeNative";
import pointer from "@ohos:multimodalInput.pointer";
import type common from "@ohos:app.ability.common";
import window from "@ohos:window";
import type image from "@ohos:multimedia.image";
import type { BusinessError } from "@ohos:base";
import type { Context } from "@ohos:abilityAccessCtrl";
import sdl from "@app:com.example.myapplication/entry/SDL2";
let sdlPageContext: common.UIAbilityContext;
export interface NapiCallback {
    setCustomCursorandCreate(pixelmapCreated: image.PixelMap, focusX: number, focusY: number): void;
    setPointer(cursorID: number): void;
    setWindowStyle(fullscree: boolean): void;
    setOrientation(w: number, h: number, resizable: number, hint: string): void;
    requestPermission(permission: string): void;
}
export class ArkNapiCallback implements NapiCallback {
    setCustomCursorandCreate(pixelmapCreated: image.PixelMap, focusX: number, focusY: number) {
        window.getLastWindow(sdlPageContext, (error: BusinessError, windowClass: window.Window) => {
            if (error.code) {
                console.error('Failed to obtain the top window. Cause: ' + JSON.stringify(error));
                return;
            }
            let windowId = windowClass.getWindowProperties().id;
            if (windowId < 0) {
                console.log(`Invalid windowId`);
                return;
            }
            try {
                pointer.setCustomCursor(windowId, pixelmapCreated, focusX, focusY).then(() => {
                    console.log(`Successfully set custom pointer`);
                });
            }
            catch (error) {
                console.log(`Failed to set the custom pointer, error=${JSON.stringify(error)}, msg=${JSON.stringify(`message`)}`);
            }
        });
    }
    setPointer(cursorID: number) {
        const SDL_SYSTEM_CURSOR_NONE = -1;
        const SDL_SYSTEM_CURSOR_ARROW = 0;
        const SDL_SYSTEM_CURSOR_IBEAM = 1;
        const SDL_SYSTEM_CURSOR_WAIT = 2;
        const SDL_SYSTEM_CURSOR_CROSSHAIR = 3;
        const SDL_SYSTEM_CURSOR_WAITARROW = 4;
        const SDL_SYSTEM_CURSOR_SIZENWSE = 5;
        const SDL_SYSTEM_CURSOR_SIZENESW = 6;
        const SDL_SYSTEM_CURSOR_SIZEWE = 7;
        const SDL_SYSTEM_CURSOR_SIZENS = 8;
        const SDL_SYSTEM_CURSOR_SIZEALL = 9;
        const SDL_SYSTEM_CURSOR_NO = 10;
        const SDL_SYSTEM_CURSOR_HAND = 11;
        let cursor_type = 0;
        switch (cursorID) {
            case SDL_SYSTEM_CURSOR_ARROW:
                cursor_type = pointer.PointerStyle.DEFAULT; //PointerIcon.TYPE_ARROW
                break;
            case SDL_SYSTEM_CURSOR_IBEAM:
                cursor_type = pointer.PointerStyle.TEXT_CURSOR; //PointerIcon.TYPE_TEXT
                break;
            case SDL_SYSTEM_CURSOR_WAIT:
                cursor_type = pointer.PointerStyle.DEFAULT; //PointerIcon.TYPE_WAIT
                break;
            case SDL_SYSTEM_CURSOR_CROSSHAIR:
                cursor_type = pointer.PointerStyle.DEFAULT; //PointerIcon.TYPE_CROSSHAIR
                break;
            case SDL_SYSTEM_CURSOR_WAITARROW:
                cursor_type = pointer.PointerStyle.DEFAULT; //PointerIcon.TYPE_WAIT
                break;
            case SDL_SYSTEM_CURSOR_SIZENWSE:
                cursor_type = pointer.PointerStyle.DEFAULT; //PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW
                break;
            case SDL_SYSTEM_CURSOR_SIZENESW:
                cursor_type = pointer.PointerStyle.DEFAULT; //PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW
                break;
            case SDL_SYSTEM_CURSOR_SIZEWE:
                cursor_type = pointer.PointerStyle.HORIZONTAL_TEXT_CURSOR; //PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW
                break;
            case SDL_SYSTEM_CURSOR_SIZENS:
                cursor_type = pointer.PointerStyle.DEFAULT; //PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW
                break;
            case SDL_SYSTEM_CURSOR_SIZEALL:
                cursor_type = pointer.PointerStyle.HAND_GRABBING; //PointerIcon.TYPE_GRAB
                break;
            case SDL_SYSTEM_CURSOR_NO:
                cursor_type = pointer.PointerStyle.DEFAULT; //PointerIcon.TYPE_NO_DROP
                break;
            case SDL_SYSTEM_CURSOR_HAND:
                cursor_type = pointer.PointerStyle.HAND_OPEN; //PointerIcon.TYPE_HAND
                break;
            default:
                cursor_type = pointer.PointerStyle.DEFAULT;
                break;
        }
        window.getLastWindow(sdlPageContext, (error: BusinessError, windowClass: window.Window) => {
            if (error.code) {
                console.error('Failed to obtain the top window. Cause: ' + JSON.stringify(error));
                return;
            }
            let windowId = windowClass.getWindowProperties().id;
            if (windowId < 0) {
                console.log(`Invalid windowId`);
                return;
            }
            try {
                pointer.setPointerStyle(windowId, cursor_type).then(() => {
                    console.log(`Successfully set mouse pointer style`);
                });
            }
            catch (error) {
                console.log(`Failed to set the pointer style, error=${JSON.stringify(error)}, msg=${JSON.stringify(`message`)}`);
            }
        });
    }
    setWindowStyle(fullscree: boolean): void {
    }
    setOrientation(w: number, h: number, resizable: number, hint: string): void {
        setOrientation(sdlPageContext, w, h, resizable, hint);
    }
    requestPermission(permission: string): void {
        requestPermission(sdlPageContext, permission);
    }
}
let callbackRef: NapiCallback = new ArkNapiCallback();
enum NativeState {
    INIT = 0,
    RESUMED = 1,
    PAUSED = 2
}
let currentNativeState: NativeState;
let nextNativeState: NativeState;
function handleNativeState(): void {
    if (nextNativeState == currentNativeState) {
        return;
    }
    if (nextNativeState == NativeState.INIT) {
        currentNativeState = nextNativeState;
        return;
    }
    if (nextNativeState == NativeState.PAUSED) {
        nativePause();
        currentNativeState = nextNativeState;
        return;
    }
    // Try a transition to resumed state
    if (nextNativeState == NativeState.RESUMED) {
        currentNativeState = nextNativeState;
    }
}
function resumeNativeThread(): void {
    nextNativeState = NativeState.RESUMED;
    handleNativeState();
}
function pauseNativeThread(): void {
    nextNativeState = NativeState.PAUSED;
    handleNativeState();
}
export function notifySdlPageShow(): void {
    resumeNativeThread();
}
export function notifySdlPageHide(): void {
    pauseNativeThread();
}
export function notifySdlAboutToAppear(resourceMannagerContext: Context, rootNode: object): void {
    sdlPageContext = resourceMannagerContext as common.UIAbilityContext;
    setResourceManager(resourceMannagerContext);
    setScreenResolution();
    sdl.init(callbackRef);
    sdl.sdlRootNode(rootNode);
    // sdl.sdlAppEntry("libentry.so", "main")
    sdl.sdlAppEntry("libentry.so", "main", "testyuv.bmp");
}
export function notifySdlAboutToDisappear() {
    nativeSendQuit();
}
