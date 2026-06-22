# InvestInsight 1.0 打包说明

最后更新：2026-06-22

## Windows 打包

在项目根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\package_windows.ps1
```

默认产物会放在项目根目录：

- `InvestInsight-Windows\InvestInsight.exe`
- `InvestInsight-Windows.zip`

脚本行为：

- 使用独立构建目录 `build-package-windows`，不会覆盖常规开发构建目录 `build`。
- Windows 包默认启用 GUI 子系统，双击启动时不会额外弹出控制台窗口。
- 若根目录已有同名产物，脚本会先删除；如果删除失败，则自动使用 `InvestInsight-Windows-1`、`InvestInsight-Windows-2` 这样的递增名称。
- 优先使用 `windeployqt` 部署 Qt 运行库；如果当前环境没有 `windeployqt`，会从 vcpkg 的 `installed\x64-windows` 目录复制 Qt DLL 和插件作为 fallback。

常用参数：

```powershell
# 指定构建目录
powershell -NoProfile -ExecutionPolicy Bypass -File .\package_windows.ps1 -BuildDir build-package-windows

# 只复制现有构建，不重新 build
powershell -NoProfile -ExecutionPolicy Bypass -File .\package_windows.ps1 -SkipBuild

# 不生成 zip
powershell -NoProfile -ExecutionPolicy Bypass -File .\package_windows.ps1 -NoZip

# 指定 vcpkg toolchain
powershell -NoProfile -ExecutionPolicy Bypass -File .\package_windows.ps1 -ToolchainFile C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

使用方式：

1. 解压 `InvestInsight-Windows.zip`。
2. 打开 `InvestInsight-Windows` 文件夹。
3. 双击 `InvestInsight.exe`。
4. 首次进入配置页，可填写 AI Provider 和 Key；不填写也可以使用规则引擎分析。

## macOS 打包

macOS 需要在 macOS 机器上运行：

```bash
chmod +x ./package_macos.sh
./package_macos.sh
```

默认产物会放在项目根目录：

- `InvestInsight-macOS.app`
- `InvestInsight-macOS.zip`

脚本行为：

- 使用独立构建目录 `build-macos`。
- 若根目录已有同名产物，脚本会先删除；如果删除失败，则自动使用递增名称。
- 优先使用 `macdeployqt` 部署 Qt Frameworks；如果当前环境没有 `macdeployqt`，会复制 `.app` 并提示目标机器可能缺少 Qt Frameworks。
- 如果没有显式指定 toolchain，会尝试从常规 `build/CMakeCache.txt` 继承 `CMAKE_TOOLCHAIN_FILE`。

常用参数：

```bash
# 指定构建目录
./package_macos.sh --build-dir build-macos

# 只复制现有构建，不重新 build
./package_macos.sh --skip-build

# 复用常规开发构建目录
./package_macos.sh --build-dir build --skip-build

# 不生成 zip
./package_macos.sh --no-zip

# 指定 toolchain
./package_macos.sh --toolchain-file /path/to/toolchain.cmake
```

也可以继续用环境变量覆盖默认值：

```bash
BUILD_DIR="$PWD/build-macos" CONFIGURATION=Release ./package_macos.sh
```

使用方式：

1. 解压 `InvestInsight-macOS.zip`。
2. 将 `InvestInsight-macOS.app` 拖入“应用程序”。
3. 第一次打开如果出现安全提示，右键点击 app，选择“打开”。
4. 首次进入配置页，可填写 AI Provider 和 Key；不填写也可以使用规则引擎分析。

## 图标资源

图标源和平台资源位于：

- `assets/app-icon.svg`：可维护的 SVG 源。
- `assets/app-icon.png`：Qt 运行时窗口图标资源。
- `assets/windows/app-icon.ico`：Windows exe 图标。
- `assets/macos/app-icon.icns`：macOS Dock/Finder 图标。
- `tools/generate_app_icons.py`：从项目图标设计重新生成 PNG、ICO、ICNS。

重新生成图标：

```powershell
& "C:\Users\oneDayDay\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" tools\generate_app_icons.py
```

如果在普通环境运行，也可以使用任何带 Pillow 的 Python：

```bash
python tools/generate_app_icons.py
```
