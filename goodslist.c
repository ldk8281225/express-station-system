/*
 * 模块：goodslist.c —— 货物清单（管理员菜单 4）
 * 职责：列出全部在库货物，按入库时间从早到晚排序；滞留件标红，含应收保管费
 * 依赖：ui.c / table.c / fee.c / hash.c
 *
 * 和历史版本的差别：
 *   以前是"滞留件清单"，只列超过 48 小时的；
 *   现在是"货物清单"，全部在库件都列，滞留件靠红字 + 末尾统计区分。
 *   这样一屏既当库存总览，又能一眼看出该催谁取件。
 *
 * 排序为什么不能直接在哈希表上做：
 *   哈希表按取件码散列，天然无序。想按时间输出，必须先遍历整张表
 *   把所有节点指针"倒"进一个数组，再用 qsort 排序 —— 这是标准套路。
 *
 * 指针数组用 gSt.count 定长：count 就是节点总数，一个不多一个不少。
 *
 * ★ v2.0：末尾增加"分区占用"和"应收保管费"两行小结。
 */

#include "station.h"

/* qsort 的比较函数：按入库时间升序（入库越早排越前） */
static int byInTime(const void *a, const void *b)
{
    const Package *x = *(const Package * const *)a;
    const Package *y = *(const Package * const *)b;

    if (x->inTime < y->inTime) return -1;
    if (x->inTime > y->inTime) return  1;
    return 0;
}

void doGoodsList(void)
{
    time_t now = time(NULL);
    Package **arr;
    int used[CAT_COUNT], total[CAT_COUNT];
    int i, n = 0, stale = 0;

    uiTitle("货 物 清 单");
    printf("全部在库货物（按入库时间从早到晚），红字为滞留件。\n");
    printf("在库超 %d 小时标记为滞留；保管费按超期天数阶梯计收\n\n", STALE_HOURS);

    if (gSt.count == 0) {
        uiColor(C_OK);
        printf("当前没有在库货物。\n");
        uiReset();
        return;
    }

    /* ---- 第 1 步：把哈希表里所有节点指针收进数组 ---- */
    arr = (Package **)malloc(sizeof(Package *) * gSt.count);
    if (!arr) return;

    for (i = 0; i < TABLE_SIZE; i++) {
        Package *p;
        for (p = gSt.bucket[i]; p; p = p->next) {
            arr[n++] = p;
        }
    }

    /* ---- 第 2 步：按入库时间排序 ---- */
    qsort(arr, n, sizeof(Package *), byInTime);

    /* ---- 第 3 步：逐行打印，滞留件标红 ---- */
    tableHead();
    for (i = 0; i < n; i++) {
        double hours = difftime(now, arr[i]->inTime) / 3600;

        if (hours > STALE_HOURS) {
            uiColor(C_WARN);
            tableRow(arr[i]);
            uiReset();
            stale++;
        } else {
            tableRow(arr[i]);
        }
    }
    tableFoot();

    /* ---- 第 4 步：统计小结 ---- */
    printf("共 %d 件在库", n);
    if (stale > 0) {
        uiColor(C_WARN);
        printf("，其中 %d 件已滞留超过 %d 小时，请尽快通知取件", stale, STALE_HOURS);
        uiReset();
        printf("。\n");
    } else {
        uiColor(C_OK);
        printf("，暂无滞留件。\n");
        uiReset();
    }

    /* ---- 第 5 步：分区占用 + 应收费用（v2.0 新增） ---- */
    shelfStatByCat(used, total);
    printf("\n分区占用  : ");
    for (i = 0; i < CAT_COUNT; i++) {
        printf("%s %d/%d", catName(i), used[i], total[i]);
        if (i < CAT_COUNT - 1) printf("   ");
    }
    printf("\n");
    printf("应收保管费: ");
    uiColor(totalReceivable() > 0 ? C_WARN : C_OK);
    printf("%d 元\n", totalReceivable());
    uiReset();

    free(arr);
}
