from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


OUT_DIR = Path("docs/design/assets")
OUT_DIR.mkdir(parents=True, exist_ok=True)

FONT_PATH = Path(r"C:\Windows\Fonts\msyh.ttc")
BOLD_FONT_PATH = Path(r"C:\Windows\Fonts\msyhbd.ttc")
if not BOLD_FONT_PATH.exists():
    BOLD_FONT_PATH = FONT_PATH

COLORS = {
    "bg": "#f5f7fb",
    "surface": "#ffffff",
    "soft": "#fbfcfe",
    "line": "#d9e0ea",
    "text": "#111827",
    "muted": "#637083",
    "tiny": "#7a8798",
    "sidebar": "#111827",
    "sidebar_muted": "#8da2bd",
    "blue": "#2563eb",
    "blue_light": "#e8f0ff",
    "red": "#dc2626",
    "red_light": "#fee2e2",
    "green": "#059669",
    "green_light": "#d1fae5",
    "amber": "#b45309",
    "amber_light": "#fef3c7",
    "cyan": "#0891b2",
    "cyan_light": "#dff7fb",
    "purple": "#7c3aed",
    "purple_light": "#ede9fe",
}

NAV_ITEMS = ["总览", "事件雷达", "板块机会", "策略跟踪", "AI 助手", "配置"]


def font(size, bold=False):
    return ImageFont.truetype(str(BOLD_FONT_PATH if bold else FONT_PATH), size)


def text(draw, x, y, value, size=18, color=None, bold=False, anchor=None):
    draw.text((x, y), value, font=font(size, bold), fill=color or COLORS["text"], anchor=anchor)


