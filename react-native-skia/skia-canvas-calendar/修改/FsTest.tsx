//App.tsx
import React from 'react';
import {GestureHandlerRootView} from 'react-native-gesture-handler';

import {
  Canvas,
  Circle,
  Group,
  matchFont,
  useFont,
  Text as SkiaText,
} from '@shopify/react-native-skia';
import {useCallback, useEffect, useMemo, useState} from 'react';
import dayjs from 'dayjs';
import {
  DeviceEventEmitter,
  Dimensions,
  LayoutChangeEvent,
  StyleSheet,
  TextStyle,
  View,
  Text,
  TouchableOpacity,
} from 'react-native';
import {Platform} from '@react-native-oh/react-native-harmony';
import {runOnJS} from 'react-native-reanimated';
import {Gesture, GestureDetector} from 'react-native-gesture-handler';
// const fontFamily = require('../');

type Direction = 'left' | 'right';

export type CalendarArrowProps = {
  direction: Direction;
  onLeftPress?: (year: boolean) => void;
  onRightPress?: (year: boolean) => void;
  // 控制每个按钮的启用与颜色：true/undefined=启用；false=禁用
  leftEnabledMonth?: boolean;
  leftEnabledYear?: boolean;
  rightEnabledMonth?: boolean;
  rightEnabledYear?: boolean;
};

const CalendarArrow = (props: CalendarArrowProps) => {
  const activeColor = '#4E5969';
  const disabledColor = '#C9CDD4';
  if (props.direction === 'right') {
    const monthEnabled = props.rightEnabledMonth !== false;
    const yearEnabled = props.rightEnabledYear !== false;
    return (
      <View style={styles.container}>
        <TouchableOpacity
          activeOpacity={1}
          disabled={!monthEnabled}
          onPress={() => monthEnabled && props.onRightPress?.(false)}>
          <Text>下一月</Text>
        </TouchableOpacity>
        <TouchableOpacity
          activeOpacity={1}
          disabled={!yearEnabled}
          onPress={() => yearEnabled && props.onRightPress?.(true)}>
          <Text>下一年</Text>
        </TouchableOpacity>
      </View>
    );
  }
  return (
    <View style={styles.container}>
      {(() => {
        const yearEnabled = props.leftEnabledYear !== false;
        return (
          <TouchableOpacity
            activeOpacity={1}
            disabled={!yearEnabled}
            onPress={() => yearEnabled && props.onLeftPress?.(true)}>
            <Text>上一年</Text>
          </TouchableOpacity>
        );
      })()}
      {(() => {
        const monthEnabled = props.leftEnabledMonth !== false;
        return (
          <TouchableOpacity
            activeOpacity={1}
            disabled={!monthEnabled}
            onPress={() => monthEnabled && props.onLeftPress?.(false)}>
            <Text>上一月</Text>
          </TouchableOpacity>
        );
      })()}
    </View>
  );
};

// 日期区高度（与选中圆直径32匹配）
const CELL_HEIGHT = 32;
// 行间距
const ROW_GAP = 6;

// 可配置的布局参数（iOS/Android 可分别设置）
// - CIRCLE_DIAMETER：日期高亮圆的直径（需求：32px）
// - DATE_MARK_GAP：日期与下方标记的垂直间距（需求：贴着，0）
// - MARK_VISUAL_HEIGHT：标记文本可视高度（需求：20px）
// - MARK_BOTTOM_PADDING：标记到底部的最小留白
const LAYOUT = {
  CIRCLE_DIAMETER: Platform.select({ios: 32, android: 32, harmony: 32})!,
  DATE_MARK_GAP: Platform.select({ios: 0, android: 0, harmony: 0})!,
  MARK_VISUAL_HEIGHT: Platform.select({ios: 20, android: 20, harmony: 20})!, // 日期下方文案可视高度
  MARK_BOTTOM_PADDING: Platform.select({ios: 4, android: 4, harmony: 4})!,
};

const screenWidth = Dimensions.get('window').width;

type DayInfo = {day: number; isCurrentMonth: boolean};

type AxzCalendarProps = {
  current?: dayjs.Dayjs;
  markedDates?: Record<string, any>;
  onLayout?: ((event: LayoutChangeEvent) => void) | undefined;
  onLeftPress?: (year: boolean) => void;
  onRightPress?: (year: boolean) => void;
  onDayPress?: (date: dayjs.Dayjs) => void;
};

