#include "providers/RealFinanceNewsProvider.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtConcurrent>
#include <functional>

#include "core/HttpClient.h"

namespace {
struct SectorKeywordMap
{
    QString sector;
    QStringList keywords;
};

QList<SectorKeywordMap> buildInfluenceMap()
{
    return {
        // ===== 科技 =====
        { "半导体",   { "半导体", "芯片", "晶圆", "光刻", "封测", "EDA", "台积电", "ASML", "英伟达", "AMD", "算力", "GPU", "存储器", "DRAM", "NAND", "HBM", "先进制程", "中芯国际", "海光", "寒武纪", "长江存储" } },
        { "电子器件", { "电子器件", "PCB", "MLCC", "被动元件", "连接器", "面板", "OLED", "LCD", "显示", "Mini LED", "Micro LED", "柔性屏", "京东方" } },
        { "电子信息", { "电子信息", "AI", "人工智能", "大模型", "ChatGPT", "GPT", "机器人", "智能", "数字经济", "数据中心", "云计算", "5G", "6G", "深度学习", "Sora", "DeepSeek", "百度", "科大讯飞", "商汤", "算力租赁", "AI手机", "AI PC", "AIGC", "具身智能", "多模态" } },
        { "计算机",   { "计算机", "信创", "国产替代", "操作系统", "数据库", "网络安全", "网安", "华为", "鸿蒙", "麒麟", "中间件", "工业软件" } },
        { "软件",     { "软件", "SaaS", "企业服务", "ERP", "低代码", "RPA", "办公软件", "金山", "用友" } },
        { "通信",     { "通信", "光纤", "光模块", "卫星", "北斗", "中兴", "运营商", "CPO", "硅光", "卫星互联网", "6G", "Wi-Fi 7", "量子通信", "中国移动", "中国电信", "中国联通" } },
        { "传媒",     { "传媒", "游戏", "影视", "广告", "短视频", "元宇宙", "虚拟现实", "VR", "AR", "MR", "Apple Vision", "腾讯游戏", "米哈游", "网易游戏", "版号", "IP" } },
        { "互联网",   { "互联网", "电商", "在线", "平台经济", "阿里", "腾讯", "字节", "京东", "拼多多", "美团", "网约车", "外卖", "直播" } },
        // ===== 新能源 + 电力 =====
        { "新能源",   { "新能源", "锂电", "电池", "宁德时代", "比亚迪", "充电桩", "储能", "氢能", "新能车", "电动车", "固态电池", "钠离子", "4680", "换电", "超充", "800V" } },
        { "光伏",     { "光伏", "太阳能", "多晶硅", "硅片", "逆变器", "隆基", "通威", "TOPCon", "HJT", "BC电池", "钙钛矿", "光伏组件" } },
        { "风电",     { "风电", "风力发电", "海上风电", "风机", "叶片", "明阳", "金风" } },
        { "电力",     { "电力", "电网", "特高压", "核电", "水电", "火电", "输配电", "虚拟电厂", "电力交易", "绿电", "电力市场化" } },
        { "电气设备", { "电气设备", "变压器", "开关柜", "配电", "电力设备", "智能电网" } },
        // ===== 医药医疗 =====
        { "医药",     { "医药", "创新药", "CXO", "临床", "FDA", "药审", "集采", "仿制药", "中药", "生物医药", "基因治疗", "ADC", "GLP-1", "减肥药", "疫苗", "抗体", "恒瑞", "百济" } },
        { "医疗",     { "医疗器械", "耗材", "体外诊断", "IVD", "影像", "手术机器人", "医美", "口腔", "眼科", "爱尔眼科", "迈瑞" } },
        // ===== 消费 =====
        { "消费",     { "消费", "零售", "内需", "促消费", "双十一", "电商", "直播带货", "免税", "国潮", "消费升级", "消费降级", "社零" } },
        { "白酒",     { "白酒", "茅台", "五粮液", "泸州老窖", "酿酒", "高端白酒", "洋河", "汾酒", "老窖" } },
        { "酿酒",     { "酿酒", "啤酒", "葡萄酒", "青岛啤酒", "华润啤酒" } },
        { "家电",     { "家电", "空调", "冰箱", "洗衣机", "美的", "格力", "海尔", "出口家电" } },
        { "电器",     { "电器", "小家电", "厨电", "扫地机器人", "智能家居" } },
        { "食品",     { "食品", "饮料", "乳业", "预制菜", "调味品", "零食", "伊利", "蒙牛", "海天" } },
        { "农业",     { "农业", "种业", "化肥", "农药", "养殖", "猪肉", "粮食", "生猪", "猪价", "猪周期", "转基因", "牧原" } },
        { "纺织",     { "纺织", "服装", "棉花", "棉价", "化纤", "涤纶", "氨纶", "品牌服饰" } },
        { "旅游",     { "旅游", "酒店", "景区", "出行", "航班", "民航", "携程", "文旅", "入境游", "免签" } },
        // ===== 汽车交通 =====
        { "汽车",     { "汽车", "整车", "智能驾驶", "无人驾驶", "自动驾驶", "车联网", "零部件", "特斯拉", "蔚来", "小鹏", "理想", "华为汽车", "智驾", "L3", "L4", "激光雷达", "线控底盘" } },
        { "交通",     { "交通", "航运", "海运", "集运", "公路", "铁路", "高铁", "航空物流", "机场" } },
        { "物流",     { "物流", "快递", "顺丰", "仓储", "供应链", "冷链", "跨境物流" } },
        // ===== 金融 =====
        { "银行",     { "银行", "LPR", "降息", "降准", "信贷", "存款利率", "MLF", "逆回购", "央行", "货币政策", "社融", "M2", "不良率", "净息差", "工商银行", "招商银行" } },
        { "证券",     { "证券", "券商", "股市", "A股", "成交量", "IPO", "注册制", "北向资金", "融资融券", "印花税", "两融", "牛市", "ETF", "基金发行", "中信证券" } },
        { "保险",     { "保险", "寿险", "财险", "新保单", "偿付能力", "中国平安", "中国人寿", "新华保险" } },
        { "信托",     { "信托", "资管", "理财", "私募", "公募", "基金管理" } },
        // ===== 地产建筑 =====
        { "房地产",   { "房地产", "地产", "楼市", "房价", "限购", "房贷利率", "公积金", "保交楼", "万科", "碧桂园", "二手房", "新房", "土拍", "认房不认贷" } },
        { "建筑",     { "建筑", "基建", "基础设施", "PPP", "专项债", "城投", "水利", "一带一路", "中国建筑", "中国中铁" } },
        { "建材",     { "建材", "水泥", "海螺", "防水材料", "涂料", "管材" } },
        { "玻璃",     { "玻璃", "浮法玻璃", "光伏玻璃", "药用玻璃" } },
        // ===== 军工 =====
        { "军工",     { "军工", "国防", "导弹", "战斗机", "军费", "航空", "航天", "卫星发射", "无人机", "军贸", "中航", "航发" } },
        { "飞机制造", { "飞机制造", "C919", "商飞", "航空发动机", "波音", "空客", "ARJ21", "宽体客机" } },
        { "船舶",     { "船舶", "造船", "LNG船", "集装箱", "中国船舶", "航母" } },
        // ===== 周期资源 =====
        { "钢铁",     { "钢铁", "铁矿石", "螺纹钢", "钢材", "产能利用率", "宝钢", "废钢" } },
        { "有色",     { "有色", "铜", "铝", "锌", "稀土", "锂", "钴", "镍", "贵金属", "黄金", "白银", "铂金", "钨", "锡", "紫金矿业" } },
        { "煤炭",     { "煤炭", "动力煤", "焦煤", "焦炭", "煤价", "中国神华", "坑口价" } },
        { "石油",     { "石油", "原油", "油价", "OPEC", "天然气", "油气", "中石油", "中海油", "页岩油", "成品油", "炼化" } },
        { "化工",     { "化工", "MDI", "PVC", "纯碱", "钛白粉", "磷化工", "氟化工", "有机硅", "万华" } },
        // ===== 环保/公用事业 =====
        { "环保",     { "环保", "碳中和", "碳交易", "碳达峰", "绿色", "节能", "污染", "垃圾处理", "污水处理", "固废" } },
        { "燃气",     { "燃气", "天然气分销", "城市燃气", "LNG", "管道气" } },
        { "水务",     { "水务", "自来水", "污水", "水处理", "海水淡化" } },
        // ===== 机械制造 =====
        { "机械",     { "机械", "工程机械", "三一重工", "徐工", "数控机床", "工业母机", "减速器", "轴承" } },
        { "机器人",   { "机器人", "人形机器人", "工业机器人", "协作机器人", "特斯拉Optimus", "优必选", "RWA" } },
        { "仪器仪表", { "仪器仪表", "传感器", "检测", "计量", "示波器" } },
        // ===== 宏观政策关联（跨板块影响）=====
        { "银行",     { "降准", "降息", "加息", "货币宽松", "紧缩", "流动性" } },
        { "房地产",   { "房地产政策", "住建部", "限贷", "限售", "棚改" } },
        { "证券",     { "资本市场改革", "全面注册制", "T+0", "转融通", "减持新规" } },
        { "新能源",   { "双碳", "碳排放", "新能源补贴", "绿色金融" } },
    };
}

QStringList inferIndustries(const QString &title, const QString &summary,
    const QStringList &knownSectors, const QList<SectorKeywordMap> &influenceMap)
{
    const QString text = title + " " + summary;
    QSet<QString> matched;
    QSet<QString> knownSet(knownSectors.begin(), knownSectors.end());

    for (const SectorKeywordMap &mapping : influenceMap) {
        if (!knownSet.contains(mapping.sector)) continue;
        for (const QString &kw : mapping.keywords) {
            if (text.contains(kw)) {
                matched.insert(mapping.sector);
                break;
            }
        }
    }

    for (const QString &sector : knownSectors) {
        if (text.contains(sector)) {
            matched.insert(sector);
        }
    }

    return matched.values();
}

double inferQualityFromAge(const QDateTime &publishedAt)
{
    const qint64 hours = publishedAt.secsTo(QDateTime::currentDateTime()) / 3600;
    if (hours <= 6) return 0.95;
    if (hours <= 24) return 0.9;
    if (hours <= 72) return 0.75;
    return 0.6;
}

double sourceQualityWeight(const QString &source)
{
    if (source.startsWith("PrimaryReuters")) return 0.98;
    if (source.startsWith("PrimaryBloomberg")) return 0.97;
    if (source.startsWith("PrimaryAP")) return 0.97;
    if (source.startsWith("Official") || source.startsWith("GovPolicy") || source.startsWith("NDRC")) return 0.98;
    if (source == "CLS") return 0.96;
    if (source.startsWith("Eastmoney")) return 0.95;
    if (source == "STCN" || source == "CnStock") return 0.94;
    if (source.startsWith("Wallstreetcn")) return 0.93;
    if (source == "THS") return 0.90;
    if (source.startsWith("Sina")) return 0.90;
    if (source.startsWith("NetEase")) return 0.88;
    return 0.85;
}

QString cleanText(const QString &text)
{
    QString t = text;
    t.remove(QRegularExpression("<[^>]*>"));
    t.replace("&nbsp;", " ");
    t.replace("&amp;", "&");
    t.replace("&lt;", "<");
    t.replace("&gt;", ">");
    t.replace("&quot;", "\"");
    return t.simplified();
}

void parseRssOrAtom(const QString &xml, const QString &sourceTag,
    std::function<void(const QString &, const QString &, const QString &, const QString &, const QDateTime &)> process)
{
    QXmlStreamReader xr(xml);
    QString title, summary, link, published;
    bool inItem = false, inEntry = false;

    auto flushItem = [&]() {
        if (title.isEmpty()) return;
        QDateTime ts = QDateTime::fromString(published, Qt::RFC2822Date);
        if (!ts.isValid()) ts = QDateTime::fromString(published, Qt::ISODate);
        if (!ts.isValid()) ts = QDateTime::currentDateTime();
        process(sourceTag, link, cleanText(title), cleanText(summary), ts);
    };

    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement()) {
            const QString n = xr.name().toString().toLower();
            if (n == "item") { inItem = true; inEntry = false; title.clear(); summary.clear(); link.clear(); published.clear(); }
            else if (n == "entry") { inItem = false; inEntry = true; title.clear(); summary.clear(); link.clear(); published.clear(); }
            else if (inItem || inEntry) {
                if (n == "title") title = xr.readElementText().trimmed();
                else if (n == "description" || n == "summary" || n == "content") summary = xr.readElementText().trimmed();
                else if (n == "link") { if (xr.attributes().hasAttribute("href")) link = xr.attributes().value("href").toString().trimmed(); else link = xr.readElementText().trimmed(); }
                else if (n == "pubdate" || n == "published" || n == "updated") published = xr.readElementText().trimmed();
            }
        } else if (xr.isEndElement()) {
            const QString n = xr.name().toString().toLower();
            if ((inItem && n == "item") || (inEntry && n == "entry")) { flushItem(); inItem = false; inEntry = false; }
        }
    }
}