def rr(draw, xy, fill, outline=None, radius=8, width=1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


def line(draw, x1, y1, x2, y2, color=None, width=1):
    draw.line((x1, y1, x2, y2), fill=color or COLORS["line"], width=width)


def card(draw, x, y, w, h, fill=None):
    rr(draw, (x, y, x + w, y + h), fill or COLORS["surface"], COLORS["line"], 8)


def pill(draw, x, y, label, fill, color=None, w=None):
    width = w or max(62, 18 + int(draw.textlength(label, font=font(15, True))))
    rr(draw, (x, y, x + width, y + 30), fill, None, 7)
    text(draw, x + width // 2, y + 6, label, 15, color or COLORS["text"], True, "ma")
    return width


def wrap(draw, value, size, max_width, bold=False):
    f = font(size, bold)
    lines = []
    current = ""
    for ch in value:
        test = current + ch
        if draw.textlength(test, font=f) <= max_width:
            current = test
        else:
            if current:
                lines.append(current)
            current = ch
    if current:
        lines.append(current)
    return lines


def canvas(width=1920, height=1080):
    img = Image.new("RGB", (width, height), COLORS["bg"])
    return img, ImageDraw.Draw(img)


def draw_shell(draw, width, height, active, title, subtitle):
    rr(draw, (0, 0, 236, height), COLORS["sidebar"], None, 0)
    text(draw, 34, 42, "InvestInsight", 28, "#ffffff", True)
    text(draw, 34, 78, "新闻驱动的板块洞察", 14, COLORS["sidebar_muted"])
    for i, item in enumerate(NAV_ITEMS):
        y = 118 + i * 56
        if item == active:
            rr(draw, (18, y, 218, y + 44), "#e8f0ff", None, 8)
            text(draw, 44, y + 13, item, 22, "#1f3b6d", True)
        else:
            text(draw, 44, y + 14, item, 20, COLORS["sidebar_muted"])
    rr(draw, (24, height - 174, 212, height - 56), "#172033", "#334155", 8)
    text(draw, 44, height - 150, "数据状态", 17, "#e5edf8", True)
    draw.ellipse((42, height - 116, 52, height - 106), fill="#22c55e")
    text(draw, 62, height - 121, "行情 09:57 更新", 14, "#cbd5e1")
    draw.ellipse((42, height - 86, 52, height - 76), fill="#f59e0b")
    text(draw, 62, height - 91, "新闻增量 16 条", 14, "#cbd5e1")

    # Top bar mirrors the selected left navigation item. It intentionally has no
    # AI/config shortcut buttons and no tab strip, so the sidebar is the only
    # primary navigation model in the design.
    draw.rectangle((236, 0, width, 76), fill=COLORS["surface"], outline=COLORS["line"])
    text(draw, 268, 30, title, 25, COLORS["text"], True)
    text(draw, 394, 33, subtitle, 18, COLORS["muted"])
    text(draw, width - 332, 31, "就绪 · 行情 09:57 · 新闻增量 16 条", 15, COLORS["muted"])


def save(img, name):
    path = OUT_DIR / name
    img.save(path)
    print(path)


def draw_metric(draw, x, y, label, value, note, accent=None):
    card(draw, x, y, 268, 112)
    text(draw, x + 22, y + 22, label, 16, COLORS["muted"])
    text(draw, x + 22, y + 52, value, 28, accent or COLORS["text"], True)
    text(draw, x + 22, y + 88, note, 13, COLORS["tiny"])


def event_radar_page():
    img, draw = canvas()
    draw_shell(draw, 1920, 1080, "事件雷达", "事件雷达", "事件状态、时间节点、传导路径和影响板块")

    card(draw, 260, 104, 424, 900)
    text(draw, 286, 138, "事件列表", 22, bold=True)
    text(draw, 564, 142, "按重要性排序", 15, COLORS["muted"])
    events = [
        ("预期", "美联储降息预期升温", "FOMC / CPI / PCE", "76%", COLORS["amber_light"]),
        ("确认", "先进封装设备订单超预期", "业绩预告 / 订单落地", "82%", COLORS["red_light"]),
        ("修正", "锂价反弹但库存仍偏高", "库存周报 / 现货价格", "68%", COLORS["cyan_light"]),
        ("待观察", "专项债发行节奏加快", "财政数据 / 基建开工", "63%", COLORS["green_light"]),
        ("风险", "外部限制政策再发酵", "商务部回应 / 企业订单", "71%", COLORS["purple_light"]),
    ]
    for i, item in enumerate(events):
        y = 176 + i * 142
        fill = "#fff7ed" if i == 0 else COLORS["soft"]
        outline = "#fed7aa" if i == 0 else COLORS["line"]
        rr(draw, (286, y, 658, y + 112), fill, outline, 8)
        pill(draw, 308, y + 18, item[0], item[4], w=76)
        text(draw, 398, y + 20, item[1], 18, COLORS["text"], True)
        text(draw, 398, y + 52, "下一节点：" + item[2], 14, COLORS["muted"])
        text(draw, 398, y + 80, "置信度 " + item[3], 14, COLORS["tiny"])

    card(draw, 708, 104, 612, 420)
    text(draw, 734, 140, "事件详情", 22, bold=True)
    pill(draw, 734, 176, "预期", COLORS["amber_light"], w=76)
    text(draw, 826, 177, "美联储降息预期升温", 24, COLORS["text"], True)
    detail = (
        "多篇海外宏观新闻显示市场对后续降息的押注升温，但事件尚未落地。"
        "系统应把它作为观察信号，而不是直接给出买入结论。"
    )
    for i, row in enumerate(wrap(draw, detail, 16, 520)):
        text(draw, 734, 230 + i * 28, row, 16, COLORS["muted"])
    text(draw, 734, 326, "关键观察点", 18, bold=True)
    checkpoints = ["美国 CPI 数据", "PCE 通胀数据", "非农就业", "FOMC 议息会议"]
    for i, cp in enumerate(checkpoints):
        pill(draw, 734 + i * 132, 360, cp, "#eef2ff", "#1f3b6d", 118)

    card(draw, 1344, 104, 512, 420)
    text(draw, 1370, 140, "影响板块", 22, bold=True)
    headers = ["板块", "方向", "强度", "关系"]
    for i, head in enumerate(headers):
        text(draw, 1370 + i * 112, 182, head, 14, "#64748b", True)
    for i, row in enumerate([
        ("黄金", "正向", "0.78", "直接"),
        ("有色金属", "正向", "0.69", "直接"),
        ("半导体", "正向", "0.57", "间接"),
        ("创新药", "正向", "0.51", "间接"),
        ("银行", "分化", "0.32", "条件"),
    ]):
        y = 220 + i * 50
        line(draw, 1370, y - 16, 1828, y - 16)
        text(draw, 1370, y, row[0], 17)
        color = COLORS["red"] if row[1] == "正向" else COLORS["amber"]
        text(draw, 1482, y, row[1], 17, color, True)
        text(draw, 1594, y, row[2], 17, "#334155")
        pill(draw, 1706, y - 8, row[3], "#f1f5f9", "#334155", 64)

    card(draw, 708, 548, 1148, 456)
    text(draw, 734, 584, "传导路径与情景分支", 22, bold=True)
    nodes = [
        (764, 660, "降息预期", COLORS["blue_light"]),
        (990, 660, "美债收益率下行", "#eef2ff"),
        (1240, 660, "成长估值修复", COLORS["red_light"]),
        (990, 808, "美元走弱", COLORS["amber_light"]),
        (1240, 808, "黄金 / 有色", COLORS["red_light"]),
        (1490, 808, "半导体 / 创新药", "#fff1f2"),
    ]
    for x, y, label, fill in nodes:
        rr(draw, (x, y, x + 172, y + 58), fill, "#cbd5e1", 8)
        text(draw, x + 86, y + 18, label, 16, COLORS["text"], True, "ma")
    arrows = [(936, 689, 990, 689), (1162, 689, 1240, 689), (1076, 718, 1076, 808),
              (1162, 837, 1240, 837), (1412, 837, 1490, 837)]
    for x1, y1, x2, y2 in arrows:
        line(draw, x1, y1, x2, y2, "#94a3b8", 2)
        draw.polygon([(x2, y2), (x2 - 10, y2 - 6), (x2 - 10, y2 + 6)], fill="#94a3b8")
    text(draw, 764, 926, "失效条件：如果降息原因转为衰退风险上升，成长股和周期股影响需要下调。", 16, COLORS["amber"], True)
    save(img, "investinsight-ui-redesign-event-radar.png")


def sector_opportunities_page():
    img, draw = canvas()
    draw_shell(draw, 1920, 1080, "板块机会", "板块机会", "板块评分、事件催化、趋势状态和风险排序")

    filters = ["全部板块", "事件催化强", "趋势跟踪", "过热谨慎", "持仓相关"]
    for i, item in enumerate(filters):
        fill = COLORS["blue"] if i == 0 else COLORS["surface"]
        color = "#ffffff" if i == 0 else COLORS["muted"]
        rr(draw, (260 + i * 128, 104, 370 + i * 128, 142), fill, "#cbd5e1", 8)
        text(draw, 315 + i * 128, 114, item, 15, color, True, "ma")

    draw_metric(draw, 260, 164, "今日最强催化", "半导体", "事件 0.82 / 趋势 0.74", COLORS["red"])
    draw_metric(draw, 548, 164, "低位观察", "有色金属", "跌幅 -0.77 / 事件 0.69", COLORS["green"])
    draw_metric(draw, 836, 164, "过热提醒", "锂电池", "涨幅 +5.06 / 事件偏弱", COLORS["amber"])
    draw_metric(draw, 1124, 164, "持仓相关", "3 个板块", "2 个观察，1 个减仓", COLORS["blue"])
    draw_metric(draw, 1412, 164, "数据质量", "91/100", "实时行情优先", COLORS["text"])

    card(draw, 260, 304, 1156, 620)
    text(draw, 286, 340, "机会列表", 22, bold=True)
    headers = ["板块", "今日", "5日", "事件", "趋势", "资金", "动作", "下一观察点"]
    xs = [286, 480, 596, 712, 828, 944, 1060, 1184]
    for x, head in zip(xs, headers):
        text(draw, x, 386, head, 14, "#64748b", True)
    rows = [
        ("半导体", "+2.19%", "+6.8%", "0.82", "0.74", "净流入", "跟踪", "先进封装订单"),
        ("有色金属", "-0.77%", "+1.2%", "0.69", "0.58", "分歧", "观察", "美元/铜价"),
        ("证券", "+1.18%", "+2.1%", "0.57", "0.44", "小幅流入", "观察", "成交额放大"),
        ("创新药", "-0.31%", "+0.9%", "0.53", "0.49", "平稳", "观察", "海外利率"),
        ("锂电池", "+5.06%", "+9.4%", "0.41", "0.36", "短线涌入", "过热", "锂价库存"),
        ("机器人", "+1.42%", "+4.5%", "0.48", "0.61", "净流入", "跟踪", "订单验证"),
        ("军工", "+0.62%", "+1.8%", "0.44", "0.55", "平稳", "观察", "地缘风险"),
        ("房地产", "-1.06%", "-2.2%", "0.38", "0.22", "流出", "回避", "政策落地"),
    ]
    for i, row in enumerate(rows):
        y = 428 + i * 56
        if i % 2:
            draw.rectangle((276, y - 18, 1392, y + 26), fill="#f8fafc")
        text(draw, xs[0], y, row[0], 17)
        text(draw, xs[1], y, row[1], 18, COLORS["red"] if row[1].startswith("+") else COLORS["green"], True)
        text(draw, xs[2], y, row[2], 16, COLORS["red"] if row[2].startswith("+") else COLORS["green"], True)
        for j in range(3, 6):
            text(draw, xs[j], y, row[j], 16, "#334155")
        fill = COLORS["red_light"] if row[6] == "跟踪" else COLORS["amber_light"] if row[6] == "观察" else COLORS["cyan_light"] if row[6] == "过热" else COLORS["green_light"]
        pill(draw, xs[6], y - 10, row[6], fill, w=70)
        text(draw, xs[7], y, row[7], 16, COLORS["muted"])

    card(draw, 1440, 304, 416, 620)
    text(draw, 1466, 340, "右侧观察篮", 22, bold=True)
    watch = [
        ("短线机会", "半导体、机器人", COLORS["red_light"]),
        ("趋势跟踪", "有色金属、创新药", COLORS["amber_light"]),
        ("过热谨慎", "锂电池", COLORS["cyan_light"]),
        ("回避/减配", "房地产", COLORS["green_light"]),
    ]
    for i, item in enumerate(watch):
        y = 386 + i * 108
        rr(draw, (1466, y, 1830, y + 78), item[2], None, 8)
        text(draw, 1490, y + 16, item[0], 18, COLORS["text"], True)
        text(draw, 1490, y + 48, item[1], 15, COLORS["muted"])
    text(draw, 1466, 852, "设计要求：列表支持搜索、排序、持仓过滤；点击板块进入详情长页。", 15, COLORS["tiny"])
    save(img, "investinsight-ui-redesign-sector-opportunities.png")


def strategy_tracking_page():
    img, draw = canvas()
    draw_shell(draw, 1920, 1080, "策略跟踪", "策略跟踪", "建议动作、持仓影响、信号表现和后续检查点")

    for i, metric in enumerate([
        ("组合风险", "中", "集中度 32%", COLORS["amber"]),
        ("建议动作", "观察为主", "2 跟踪 / 1 过热", COLORS["blue"]),
        ("信号新鲜度", "18m", "延迟可接受", COLORS["green"]),
        ("待验证事件", "4 个", "FOMC、CPI、锂价", COLORS["text"]),
    ]):
        draw_metric(draw, 260 + i * 396, 104, metric[0], metric[1], metric[2], metric[3])

    card(draw, 260, 252, 720, 360)
    text(draw, 286, 288, "持仓影响", 22, bold=True)
    headers = ["持仓板块", "金额", "今日", "系统建议", "原因"]
    xs = [286, 430, 552, 674, 790]
    for x, head in zip(xs, headers):
        text(draw, x, 334, head, 14, "#64748b", True)
    rows = [
        ("半导体", "30,000", "+2.19%", "继续跟踪", "事件催化强，注意过热"),
        ("有色金属", "18,000", "-0.77%", "观察", "降息预期支撑但价格分歧"),
        ("锂电池", "12,000", "+5.06%", "过热谨慎", "涨幅领先于事件确认"),
        ("证券", "8,000", "+1.18%", "观察", "流动性预期改善"),
    ]
    for i, row in enumerate(rows):
        y = 374 + i * 56
        line(draw, 286, y - 18, 954, y - 18)
        text(draw, xs[0], y, row[0], 17)
        text(draw, xs[1], y, row[1], 16)
        text(draw, xs[2], y, row[2], 17, COLORS["red"] if row[2].startswith("+") else COLORS["green"], True)
        pill(draw, xs[3], y - 10, row[3], COLORS["amber_light"] if row[3] == "观察" else COLORS["red_light"] if row[3] == "继续跟踪" else COLORS["cyan_light"], w=86)
        text(draw, xs[4], y, row[4], 15, COLORS["muted"])

    card(draw, 1004, 252, 852, 360)
    text(draw, 1030, 288, "信号跟踪", 22, bold=True)
    headers = ["信号", "首次发现", "1日", "3日", "状态", "复盘"]
    xs = [1030, 1260, 1390, 1498, 1606, 1718]
    for x, head in zip(xs, headers):
        text(draw, x, 334, head, 14, "#64748b", True)
    rows = [
        ("半导体先进封装订单", "09:42", "+2.1%", "-", "观察中", "等待二次确认"),
        ("有色受降息预期影响", "09:50", "-0.7%", "-", "分歧", "看铜价美元"),
        ("锂电池锂价反弹", "10:05", "+5.0%", "-", "过热", "谨慎追高"),
        ("证券流动性预期", "10:12", "+1.1%", "-", "观察中", "看成交额"),
    ]
    for i, row in enumerate(rows):
        y = 374 + i * 56
        line(draw, 1030, y - 18, 1830, y - 18)
        for j, value in enumerate(row):
            color = COLORS["red"] if value.startswith("+") else COLORS["amber"] if value in ["分歧", "过热"] else COLORS["text"]
            text(draw, xs[j], y, value, 15 if j in [0, 5] else 16, color, j in [2, 4])

    card(draw, 260, 636, 1596, 352)
    text(draw, 286, 672, "执行建议", 22, bold=True)
    advice = [
        ("今日不做追涨", "锂电池涨幅已经领先事件确认，作为过热提醒而不是买入信号。", COLORS["cyan_light"]),
        ("保留观察仓位", "半导体事件催化强，但需要看资金是否连续净流入。", COLORS["red_light"]),
        ("设置复查节点", "有色金属等待美元指数、铜价和美债收益率同步验证。", COLORS["amber_light"]),
    ]
    for i, item in enumerate(advice):
        x = 286 + i * 510
        rr(draw, (x, 716, x + 470, 900), item[2], None, 8)
        text(draw, x + 24, 750, item[0], 21, COLORS["text"], True)
        for j, row in enumerate(wrap(draw, item[1], 16, 410)):
            text(draw, x + 24, 798 + j * 28, row, 16, COLORS["muted"])
    save(img, "investinsight-ui-redesign-strategy-tracking.png")


def ai_assistant_page():
    img, draw = canvas()
    draw_shell(draw, 1920, 1080, "AI 助手", "AI 助手", "基于当前分析结果的问答和复盘")

    card(draw, 260, 104, 420, 900)
    text(draw, 286, 140, "当前上下文", 22, bold=True)
    context = [
        ("市场状态", "偏强震荡，风险 69/100"),
        ("重点事件", "美联储降息预期、先进封装订单"),
        ("强势板块", "半导体、锂电池、机器人"),
        ("风险板块", "锂电池过热，地产偏弱"),
        ("持仓", "半导体 / 有色 / 锂电池 / 证券"),
    ]
    for i, item in enumerate(context):
        y = 188 + i * 88
        rr(draw, (286, y, 654, y + 64), COLORS["soft"], COLORS["line"], 8)
        text(draw, 308, y + 14, item[0], 15, COLORS["muted"])
        text(draw, 308, y + 38, item[1], 16, COLORS["text"], True)
    text(draw, 286, 684, "快捷问题", 18, bold=True)
    chips = ["为什么推荐半导体？", "锂电池还能追吗？", "有色金属看什么指标？", "我的持仓要调吗？"]
    for i, chip in enumerate(chips):
        pill(draw, 286, 724 + i * 44, chip, COLORS["blue_light"], "#1f3b6d", 188)

    card(draw, 708, 104, 1148, 900)
    text(draw, 734, 140, "对话", 22, bold=True)
    messages = [
        ("user", "半导体今天涨了 2.19%，现在还能继续看吗？"),
        ("ai", "可以继续跟踪，但不建议只因为今日涨幅追高。当前主要理由是先进封装订单和海外限制事件带来的中期逻辑增强。接下来应观察资金是否连续净流入，以及龙头个股是否放量但不过热。"),
        ("user", "有色金属今天是跌的，为什么还在观察里？"),
        ("ai", "因为它的行情和事件信号分化。今日涨幅为 -0.77%，但降息预期可能通过美元走弱和商品价格影响黄金、有色。这个结论需要铜价、美元指数和美债收益率共同验证。"),
    ]
    y = 190
    for speaker, msg in messages:
        if speaker == "user":
            rr(draw, (1110, y, 1810, y + 62), COLORS["blue"], None, 8)
            color = "#ffffff"
            x = 1130
        else:
            lines = wrap(draw, msg, 16, 940)
            h = 42 + len(lines) * 26
            rr(draw, (734, y, 1718, y + h), COLORS["soft"], COLORS["line"], 8)
            color = COLORS["text"]
            x = 758
            for j, row in enumerate(lines):
                text(draw, x, y + 22 + j * 26, row, 16, color)
            y += h + 28
            continue
        text(draw, x, y + 20, msg, 16, color)
        y += 90
    rr(draw, (734, 914, 1718, 970), "#ffffff", "#cbd5e1", 8)
    text(draw, 760, 933, "输入问题，例如：请解释半导体的事件传导路径", 16, COLORS["tiny"])
    rr(draw, (1734, 914, 1810, 970), COLORS["blue"], None, 8)
    text(draw, 1756, 932, "发送", 17, "#ffffff", True)
    save(img, "investinsight-ui-redesign-ai-assistant.png")


def config_page():
    img, draw = canvas()
    draw_shell(draw, 1920, 1080, "配置", "配置", "AI 接入、持仓、数据刷新和提醒设置")

    card(draw, 260, 104, 740, 412)
    text(draw, 286, 140, "AI 接入配置", 22, bold=True)
    providers = [
        ("DeepSeek", "已配置", "deepseek-chat"),
        ("OpenAI", "未配置", "gpt-4o"),
        ("通义千问", "未配置", "qwen-plus"),
        ("Claude", "未配置", "claude-3-5-haiku"),
        ("Gemini", "未配置", "gemini-1.5-flash"),
    ]
    for i, item in enumerate(providers):
        y = 188 + i * 56
        text(draw, 286, y, item[0], 16, COLORS["text"], True)
        rr(draw, (400, y - 10, 828, y + 24), COLORS["soft"], COLORS["line"], 7)
        text(draw, 416, y - 2, "sk-... / " + item[2], 14, COLORS["tiny"])
        fill = COLORS["green_light"] if item[1] == "已配置" else "#f1f5f9"
        pill(draw, 848, y - 10, item[1], fill, COLORS["green"] if item[1] == "已配置" else COLORS["muted"], 76)
    pill(draw, 286, 476, "启用 AI 深度分析", COLORS["blue_light"], "#1f3b6d", 148)
    text(draw, 464, 482, "深度分析板块数 TopN：20", 15, COLORS["muted"])

    card(draw, 1024, 104, 832, 412)
    text(draw, 1050, 140, "我的持仓", 22, bold=True)
    headers = ["板块", "类型", "买入日期", "金额", "状态"]
    xs = [1050, 1210, 1340, 1502, 1640]
    for x, head in zip(xs, headers):
        text(draw, x, 188, head, 14, "#64748b", True)
    rows = [
        ("半导体", "基金", "2026-05-20", "30,000", "持有"),
        ("有色金属", "ETF", "2026-06-03", "18,000", "持有"),
        ("锂电池", "基金", "2026-06-11", "12,000", "观察"),
        ("证券", "ETF", "2026-06-14", "8,000", "持有"),
    ]
    for i, row in enumerate(rows):
        y = 230 + i * 56
        line(draw, 1050, y - 18, 1816, y - 18)
        for j, value in enumerate(row):
            text(draw, xs[j], y, value, 16, COLORS["text"] if j != 4 else COLORS["blue"], j == 4)
    rr(draw, (1050, 466, 1170, 504), COLORS["blue"], None, 8)
    text(draw, 1082, 476, "新增持仓", 16, "#ffffff", True)

    card(draw, 260, 548, 740, 360)
    text(draw, 286, 584, "后台刷新与提醒", 22, bold=True)
    settings = [
        ("盘中增量扫描", "每 15 分钟", COLORS["green_light"]),
        ("关注板块优先", "半导体、有色、锂电池", COLORS["blue_light"]),
        ("提醒阈值", "事件催化 > 0.65 或过热风险 > 0.70", COLORS["amber_light"]),
    ]
    for i, item in enumerate(settings):
        y = 638 + i * 78
        rr(draw, (286, y, 954, y + 54), item[2], None, 8)
        text(draw, 310, y + 16, item[0], 17, COLORS["text"], True)
        text(draw, 508, y + 16, item[1], 16, COLORS["muted"])

    card(draw, 1024, 548, 832, 360)
    text(draw, 1050, 584, "数据源健康", 22, bold=True)
    sources = ["同花顺实时行情", "同花顺 K 线", "东方财富板块", "新浪资金流", "多源财经新闻"]
    for i, source in enumerate(sources):
        y = 638 + i * 50
        draw.ellipse((1052, y + 4, 1064, y + 16), fill="#22c55e")
        text(draw, 1080, y, source, 16)
        text(draw, 1320, y, "正常", 16, COLORS["green"], True)
        text(draw, 1430, y, "最近更新 09:57", 14, COLORS["tiny"])
    save(img, "investinsight-ui-redesign-config.png")


def sector_detail_long_page():
    width, height = 1920, 3600
    img, draw = canvas(width, height)
    draw_shell(draw, width, height, "板块机会", "半导体详情", "半导体详情：行情、事件、路径、技术面和证据新闻")

    y = 104
    card(draw, 260, y, 1596, 188)
    text(draw, 286, y + 34, "半导体", 30, COLORS["text"], True)
    text(draw, 286, y + 78, "上一交易日 +2.18% · 趋势强势看多 · 综合评分 0.64 · 风险 69/100", 20, COLORS["red"], True)
    text(draw, 286, y + 120, "结论：当前行情强，但详情页必须同时展示收益分层、信号解释、技术指标、资金流、回测和新闻证据。", 17, COLORS["muted"])
    rr(draw, (286, y + 150, 1830, y + 176), "#fee2e2", None, 4)
    text(draw, 306, y + 153, "投资信号：强势看多。该结论用于跟踪和复盘，不等同于无条件买入。", 15, COLORS["red"], True)
    pill(draw, 1560, y + 44, "强势看多", COLORS["red_light"], COLORS["text"], 112)
    pill(draw, 1690, y + 44, "需防追高", COLORS["amber_light"], COLORS["text"], 124)

    y = 324
    metrics = [
        ("短期收益", "+10.2%", "短线热度和 5 日动量"),
        ("中期收益", "-12.6%", "中期仍有回撤压力"),
        ("长期收益", "+71.8%", "长期趋势保持强势"),
        ("风险分", "69/100", "过热和拥挤度需跟踪"),
    ]
    for i, item in enumerate(metrics):
        card(draw, 260 + i * 404, y, 374, 136, "#f0fdf4" if i == 0 else "#fee2e2" if i == 1 else "#fff1f2" if i == 2 else "#fff7ed")
        text(draw, 286 + i * 404, y + 26, item[0], 17, COLORS["muted"])
        value_color = COLORS["green"] if item[1].startswith("-") else COLORS["red"] if item[1].startswith("+") else COLORS["amber"]
        text(draw, 286 + i * 404, y + 62, item[1], 34, value_color, True)
        text(draw, 286 + i * 404, y + 108, item[2], 14, COLORS["tiny"])

    y = 492
    card(draw, 260, y, 760, 380)
    text(draw, 286, y + 36, "信号解释", 22, bold=True)
    signal_lines = [
        "综合判断：动量强、趋势线向上、成交量放大，短线仍处强势区。",
        "正向因素：产业政策、先进封装、国产替代、AI 算力需求和资金关注。",
        "风险因素：短期涨幅较大，若新闻无二次确认或资金转弱，评分需要下调。",
        "页面要求：保留原有“看多/看空原因”列表，避免只展示最终结论。",
    ]
    for i, item in enumerate(signal_lines):
        bullet_color = COLORS["red"] if i < 2 else COLORS["amber"]
        draw.ellipse((286, y + 92 + i * 54, 298, y + 104 + i * 54), fill=bullet_color)
        for j, row in enumerate(wrap(draw, item, 15, 640)):
            text(draw, 314, y + 84 + i * 54 + j * 22, row, 15, COLORS["muted"])

    card(draw, 1044, y, 812, 380)
    text(draw, 1070, y + 36, "核心评分雷达", 22, bold=True)
    scores = [
        ("技术强度", 88, COLORS["green"]),
        ("新闻热度", 83, COLORS["green"]),
        ("估值分位", 60, COLORS["amber"]),
        ("拥挤度", 57, COLORS["amber"]),
        ("资金流", 44, COLORS["amber"]),
        ("数据质量", 88, COLORS["green"]),
        ("源一致性", 67, COLORS["green"]),
    ]
    for i, (label, value, color) in enumerate(scores):
        yy = y + 92 + i * 38
        text(draw, 1070, yy, label, 15, COLORS["muted"])
        rr(draw, (1208, yy + 2, 1698, yy + 16), "#eef2f7", None, 7)
        rr(draw, (1208, yy + 2, 1208 + int(4.9 * value), yy + 16), color, None, 7)
        text(draw, 1720, yy - 4, str(value), 18, color, True)

    y = 904
    card(draw, 260, y, 760, 420)
    text(draw, 286, y + 36, "事件驱动", 22, bold=True)
    events = [
        ("确认", "先进封装设备订单超预期", "直接影响：设备、材料、封测"),
        ("预期", "海外限制政策继续发酵", "间接影响：国产替代逻辑增强"),
        ("预期", "降息预期缓解成长估值压力", "间接影响：高弹性成长板块"),
    ]
    for i, item in enumerate(events):
        yy = y + 84 + i * 98
        rr(draw, (286, yy, 986, yy + 72), COLORS["soft"], COLORS["line"], 8)
        pill(draw, 310, yy + 20, item[0], COLORS["red_light"] if item[0] == "确认" else COLORS["amber_light"], w=68)
        text(draw, 396, yy + 16, item[1], 18, COLORS["text"], True)
        text(draw, 396, yy + 46, item[2], 15, COLORS["muted"])

    card(draw, 1044, y, 812, 420)
    text(draw, 1070, y + 36, "影响路径", 22, bold=True)
    nodes = [(1090, y + 116, "海外限制"), (1310, y + 116, "国产替代"), (1530, y + 116, "设备/材料需求"),
             (1310, y + 256, "资金关注度提升"), (1530, y + 256, "板块趋势强化")]
    for x, yy, label in nodes:
        rr(draw, (x, yy, x + 162, yy + 58), COLORS["blue_light"] if yy < y + 200 else COLORS["red_light"], "#cbd5e1", 8)
        text(draw, x + 81, yy + 18, label, 15, COLORS["text"], True, "ma")
    for x1, yy1, x2, yy2 in [(1252, y + 145, 1310, y + 145), (1472, y + 145, 1530, y + 145),
                             (1390, y + 174, 1390, y + 256), (1472, y + 285, 1530, y + 285)]:
        line(draw, x1, yy1, x2, yy2, "#94a3b8", 2)
        draw.polygon([(x2, yy2), (x2 - 10, yy2 - 6), (x2 - 10, yy2 + 6)], fill="#94a3b8")
    text(draw, 1070, y + 356, "失效条件：若订单无法传导到业绩，或板块连续放量冲高但资金转弱，应下调评分。", 15, COLORS["amber"], True)

    def mini_chart(x, y, w, h, color, title, value):
        rr(draw, (x, y, x + w, y + h), "#fbfcfe", COLORS["line"], 8)
        text(draw, x + 14, y + 12, title, 14, COLORS["muted"])
        text(draw, x + w - 18, y + 12, value, 14, color, True, "ra")
        pts = [(x + 18 + i * (w - 36) / 10, y + h - 30 - ((i * 13 + (i % 3) * 17) % 74)) for i in range(11)]
        draw.line(pts, fill=color, width=3)

    y = 1356
    card(draw, 260, y, 1596, 650)
    text(draw, 286, y + 36, "趋势图与技术指标", 22, bold=True)
    mini_chart(286, y + 84, 286, 132, COLORS["red"], "短线", "+10.2%")
    mini_chart(600, y + 84, 286, 132, COLORS["amber"], "中线", "-12.6%")
    mini_chart(914, y + 84, 286, 132, COLORS["purple"], "长线", "+71.8%")
    chart_x, chart_y, chart_w, chart_h = 286, y + 252, 1150, 318
    draw.rectangle((chart_x, chart_y, chart_x + chart_w, chart_y + chart_h), fill="#fbfcfe", outline=COLORS["line"])
    for i in range(6):
        yy = chart_y + 30 + i * 48
        line(draw, chart_x, yy, chart_x + chart_w, yy, "#e5e7eb")
    points = [(chart_x + i * 70, chart_y + 235 - ((i % 5) * 18) - i * 3) for i in range(16)]
    draw.line(points, fill=COLORS["blue"], width=4)
    for idx, label in [(4, "订单"), (8, "政策"), (12, "资金")]:
        x, yy = points[idx]
        draw.ellipse((x - 6, yy - 6, x + 6, yy + 6), fill=COLORS["red"])
        text(draw, x - 16, yy - 34, label, 13, COLORS["red"], True)
    tech_box_x = 1480
    techs = [("MACD", "金叉扩张", COLORS["red"]), ("RSI", "64，中性偏强", COLORS["text"]),
             ("KDJ", "高位钝化", COLORS["amber"]), ("均线", "5/20 日上行", COLORS["red"]),
             ("BOLL", "接近上轨", COLORS["amber"]), ("成交量", "温和放大", COLORS["blue"])]
    for i, item in enumerate(techs):
        yy = y + 92 + i * 72
        rr(draw, (tech_box_x, yy, tech_box_x + 318, yy + 50), COLORS["soft"], COLORS["line"], 8)
        text(draw, tech_box_x + 18, yy + 15, item[0], 16, COLORS["muted"])
        text(draw, tech_box_x + 116, yy + 15, item[1], 16, item[2], True)
    text(draw, 286, y + 604, "设计要求：保留多周期小图、K 线/成交量/MACD/资金流/KDJ 等原有数据密度，事件点可点击展开证据。", 15, COLORS["tiny"])

    y = 2038
    card(draw, 260, y, 760, 396)
    text(draw, 286, y + 36, "阶段收益与回测", 22, bold=True)
    backtest = [
        ("近 5 日", "+13.86%", COLORS["red"]),
        ("近 20 日", "+12.50%", COLORS["red"]),
        ("近 60 日", "-11.36%", COLORS["green"]),
        ("累计收益", "+54.87%", COLORS["red"]),
        ("回测胜率", "89%", COLORS["red"]),
        ("信号覆盖率", "70%", COLORS["red"]),
    ]
    for i, item in enumerate(backtest):
        x = 286 + (i % 2) * 330
        yy = y + 94 + (i // 2) * 82
        rr(draw, (x, yy, x + 286, yy + 56), COLORS["soft"], COLORS["line"], 8)
        text(draw, x + 18, yy + 18, item[0], 15, COLORS["muted"])
        text(draw, x + 160, yy + 16, item[1], 20, item[2], True)

    card(draw, 1044, y, 812, 396)
    text(draw, 1070, y + 36, "资金流与相关板块", 22, bold=True)
    flows = [
        ("今日资金", "小幅净流入", COLORS["red"]),
        ("5 日资金", "持续改善", COLORS["red"]),
        ("北向敏感度", "中等", COLORS["amber"]),
        ("市场宽度", "涨 918 / 跌 1392", COLORS["text"]),
    ]
    for i, item in enumerate(flows):
        yy = y + 92 + i * 48
        text(draw, 1070, yy, item[0], 15, COLORS["muted"])
        text(draw, 1230, yy, item[1], 16, item[2], True)
    text(draw, 1070, y + 304, "联动板块：芯片、先进封装、半导体设备、材料、AI 算力。", 16, COLORS["muted"])
    text(draw, 1070, y + 338, "页面要求：相关板块应展示涨跌、评分和跳转入口。", 15, COLORS["tiny"])

    y = 2466
    card(draw, 260, y, 1596, 472)
    text(draw, 286, y + 36, "新闻证据与 AI/规则解释", 22, bold=True)
    news = [
        ("09:42", "财联社", "先进封装设备订单超预期，产业链公司排产提升", "正向"),
        ("09:55", "证券时报", "国产替代方向再获政策支持，材料端关注度提升", "正向"),
        ("10:08", "东方财富", "半导体板块资金净流入扩大，封测方向领涨", "正向"),
        ("10:21", "华尔街见闻", "海外限制政策继续扰动供应链，国产化逻辑升温", "正向"),
        ("10:36", "同花顺", "部分高位题材波动加大，短线交易拥挤度提升", "风险"),
        ("10:52", "新浪财经", "消费电子链订单恢复仍需确认，业绩兑现存在分化", "中性"),
    ]
    for i, item in enumerate(news):
        yy = y + 90 + i * 54
        if i % 2:
            draw.rectangle((286, yy - 16, 1818, yy + 26), fill="#f8fafc")
        text(draw, 286, yy, item[0], 15, COLORS["tiny"])
        pill(draw, 372, yy - 10, item[1], "#eef2ff", "#1f3b6d", 86)
        text(draw, 480, yy, item[2], 16, COLORS["text"])
        tag_fill = COLORS["red_light"] if item[3] == "正向" else COLORS["amber_light"] if item[3] == "风险" else "#f1f5f9"
        pill(draw, 1704, yy - 10, item[3], tag_fill, w=64)
    text(draw, 286, y + 430, "解释要求：新闻按事件归组，保留来源、时间、方向和证据权重；AI 解释只能补充，不覆盖规则和数据来源。", 15, COLORS["amber"], True)

    y = 2970
    card(draw, 260, y, 1596, 300)
    text(draw, 286, y + 36, "数据质量与调试信息", 22, bold=True)
    quality = [
        "今日涨幅：同花顺实时分时优先，当前 +2.18%；K 线只用于趋势图和历史序列。",
        "数据覆盖：行情、K 线、资金流、估值、拥挤度、新闻、AI 解释均需展示来源状态。",
        "缺失处理：当资金流或估值缺失时，在详情页显示缺口，不用空白区域掩盖。",
        "调试入口：保留数据源、最新日期、源一致性、数据质量、规则/AI 可用性。",
    ]
    for i, item in enumerate(quality):
        text(draw, 286, y + 92 + i * 34, item, 16, COLORS["muted"])
    text(draw, 286, y + 250, "长图目标：后续实现时，板块详情页既要解释投资逻辑，也要保留当前页面已有的完整量化指标。", 16, COLORS["amber"], True)
    save(img, "investinsight-ui-redesign-sector-detail-long.png")


if __name__ == "__main__":
    event_radar_page()
    sector_opportunities_page()
    strategy_tracking_page()
    ai_assistant_page()
    config_page()
    sector_detail_long_page()
