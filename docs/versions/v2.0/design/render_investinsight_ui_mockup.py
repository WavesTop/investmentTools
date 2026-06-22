from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


W, H = 1920, 1080
OUT = Path("docs/versions/v2.0/design/assets/investinsight-ui-redesign-dashboard.png")
OUT.parent.mkdir(parents=True, exist_ok=True)

FONT_PATH = Path(r"C:\Windows\Fonts\msyh.ttc")
BOLD_FONT_PATH = Path(r"C:\Windows\Fonts\msyhbd.ttc")
if not BOLD_FONT_PATH.exists():
    BOLD_FONT_PATH = FONT_PATH


def font(size, bold=False):
    return ImageFont.truetype(str(BOLD_FONT_PATH if bold else FONT_PATH), size)


img = Image.new("RGB", (W, H), "#f5f7fb")
d = ImageDraw.Draw(img)

C = {
    "surface": "#ffffff",
    "soft": "#fbfcfe",
    "line": "#d9e0ea",
    "text": "#111827",
    "muted": "#637083",
    "tiny": "#7a8798",
    "blue": "#2563eb",
    "blue_light": "#e8f0ff",
    "red": "#dc2626",
    "red_light": "#fee2e2",
    "green": "#059669",
    "green_light": "#d1fae5",
    "amber": "#b45309",
    "amber_light": "#fef3c7",
    "cyan_light": "#dff7fb",
    "sidebar": "#111827",
    "sidebar_muted": "#8da2bd",
}


def rr(xy, fill, outline=None, r=8, w=1):
    d.rounded_rectangle(xy, radius=r, fill=fill, outline=outline, width=w)


def rect(xy, fill, outline=None, w=1):
    d.rectangle(xy, fill=fill, outline=outline, width=w)


def txt(x, y, s, size=18, color=None, bold=False, anchor=None):
    d.text((x, y), s, font=font(size, bold), fill=color or C["text"], anchor=anchor)


def line(x1, y1, x2, y2, color=None, width=1):
    d.line((x1, y1, x2, y2), fill=color or C["line"], width=width)


def card(x, y, w, h):
    rr((x, y, x + w, y + h), C["surface"], C["line"], 8)