// 各新闻源的抓取函数，每个返回 QList<RawHeadline>
struct RawNewsItem { QString source; QString url; QString title; QString summary; QDateTime ts; };

QList<RawNewsItem> fetchGoogleNewsPrimary()
{
    QList<RawNewsItem> items;
    struct QueryDef { QString tag; QString query; };
    const QList<QueryDef> queries = {
        { "PrimaryReuters", "site:reuters.com finance OR markets OR economy when:1d" },
        { "PrimaryBloomberg", "site:bloomberg.com markets OR economy OR inflation when:1d" },
        { "PrimaryAP", "site:apnews.com business OR markets OR economy when:1d" },
    };
    for (const QueryDef &q : queries) {
        const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(q.query));
        const QString url = "https://news.google.com/rss/search?q=" + encoded + "&hl=en-US&gl=US&ceid=US:en";
        const auto r = HttpClient::get(url, 10000, 0);
        if (!r.ok || r.body.isEmpty()) continue;
        parseRssOrAtom(r.body, q.tag, [&](const QString &src, const QString &u, const QString &t, const QString &s, const QDateTime &dt) {
            items.push_back({ src, u, t, s, dt });
        });
    }
    return items;
}

QList<RawNewsItem> fetchOfficialPrimary()
{
    QList<RawNewsItem> items;
    struct FeedDef { QString tag; QString url; };
    const QList<FeedDef> feeds = {
        { "OfficialUSDefense", "https://www.defense.gov/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=945&max=80" },
        { "OfficialUSState", "https://www.state.gov/briefings-statements/feed/" },
    };
    for (const FeedDef &feed : feeds) {
        const auto r = HttpClient::get(feed.url, 10000, 0);
        if (!r.ok || r.body.isEmpty()) continue;
        parseRssOrAtom(r.body, feed.tag, [&](const QString &src, const QString &u, const QString &t, const QString &s, const QDateTime &dt) {
            items.push_back({ src, u, t, s, dt });
        });
    }
    return items;
}

