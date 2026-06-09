# InvestInsight

A 股板块分析与投资辅助工具，基于 Qt C++ 构建。实时拉取板块行情、指数数据，提供技术指标分析、资金流向追踪、个人持仓诊断及 AI 辅助投资建议。

## 功能概览

- **板块 & 指数行情**：实时拉取 A 股板块（东方财富 / 新浪多源）、A 股指数（腾讯 / 新浪）、美股指数（Yahoo Finance）
- **技术分析**：MACD / RSI / KDJ / MA / BOLL 等指标计算，日线 / 周线 / 月线 K 线合成
- **策略引擎**：趋势阶段识别、板块轮动检测、过热预警、资金结构分析
- **持仓管理**：支持多批次买入记录、首次收益日计算、分类型（股票 / ETF / 基金）盈亏统计
- **AI 分析**：接入 DeepSeek 等大模型，生成个性化投资建议
- **数据总览**：可视化仪表盘展示市场全貌、持仓损益、板块排行

## 环境要求

| 依赖 | 版本 |
|------|------|
| C++ 标准 | C++17 |
| CMake | ≥ 3.16 |
| Qt | Qt5 或 Qt6（Core, Widgets, Network, Concurrent） |

### macOS

```bash
brew install cmake qt
```

### Ubuntu / Debian

```bash
sudo apt install cmake qt6-base-dev libqt6-concurrent6
```

## 编译 & 运行

```bash
# 一键构建并启动
./run_gui.sh

# 或手动构建
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
export DYLD_FRAMEWORK_PATH="$(pwd)/../third_party"  # macOS only
./InvestInsight
```

## 项目结构

```
investinsight/
├── CMakeLists.txt              # 构建配置
├── run_gui.sh                  # 一键构建启动脚本
├── src/
│   ├── main.cpp                # 入口
│   ├── ui/
│   │   └── MainWindow.{h,cpp} # 主界面（设置、仪表盘、板块列表、策略、AI 对话）
│   ├── core/                   # 核心逻辑
│   │   ├── HttpClient          # HTTP 请求封装
│   │   ├── SectorFetcher       # 板块数据拉取（东方财富K线 + 新浪资金流）
│   │   ├── MarketContext       # 市场指数 & 宏观环境
│   │   ├── TechIndicators      # 技术指标计算
│   │   ├── StrategyEngine      # 综合策略评分
│   │   ├── AIAnalyzer          # AI 大模型接入
│   │   ├── InsightOrchestrator # 数据流编排
│   │   └── ...                 # 轮动检测、趋势分析、资金结构等
│   ├── domain/                 # 数据结构定义
│   └── providers/              # 数据源适配
├── third_party/
│   └── agl_stub.c              # macOS AGL framework 兼容 stub
└── summary/                    # 设计文档
```

## 数据源

| 数据 | 来源 | 用途 |
|------|------|------|
| 板块实时行情 | 东方财富 push2 / 新浪 | 涨跌幅、换手率、涨跌家数 |
| 板块 K 线 | 东方财富 push2his | 日线 OHLCV、技术指标计算 |
| 板块 K 线（回退） | 新浪 ETF 代理 | 东方财富不可用时的备选 |
| 板块资金流 | 新浪 MoneyFlow | 主力资金净流入 |
| A 股指数 | 腾讯 qt.gtimg.cn | 上证、深证、创业板、沪深 300、中证 500 |
| 美股指数 | Yahoo Finance | 纳斯达克、标普 500、道琼斯 |
