# 为什么需要 resize？

## 📋 问题

为什么要 resize？不 resize 会怎样？

## 🎯 答案

**resize 的作用：**
1. ✅ 更新 WindowSurfaceHolder 的尺寸
2. ✅ 标记 Surface 需要重建
3. ✅ 确保下次渲染时使用新尺寸

**如果不 resize：**
- ❌ Surface 保持旧尺寸
- ❌ 新内容被裁剪
- ❌ 画面显示不正确

---

## 1. resize 的作用

### 1.1 resize 做了什么？

```cpp
// RNSkOpenGLCanvasProvider.h:218
void resize(int width, int height) {
    _width = width;        // ← 更新宽度
    _height = height;      // ← 更新高度
    _skSurface = nullptr;  // ← 标记 Surface 需要重建
}
```

**三个关键操作：**
1. **更新宽度**：`_width = width`
2. **更新高度**：`_height = height`
3. **标记重建**：`_skSurface = nullptr`

### 1.2 为什么需要这三个操作？

#### 操作 1：更新宽度和高度

**作用：** 保存新的尺寸，供后续使用

**如果不更新：**
```cpp
// 不更新 _width 和 _height
void resize(int width, int height) {
    // _width 和 _height 还是旧值（例如：290px）
    // 但实际需要新值（例如：348px）
}

// 下次 getSurface() 时
auto renderTarget = GrBackendRenderTargets::MakeGL(
    _width, _height, ...);  // ← 使用旧尺寸（290px）❌
```

**结果：** Surface 还是旧尺寸，新内容被裁剪

#### 操作 2：标记 Surface 需要重建

**作用：** 告诉系统"Surface 需要重建"

**如果不标记：**
```cpp
// 不设置 _skSurface = nullptr
void resize(int width, int height) {
    _width = width;
    _height = height;
    // _skSurface 还是指向旧的 Surface（290px）
}

// 下次 getSurface() 时
if (_skSurface == nullptr) {  // ← false，不会重建
    // 不会执行
}
return _skSurface;  // ← 返回旧的 Surface（290px）❌
```

**结果：** Surface 还是旧尺寸，新内容被裁剪

---

## 2. 不 resize 会怎样？

### 2.1 场景：5 行切到 6 行

**初始状态（5 行）：**
```
WindowSurfaceHolder:
  _width: 375px
  _height: 290px  ← 5 行的高度
  _skSurface: SkSurface(375px × 290px)
```

**Canvas 尺寸变化（6 行）：**
```
Canvas height: 348px  ← 新高度
```

**如果不 resize：**
```cpp
// 不调用 resize()
void surfaceSizeChanged(int width, int height) {
    // 不调用 _surfaceHolder->resize(width, height);
    // _surfaceHolder 的尺寸还是旧的
}

// WindowSurfaceHolder 状态：
_width: 375px
_height: 290px  ← 还是旧高度 ❌
_skSurface: SkSurface(375px × 290px)  ← 还是旧 Surface ❌
```

**下次渲染时：**
```cpp
renderToCanvas() {
    surface = getSurface();
    // 返回旧的 Surface（375px × 290px）
    
    canvas->drawRect(0, 0, 375, 348);  // 尝试绘制 348px 高度
    // 但 Surface 只有 290px 高
    // 结果：Y > 290px 的部分被裁剪 ❌
}
```

**结果：** 第 6 行不显示（被裁剪）

---

## 3. resize 的完整流程

### 3.1 resize 前

```
Canvas: 375px × 348px  ✅ 新尺寸
WindowSurfaceHolder:
  _width: 375px
  _height: 290px  ⚠️ 旧高度
  _skSurface: SkSurface(375px × 290px)  ⚠️ 旧 Surface
```

### 3.2 resize 执行

```cpp
resize(375, 348) {
    _width = 375;   // 更新宽度
    _height = 348;  // 更新高度
    _skSurface = nullptr;  // 标记需要重建
}
```

### 3.3 resize 后

