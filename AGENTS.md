# InvestInsight Codex 工作约定

本项目后续由 Codex 处理代码前，先阅读：

- `docs/README.md`
- `docs/codex/PROJECT_CONTEXT.md`
- `docs/product/InvestInsight-product-overview.md`

这些文件分别记录文档入口、当前代码地图和产品说明，用来减少重复扫描代码的时间。

## 修改同步要求

如果本次改动涉及以下任一内容，必须同步更新上面的文档：

- 数据源、行情口径、新闻源、板块池或板块分类逻辑。
- `InsightOrchestrator` 的分析流水线、评分因子、权重、阈值或推荐动作。
- AI 分析、新闻影响识别、短期/长期判断方式。
- UI 主流程、后台刷新、提醒、持仓管理或用户可见的产品定位。
- 新增调试命令、构建命令、运行方式或重要验证步骤。
- 打包脚本、启动脚本、CMake 部署参数、应用图标或发布产物结构。

## 文档组织规则

- `docs/README.md` 是文档总入口；新增或迁移重要文档后必须同步更新该入口。
- 版本相关的设计稿、规格、实施计划和截图素材统一放入 `docs/versions/vX.Y/`，例如 `docs/versions/v2.0/design/`、`docs/versions/v2.0/plans/`、`docs/versions/v2.0/specs/`。
- 版本文档文件名使用稳定主题名，不使用日期作为文件名主键；日期只写在文档正文的“最后更新”中。
- 不再新增 `docs/design` 或 `docs/superpowers` 这类来源型目录；旧文档若继续演进，应先迁入对应版本目录。
- 跨版本长期有效的文档保留在职责型目录中，例如 `docs/codex/`、`docs/product/`。

## 跨平台要求

- 修改代码时默认同时考虑 Windows 和 macOS；除非用户明确说明只处理某一个平台，否则视为两个平台都需要可用。
- 涉及平台差异时，需要检查对应平台分支、启动脚本、资源路径、图标、Qt 部署和构建参数，避免只修 Windows 或只修 macOS。
- 如果改动影响打包、发布、运行脚本或构建参数，必须同时检查并按需更新 `package_windows.ps1`、`package_macos.sh`、`run_gui.bat`、`run_gui.sh` 和对应版本的打包说明，例如 `docs/versions/v1.0/release/PACKAGING.md`。

## 提交与验证规则

- 每次 commit 尽量控制在 500 行以内，200 到 300 行最佳；如果历史文档、设计图或二进制资源导致无法严格控制，需要在提交说明或交付说明中解释原因。
- 每次提交前必须完成与本次改动范围匹配的功能测试或构建验证，验证通过后才可以提交。
- 修改完成并确认无误后，必须提交到本地 commit；提交信息使用中文，并参考历史格式，例如 `功能(ui): ...`、`修复(ui): ...`、`工具(打包): ...`。
- commit 只允许提交到本地仓库，不可以直接 push 到远端；需要远端操作时必须由用户单独确认。
- 较大的功能必须按职责拆成多个小提交，避免把文档、UI 重构、数据逻辑和打包改动混在一个 commit 中。

## 关键约束

- 板块“今日涨幅”以同花顺实时分时数据为最高优先级；日 K 线主要用于图表和兜底，不能在已有实时涨幅时覆盖它。
- 涉及板块行情口径时，修改后运行 `build\Release\InvestInsight.exe --dump-sector-changes`，重点核对有色金属、半导体、锂电池。
- 常规代码修改后运行 `cmake --build build --config Release -- /m`。
- UI 重构或界面样式修改后运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1`。
- 不要提交 AI Key、个人持仓明细或本地缓存数据。
