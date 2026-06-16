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
- Qt5（≥ 5.15）或 Qt6（Core、Widgets、Network、Concurrent）
- 编译器：GCC / Clang / MSVC（Visual Studio 2019+）

### macOS

```bash
brew install cmake qt
```

### Ubuntu / Debian

```bash
sudo apt install cmake qt6-base-dev libqt6-concurrent6
```

### Windows

Windows 下推荐使用 **vcpkg + Visual Studio + CMake** 的组合。以下为完整步骤：

#### 1. 安装 Visual Studio

从 [Visual Studio 官网](https://visualstudio.microsoft.com/) 下载 Community 版（免费），安装时勾选 **"使用 C++ 的桌面开发"** 工作负载。

#### 2. 安装 CMake

从 [CMake 官网](https://cmake.org/download/) 下载安装，安装时勾选 **"Add CMake to the system PATH"**。

或使用 winget：

```powershell
winget install Kitware.CMake
```

#### 3. 安装 vcpkg 并安装 Qt5

```powershell
# 克隆 vcpkg 到 C:\vcpkg（路径可自选，建议不含中文和空格）
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# 安装 Qt5（包含 Core、Widgets、Network、Concurrent，约需 15 分钟）
C:\vcpkg\vcpkg.exe install qt5-base:x64-windows
```

> 首次安装 vcpkg 会自动编译 OpenSSL、zlib、freetype 等依赖，耗时较长属正常现象。

#### 4. 配置与编译

```powershell
cd C:\code\investmentTools
mkdir build
cd build

# CMake 配置（指定 vcpkg 工具链）
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# 编译（/m 启用并行编译）
cmake --build . --config Release -- /m
```

编译产物位于 `build\Release\InvestInsight.exe`。

#### 5. 运行

```powershell
.\Release\InvestInsight.exe
```

#### Windows 常见问题

| 问题 | 解决方案 |
|------|----------|
| CMake 找不到 Qt | 确认 `cmake` 命令带了 `-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake` |
| 中文乱码编译错误 | CMakeLists.txt 已包含 MSVC `/utf-8` 编译选项，若仍有问题确认源文件以 UTF-8 BOM 保存 |
| 缺少 DLL 无法启动 | 将 `C:\vcpkg\installed\x64-windows\bin` 加入系统 PATH，或将其中的 Qt5*.dll 复制到 exe 同目录 |
| `cl.exe` 未找到 | 使用 Visual Studio 的 "Developer PowerShell" 或 "x64 Native Tools Command Prompt" 执行编译命令 |

## 编译与运行（macOS / Linux）

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
