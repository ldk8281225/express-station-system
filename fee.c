/*
 * 模块：fee.c —— 超期保管费（v2.0 新增）
 * 职责：判定超期天数、按阶梯费率算钱、汇总全站应收费用
 * 依赖：无（只用到 Package 结构和 station.h 顶部那几个常量）
 * 被依赖：ui.c（凭条上要提醒）、table.c（表格里的保管费列）、report.c（应收合计）
 *
 * 规则写在 station.h 顶部那几行宏里，改费率不用动这个文件：
 *   免费保管 FREE_HOURS 小时；超出后前 TIER1_DAYS 天 FEE_DAY1 元/天，
 *   第 3 天起 FEE_DAY2 元/天，不足一天按一天计。
 *
 * 为什么单独成一个模块：计费属于"业务规则"，不属于"通用工具"，
 * 而且它以后最容易变（免费时长、阶梯、封顶、会员折扣……）。
 * 单独一个文件，改规则时不会在 ui.c / table.c / report.c 里到处找。
 */

#include "station.h"

/*
 * 超期天数：没超过免费时长返回 0；超过则向上取整（不满一天算一天）。
 *
 * 这里没用 ceil()，是为了不引入数学库（gcc 下要加 -lm）。
 * 整数除法拿到整天数，再看有没有余下的零头小时，有就加一天，效果一样。
 */
int overdueDays(time_t inTime, time_t now)
{
    double h = difftime(now, inTime) / 3600.0;
    int days;

    if (h <= FREE_HOURS) return 0;
    h -= FREE_HOURS;

    days = (int)(h / 24.0);
    if (h - days * 24.0 > 0.0) days++;      /* 零头不满一天，也按一天收 */
    return days > 0 ? days : 1;
}

/* 应收保管费（元）。整数计价，凭条和表格里显示成 "4元" 这种 */
int storageFee(time_t inTime, time_t now)
{
    int d = overdueDays(inTime, now);

    if (d == 0) return 0;
    if (d <= TIER1_DAYS) return d * FEE_DAY1;                 /* 阶梯一 */
    return TIER1_DAYS * FEE_DAY1 + (d - TIER1_DAYS) * FEE_DAY2; /* 阶梯二 */
}

/* 格式化成 "4元"；没超期的返回 "-"，表格里看着干净 */
const char *feeStr(time_t inTime, time_t now, char *buf)
{
    int f = storageFee(inTime, now);

    if (f <= 0) strcpy(buf, "-");
    else        sprintf(buf, "%d元", f);
    return buf;
}

/*
 * 全站在库件的应收保管费合计 —— 运营报表用。
 * 又是一次"遍历所有桶 + 沿链走"，和 report.c 统计、store.c 保存同一个套路：
 * 哈希表按取件码散列，想按别的维度汇总就只能全表扫描。
 */
int totalReceivable(void)
{
    time_t now = time(NULL);
    int i, sum = 0;

    for (i = 0; i < TABLE_SIZE; i++) {
        Package *p;
        for (p = gSt.bucket[i]; p; p = p->next) {
            sum += storageFee(p->inTime, now);
        }
    }
    return sum;
}
