#include "core/AIAnalyzer.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

void verifyCollaborationParser()
{
    AIAnalyzer analyzer;
    const QString payload = QString::fromUtf8(R"json(
{
  "sectors": {
    "半导体": {
      "readable_title": "先进封装需求继续升温",
      "summary": "产业链订单改善正在被市场重新定价。",
      "why_it_matters": "订单能否兑现会影响设备和材料板块的短期估值弹性。",
      "impact_path": "订单改善 -> 设备稼动率 -> 半导体设备估值 -> 财报验证",
      "affected_sectors": ["半导体设备：正向", "半导体材料：正向"],
      "primary_reason": "订单改善与资金流入同时出现，短期关注度提升。",
      "primary_risk": "若财报确认不足，前期涨幅可能回吐。",
      "next_checkpoint": "跟踪头部设备公司月度订单和一季报指引。",
      "confidence": 0.82,
      "source_refs": ["财联社：先进封装订单改善"],
      "disagreement_notes": "AI 偏积极，规则建议仍需等待回调确认。"
    }
  }
}
)json");

    const AICollaborationParseResult parsed = analyzer.parseCollaborationResponse(payload);
    expect(parsed.valid, "collaboration parser accepts valid JSON");
    expect(parsed.sectors.contains(QString::fromUtf8("半导体")), "collaboration parser stores sector key");

    const AIReadableInsight insight = parsed.sectors.value(QString::fromUtf8("半导体"));
    expect(insight.valid, "collaboration parser marks readable insight valid");
    expect(insight.readableTitle.contains(QString::fromUtf8("先进封装")), "collaboration parser reads title");
    expect(insight.impactPath.contains(QString::fromUtf8("财报验证")), "collaboration parser reads impact path");
    expect(insight.affectedSectors.size() == 2, "collaboration parser reads affected sectors");
    expect(insight.primaryReason.contains(QString::fromUtf8("订单改善")), "collaboration parser reads primary reason");
    expect(insight.primaryRisk.contains(QString::fromUtf8("回吐")), "collaboration parser reads primary risk");
    expect(insight.nextCheckpoint.contains(QString::fromUtf8("订单")), "collaboration parser reads next checkpoint");
    expect(insight.sourceRefs.size() == 1, "collaboration parser reads source refs");
}

void verifyInvalidJsonFallback()
{
    AIAnalyzer analyzer;
    const AICollaborationParseResult parsed = analyzer.parseCollaborationResponse(
        QString::fromUtf8("not-json"));
    expect(!parsed.valid, "collaboration parser rejects invalid JSON");
    expect(!parsed.errorMessage.isEmpty(), "collaboration parser explains invalid JSON");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    verifyCollaborationParser();
    verifyInvalidJsonFallback();

    if (failures > 0) {
        QTextStream(stderr) << failures << " AI analyzer smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "AIAnalyzer smoke passed.\n";
    return 0;
}