```
Canvas: 375px × 348px  ✅
WindowSurfaceHolder:
  _width: 375px  ✅ 已更新
  _height: 348px  ✅ 已更新
  _skSurface: nullptr  ✅ 标记需要重建
```

### 3.4 下次渲染时重建

```cpp
getSurface() {
    if (_skSurface == nullptr) {  // ← true，需要重建
        // 使用新的 _width 和 _height
        auto renderTarget = GrBackendRenderTargets::MakeGL(
            _width, _height, ...);  // 375px, 348px ✅
        
        _skSurface = WrapBackendRenderTarget(...);
        // 创建新的 Surface（375px × 348px）✅
    }
    return _skSurface;
}
```

### 3.5 最终状态

```
Canvas: 375px × 348px  ✅
WindowSurfaceHolder:
  _width: 375px  ✅
  _height: 348px  ✅
  _skSurface: SkSurface(375px × 348px)  ✅ 新 Surface

所有层尺寸一致 ✅
```

---

## 4. 为什么不能直接更新 Surface？

### 4.1 问题：为什么不直接更新 Surface 尺寸？

**答案：Surface 的尺寸是创建时固定的，不能直接修改！**

```cpp
// Surface 创建时
_skSurface = SkSurfaces::WrapBackendRenderTarget(
    directContext, renderTarget, ...);
// renderTarget 的尺寸决定了 Surface 的尺寸
// 一旦创建，尺寸就固定了
```

**类比：**
- Surface 就像一张画布
- 画布的尺寸是创建时决定的
- 不能直接"拉伸"画布
- 必须创建新的画布

### 4.2 必须重建 Surface

**正确方式：**
```cpp
// 1. 标记旧 Surface 无效
_skSurface = nullptr;

// 2. 下次渲染时重建
getSurface() {
    if (_skSurface == nullptr) {
        // 使用新尺寸创建新 Surface
        _skSurface = WrapBackendRenderTarget(...);
    }
}
```

**错误方式（不可能）：**
```cpp
// 不能直接修改 Surface 尺寸
_skSurface->resize(width, height);  // ❌ 不存在这个方法
```

---

## 5. resize 的时机

### 5.1 什么时候调用 resize？

**触发时机：**
```
Canvas 尺寸变化
    ↓
XComponent.onSizeChange
    ↓
onSurfaceSizeChanged NAPI
    ↓
PluginRender::SurfaceSizeChanged
    ↓
surfaceSizeChanged()
    ↓
resize()  ← 这里调用
```

### 5.2 为什么不在其他地方 resize？

**问题：为什么不立即重建 Surface？**

**答案：resize 是"标记"，重建是"懒加载"**

**原因 1：性能考虑**
```cpp
// 如果立即重建
void resize(int width, int height) {
    _width = width;
    _height = height;
    _skSurface = nullptr;
    getSurface();  // ← 立即重建
    // 问题：
    // - 可能没有渲染上下文
    // - 可能不在正确的线程
    // - 可能浪费资源（如果后续又变化）
}
```

**原因 2：懒加载机制**
```cpp
// 懒加载：需要时才创建
getSurface() {
    if (_skSurface == nullptr) {  // ← 需要时才重建
        // 此时有渲染上下文
        // 此时在正确的线程
        // 此时尺寸已经稳定
    }
}
```

**原因 3：避免频繁重建**
```cpp
// 如果尺寸快速变化
resize(375, 290);  // 第一次
resize(375, 300);  // 第二次（很快）
resize(375, 310);  // 第三次（很快）
resize(375, 348);  // 最终

// 懒加载：只重建一次（最终尺寸）
// 立即重建：重建 4 次（浪费性能）
```

---

## 6. resize 的完整作用

### 6.1 三个作用

**作用 1：更新尺寸信息**
```cpp
_width = width;   // 保存新宽度
_height = height; // 保存新高度
```

