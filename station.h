#ifndef STATION_H
#define STATION_H

/*
 * 快递驿站智能管理系统 —— 公共定义
 *
 * 本文件是唯一一个头文件，所有模块共用的常量、结构体、函数声明都在这里，
 * 按模块分节排列。建议配合每个 .c 文件开头的"模块说明"一起看。
 *
 * 推荐阅读顺序（从底层到业务）：
 *   hash.c -> util.c -> fee.c -> ui.c -> table.c -> store.c -> log.c
 *   -> inbound.c -> outbound.c -> query.c -> goodslist.c -> report.c
 *   -> experiment.c -> demo.c -> main.c
 *
 * ==================== v2.0 新增：双角色模式 ====================
 * 程序启动后先选身份，两种身份看到的菜单完全不同：
 *   取件模式   —— 只有"取件"和"尾号查询"两项，面向取件人
 *   管理员模式 —— 需通过口令，开放入库、后台管理、报表等全部功能
 * 分角色不是为了炫技，而是把"取件人能碰的操作"和"管理员权限"
 * 在界面层就隔开：业务函数本身完全共用，只是出口不同。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==================== 可调参数 ==================== */
#define TABLE_SIZE   1013    /* 哈希桶个数，取素数散列更均匀 */
#define SHELF_ROWS   6       /* 货架 6 排，编号 A~F */
#define SHELF_COLS   20      /* 每排 20 层，编号 01~20，共 120 格 */
#define CODE_LEN     11      /* 取件码长度，形如 A-05-122222 */
#define STALE_HOURS  48      /* 在站超过 48 小时算滞留件 */
#define LOG_SHOW     20      /* 日志只回显最后这么多条 */

/* ---- 身份与权限 ----
 * 管理员口令就写在这一行，想换直接改这里，不用翻代码。
 * 演示前若嫌麻烦，也可以临时把它改成一个空格。*/
#define ADMIN_PASS   "8888"
#define PASS_TRIES   3       /* 口令最多允许输错几次，超了退回身份选择 */

/* ---- 超期保管费（单位：元）----
 * 免费保管 FREE_HOURS 小时（和滞留判定同一个口径）；
 * 超出后前 TIER1_DAYS 天按 FEE_DAY1 元/天，第 3 天起按 FEE_DAY2 元/天，
 * 不足一天按一天计。想改费率，只动这四行。*/
#define FREE_HOURS   48
#define FEE_DAY1     1
#define FEE_DAY2     2
#define TIER1_DAYS   2

/* ---- 货架分区（分类分区存放）----
 * 六排货架按包裹属性切成 5 个区（映射见 hash.c 的 catRows）：
 *     A、B 排 -> 普通件        C 排 -> 易碎品       D 排 -> 电子产品
 *     E 排    -> 液体品        F 排 -> 生鲜冷链
 *
 * 注意"类别"没有单独存进数据文件：因为"排"和"类别"是一一对应的，
 * 拿到货架号就能反推类别（util.c 的 catOfShelf）。少存一列冗余数据，
 * 也就不会出现"类别字段和货架位置对不上"的不一致。
 *
 * 某一类放满时不会导致入库失败：allocShelfByCat 会自动降级到其它空位，
 * 并回传一个标记，由 inbound.c 提示工作人员。*/
#define CAT_COUNT    5

/* ---- 技术细节开关（界面沉浸感）----
 * 0 = 沉浸模式（默认）：面向真实使用者。
 *     不打印哈希比较次数、装填因子这类内部指标，也不暴露数据文件名和默认口令，
 *     界面看起来就是一个正常运行的驿站后台，看不出"课程设计"的痕迹。
 * 1 = 展示模式：把上面这些技术细节全部打印出来。
 *     答辩现场想证明"取件走的确实是哈希查找"时，改成 1 重新编译即可。
 *
 * 一句话：平时给用户看，答辩给老师看，一行宏切换。
 * （用 #ifndef 包住，所以也可以不动源码，编译时加 -DSHOW_TECH_DETAIL=1 覆盖。）*/
#ifndef SHOW_TECH_DETAIL
#define SHOW_TECH_DETAIL  0
#endif

