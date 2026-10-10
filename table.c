/*
 * 模块：table.c —— 包裹表格输出
 * 职责：统一"包裹列表"的画法，出库/尾号查询/货物清单三个模块共用
 * 依赖：util.c、ui.c、fee.c
 * 被依赖：outbound.c / query.c / goodslist.c
 *
 * 为什么单独成文件：这几个模块都要打印同样的表格，
 * 各写一份就会出现"有的表歪、有的表不歪"，抽出来最好维护。
 *
 * v2.0 改动：新增"保管费"列（第 7 列），因此把时间列压成 "10-10 13:17"，
 * 让整表宽度控制在 77 列以内 —— 80 列的标准控制台窗口不会折行。
 */

#include "station.h"

/* 各列宽度（按显示宽度算，中文一个字占 2 列）
 * 合计 4+11+11+5+11+7+6 = 55，加列间分隔后整表 77 列。 */
#define W_SHELF  4          /* "A-03" */
#define W_CODE   CODE_LEN   /* 11，刚好放下 A-05-122222 */
#define W_PHONE 11          /* "138****9019" 正好 11 列 */
#define W_FIRM   5
#define W_TIME  11          /* "10-10 13:17"，年份在报表和日志里看 */
#define W_AGE    7          /* "70.6h" */
#define W_FEE    6          /* "12元" */

/* 画一段横线：内容区宽 w，两侧各留 1 空格 -> w + 2 个 '-' */
static void rule(int w)
{
    int i;
    putchar('+');
    for (i = 0; i < w + 2; i++) putchar('-');
}

/* 整条分隔线：宽度由上面各列宽度算出来，
 * 这样以后改列宽时不会忘记同步横线（以前是手写死的，很容易忘） */
static void line(void)
{
    rule(W_SHELF); rule(W_CODE); rule(W_PHONE);
    rule(W_FIRM);  rule(W_TIME); rule(W_AGE);  rule(W_FEE);
    printf("+\n");
}

void tableHead(void)
{
    line();
    printf("| "); uiPad("货架",     W_SHELF);
    printf(" | "); uiPad("取件码",   W_CODE);
    printf(" | "); uiPad("收件人",   W_PHONE);
    printf(" | "); uiPad("快递",     W_FIRM);
    printf(" | "); uiPad("入库时间", W_TIME);
    printf(" | "); uiPad("在库时长", W_AGE);
    printf(" | "); uiPad("保管费",   W_FEE);
    printf(" |\n");
    line();
}

void tableRow(const Package *p)
{
    char ts[32], phone[32], age[16], fee[16];
    time_t now = time(NULL);
    double hours = difftime(now, p->inTime) / 3600;

    timeStr(p->inTime, "%m-%d %H:%M", ts, sizeof(ts));   /* 年月在报表里看 */
    maskPhone(p, phone);
    sprintf(age, "%.1fh", hours);
    feeStr(p->inTime, now, fee);                          /* 未超期给的是 "-" */

    printf("| "); uiPad(p->shelf,   W_SHELF);
    printf(" | "); uiPad(p->code,    W_CODE);
    printf(" | "); uiPad(phone,      W_PHONE);
    printf(" | "); uiPad(p->courier, W_FIRM);
    printf(" | "); uiPad(ts,         W_TIME);
    printf(" | "); uiPad(age,        W_AGE);
    printf(" | "); uiPad(fee,        W_FEE);
    printf(" |\n");
}

void tableFoot(void)
{
    line();
}