**作用 2：标记 Surface 无效**
```cpp
_skSurface = nullptr;  // 告诉系统"旧 Surface 不能用了"
```

**作用 3：触发重建机制**
```cpp
// 下次 getSurface() 时
if (_skSurface == nullptr) {  // ← 检查标记
    // 使用新尺寸重建
}
```

### 6.2 为什么三个操作缺一不可？

**如果只更新尺寸，不标记重建：**
```cpp
void resize(int width, int height) {
    _width = width;   // ✅ 更新了
    _height = height; // ✅ 更新了
    // 但 _skSurface 还是旧的 ❌
}
// 结果：Surface 还是旧尺寸
```

**如果只标记重建，不更新尺寸：**
```cpp
void resize(int width, int height) {
    _skSurface = nullptr;  // ✅ 标记了
    // 但 _width 和 _height 还是旧的 ❌
}
// 结果：重建时使用旧尺寸
```

**必须三个操作都做：**
```cpp
void resize(int width, int height) {
    _width = width;        // ✅ 更新尺寸
    _height = height;      // ✅ 更新尺寸
    _skSurface = nullptr;  // ✅ 标记重建
}
// 结果：下次重建时使用新尺寸 ✅
```

---

## 7. 实际例子

### 7.1 场景：5 行切到 6 行

**初始状态：**
```
WindowSurfaceHolder:
  _width: 375px
  _height: 290px  ← 5 行
  _skSurface: SkSurface(375px × 290px)
```

**Canvas 尺寸变化：**
```
Canvas height: 348px  ← 6 行
```

**调用 resize：**
```cpp
resize(375, 348) {
    _width = 375;   // ✅ 更新
    _height = 348;  // ✅ 更新（从 290px → 348px）
    _skSurface = nullptr;  // ✅ 标记重建
}
```

**resize 后：**
```
WindowSurfaceHolder:
  _width: 375px  ✅
  _height: 348px  ✅ 新高度
  _skSurface: nullptr  ✅ 标记需要重建
```

**下次渲染时：**
```cpp
getSurface() {
    if (_skSurface == nullptr) {  // ← true
        // 使用新的 _width 和 _height
        auto renderTarget = GrBackendRenderTargets::MakeGL(
            375, 348, ...);  // ✅ 新尺寸
        
        _skSurface = WrapBackendRenderTarget(...);
        // 创建新的 Surface（375px × 348px）✅
    }
}
```

**最终状态：**
```
WindowSurfaceHolder:
  _width: 375px  ✅
  _height: 348px  ✅
  _skSurface: SkSurface(375px × 348px)  ✅ 新 Surface

所有 6 行都能显示 ✅
```

---

## 8. 总结

### 8.1 为什么要 resize？

**原因：**
1. ✅ **更新尺寸信息**：保存新的宽度和高度
2. ✅ **标记 Surface 无效**：告诉系统旧 Surface 不能用了
3. ✅ **触发重建机制**：确保下次渲染时使用新尺寸重建

### 8.2 不 resize 会怎样？

**问题：**
- ❌ Surface 保持旧尺寸
- ❌ 新内容被裁剪
- ❌ 画面显示不正确（例如：第 6 行不显示）

### 8.3 关键理解

1. **Surface 尺寸是固定的**：创建后不能直接修改
2. **必须重建**：通过 `_skSurface = nullptr` 标记，下次重建
3. **懒加载机制**：需要时才重建，避免浪费性能
4. **三个操作缺一不可**：更新尺寸 + 标记重建

### 8.4 resize 的本质

**resize 不是"改变"Surface 尺寸，而是：**
1. **更新尺寸信息**（_width, _height）
2. **标记需要重建**（_skSurface = nullptr）
3. **触发重建机制**（下次 getSurface() 时重建）

**类比：**
- resize 就像"拆掉旧房子，准备建新房子"
- 不是"扩建旧房子"，而是"重建新房子"

---

**文档版本**: 1.0  
**最后更新**: 2025-01-XX  
**维护者**: AI Assistant