const CalendarDay = React.memo(
  ({
    day,
    isCurrentMonth,
    isToday,
    isSelected,
    markText,
    // showDot,
    font,
    markFont,
    centerX,
    centerY,
    textWidth,
    fontSize,
    markFontSize,
  }: {
    day: number;
    isCurrentMonth: boolean;
    isToday: boolean;
    isSelected: boolean | null;
    markText?: string;
    // showDot?: boolean;
    font: ReturnType<typeof useFont>;
    markFont: ReturnType<typeof useFont>;
    centerX: number;
    centerY: number;
    textWidth: number;
    fontSize: number;
    markFontSize: number;
  }) => {
    // 像素对齐，减少1px抖动
    const cx = Math.round(centerX);
    const cy = Math.round(centerY);
    const textX = Math.round(cx - textWidth / 2);
    // 使用字体度量精确定位基线，避免选中态与未选中态上下偏移
    const metrics: any = (font as any)?.getMetrics?.() ?? null;
    const ascent = metrics?.ascent ?? -fontSize * 0.8;
    const descent = metrics?.descent ?? fontSize * 0.2;
    // baseline = centerY - (ascent + descent)/2
    const textY = Math.round(cy - (ascent + descent) / 2);
    const markWidth = markFont?.getTextWidth(markText ?? '');
    const markStartX = Math.round(cx - (markWidth ?? 0) / 2);
    // 计算当前行的上/下边界，约束绘制元素不越界
    const rowTopY = cy - CELL_HEIGHT / 2;
    const rowBottomY = cy + CELL_HEIGHT / 2;
    // 标记文本：紧贴日期下方（gap=0），mark 文本“上沿”与日期文字“下沿”对齐
    const markMetrics: any = (markFont as any)?.getMetrics?.() ?? null;
    const markAscent = markMetrics?.ascent ?? -markFontSize * 0.8;
    const markDescent = markMetrics?.descent ?? markFontSize * 0.2;
    // 在 20px 标记区内垂直居中：
    // markCenter = rowBottomY + MARK_VISUAL_HEIGHT / 2
    // baseline = markCenter - (ascent + descent)/2
    const markCenter = rowBottomY + LAYOUT.MARK_VISUAL_HEIGHT / 2;
    const markStartY = Math.round(markCenter - (markAscent + markDescent) / 2);

    const {width} = Dimensions.get('window');
    const CELL_WIDTH = width / 7;
    // 圆半径：直径 32px（半径 16px），并限制不超过单元格边界
    const circleR = Math.min(
      LAYOUT.CIRCLE_DIAMETER / 2,
      Math.min(CELL_WIDTH, CELL_HEIGHT) / 2,
    );
    // 小圆点（用于“有数据”提示），位于日期下方，优先不与 markText 重叠
    // const dotRadius = 2.5;
    // // 尽量放在日期与标记文本之间，留出 3px 的间距避免遮挡
    // const idealDotY = textY + fontSize * 0.55;
    // const maxDotY = (markStartY ?? textY + fontSize) - 3 - dotRadius;
    // const dotY = Math.min(
    //   rowBottomY - LAYOUT.MARK_BOTTOM_PADDING - dotRadius,
    //   Math.min(idealDotY, maxDotY),
    // );
    console.log(day);

    return (
      <Group>
        {isToday && <Circle cx={cx} cy={cy} r={circleR} color="#EAFAF2" />}
        {isSelected && <Circle cx={cx} cy={cy} r={circleR} color="#08A86D" />}
        <SkiaText
          x={textX}
          y={textY}
          text={day.toString()}
          font={font}
          color={
            isCurrentMonth ? (isSelected ? 'white' : '#1D2129') : '#C9CDD4'
          }
        />
        {markText && isCurrentMonth && (
          <SkiaText
            x={markStartX}
            y={markStartY}
            text={markText}
            font={markFont}
            color="#86909C"
          />
        )}
        {/* 小圆点显示已取消 */}
      </Group>
    );
  },
  (prev, next) => {
    // 只要相关状态没变，组件不重新渲染
    return (
      prev.day === next.day &&
      prev.isCurrentMonth === next.isCurrentMonth &&
      prev.isToday === next.isToday &&
      prev.isSelected === next.isSelected &&
      prev.markText === next.markText
    );
  },
);