QList<RawNewsItem> fetchEastmoney()
{
    QList<RawNewsItem> items;
    struct Feed { QString tag; int column; int pages; };
    const QList<Feed> feeds = {
        { "EastmoneyFocus",   101, 2 },
        { "Eastmoney724",     102, 2 },
        { "EastmoneyCompany", 103, 1 },
        { "EastmoneyStock",   105, 2 },
        { "EastmoneyFund",    109, 1 },
        { "EastmoneyGlobal",  106, 1 },
        { "EastmoneyPolicy",  104, 1 },
    };
    for (const Feed &feed : feeds) {
        for (int page = 1; page <= feed.pages; ++page) {
            const QString url = QString(
                "https://np-listapi.eastmoney.com/comm/web/getFastNewsList?"
                "client=web&biz=web_724&fastColumn=%1&sortEnd=&pageSize=50&pageIndex=%2&req_trace=investinsight")
                .arg(feed.column).arg(page);
            const auto r = HttpClient::get(url, 8000, 0);
            if (!r.ok || r.body.isEmpty()) continue;
            const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonArray list = doc.object().value("data").toObject().value("fastNewsList").toArray();
            for (const QJsonValue &v : list) {
                const QJsonObject obj = v.toObject();
                const QString title = obj.value("title").toString().trimmed();
                const QString summary = obj.value("summary").toString().simplified();
                const QString showTime = obj.value("showTime").toString().trimmed();
                QDateTime ts = QDateTime::fromString(showTime, "yyyy-MM-dd hh:mm:ss");
                if (!ts.isValid()) ts = QDateTime::currentDateTime();
                const QString code = obj.value("code").toString();
                items.push_back({ feed.tag, "https://finance.eastmoney.com/a/" + code + ".html", title, summary, ts });
            }
        }
    }
    return items;
}

