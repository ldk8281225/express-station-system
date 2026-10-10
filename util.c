/*
 * 模块：util.c —— 通用小工具
 * 职责：放一些"谁都可能用到、但不属于任何业务"的小函数
 * 依赖：hash.c（genCode 要查重）
 * 被依赖：table.c / log.c / ui.c / inbound.c / demo.c
 *
 * 前三个函数都很短，单独成文件是为了避免在各个业务模块里重复写；
 * v2.0 加的"分类映射"和"今日计数"同理 —— 它们被好几个模块调用，
 * 放在这里最省事，也最不容易改漏。
 */

#include "station.h"

/* 时间转字符串。fmt 传 "%Y-%m-%d %H:%M" 或 "%Y-%m-%d %H:%M:%S" */
void timeStr(time_t t, const char *fmt, char *buf, int size)
{
    strftime(buf, size, fmt, localtime(&t));
}

/* 手机号脱敏：138****9019（只显示前 3 位和尾 4 位） */
void maskPhone(const Package *p, char *out)
{
    sprintf(out, "%.3s****%s", p->phone, p->tail);
}

/*
 * 拼一个取件码：货架号-层数-日期+4位随机数，形如 A-05-122222
 * 各段含义见 station.h 里 genCode 上方的说明。
 * 总长固定 11 位：1 + 1 + 2 + 1 + 2 + 4。
 */
void formatCode(char *out, char row, int layer, int day, int rnd)
{
    sprintf(out, "%c-%02d-%02d%04d", row, layer, day, rnd % 10000);
}

/*
 * 生成一个不重复的取件码。
 *
 * 和旧版"8 位纯随机数"的区别：货架号和入库日期是**确定**的，
 * 所以取件码本身就告诉用户"去哪个货架、第几层、哪天入的库"，
 * 只有末 4 位随机数需要重摇来避免撞码。
 *
 * 调用顺序有讲究：必须先 allocShelf 拿到货架，再调本函数，
 * 否则码里的货架号和包裹实际位置对不上。
 */
void genCode(const char *shelf, time_t t, char *out)
{
    int  day   = localtime(&t)->tm_mday;   /* 入库日期：几号 */
    char row   = shelf[0];                 /* 货架号：A~F */
    int  layer = atoi(shelf + 2);          /* 层数："A-05" 里的 05 */

    do {
        formatCode(out, row, layer, day, rand() % 10000);
    } while (hashFind(out, NULL));     /* 撞码了就重摇随机数 */
}

/* 就地转成大写（只动 a~z，其它字符原样）：
 * 取件码里带了字母，用户手输小写也能查到 */
void strUpper(char *s)
{
    for (; *s; s++) {
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 'a' + 'A');
    }
}

/* ==================== 分类分区 ==================== */

/* 五个区类的名字，下标就是类别号，与下面的排区间一一对应 */
static const char *CAT_NAMES[CAT_COUNT] = { "普通", "易碎", "电子", "液体", "生鲜" };

/*
 * 类别 -> 占用的排区间。这就是"分区方案"本身：
 *   A、B 排给普通件（量最大，占两排）；C 易碎；D 电子；E 液体；F 生鲜。
 * 以后想调整分区比例，只改这两行数组，其余代码全部自动跟着变 ——
 * 排区间是唯一的事实来源。
 */
static const int CAT_R0[CAT_COUNT] = { 0, 2, 3, 4, 5 };   /* 起始排 */
static const int CAT_R1[CAT_COUNT] = { 1, 2, 3, 4, 5 };   /* 结束排 */

const char *catName(int idx)
{
    if (idx < 0 || idx >= CAT_COUNT) return CAT_NAMES[0];
    return CAT_NAMES[idx];
}

int catIndex(const char *name)
{
    int i;

    for (i = 0; i < CAT_COUNT; i++) {
        if (strcmp(name, CAT_NAMES[i]) == 0) return i;
    }
    return 0;      /* 认不出来就当普通件：宁可放错区，也不能丢件 */
}

int catRowFrom(int cat)
{
    if (cat < 0 || cat >= CAT_COUNT) return CAT_R0[0];
    return CAT_R0[cat];
}

int catRowTo(int cat)
{
    if (cat < 0 || cat >= CAT_COUNT) return CAT_R1[0];
    return CAT_R1[cat];
}

int catOfRow(int row)
{
    int i;

    for (i = 0; i < CAT_COUNT; i++) {
        if (row >= CAT_R0[i] && row <= CAT_R1[i]) return i;
    }
    return 0;
}

/*
 * 由货架号反推类别："C-05" -> "易碎"。
 * 能这么推，是因为分区是按"排"划的，一排只服务一个类别 ——
 * 于是数据文件里不必再存一份类别字段，"排"本身就是类别。
 * 好处是不会出现"类别字段和货架位置对不上"的不一致数据。
 */
const char *catOfShelf(const char *shelf)
{
    return catName(catOfRow(shelf[0] - 'A'));
}

/* ==================== 今日计数 ==================== */

/* 今天日期压成一个整数，如 20261010 —— 拿来比较"是不是同一天"最省事 */
int todayStamp(void)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);

    return (lt->tm_year + 1900) * 10000 + (lt->tm_mon + 1) * 100 + lt->tm_mday;
}

/*
 * 跨天就清零今日出入库计数。在每次写操作（入库 / 出库）之前调一次。
 * 之所以要在库里做而不是靠定时器：课设程序可能跨夜连续开着，
 * 也可能今天关明天开 —— 这两种情况下"今日"都得重新开始算。
 */
void rollDay(void)
{
    int d = todayStamp();

    if (d != gSt.dayStamp) {
        gSt.dayStamp = d;
        gSt.todayIn  = 0;
        gSt.todayOut = 0;
    }
}
