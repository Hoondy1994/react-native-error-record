if (!("finalizeConstruction" in ViewPU.prototype)) {
    Reflect.set(ViewPU.prototype, "finalizeConstruction", () => { });
}
interface Index_Params {
    mWidth?: number;
    mHeight?: number;
    x?: number;
    y?: number;
}
import { notifySdlPageShow, notifySdlAboutToAppear, notifySdlAboutToDisappear, notifySdlPageHide, } from "@bundle:com.example.myapplication/entry/ets/service/SdlModule";
import display from "@ohos:display";
import window from "@ohos:window";
import qt from "@app:com.example.myapplication/entry/entry";
let storage = LocalStorage.getShared();
let rootNode: object;
class Index extends ViewPU {
    constructor(parent, params, __localStorage, elmtId = -1, paramsLambda = undefined, extraInfo) {
        super(parent, __localStorage, elmtId, extraInfo);
        if (typeof paramsLambda === "function") {
            this.paramsGenerator_ = paramsLambda;
        }
        this.mWidth = 100;
        this.mHeight = 100;
        this.x = 0;
        this.y = 0;
        this.setInitiallyProvidedValue(params);
        this.finalizeConstruction();
    }
    setInitiallyProvidedValue(params: Index_Params) {
        if (params.mWidth !== undefined) {
            this.mWidth = params.mWidth;
        }
        if (params.mHeight !== undefined) {
            this.mHeight = params.mHeight;
        }
        if (params.x !== undefined) {
            this.x = params.x;
        }
        if (params.y !== undefined) {
            this.y = params.y;
        }
    }
    updateStateVars(params: Index_Params) {
    }
    purgeVariableDependenciesOnElmtId(rmElmtId) {
    }
    aboutToBeDeleted() {
        SubscriberManager.Get().delete(this.id__());
        this.aboutToBeDeletedInternal();
    }
    private mWidth: number;
    private mHeight: number;
    private x: number;
    private y: number;
    aboutToAppear(): void {
        let windowStage = storage.get<window.WindowStage>('windowStage') as window.WindowStage;
        let windowClass = windowStage.getMainWindowSync();
        let windowProperties = windowClass.getWindowProperties();
        let windowRect = windowProperties.windowRect;
        let windowAvoidArea = windowClass.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM);
        let topRect = windowAvoidArea.topRect;
        let bottomRect = windowAvoidArea.bottomRect;
        this.mWidth = windowRect.width;
        this.mHeight = windowRect.height - topRect.height - bottomRect.height;
    }
    aboutToDisappear(): void {
        qt.removeNode('RootXC');
        notifySdlAboutToDisappear();
    }
    onPageShow(): void {
        notifySdlPageShow();
    }
    onPageHide(): void {
        notifySdlPageHide();
    }
    initialRender() {
        this.observeComponentCreation2((elmtId, isInitialRender) => {
            Stack.create({ alignContent: Alignment.TopStart });
            Gesture.create(GesturePriority.Low);
            PanGesture.create();
            PanGesture.pop();
            Gesture.pop();
            Stack.onClick(() => {
            });
            Stack.width('100%');
            Stack.height('100%');
        }, Stack);
        this.observeComponentCreation2((elmtId, isInitialRender) => {
            XComponent.create({
                id: 'RootXC',
                type: XComponentType.NODE,
                libraryname: 'nativerender'
            }, "com.example.myapplication/entry");
            XComponent.onAppear(() => {
                let scaledDensity = display.getDefaultDisplaySync().scaledDensity;
                display.on('change', (id: number) => {
                    const densityDPI = display.getDefaultDisplaySync().densityDPI.toString();
                    const scaledDensity = display.getDefaultDisplaySync().scaledDensity.toString();
                    const densityPixels = display.getDefaultDisplaySync().densityPixels.toString();
                    const xDPI = display.getDefaultDisplaySync().xDPI.toString();
                    const yDPI = display.getDefaultDisplaySync().yDPI.toString();
                    const widthStr = display.getDefaultDisplaySync().width.toString();
                    const heightStr = display.getDefaultDisplaySync().height.toString();
                    console.log('densityDPI(显示设备屏幕的物理像素密度，表示每英寸上的像素点数):' + densityDPI
                        + '\nscaledDensity(显示设备的显示字体的缩放因子):' + scaledDensity
                        + '\ndensityPixels(显示设备逻辑像素的密度，代表物理像素与逻辑像素的缩放系数):' + densityPixels
                        + '\nxDPI(x方向中每英寸屏幕的确切物理像素值):' + xDPI
                        + '\nyDPI(y方向中每英寸屏幕的确切物理像素值):' + yDPI
                        + '\nwidth:' + widthStr
                        + '\nheight:' + heightStr);
                });
                qt.setScaledDensity(scaledDensity);
                rootNode = qt.createNativeNode('RootXC', this.mWidth, this.mHeight, this.x, this.y);
                notifySdlAboutToAppear(getContext(this), rootNode);
            });
            XComponent.width('100%');
            XComponent.height('100%');
            XComponent.id('RootXComponent');
            XComponent.backgroundColor(Color.Yellow);
        }, XComponent);
        Stack.pop();
    }
    rerender() {
        this.updateDirtyElements();
    }
    static getEntryName(): string {
        return "Index";
    }
}
if (storage && storage.routeName != undefined && storage.storage != undefined) {
    registerNamedRoute(() => new Index(undefined, {}, storage.useSharedStorage ? LocalStorage.getShared() : storage.storage), storage.routeName, { bundleName: "com.example.myapplication", moduleName: "entry", pagePath: "pages/Index", pageFullPath: "entry/src/main/ets/pages/Index", integratedHsp: "false", moduleType: "followWithHap" });
}
else if (storage && storage.routeName != undefined && storage.storage == undefined) {
    registerNamedRoute(() => new Index(undefined, {}, storage.useSharedStorage ? LocalStorage.getShared() : storage.storage), storage.routeName, { bundleName: "com.example.myapplication", moduleName: "entry", pagePath: "pages/Index", pageFullPath: "entry/src/main/ets/pages/Index", integratedHsp: "false", moduleType: "followWithHap" });
}
else if (storage && storage.routeName == undefined && storage.storage != undefined) {
    registerNamedRoute(() => new Index(undefined, {}, storage.useSharedStorage ? LocalStorage.getShared() : storage.storage), "", { bundleName: "com.example.myapplication", moduleName: "entry", pagePath: "pages/Index", pageFullPath: "entry/src/main/ets/pages/Index", integratedHsp: "false", moduleType: "followWithHap" });
}
else if (storage && storage.useSharedStorage != undefined) {
    registerNamedRoute(() => new Index(undefined, {}, storage.useSharedStorage ? LocalStorage.getShared() : undefined), "", { bundleName: "com.example.myapplication", moduleName: "entry", pagePath: "pages/Index", pageFullPath: "entry/src/main/ets/pages/Index", integratedHsp: "false", moduleType: "followWithHap" });
}
else {
    registerNamedRoute(() => new Index(undefined, {}, storage), "", { bundleName: "com.example.myapplication", moduleName: "entry", pagePath: "pages/Index", pageFullPath: "entry/src/main/ets/pages/Index", integratedHsp: "false", moduleType: "followWithHap" });
}