QList<RawNewsItem> fetchSina()
{
    QList<RawNewsItem> items;
    struct Feed { QString tag; int lid; int pages; };
    const QList<Feed> feeds = {
        { "SinaFinance",  2516, 2 },
        { "SinaMacro",    2509, 1 },
        { "SinaStock",    2512, 2 },
        { "SinaCompany",  2515, 1 },
        { "SinaIndustry", 2514, 1 },
        { "SinaFund",     2513, 1 },
    };
    for (const Feed &feed : feeds) {
        for (int page = 1; page <= feed.pages; ++page) {
            const QString url = QString(
                "https://feed.mix.sina.com.cn/api/roll/get?pageid=153&lid=%1&k=&num=50&page=%2")
                .arg(feed.lid).arg(page);
            const auto r = HttpClient::get(url, 8000, 0);
            if (!r.ok || r.body.isEmpty()) continue;
            const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonArray dataArr = doc.object().value("result").toObject().value("data").toArray();
            for (const QJsonValue &v : dataArr) {
                const QJsonObject obj = v.toObject();
                const QString title = obj.value("title").toString().trimmed();
                const QString newsUrl = obj.value("url").toString().trimmed();
                const QString intro = obj.value("intro").toString().simplified();
                const qint64 ctime = obj.value("ctime").toString().toLongLong();
                QDateTime ts = QDateTime::fromSecsSinceEpoch(ctime);
                if (!ts.isValid()) ts = QDateTime::currentDateTime();
                items.push_back({ feed.tag, newsUrl, title, intro, ts });
            }
        }
    }
    return items;
}

