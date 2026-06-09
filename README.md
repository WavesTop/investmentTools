# InvestInsight

A 股板块分析与投资辅助桌面工具，基于 Qt/C++17 构建。

## 功能

- 板块 & 指数实时行情（东方财富、新浪、腾讯、Yahoo Finance 多源）
- 技术指标分析（MACD、RSI、KDJ、MA、BOLL）与日/周/月 K 线
- 板块轮动检测、趋势阶段识别、过热预警、资金结构分析
- 个人持仓管理（多批次、多类型、首次收益日、分批盈亏）
- AI 大模型辅助分析（DeepSeek 等）

## 环境要求

- C++17、CMake ≥ 3.16
- Qt5 或 Qt6（Core、Widgets、Network、Concurrent）

```bash
# macOS
brew install cmake qt

# Ubuntu / Debian
sudo apt install cmake qt6-base-dev libqt6-concurrent6
```

## 编译与运行

```bash
./run_gui.sh

# 或手动：
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./InvestInsight
```

> macOS 下如遇 AGL 加载失败，需设置 `export DYLD_FRAMEWORK_PATH=<项目>/third_party`。

## 项目结构

```
├── CMakeLists.txt          # 构建配置
├── run_gui.sh              # 一键构建启动脚本
├── src/
│   ├── main.cpp
│   ├── ui/                 # 界面层（MainWindow）
│   ├── core/               # 核心逻辑
│   │   ├── HttpClient      # HTTP 请求
│   │   ├── SectorFetcher   # 板块数据拉取
│   │   ├── MarketContext   # 指数与宏观环境
│   │   ├── TechIndicators  # 技术指标计算
│   │   ├── StrategyEngine  # 策略评分
│   │   ├── AIAnalyzer      # AI 接入
│   │   └── ...             # 轮动、趋势、资金流等分析模块
│   ├── domain/             # 数据结构
│   └── providers/          # 数据源适配
└── third_party/            # 平台兼容 stub
```

## 数据源

| 数据 | 来源 |
|------|------|
| 板块行情 & K 线 | 东方财富 push2 / push2his |
| 板块 K 线（回退） | 新浪 ETF 代理 |
| 板块资金流 | 新浪 MoneyFlow |
| A 股指数 | 腾讯 qt.gtimg.cn / 新浪 |
| 美股指数 | Yahoo Finance |