const AxzCalendar = (props: AxzCalendarProps) => {
  const [currentMonth, setCurrentMonth] = useState(dayjs());
  const [selectedDay, setSelectedDay] = useState<dayjs.Dayjs | null>(null);
  // 日历日期字号固定为 15px（不随屏幕缩放）
  const fontSize = 15;
  const markFontSize = 12;
  // const sCanvas = useRef<SkiaDomView>(undefined);
  useEffect(() => {
    if (props.current) {
      setCurrentMonth(props.current);
    }
  }, [props.current]);

  const {width} = Dimensions.get('window');
  const CELL_WIDTH = width / 7;

  const totalCalendarWidth = CELL_WIDTH * 7;
  const horizontalOffset = (screenWidth - totalCalendarWidth) / 2;

  const rowCount = useMemo(() => {
    const startDay = currentMonth.startOf('month').day(); // 0~6
    const daysInMonth = currentMonth.daysInMonth();
    return Math.ceil((startDay + daysInMonth) / 7);
  }, [currentMonth]);

  // 日期网格总高度 = 行数 * (日期区高度 + 标记区高度 + 行间距) - 最后一行不需要行间距
  // const gridHeight = useMemo(() => {

  // }, [rowCount]);
  const perRow = CELL_HEIGHT + LAYOUT.MARK_VISUAL_HEIGHT + ROW_GAP;
  const gridHeight = rowCount > 0 ? rowCount * perRow - ROW_GAP : 0;
  // 调试虚线已移除

  // 构造补齐后的日期数组，元素为 { day, isCurrentMonth }
  const days = useMemo(() => {
    const startOfMonth = currentMonth.startOf('month');
    const daysInMonth = currentMonth.daysInMonth();
    const startDay = startOfMonth.day(); // 0-6 星期几开始

    const prevMonth = currentMonth.subtract(1, 'month');
    const daysInPrevMonth = prevMonth.daysInMonth();

    const totalCells = rowCount * 7; // 固定6行7列

    const result: DayInfo[] = [];

    // 1. 上个月补齐天数（前面空白补成上个月日期）
    for (let i = startDay - 1; i >= 0; i--) {
      result.push({
        day: daysInPrevMonth - i,
        isCurrentMonth: false,
      });
    }

    // 2. 本月日期
    for (let i = 1; i <= daysInMonth; i++) {
      result.push({
        day: i,
        isCurrentMonth: true,
      });
    }

    // 3. 下个月补齐天数（填充剩余格子）
    while (result.length < totalCells) {
      result.push({
        day: result.length - startDay - daysInMonth + 1,
        isCurrentMonth: false,
      });
    }

    return result;
  }, [currentMonth]);

  const onSelectDay = useCallback(
    (dayNum: number, isCurrentMonth: boolean) => {
      if (!isCurrentMonth) return; // 只允许选中当前月日期
      const selected = currentMonth.date(dayNum); // ✅ 在 JS 线程中操作 dayjs
      setSelectedDay(selected);
      DeviceEventEmitter.emit('onDayPress', selected);
    },
    [currentMonth, props.onDayPress],
  );

  let font;
  let markFont;

  font = useFont(require("./DINAlternate-Bold.ttf"),12)

   markFont = useFont(require("./DINAlternate-Bold.ttf"),12)

  // font = matchFont({
  //   fontFamily: 'Sans',
  //   fontSize: fontSize,
  //   fontStyle: 'italic',
  //   fontWeight: 'bold',
  // });

  // 缓存 1~31 的文本度量信息
  const textMetricsMap = useMemo(() => {
    const map = new Map<string, number>();
    if (!font) return map;

    for (let i = 1; i <= 31; i++) {
      const text = i.toString();
      map.set(text, font.getTextWidth(text));
    }
    return map;
  }, [font]);

  // 手势
  const tapGesture = Gesture.Tap().onEnd(e => {
    const col = Math.floor(e.x / CELL_WIDTH);
    const rowHeightTotal = CELL_HEIGHT + LAYOUT.MARK_VISUAL_HEIGHT + ROW_GAP;
    const row = Math.floor(e.y / rowHeightTotal);
    const index = row * 7 + col;
    const dayInfo = days[index];
    if (dayInfo) {
      runOnJS(onSelectDay)(dayInfo.day, dayInfo.isCurrentMonth);
    }
  });

  const renderCalendarHeader = useCallback(
    (date: dayjs.Dayjs) => {
      const header = date.format('YYYY年MM月');
      const textStyle: TextStyle = {
        fontSize: 17,
        color: '#1D2129',
        fontWeight: '500',
        lineHeight: 24,
      };

      // 计算边界：1975-01 至 当前月份（不超过当月）
      const minMonth = dayjs('1975-01-01').startOf('month');
      const maxMonth = dayjs().startOf('month');
      const canMonthLeft = currentMonth.startOf('month').isAfter(minMonth);
      const canYearLeft = currentMonth.year() > 1975;
      const canMonthRight = currentMonth.startOf('month').isBefore(maxMonth);
      const canYearRight = currentMonth.year() < maxMonth.year();

      return (
        <View style={styles.headerRow}>
          <View style={styles.left}>
            <CalendarArrow
              direction="left"
              leftEnabledMonth={canMonthLeft}
              leftEnabledYear={canYearLeft}
              onLeftPress={year => {
                setSelectedDay(null);
                props?.onLeftPress?.(year);
              }}
            />
          </View>
          <View style={styles.center}>
            <Text style={[styles.year, textStyle]}>{header}</Text>
          </View>
          <View style={styles.right}>
            <CalendarArrow
              direction="right"
              rightEnabledMonth={canMonthRight}
              rightEnabledYear={canYearRight}
              onRightPress={year => {
                setSelectedDay(null);
                props?.onRightPress?.(year);
              }}
            />
          </View>
        </View>
      );
    },
    [props.onLeftPress, props.onRightPress],
  );
  if (!font) return null;
  if (!rowCount) return null;
  return (
    <View
      style={{
        width: '100%',
        backgroundColor: 'red',
      }}>
      {renderCalendarHeader(currentMonth)}
      <View
        style={{
          flexDirection: 'row',
          paddingHorizontal: horizontalOffset,
          marginTop: 6,
          marginBottom: 6,
        }}>
        {['日', '一', '二', '三', '四', '五', '六'].map((w, i) => (
          <View
            key={`w-${i}`}
            style={{width: CELL_WIDTH, alignItems: 'center'}}>
            <Text style={styles.weekItem}>{w}</Text>
          </View>
        ))}
      </View>
      <GestureDetector gesture={tapGesture}>
        <Canvas
          style={{
            width: '100%',
            height: gridHeight,
            backgroundColor: 'red',
          }}>
          {days.map(({day, isCurrentMonth}, index) => {
            const col = index % 7;
            const row = Math.floor(index / 7);
            // console.log(row);
            const centerX =
              col * CELL_WIDTH + CELL_WIDTH / 2 + horizontalOffset;
            // 每行总高度（日期区 + 标记区 + 行间距）
            const rowHeightTotal =
              CELL_HEIGHT + LAYOUT.MARK_VISUAL_HEIGHT + ROW_GAP;
            const rowTopY = row * rowHeightTotal;

            let centerY = rowTopY + CELL_HEIGHT / 2;
            const textWidth =
              textMetricsMap.get(day.toString()) ??
              font?.getTextWidth(day.toString());
            // 只有当前月(可选)并且是今天，才显示“今天背景圆”
            const isToday =
              isCurrentMonth && currentMonth.date(day).isSame(dayjs(), 'day');
            const mark =
              props.markedDates?.[currentMonth.date(day).format('YYYY-MM-DD')];
            const isSelected =
              isCurrentMonth &&
              selectedDay &&
              currentMonth.date(day).isSame(selectedDay, 'day');
            const markText = mark?.markText;
            // const showDot = !!mark?.showDot;
            return (
              <CalendarDay
                key={index}
                day={day}
                isCurrentMonth={isCurrentMonth}
                isToday={isToday}
                isSelected={isSelected}
                markText={`${markText ?? ''}`}
                // showDot={showDot}
                font={font}
                markFont={markFont}
                centerX={centerX}
                centerY={centerY}
                textWidth={textWidth}
                fontSize={fontSize}
                markFontSize={markFontSize}
              />
            );
          })}
        </Canvas>
      </GestureDetector>
    </View>
  );
};