QList<RawNewsItem> fetchTHS()
{
    QList<RawNewsItem> items;
    for (int page = 1; page <= 3; ++page) {
        const QString url = QString(
            "https://news.10jqka.com.cn/tapp/news/push/stock/"
            "?page=%1&tag=&track=website&pagesize=50").arg(page);
        const auto r = HttpClient::get(url, 8000, 0);
        if (!r.ok || r.body.isEmpty()) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
        if (!doc.isObject()) continue;
        const QJsonArray list = doc.object().value("data").toObject().value("list").toArray();
        for (const QJsonValue &v : list) {
            const QJsonObject obj = v.toObject();
            const QString title = obj.value("title").toString().trimmed();
            const QString newsUrl = obj.value("url").toString().trimmed();
            const QString digest = obj.value("digest").toString().simplified();
            const QString ctime = obj.value("ctime").toString();
            QDateTime ts = QDateTime::fromString(ctime, "yyyy-MM-dd hh:mm:ss");
            if (!ts.isValid()) {
                const qint64 sec = ctime.toLongLong();
                ts = sec > 0 ? QDateTime::fromSecsSinceEpoch(sec) : QDateTime::currentDateTime();
            }
            items.push_back({ "THS", newsUrl, title, digest, ts });
        }
    }
    return items;
}

QList<RawNewsItem> fetchWallstreetcn()
{
    QList<RawNewsItem> items;
    const QStringList channels = { "global-channel", "us-stock-channel" };
    for (const QString &ch : channels) {
        QString cursor;
        for (int page = 0; page < 2; ++page) {
            QString url = QString(
                "https://api-one-wscn.awtmt.com/apiv1/content/lives?"
                "channel=%1&client=pc&limit=50").arg(ch);
            if (page == 0) url += "&first_page=true";
            else if (!cursor.isEmpty()) url += "&cursor=" + cursor;
            else break;
            const auto r = HttpClient::get(url, 10000, 0);
            if (!r.ok || r.body.isEmpty()) break;
            const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
            if (!doc.isObject()) break;
            const QJsonObject dataObj = doc.object().value("data").toObject();
            const QJsonArray arr = dataObj.value("items").toArray();
            cursor = QString::number(dataObj.value("next_cursor").toVariant().toLongLong());
            if (arr.isEmpty()) break;
            const QString srcTag = "Wallstreetcn_" + ch;
            for (const QJsonValue &v : arr) {
                const QJsonObject obj = v.toObject();
                const QString title = obj.value("title").toString().trimmed();
                const QString content = obj.value("content_text").toString().simplified();
                const qint64 ts = obj.value("display_time").toVariant().toLongLong();
                QDateTime dt = ts > 0 ? QDateTime::fromSecsSinceEpoch(ts) : QDateTime::currentDateTime();
                const QString uri = obj.value("uri").toString();
                const QString headline = title.isEmpty() ? content : title;
                const QString detail = title.isEmpty() ? content : (content.isEmpty() ? title : content);
                items.push_back({ srcTag, uri.isEmpty() ? "https://wallstreetcn.com/live/global" : uri, headline, detail, dt });
            }
        }
    }
    return items;
}