def node(x, y, w, h, label, fill, outline):
    rr((x, y, x + w, y + h), fill, outline, 8)
    txt(x + w // 2, y + 19, label, 15, "#1f2937", True, anchor="ma")


# Sidebar
rect((0, 0, 236, H), C["sidebar"])
txt(34, 42, "InvestInsight", 28, "#ffffff", True)
txt(34, 78, "新闻驱动的板块洞察", 14, C["sidebar_muted"])
rr((18, 118, 218, 162), "#e8f0ff", None, 8)
txt(44, 131, "总览", 22, "#1f3b6d", True)
for i, item in enumerate(["事件雷达", "板块机会", "策略跟踪", "AI 助手", "配置"]):
    txt(44, 188 + i * 56, item, 20, C["sidebar_muted"])

rr((24, 906, 212, 1024), "#172033", "#334155", 8)
txt(44, 930, "数据状态", 17, "#e5edf8", True)
d.ellipse((42, 964, 52, 974), fill="#22c55e")
txt(62, 959, "行情 09:57 更新", 14, "#cbd5e1")
d.ellipse((42, 994, 52, 1004), fill="#f59e0b")
txt(62, 989, "新闻增量 16 条", 14, "#cbd5e1")

# Header: page title only. Primary actions live inside the active workspace page.
rect((236, 0, W, 76), C["surface"], C["line"])
txt(268, 30, "综合总览", 25, C["text"], True)
txt(394, 33, "2026-06-20 09:57 · AI 已启用 · 覆盖 186 个板块", 18, C["muted"])
txt(1584, 31, "就绪 · 行情 09:57 · 新闻增量 16 条", 15, C["muted"])

# Metric cards
for x, w in [(260, 392), (672, 312), (1004, 312), (1336, 520)]:
    card(x, 104, w, 124)
txt(286, 128, "市场温度", 17, C["muted"])
txt(286, 160, "偏强震荡", 32, C["text"], True)
txt(510, 167, "+0.62", 20, C["red"], True)
txt(286, 202, "涨跌家数、指数、资金流综合判断", 13, C["tiny"])
txt(698, 128, "新闻新鲜度", 17, C["muted"])
txt(698, 160, "18m", 32, C["text"], True)
txt(774, 167, "较新", 20, C["blue"], True)
txt(698, 202, "首次发现到触发提醒的延迟", 13, C["tiny"])
txt(1030, 128, "风险拥挤度", 17, C["muted"])
txt(1030, 160, "中等", 32, C["text"], True)
txt(1122, 167, "过热 3 个", 17, C["amber"], True)
txt(1030, 202, "高热度、高涨幅、资金背离", 13, C["tiny"])
txt(1362, 128, "分析控制", 17, C["muted"])
rr((1362, 158, 1504, 202), C["blue"], None, 8)
txt(1394, 170, "开始分析", 18, "#ffffff", True)
txt(1532, 160, "AI 已启用", 17, C["text"], True)
txt(1532, 194, "点击后刷新行情、新闻和事件雷达", 13, C["tiny"])

# Event radar
card(260, 252, 560, 382)
txt(286, 280, "关键事件雷达", 20, C["text"], True)
txt(690, 283, "按重要性排序", 15, C["muted"])
events = [
    ("#fff7ed", "#fed7aa", C["amber_light"], "预期", "美联储降息预期升温", "影响：有色、黄金、半导体、创新药 · 置信度 76%"),
    ("#f8fafc", C["line"], C["red_light"], "确认", "先进封装设备订单超预期", "影响：半导体、设备、材料 · 置信度 82%"),
    ("#f8fafc", C["line"], C["cyan_light"], "修正", "锂价反弹但库存仍偏高", "影响：锂电池、新能源车 · 置信度 68%"),
]
for i, event in enumerate(events):
    y = 314 + i * 100
    rr((286, y, 794, y + 84), event[0], event[1], 8)
    rr((304, y + 20, 372, y + 46), event[2], None, 6)
    txt(319, y + 22, event[3], 15, "#1f2937", True)
    txt(388, y + 24, event[4], 17, C["text"])
    txt(388, y + 54, event[5], 13, C["tiny"])

# Opportunity table
card(844, 252, 648, 382)
txt(870, 280, "板块机会与风险", 20, C["text"], True)
txt(1354, 283, "红色为 A 股正向涨幅", 15, C["muted"])
line(870, 316, 1466, 316)
for x, head in [(870, "板块"), (1070, "今日"), (1160, "事件"), (1260, "趋势"), (1360, "动作")]:
    txt(x, 330, head, 14, "#64748b", True)
line(870, 366, 1466, 366)
rows = [
    ("半导体", "+2.19%", C["red"], "0.82", "0.74", "跟踪", C["red_light"]),
    ("有色金属", "-0.77%", C["green"], "0.69", "0.58", "观察", C["amber_light"]),
    ("锂电池", "+5.06%", C["red"], "0.41", "0.36", "过热", C["cyan_light"]),
    ("证券", "+1.18%", C["red"], "0.57", "0.44", "观察", C["amber_light"]),
    ("创新药", "-0.31%", C["green"], "0.53", "0.49", "观察", C["amber_light"]),
]
for i, row in enumerate(rows):
    y = 386 + i * 54
    txt(870, y, row[0], 17, C["text"])
    txt(1070, y, row[1], 20, row[2], True)
    txt(1160, y, row[3], 17, "#334155")
    txt(1260, y, row[4], 17, "#334155")
    rr((1358, y - 12, 1432 if row[5] != "过热" else 1450, y + 20), row[6], None, 6)
    txt(1374, y - 8, row[5], 15, "#1f2937", True)
    line(870, y + 34, 1466, y + 34)

# Risk panel
card(1516, 252, 340, 382)
txt(1542, 280, "风险与失效条件", 20, C["text"], True)
risks = [
    ("#fff7ed", "#fed7aa", C["amber"], "未发生事件不直接推买入", "降息仍是预期，需等 CPI/PCE 验证"),
    ("#f0fdf4", "#bbf7d0", C["green"], "锂电池短线过热", "涨幅过大但事件催化不足"),
    ("#f8fafc", C["line"], C["text"], "数据质量", "同花顺实时涨幅优先\n新闻源 12 路并发，去重后 42 条"),
]
for i, (fill, outline, color, title, body) in enumerate(risks):
    y = [320, 408, 496][i]
    h = [70, 70, 92][i]
    rr((1542, y, 1830, y + h), fill, outline, 8)
    txt(1564, y + 20, title, 17, color, True)
    for j, line_text in enumerate(body.split("\n")):
        txt(1564, y + 48 + j * 22, line_text, 13, C["tiny"])

# Impact path
card(260, 658, 760, 350)
txt(286, 686, "事件传导路径", 20, C["text"], True)
txt(286, 718, "示例：美联储降息预期如何影响板块", 15, C["muted"])
node(306, 770, 150, 58, "降息预期", C["blue_light"], None)
line(456, 799, 548, 799, "#94a3b8", 2)
d.polygon([(548, 799), (536, 792), (536, 806)], fill="#94a3b8")
node(548, 770, 170, 58, "美债收益率下行", "#eef2ff", "#c7d2fe")
line(718, 799, 810, 799, "#94a3b8", 2)
d.polygon([(810, 799), (798, 792), (798, 806)], fill="#94a3b8")
node(810, 770, 170, 58, "成长估值修复", "#fff1f2", "#fecdd3")
line(634, 828, 634, 890, "#94a3b8", 2)
d.polygon([(634, 890), (627, 878), (641, 878)], fill="#94a3b8")
node(548, 890, 170, 58, "美元走弱", "#fef3c7", "#fde68a")
line(718, 919, 810, 919, "#94a3b8", 2)
d.polygon([(810, 919), (798, 912), (798, 926)], fill="#94a3b8")
node(810, 890, 170, 58, "黄金 / 有色", "#fee2e2", "#fecaca")
txt(306, 972, "设计要求：每条结论必须能展开证据新闻、状态、下一观察点和失效条件。", 13, C["tiny"])

# Sector detail first screen
card(1044, 658, 812, 350)
txt(1070, 686, "板块详情首屏布局", 20, C["text"], True)
txt(1070, 718, "后续点击某个板块时，优先解释“为什么”和“接下来观察什么”", 15, C["muted"])
rr((1070, 752, 1830, 812), C["soft"], C["line"], 8)
txt(1094, 772, "半导体", 17, C["text"])
txt(1232, 772, "今日 +2.19%", 20, C["red"], True)
txt(1374, 774, "综合 0.74", 17, "#334155")
txt(1514, 774, "事件 0.82", 17, "#334155")
txt(1654, 774, "风险 中", 17, C["amber"], True)
metrics = [
    ("短线热度", "高", "#fff7ed", "#fed7aa"),
    ("事件催化", "强", "#fee2e2", "#fecaca"),
    ("中期趋势", "上行", "#eef2ff", "#c7d2fe"),
    ("兑现风险", "中", "#f0fdf4", "#bbf7d0"),
]
for i, (label, value, fill, outline) in enumerate(metrics):
    x = 1070 + i * 194
    rr((x, 836, x + 178, 932), fill, outline, 8)
    txt(x + 22, 862, label, 17, C["muted"])
    txt(x + 22, 886, value, 30, C["text"], True)
txt(
    1070,
    966,
    "组件拆分目标：DashboardRenderer、EventRadarRenderer、SectorDetailRenderer 独立渲染，MainWindow 只负责状态和导航。",
    13,
    C["tiny"],
)

img.save(OUT)
print(OUT)
