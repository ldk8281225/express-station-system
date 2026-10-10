/*
 * 模块：report.c —— 运营报表（管理员菜单 5）
 * 职责：一次遍历算出所有经营指标：出入库量、在库时长、滞留、货架占用、应收费用
 * 依赖：ui.c / hash.c / fee.c
 *
 * 报表是"读"操作的大集合。这里刻意做成"一次遍历算完所有指标"：
 *   遍历哈希表 -> 非空桶数、最长链长、在库时长、滞留件数
 *   遍历货架数组 -> 已占用格数、分区占用
 * 再算出装填因子 α = 元素个数 / 桶数，这是衡量哈希表好坏的核心指标。
 *
 * ★ v2.0 新增三类指标，让报表从"库存快照"变成"经营看板"：
 *   今日出入库与净增   —— 完成率视角
 *   平均 / 最长在库时长 —— 周转视角
 *   应收保管费         —— 营收视角
 */

#include "station.h"

/* 货架占用率超过 80% 就标红提醒 */
#define WARN_RATIO 0.8

void doStatistics(void)
{
    int i, r, c;
    int used = 0, maxChain = 0, busy = 0;
    int total = SHELF_ROWS * SHELF_COLS;
    int stale = 0;
    int usedByCat[CAT_COUNT], totalByCat[CAT_COUNT];
    double sumHours = 0.0, maxHours = 0.0;
    time_t now = time(NULL);

    uiTitle("运 营 报 表");

    /* ---- 第 1 步：扫描哈希表，一次把冲突情况和在库时长都算出来 ----
     * 反正都要沿链走一遍，就没必要为"在库时长"再遍历第二次。 */
    for (i = 0; i < TABLE_SIZE; i++) {
        Package *p = gSt.bucket[i];
        int len = 0;

        while (p) {                     /* 这个桶的链有多长 */
            double h = difftime(now, p->inTime) / 3600.0;
            len++;
            sumHours += h;
            if (h > maxHours)   maxHours = h;
            if (h > STALE_HOURS) stale++;
            p = p->next;
        }
        if (len > 0) used++;            /* 非空桶计数 */
        if (len > maxChain) maxChain = len;
    }

    /* ---- 第 2 步：扫描货架二维数组，看占用情况 ---- */
    for (r = 0; r < SHELF_ROWS; r++) {
        for (c = 0; c < SHELF_COLS; c++) {
            if (gSt.shelf[r][c]) busy++;
        }
    }
    shelfStatByCat(usedByCat, totalByCat);

    /* ---- 第 3 步：业务量与今日完成情况 ---- */
    printf("[ 业务量 ]\n");
    printf("  在站件数 : %d 件\n", gSt.count);
    printf("  今日入库 : %d 件     今日出库 : %d 件     净增 : %+d 件\n",
           gSt.todayIn, gSt.todayOut, gSt.todayIn - gSt.todayOut);
    printf("  累计入库 : %d 件     累计出库 : %d 件\n", gSt.totalIn, gSt.totalOut);
    if (gSt.totalIn > 0) {
        printf("  累计周转率 : %.1f%%\n",
               gSt.totalOut * 100.0 / gSt.totalIn);
    }

    /* ---- 第 4 步：在库时长与滞留 ---- */
    printf("\n[ 周转情况 ]\n");
    if (gSt.count > 0) {
        printf("  平均在库时长 : %.1f 小时\n", sumHours / gSt.count);
        printf("  最长在库时长 : %.1f 小时\n", maxHours);
    }
    printf("  滞留件数 : ");
    uiColor(stale > 0 ? C_WARN : C_OK);
    printf("%d 件", stale);
    uiReset();
    if (gSt.count > 0) printf("（占在库 %.1f%%）", stale * 100.0 / gSt.count);
    printf("，超过 %d 小时未取即计滞留\n", STALE_HOURS);

    /* ---- 第 5 步：货架总占用 ---- */
    printf("\n[ 货架使用 ]\n");
    printf("  总占用 : %d / %d 格\n", busy, total);
    printf("  占用率 : ");
    uiColor(busy > total * WARN_RATIO ? C_WARN : C_OK);
    {
        int k, bars = busy * 20 / total;
        printf("[");
        for (k = 0; k < 20; k++) putchar(k < bars ? '#' : ' ');
        printf("]");
    }
    uiReset();
    printf(" %.1f%%\n", busy * 100.0 / total);

    /* 分区占用：每类画一条小条，一眼看出哪一区快满了 */
    for (i = 0; i < CAT_COUNT; i++) {
        int k, bars, t = totalByCat[i] > 0 ? totalByCat[i] : 1;

        bars = usedByCat[i] * 12 / t;
        printf("    %s区 ", catName(i));
        uiColor(usedByCat[i] >= t ? C_WARN : C_OK);
        printf("[");
        for (k = 0; k < 12; k++) putchar(k < bars ? '#' : ' ');
        printf("] %d/%d\n", usedByCat[i], totalByCat[i]);
        uiReset();
    }

    /* ---- 第 6 步：应收保管费 ---- */
    printf("\n[ 营收 ]\n");
    printf("  应收保管费 : ");
    uiColor(totalReceivable() > 0 ? C_WARN : C_OK);
    printf("%d 元", totalReceivable());
    uiReset();
    printf("（免费 %d 小时，超期前 %d 天 %d 元/天，之后 %d 元/天）\n",
           FREE_HOURS, TIER1_DAYS, FEE_DAY1, FEE_DAY2);

    /* ---- 第 7 步：哈希表运行状况（仅展示模式下输出）----
     * 运营报表是给店长看的，装填因子、最长链这些是"实现"层面的东西，
     * 摆在报表末尾会立刻把系统打回作业形态，所以默认不输出。*/
#if SHOW_TECH_DETAIL
    printf("\n[ 哈希表 ]\n");
    printf("  桶 %d 个，非空 %d 个，最长链 %d，装填因子 %.4f\n",
           TABLE_SIZE, used, maxChain, (double)gSt.count / TABLE_SIZE);
    printf("  说明：装填因子越接近 1 冲突越多，真实系统会扩容重散列。\n");
#else
    (void)used;        /* 沉浸模式下不输出，显式吞掉，免得 gcc 报"赋值后未使用" */
    (void)maxChain;
#endif
}