QList<RawNewsItem> fetchNetEase()
{
    QList<RawNewsItem> items;
    const QStringList paths = {
        "00259BVP/news_flow_index.js",
        "00259BVP/news_flow_stock.js",
        "00259BVP/news_flow_biz.js",
    };
    for (const QString &path : paths) {
        const QString url = "https://money.163.com/special/" + path + "?callback=data_callback";
        const auto r = HttpClient::get(url, 8000, 0);
        if (!r.ok || r.body.isEmpty()) continue;
        QString body = r.body;
        static QRegularExpression re(R"(data_callback\(([\s\S]*)\)\s*$)");
        const auto match = re.match(body);
        if (!match.hasMatch()) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(match.captured(1).toUtf8());
        if (!doc.isArray()) continue;
        const QString srcTag = "NetEase_" + path.section('/', -1).section('.', 0, 0);
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject obj = v.toObject();
            const QString title = obj.value("title").toString().trimmed();
            const QString newsUrl = obj.value("docurl").toString().trimmed();
            const QString digest = obj.value("digest").toString().simplified();
            items.push_back({ srcTag, newsUrl, title, digest, QDateTime::currentDateTime() });
        }
    }
    return items;
}

// ===== 财联社（cls.cn）实时快讯 =====
QList<RawNewsItem> fetchCLS()
{
    QList<RawNewsItem> items;
    // 财联社电报 API
    for (int page = 1; page <= 2; ++page) {
        const QString url = QString(
            "https://www.cls.cn/nodeapi/updateTelegraph?"
            "app=CailianpressWeb&os=web&sv=8.4.6"
            "&page=%1&rn=50&category=&lastTime=").arg(page);
        const auto r = HttpClient::get(url, 8000, 0);
        if (!r.ok || r.body.isEmpty()) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
        if (!doc.isObject()) continue;
        const QJsonArray rolls = doc.object().value("data").toObject().value("roll_data").toArray();
        for (const QJsonValue &v : rolls) {
            const QJsonObject obj = v.toObject();
            QString title = obj.value("title").toString().trimmed();
            const QString content = obj.value("content").toString().simplified();
            if (title.isEmpty()) title = content.left(80);
            const qint64 ctime = obj.value("ctime").toVariant().toLongLong();
            QDateTime ts = ctime > 0 ? QDateTime::fromSecsSinceEpoch(ctime) : QDateTime::currentDateTime();
            const qint64 id = obj.value("id").toVariant().toLongLong();
            items.push_back({ "CLS", "https://www.cls.cn/detail/" + QString::number(id), title, content, ts });
        }
    }
    return items;
}

// ===== 证券时报（stcn.com）=====
QList<RawNewsItem> fetchSTCN()
{
    QList<RawNewsItem> items;
    const QStringList feeds = {
        "https://www.stcn.com/article/list/kx.html",
        "https://www.stcn.com/article/list/gp.html",
    };
    static QRegularExpression titleRe("<a[^>]*href=\"(/article/detail/\\d+\\.html)\"[^>]*>([^<]+)</a>");
    for (const QString &feedUrl : feeds) {
        const auto r = HttpClient::get(feedUrl, 8000, 0);
        if (!r.ok || r.body.isEmpty()) continue;
        QRegularExpressionMatchIterator mit = titleRe.globalMatch(r.body);
        int count = 0;
        while (mit.hasNext() && count < 40) {
            const auto m = mit.next();
            const QString link = "https://www.stcn.com" + m.captured(1);
            const QString title = cleanText(m.captured(2));
            if (title.size() < 6) continue;
            items.push_back({ "STCN", link, title, "", QDateTime::currentDateTime() });
            ++count;
        }
    }
    return items;
}

// ===== 中国证券网/上海证券报（cnstock.com）=====
QList<RawNewsItem> fetchCnStock()
{
    QList<RawNewsItem> items;
    const QStringList feeds = {
        "https://news.cnstock.com/news/sns_bwkx/index.html",
        "https://news.cnstock.com/news/sns_yw/index.html",
    };
    static QRegularExpression titleRe("<a[^>]*href=\"(https?://news\\.cnstock\\.com/[^\"]+)\"[^>]*title=\"([^\"]+)\"");
    for (const QString &feedUrl : feeds) {
        const auto r = HttpClient::get(feedUrl, 8000, 0);
        if (!r.ok || r.body.isEmpty()) continue;
        QRegularExpressionMatchIterator mit = titleRe.globalMatch(r.body);
        int count = 0;
        while (mit.hasNext() && count < 40) {
            const auto m = mit.next();
            const QString title = cleanText(m.captured(2));
            if (title.size() < 6) continue;
            items.push_back({ "CnStock", m.captured(1), title, "", QDateTime::currentDateTime() });
            ++count;
        }
    }
    return items;
}

