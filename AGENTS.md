# InvestInsight Codex 工作约定

本项目后续由 Codex 处理代码前，先阅读：

- `docs/codex/PROJECT_CONTEXT.md`
- `docs/product/InvestInsight-product-overview.md`

这两个文件分别记录当前代码地图和产品说明，用来减少重复扫描代码的时间。

## 修改同步要求

如果本次改动涉及以下任一内容，必须同步更新上面的文档：

- 数据源、行情口径、新闻源、板块池或板块分类逻辑。
- `InsightOrchestrator` 的分析流水线、评分因子、权重、阈值或推荐动作。
- AI 分析、新闻影响识别、短期/长期判断方式。
- UI 主流程、后台刷新、提醒、持仓管理或用户可见的产品定位。
- 新增调试命令、构建命令、运行方式或重要验证步骤。

## 关键约束

- 板块“今日涨幅”以同花顺实时分时数据为最高优先级；日 K 线主要用于图表和兜底，不能在已有实时涨幅时覆盖它。
- 涉及板块行情口径时，修改后运行 `build\Release\InvestInsight.exe --dump-sector-changes`，重点核对有色金属、半导体、锂电池。
- 常规代码修改后运行 `cmake --build build --config Release -- /m`。
- 不要提交 AI Key、个人持仓明细或本地缓存数据。
