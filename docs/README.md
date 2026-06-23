# InvestInsight 文档入口

最后更新：2026-06-22

## 先读什么

处理代码前优先阅读：

1. `docs/codex/PROJECT_CONTEXT.md`：代码地图、核心流程、验证命令。
2. `docs/product/InvestInsight-product-overview.md`：产品定位、当前能力、后续方向。
3. 本文档：确认版本文档和发布文档的位置。

## 按职责查看

| 目录 | 内容 |
| --- | --- |
| `docs/codex/` | 给 Codex 和后续开发者看的项目上下文。 |
| `docs/product/` | 产品说明、能力边界和演进方向。 |
| `docs/versions/` | 按版本归档的设计稿、规格、实施计划、截图素材和发布说明。 |

## 按版本查看

| 版本 | 重点文档 |
| --- | --- |
| `docs/versions/v1.0/` | 1.0 发布与打包说明，当前可执行软件形态。 |
| `docs/versions/v2.0/` | 2.0 工作台 UI、事件传导引擎、新闻影响识别和后续重构计划。 |
| `docs/versions/v2.1/` | 2.1 事件传导引擎补完计划，聚焦事件状态、时间节点、路径规则、评分和追踪。 |
| `docs/versions/v2.2/` | 2.2 UI 内容可读性优化计划，聚焦事件证据链、理由去重、板块机会表格和截图验证。 |

## 文档命名规则

- 版本相关文档放入 `docs/versions/vX.Y/`，不要再新建 `docs/design` 或 `docs/superpowers`。
- 文件名使用稳定主题名，例如 `ui-workbench-redesign.md`、`event-impact-engine-design.md`。
- 日期写在正文的“最后更新”中，不作为文件名主键。
- 打包脚本、启动脚本、CMake 部署参数或图标资源变更时，同步更新对应版本的 `release/PACKAGING.md`。