// ===== 政策类新闻（中国政府网 + 国务院政策文件）=====
QList<RawNewsItem> fetchPolicyNews()
{
    QList<RawNewsItem> items;
    // 中国政府网最新政策
    const QString govUrl = "https://sousuo.www.gov.cn/search-gov/data?"
        "t=zhengcelibrary_gw&q=&timetype=timeqb&mintime=&maxtime="
        "&sort=pubtime&sortType=1&searchfield=title&pcodeJig498=&childtype="
        "&subchildtype=&tsbq=&pubtimeyear=&puborg=&pcodeYear=&pcodeNum="
        "&pageSize=20&pageNum=0";
    const auto r = HttpClient::get(govUrl, 8000, 0);
    if (r.ok && !r.body.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
        const QJsonArray list = doc.object().value("searchVO").toObject().value("listVO").toArray();
        for (const QJsonValue &v : list) {
            const QJsonObject obj = v.toObject();
            const QString title = obj.value("title").toString().trimmed();
            const QString link = obj.value("url").toString().trimmed();
            const QString pubTime = obj.value("pubtimeStr").toString();
            QDateTime ts = QDateTime::fromString(pubTime, "yyyy-MM-dd");
            if (!ts.isValid()) ts = QDateTime::currentDateTime();
            if (!title.isEmpty())
                items.push_back({ "GovPolicy", link, title, "", ts });
        }
    }

    // 国家发改委新闻
    const QString ndrcUrl = "https://www.ndrc.gov.cn/xxgk/jd/jd/index.html";
    const auto nr = HttpClient::get(ndrcUrl, 8000, 0);
    if (nr.ok && !nr.body.isEmpty()) {
        static QRegularExpression re("<a[^>]*href=\"(\\./[^\"]+)\"[^>]*>([^<]+)</a>");
        QRegularExpressionMatchIterator mit = re.globalMatch(nr.body);
        int count = 0;
        while (mit.hasNext() && count < 20) {
            const auto m = mit.next();
            const QString title = cleanText(m.captured(2));
            if (title.size() < 8) continue;
            items.push_back({ "NDRC", "https://www.ndrc.gov.cn/xxgk/jd/jd/" + m.captured(1).mid(2), title, "", QDateTime::currentDateTime() });
            ++count;
        }
    }

    return items;
}

QList<RawNewsItem> fetchSinaHotAndMore()
{
    QList<RawNewsItem> items;

    // 新浪热门
    {
        const QString url = "https://top.finance.sina.com.cn/ws/GetTopDataList.php?"
            "top_type=day&top_cat=finance_0_suda&top_time=&top_show_num=50&top_order=DESC&js_var=all_1_data";
        const auto r = HttpClient::get(url, 8000, 0);
        if (r.ok && !r.body.isEmpty()) {
            static QRegularExpression re(R"(var\s+\w+\s*=\s*(\[[\s\S]*\])\s*;)");
            const auto match = re.match(r.body);
            if (match.hasMatch()) {
                const QJsonDocument doc = QJsonDocument::fromJson(match.captured(1).toUtf8());
                if (doc.isArray()) {
                    for (const QJsonValue &v : doc.array()) {
                        const QJsonObject obj = v.toObject();
                        items.push_back({ "SinaHot", obj.value("url").toString().trimmed(),
                            obj.value("title").toString().trimmed(), "", QDateTime::currentDateTime() });
                    }
                }
            }
        }
    }

    // 新浪财经热门话题
    {
        const QString url = "https://top.finance.sina.com.cn/ws/GetTopDataList.php?"
            "top_type=day&top_cat=finance_0_suda&top_time=&top_show_num=50&top_order=DESC&js_var=all_1_data01";
        const auto r = HttpClient::get(url, 8000, 0);
        if (r.ok && !r.body.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
            if (doc.isObject()) {
                const QJsonArray list = doc.object().value("data").toArray();
                for (const QJsonValue &v : list) {
                    const QJsonObject obj = v.toObject();
                    items.push_back({ "SinaHot", "",
                        obj.value("title").toString().trimmed(),
                        obj.value("summary").toString().simplified(), QDateTime::currentDateTime() });
                }
            }
        }
    }

    return items;
}

} // namespace

