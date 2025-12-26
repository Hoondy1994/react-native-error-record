import React, { useState, useEffect, useRef } from "react";
import { StyleSheet, View, ScrollView, Dimensions, TouchableOpacity, Text, ActivityIndicator } from "react-native";
import FastImage, {
  ResizeMode,
  OnLoadEvent,
  OnProgressEvent,
} from "react-native-fast-image";
import { base64Images } from "./base64Images";

const FastImageDemo = () => {
  const [screenWidth, setScreenWidth] = useState(Dimensions.get("window").width);
  const [screenHeight, setScreenHeight] = useState(Dimensions.get("window").height);
  const [isFolded, setIsFolded] = useState(Dimensions.get("window").width < 600);
  const [startTime, setStartTime] = useState(0);
  const [loadTimes, setLoadTimes] = useState<{ [key: number]: number }>({});
  const [loadedCount, setLoadedCount] = useState(0);
  const [isLoading, setIsLoading] = useState(false);
  const dimensionSubscription = useRef<any>(null);
  const loadStartTimes = useRef<{ [key: number]: number }>({});

  // Base64 编码的测试图片（排除网络问题）
  // 这些图片用于验证解码/渲染性能，不涉及网络请求
  // 格式：data:image/png;base64,<base64数据>
  // 
  // ⚠️ 注意：当前使用的是占位符图片（1x1 像素）
  // 要测试真实的解码性能，请替换为真实的 800x600 图片的 Base64 编码
  // 替换方法：修改 example/src/skia/base64Images.ts 文件
  const images = base64Images;

  const startTimer = () => {
    const now = performance.now();
    setStartTime(now);
    setLoadTimes({});
    setLoadedCount(0);
    setIsLoading(true);
    loadStartTimes.current = {};
    console.log("[开始渲染]", new Date().toISOString());
    console.log("[屏幕尺寸]", `${screenWidth}x${screenHeight} -> 即将变化`);
  };

  const handleResize = ({ window }: any) => {
    console.log("[屏幕尺寸变化]", `${screenWidth}x${screenHeight} -> ${window.width}x${window.height}`);
    startTimer();
    setScreenWidth(window.width);
    setScreenHeight(window.height);
    setIsFolded(window.width < 600);
  };

  const handleLoadStart = (index: number) => {
    loadStartTimes.current[index] = performance.now();
    const timeFromStart = loadStartTimes.current[index] - startTime;
    console.log(`[图片${index + 1}开始加载] 距离渲染开始: ${timeFromStart.toFixed(2)}ms`);
  };

  const handleLoad = (index: number, e: OnLoadEvent) => {
    const loadStartTime = loadStartTimes.current[index] || startTime;
    const loadTime = performance.now() - loadStartTime;
    const totalTime = performance.now() - startTime;
    
    const newLoadTimes = { ...loadTimes, [index]: loadTime };
    const newLoadedCount = Object.keys(newLoadTimes).length;
    
    console.log(`[图片${index + 1}加载完成] 加载耗时: ${loadTime.toFixed(2)}ms, 总耗时: ${totalTime.toFixed(2)}ms`);
    console.log(`[图片${index + 1}尺寸] width=${e.nativeEvent.width} height=${e.nativeEvent.height}`);
    
    setLoadTimes(newLoadTimes);
    setLoadedCount(newLoadedCount);

    if (newLoadedCount === images.length) {
      const finalTime = performance.now() - startTime;
      setIsLoading(false);
      console.log(`[✅ 所有图片加载完成] 总耗时: ${finalTime.toFixed(2)}ms`);
      console.log(`[📊 性能分析] 平均加载时间: ${(Object.values(newLoadTimes).reduce((a, b) => a + b, 0) / newLoadedCount).toFixed(2)}ms`);
      if (finalTime > 1000) {
        console.log(`[⚠️ 延迟明显] 总时间超过1000ms，存在渲染延迟问题！`);
      } else {
        console.log(`[✓ 性能正常] 总时间在可接受范围内`);
      }
    }
  };

  const simulateResize = () => {
    const newWidth = isFolded ? 1200 : 400;
    const newHeight = isFolded ? 1600 : 800;
    console.log(`[🔄 模拟尺寸变化] ${screenWidth}x${screenHeight} -> ${newWidth}x${newHeight}`);
    startTimer();
    setScreenWidth(newWidth);
    setScreenHeight(newHeight);
    setIsFolded(!isFolded);
  };

  useEffect(() => {
    startTimer();
    dimensionSubscription.current = Dimensions.addEventListener("change", handleResize);
    return () => {
      if (dimensionSubscription.current) {
        dimensionSubscription.current.remove();
      }
    };
  }, []);

  const avgTime = Object.values(loadTimes).length > 0
    ? (Object.values(loadTimes).reduce((a, b) => a + b, 0) / Object.values(loadTimes).length).toFixed(2)
    : "0";

  const maxTime = Object.values(loadTimes).length > 0
    ? Math.max(...Object.values(loadTimes)).toFixed(2)
    : "0";

  const totalTime = loadedCount === images.length && startTime > 0
    ? (performance.now() - startTime).toFixed(2)
    : "计算中...";

  return (
    <ScrollView style={styles.container}>
      <View style={styles.content}>
        <Text style={styles.title}>折叠屏 FastImage 延迟测试 (Base64)</Text>
        <Text style={styles.info}>
          屏幕: {screenWidth}px | {isFolded ? "折叠" : "展开"} | 已加载: {loadedCount}/{images.length}
        </Text>
        <Text style={styles.base64Hint}>
          📝 当前使用 Base64 图片（排除网络问题），测试解码/渲染性能
        </Text>

        {isLoading && (
          <View style={styles.loadingIndicator}>
            <ActivityIndicator size="small" color="#007AFF" />
            <Text style={styles.loadingText}>图片加载中，请稍候...</Text>
          </View>
        )}

        <View style={styles.metrics}>
          <Text style={styles.metricTitle}>📊 性能指标</Text>
          <Text style={styles.metricText}>平均加载: {avgTime} ms</Text>
          <Text style={styles.metricText}>最长加载: {maxTime} ms</Text>
          <Text style={styles.metricText}>总渲染时间: {totalTime} ms</Text>
          {loadedCount === images.length && (
            <Text style={[styles.metricText, parseFloat(totalTime) > 1000 ? styles.warning : styles.success]}>
              状态: {parseFloat(totalTime) > 1000 ? "⚠️ 延迟明显" : "✓ 性能正常"}
            </Text>
          )}
        </View>

        <TouchableOpacity 
          style={[styles.button, isLoading && styles.buttonDisabled]} 
          onPress={simulateResize}
          disabled={isLoading}
        >
          <Text style={styles.buttonText}>
            🔄 模拟尺寸变化 ({isFolded ? "展开" : "折叠"})
          </Text>
          {isLoading && <Text style={styles.buttonHint}>等待图片加载完成...</Text>}
        </TouchableOpacity>

        <View style={styles.tipBox}>
          <Text style={styles.tipTitle}>💡 如何判断是否复现延迟</Text>
          <Text style={styles.tipText}>
            1. 点击"模拟尺寸变化"按钮{"\n"}
            2. 等待所有图片加载完成（看到"所有图片加载完成"日志）{"\n"}
            3. 查看"总渲染时间"{"\n"}
            4. 如果总时间超过 1000ms，说明有延迟{"\n"}
            5. 查看控制台日志，每张图片的加载时间都有记录
          </Text>
        </View>

        {images.map((uri, index) => {
          const loadTime = loadTimes[index];
          const isLoaded = loadTime !== undefined;
          
          return (
            <View key={index} style={styles.imageBox}>
              <View style={styles.imageHeader}>
                <Text style={styles.imageLabel}>
                  图片 {index + 1}
                </Text>
                {isLoaded ? (
                  <Text style={loadTime > 500 ? styles.slow : styles.fast}>
                    {loadTime.toFixed(0)}ms {loadTime > 500 ? "⚠️" : "✓"}
                  </Text>
                ) : (
                  <ActivityIndicator size="small" color="#007AFF" />
                )}
              </View>
              <FastImage
                style={[styles.image, { width: screenWidth * 0.9 }]}
                source={{ uri }}
                resizeMode={FastImage.resizeMode.cover}
                onLoadStart={() => {
                  handleLoadStart(index);
                  console.log(`图片${index + 1} onLoadStart`);
                }}
                onProgress={(e: OnProgressEvent) => {
                  const progress = ((e.nativeEvent.loaded / e.nativeEvent.total) * 100).toFixed(1);
                  console.log(
                    `图片${index + 1} onProgress ${progress}% (${e.nativeEvent.loaded}/${e.nativeEvent.total})`
                  );
                }}
                onLoad={(e: OnLoadEvent) => {
                  handleLoad(index, e);
                }}
                onError={() => {
                  console.log(`图片${index + 1} onError`);
                }}
                onLoadEnd={() => {
                  console.log(`图片${index + 1} onLoadEnd`);
                }}
              />
            </View>
          );
        })}

        <View style={styles.analysisBox}>
          <Text style={styles.analysisTitle}>🔍 当前状态分析</Text>
          {loadedCount === 0 && (
            <Text style={styles.analysisText}>
              • 等待图片开始加载...{"\n"}
              • 当前使用 Base64 图片，不涉及网络请求{"\n"}
              • 如果延迟明显，说明是解码/渲染问题
            </Text>
          )}
          {loadedCount > 0 && loadedCount < images.length && (
            <Text style={styles.analysisText}>
              • 已加载 {loadedCount}/{images.length} 张图片{"\n"}
              • 继续等待剩余图片加载...{"\n"}
              • 查看控制台了解每张图片的加载进度
            </Text>
          )}
          {loadedCount === images.length && (
            <Text style={styles.analysisText}>
              • ✅ 所有图片已加载完成{"\n"}
              • 总渲染时间: {totalTime}ms{"\n"}
              • {parseFloat(totalTime) > 1000 ? "⚠️ 存在明显延迟，问题已复现！" : "✓ 性能正常，无明显延迟"}
            </Text>
          )}
        </View>
      </View>
    </ScrollView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "#f5f5f5",
  },
  content: {
    padding: 16,
  },
  title: {
    fontSize: 20,
    fontWeight: "bold",
    textAlign: "center",
    marginBottom: 12,
    color: "#333",
  },
  info: {
    fontSize: 14,
    textAlign: "center",
    color: "#666",
    marginBottom: 16,
  },
  loadingIndicator: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: "#E3F2FD",
    padding: 12,
    borderRadius: 8,
    marginBottom: 16,
  },
  loadingText: {
    marginLeft: 8,
    fontSize: 14,
    color: "#1976D2",
  },
  metrics: {
    backgroundColor: "#fff",
    padding: 16,
    borderRadius: 8,
    marginBottom: 16,
    borderLeftWidth: 4,
    borderLeftColor: "#007AFF",
  },
  metricTitle: {
    fontSize: 16,
    fontWeight: "bold",
    color: "#333",
    marginBottom: 12,
  },
  metricText: {
    fontSize: 14,
    color: "#333",
    marginBottom: 4,
    fontFamily: "monospace",
  },
  warning: {
    color: "#FF3B30",
    fontWeight: "bold",
  },
  success: {
    color: "#34C759",
    fontWeight: "bold",
  },
  button: {
    backgroundColor: "#007AFF",
    padding: 16,
    borderRadius: 8,
    marginBottom: 20,
  },
  buttonDisabled: {
    backgroundColor: "#ccc",
    opacity: 0.6,
  },
  buttonText: {
    color: "#fff",
    fontSize: 16,
    fontWeight: "600",
    textAlign: "center",
  },
  buttonHint: {
    color: "#fff",
    fontSize: 12,
    textAlign: "center",
    marginTop: 4,
    opacity: 0.8,
  },
  tipBox: {
    backgroundColor: "#E3F2FD",
    padding: 16,
    borderRadius: 8,
    marginBottom: 20,
  },
  tipTitle: {
    fontSize: 16,
    fontWeight: "bold",
    color: "#1976D2",
    marginBottom: 8,
  },
  tipText: {
    fontSize: 13,
    color: "#1976D2",
    lineHeight: 20,
  },
  imageBox: {
    marginBottom: 20,
    backgroundColor: "#fff",
    padding: 12,
    borderRadius: 8,
  },
  imageHeader: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
    marginBottom: 8,
  },
  imageLabel: {
    fontSize: 14,
    fontWeight: "600",
    color: "#333",
  },
  fast: {
    color: "#34C759",
    fontSize: 12,
    fontWeight: "600",
  },
  slow: {
    color: "#FF3B30",
    fontSize: 12,
    fontWeight: "600",
  },
  image: {
    height: 200,
    borderRadius: 8,
    backgroundColor: "#f0f0f0",
  },
  analysisBox: {
    backgroundColor: "#FFF3E0",
    padding: 16,
    borderRadius: 8,
    marginTop: 20,
  },
  analysisTitle: {
    fontSize: 16,
    fontWeight: "bold",
    color: "#F57C00",
    marginBottom: 8,
  },
  analysisText: {
    fontSize: 13,
    color: "#F57C00",
    lineHeight: 20,
  },
  base64Hint: {
    fontSize: 12,
    textAlign: "center",
    color: "#007AFF",
    marginTop: 8,
    marginBottom: 8,
    fontStyle: "italic",
    backgroundColor: "#E3F2FD",
    padding: 8,
    borderRadius: 4,
  },
});

export default FastImageDemo;