const styles = StyleSheet.create({
  headerRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 20,
  },
  center: {
    marginTop: 10,
    marginBottom: 10,
    flex: 0.8,
    alignItems: 'center',
  },
  year: {
    marginRight: 5,
  },
  left: {
    flex: 0.2,
  },
  right: {
    flex: 0.2,
  },
  weekItem: {
    color: '#86909C',
    // 星期标题字号固定 15px
    fontSize: 15,
    lineHeight: 22,
  },

  container: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  monthLeft: {
    paddingHorizontal: 8,
    paddingVertical: 8,
    width: 20,
    height: 20,
  },
  yearLeft: {
    paddingRight: 8,
    paddingVertical: 8,
    width: 20,
    height: 20,
  },
  monthRight: {
    paddingHorizontal: 8,
    paddingVertical: 8,
    width: 20,
    height: 20,
  },
  yearRight: {
    paddingLeft: 8,
    paddingVertical: 8,
    width: 20,
    height: 20,
  },
});

const App = () => {
  const [current, setCurrent] = useState('2026-01-04');

  return (
    <GestureHandlerRootView style={{flex: 1, backgroundColor: 'yellow'}}>
      <View style={{marginTop: 100}}>
        <AxzCalendar
          current={dayjs(current)}
          onLeftPress={year => {
            const minMonth = dayjs('1975-01-01').startOf('month');
            const prev = year
              ? dayjs(current).subtract(1, 'year')
              : dayjs(current).subtract(1, 'month');
            const clamped = prev.isBefore(minMonth) ? minMonth : prev;
            setCurrent(clamped.startOf('month').format('YYYY-MM-DD'));
          }}
          onRightPress={year => {
            const maxMonth = dayjs().startOf('month');
            const next = year
              ? dayjs(current).add(1, 'year')
              : dayjs(current).add(1, 'month');
            const clamped = next.isAfter(maxMonth) ? maxMonth : next;
            setCurrent(clamped.startOf('month').format('YYYY-MM-DD'));
          }}></AxzCalendar>
      </View>
    </GestureHandlerRootView>
  );
};

export default App;