void RealFinanceNewsProvider::setSectorNames(const QStringList &sectorNames)
{
    m_sectorNames = sectorNames;
}

QList<RawInsight> RealFinanceNewsProvider::fetchInsights()
{
    QList<RawInsight> insights;
    QSet<QString> deduplicateKeys;
    QSet<QString> headlineDedup;
    m_rawHeadlines.clear();
    const QList<SectorKeywordMap> influenceMap = buildInfluenceMap();

    // 并行拉取所有新闻源（12路）
    QFuture<QList<RawNewsItem>> fEastmoney   = QtConcurrent::run(fetchEastmoney);
    QFuture<QList<RawNewsItem>> fSina        = QtConcurrent::run(fetchSina);
    QFuture<QList<RawNewsItem>> fTHS         = QtConcurrent::run(fetchTHS);
    QFuture<QList<RawNewsItem>> fWallstreetcn = QtConcurrent::run(fetchWallstreetcn);
    QFuture<QList<RawNewsItem>> fNetEase     = QtConcurrent::run(fetchNetEase);
    QFuture<QList<RawNewsItem>> fGoogle      = QtConcurrent::run(fetchGoogleNewsPrimary);
    QFuture<QList<RawNewsItem>> fOfficial    = QtConcurrent::run(fetchOfficialPrimary);
    QFuture<QList<RawNewsItem>> fHotMore     = QtConcurrent::run(fetchSinaHotAndMore);
    QFuture<QList<RawNewsItem>> fCLS         = QtConcurrent::run(fetchCLS);
    QFuture<QList<RawNewsItem>> fSTCN        = QtConcurrent::run(fetchSTCN);
    QFuture<QList<RawNewsItem>> fCnStock     = QtConcurrent::run(fetchCnStock);
    QFuture<QList<RawNewsItem>> fPolicy      = QtConcurrent::run(fetchPolicyNews);

    struct NamedResult { QString name; QFuture<QList<RawNewsItem>> *future; };
    QList<NamedResult> allFutures = {
        { "Eastmoney",    &fEastmoney },
        { "Sina",         &fSina },
        { "THS",          &fTHS },
        { "Wallstreetcn", &fWallstreetcn },
        { "NetEase",      &fNetEase },
        { "GoogleNews",   &fGoogle },
        { "Official",     &fOfficial },
        { "HotMore",      &fHotMore },
        { "CLS",          &fCLS },
        { "STCN",         &fSTCN },
        { "CnStock",      &fCnStock },
        { "Policy",       &fPolicy },
    };

    auto process = [&](const RawNewsItem &item) {
        if (item.title.isEmpty()) return;
        if (!headlineDedup.contains(item.title)) {
            headlineDedup.insert(item.title);
            m_rawHeadlines.push_back({ item.source, item.title, item.summary, item.ts });
        }
        const QStringList industries = inferIndustries(item.title, item.summary, m_sectorNames, influenceMap);
        const double quality = inferQualityFromAge(item.ts) * sourceQualityWeight(item.source);
        for (const QString &industry : industries) {
            const QString dedupKey = industry + "|" + item.title + "|" + item.source + "|" + item.url;
            if (deduplicateKeys.contains(dedupKey)) continue;
            deduplicateKeys.insert(dedupKey);
            insights.push_back({ item.source, item.url, industry, item.title, item.summary, item.ts, quality });
        }
    };

    for (auto &nr : allFutures) {
        nr.future->waitForFinished();
        const QList<RawNewsItem> items = nr.future->result();
        for (const RawNewsItem &item : items)
            process(item);
        qDebug() << "[NewsProvider] After" << nr.name << ":" << m_rawHeadlines.size() << "raw," << insights.size() << "matched";
    }

    qDebug() << "[NewsProvider] ===== FINAL:" << m_rawHeadlines.size() << "raw headlines,"
             << insights.size() << "sector-matched insights =====";
    return insights;
}

QString RealFinanceNewsProvider::providerName() const
{
    return "MultiSourceNews";
}