#define DATA_FILE "packages.dat"   /* 数据包在站数据 */
#define LOG_FILE  "station.log"    /* 操作日志 */

/* 控制台颜色 */
#define C_TITLE 11    /* 青：标题 */
#define C_OK    10    /* 绿：成功 */
#define C_WARN  12    /* 红：警告 */
#define C_KEY   14    /* 黄：关键信息 */

/* ==================== 数据结构 ==================== */

/* 包裹：既是数据记录，也是哈希桶链表的节点（拉链法） */
typedef struct Package {
    char code[16];         /* 取件码，形如 "A-05-122222"（见下方 genCode 说明） */
    char phone[16];        /* 手机号 */
    char tail[5];          /* 手机尾 4 位，取件时核验身份 */
    char courier[16];      /* 快递公司 */
    char shelf[8];         /* 货架位，形如 "A-03"，由 allocShelf 分配 */
    time_t inTime;         /* 入库时间，用于判滞留、算超期保管费 */
    struct Package *next;  /* 拉链指针：同桶冲突时串起来 */
} Package;

/* 驿站：一张哈希表 + 一张货架占用表 + 几个计数器 */
typedef struct {
    Package *bucket[TABLE_SIZE];               /* 桶数组 */
    int count;                                 /* 当前在站件数 */
    int totalIn, totalOut;                     /* 累计入库、累计出库 */
    int todayIn, todayOut;                     /* 今日入库、今日出库（跨天自动清零） */
    int dayStamp;                              /* 今日计数对应的日期，形如 20261010 */
    int shelf[SHELF_ROWS][SHELF_COLS];         /* 0=空 1=占用 */
} Station;

/*
 * ★ 全局驿站对象 gSt
 *
 * 全系统只有这一份数据，定义在 main.c，其它模块用 extern 引用。
 * 有了它，所有函数都不再需要传 Station *st 参数：
 *      旧：doPickup(&st);   hashFind(&st, code, &cmp);
 *      新：doPickup();      hashFind(code, &cmp);
 * 想在 Station 里加个新字段（比如 v2.0 的今日计数），只改这个结构体
 * 和相关函数即可，不用再动一路传参的调用点。
 *
 * 唯一的例外是 experiment.c 的性能实验：它需要一张独立的临时表，
 * 做法是先把 gSt 备份到局部变量、把 gSt 清空当实验台，用完再还原。
 */
extern Station gSt;

/* ==================== util.c：通用小工具 ==================== */
void timeStr(time_t t, const char *fmt, char *buf, int size);  /* 时间转字符串 */
void maskPhone(const Package *p, char *out);                   /* 138****9019 */

/*
 * 取件码生成。格式：货架号-层数-日期+4位随机数，形如 A-05-122222
 *   A    货架号（A~F）    —— 用户看码就知道去哪个货架
 *   05   层数（01~20）    —— 到货架后看第几层
 *   12   入库日期（1~31） —— 入库那天是几号
 *   2222 四位随机数       —— 同一天、同一货架靠它区分
 *
 * shelf 必须来自 allocShelf（要和包裹真实放的货架一致），
 * t 传包裹的入库时间：两者定下来后，随机数才保证不重复。
 *
 * 顺带一提：因为货架是分区编码的，取件码里的 A~F 同时也就告诉了
 * 工作人员"这是什么类别的件"——分类信息是免费搭车带上去的。
 */
void formatCode(char *out, char row, int layer, int day, int rnd);
void genCode(const char *shelf, time_t t, char *out);

/* 把字符串就地转成大写：用户手输 "a-05-122222" 也能查到 */
void strUpper(char *s);

/* ---- 分类分区 ---- */
const char *catName(int idx);              /* 0~4 -> "普通"/"易碎"/"电子"/"液体"/"生鲜" */
int         catIndex(const char *name);    /* 反向：类别名 -> 0~4，认不出按普通件(0) */
const char *catOfShelf(const char *shelf); /* 由货架号反推类别："C-05" -> "易碎" */
int         catRowFrom(int cat);           /* 类别 -> 占用排区间的起始排（0 = A 排） */
int         catRowTo(int cat);             /* 类别 -> 占用排区间的结束排 */
int         catOfRow(int row);             /* 排号 -> 类别，是 catRowFrom/To 的反函数 */

/* ---- 今日计数（跨天自动清零） ---- */
int  todayStamp(void);   /* 今天日期的整数形式，如 20261010 */
void rollDay(void);      /* 发现跨天就把今日出入库计数清零；写操作前调一次 */

/* ==================== fee.c：超期保管费 ==================== */
int         overdueDays(time_t inTime, time_t now);          /* 超期天数，未超期返回 0 */
int         storageFee(time_t inTime, time_t now);           /* 应收保管费（元，整数） */
const char *feeStr(time_t inTime, time_t now, char *buf);    /* 格式化成 "3元" / "-" */
int         totalReceivable(void);                           /* 全站在库件的应收费用合计 */

/* ==================== hash.c：哈希表与货架 ==================== */
unsigned int hashIndex(const char *code);        /* 取件码 -> 桶下标 */
void     stationInit(void);                      /* 清空整张表 */
void     stationFree(void);                      /* 释放所有节点 */
Package *hashFind(const char *code, int *cmp);   /* 查找，cmp 记录比较次数 */
int      hashInsert(Package *p);                 /* 插入，取件码重复返回 0 */
Package *hashRemove(const char *code);           /* 摘除节点，交给调用方 free */
int      allocShelf(char *out);                  /* 分配一个空货架位（不分类别） */
int      allocShelfByCat(const char *cat, char *out);
void     freeShelf(const char *shelf);           /* 释放货架位 */
void     markShelf(const char *shelf);           /* 载入数据时回填占用表 */
void     shelfStatByCat(int *used, int *total);  /* 按分区统计占用：used[i]/total[i] */

/* ==================== ui.c：控制台界面 ==================== */
void uiInit(void);
void uiCleanup(void);                    /* 退出前还原控制台代码页 */
void uiClear(void);
void uiColor(int attr);
void uiReset(void);
void uiPause(void);
void uiTitle(const char *text);
void uiRoleMenu(void);                   /* 身份选择（启动第一屏） */
void uiUserMenu(void);                   /* 取件模式菜单 */
void uiAdminMenu(void);                  /* 管理员菜单 */
void uiReceipt(const Package *p, const char *title, time_t when);
void uiPad(const char *s, int width);    /* 按显示宽度左对齐（中文算 2 列） */
void uiCenter(const char *s, int width); /* 在 width 列内容区里居中，两侧 | 封口 */
int  readLine(char *buf, int size);      /* 安全读一行；返回 0 表示输入已结束(EOF) */
int  readPass(char *buf, int size);      /* 读口令，回显 *；返回 0 表示输入已结束 */

/* ==================== table.c：包裹表格 ==================== */
void tableHead(void);                 /* 打印表头 */
void tableRow(const Package *p);      /* 打印一行包裹 */
void tableFoot(void);                 /* 打印表尾横线 */

/* ==================== store.c：数据保存与载入 ==================== */
int loadData(void);   /* 恢复数据：-1=无数据文件，>=0=在站件数 */
int saveData(void);   /* 退出时保存数据 */

/* ==================== log.c：操作日志 ==================== */
void logEvent(const char *action, const char *code, const char *detail);
void doViewLog(void);        /* 菜单：回显最近若干条日志 */

/* ==================== inbound.c：快递员入库 ==================== */
void doCheckIn(void);

/* ==================== outbound.c：用户取件出库 ==================== */
void doPickup(void);

/* ==================== query.c：按手机尾号查询 ==================== */
void doQueryByTail(void);

/* ==================== goodslist.c：货物清单 ==================== */
void doGoodsList(void);      /* 列出全部在库货物，滞留件标红，含保管费 */

/* ==================== report.c：运营报表 ==================== */
void doStatistics(void);

/* ==================== experiment.c：哈希性能实验 ==================== */
void doExperiment(void);

/* ==================== demo.c：演示数据 ==================== */
void seedDemo(int n);
void doSeedDemo(void);       /* 菜单：交互式生成演示数据 */

#endif
