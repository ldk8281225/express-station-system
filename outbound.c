/*
 * 模块：outbound.c —— 用户取件出库（取件模式菜单 1 / 管理员菜单 2）
 * 职责：凭取件码找包裹 -> 核验手机尾号 -> 结算超期保管费 -> 摘出哈希表 -> 释放货架
 * 依赖：hash.c / util.c / fee.c / ui.c / table.c / log.c
 *
 * 这是全系统"删"操作的入口，也是哈希查找最能体现价值的地方。
 * 四道关卡缺一不可：码存在吗？尾号对吗？费用说清了吗？用户确认了吗？
 *
 * 内存约定：hashRemove 只把节点"摘下来"，不负责释放，
 * 所以本函数在打印完凭条后要自己 free(gone)。
 *
 * ★ v2.0：取件时结算超期保管费。
 * 注意费用是"算出来给工作人员看"的，不参与任何扣款 ——
 * 课设里没有支付通道，硬做会引出一个假的余额字段；
 * 做成"当场提示应缴金额 + 线下收"既真实又不引入虚构数据。
 */

#include "station.h"

void doPickup(void)
{
    char code[32], tail[16], yes[8];
    Package *p, *gone;
    int cmp = 0;
    int fee, days;
    time_t now;

    uiTitle("用 户 取 件");

    /* ---- 第 1 步：按取件码查找 ---- */
    printf("取件码 (如 A-05-122222): ");
    readLine(code, sizeof(code));
    strUpper(code);              /* 码里带字母，用户输小写 a-05-122222 也要能查到 */

    p = hashFind(code, &cmp);
    if (!p) {
        uiColor(C_WARN);
        printf("[失败] 取件码无效或包裹已取走");
#if SHOW_TECH_DETAIL
        printf("（本次比较 %d 次）", cmp);
#endif
        printf("。\n");
        uiReset();
        return;
    }

    tableHead();
    tableRow(p);
    tableFoot();
#if SHOW_TECH_DETAIL
    /* 比较次数是算法层面的证据，正常取件流程里不需要出现 */
    printf("（哈希查找比较 %d 次）\n\n", cmp);
#else
    printf("\n");
#endif

    /* ---- 第 2 步：核验身份（手机尾号后 4 位） ---- */
    printf("手机尾号后 4 位: ");
    readLine(tail, sizeof(tail));
    if (strcmp(tail, p->tail) != 0) {
        uiColor(C_WARN);
        printf("[拒绝] 尾号不符，不能取件。\n");
        uiReset();
        return;
    }

    /* ---- 第 3 步：结算超期保管费（先算，再确认）---- */
    now = time(NULL);
    days = overdueDays(p->inTime, now);
    fee  = storageFee(p->inTime, now);
    if (fee > 0) {
        uiColor(C_WARN);
        printf("[超期] 已超免费保管期 %d 天（免费时长 %d 小时），应缴保管费 %d 元。\n",
               days, FREE_HOURS, fee);
        uiReset();
    }

    /* ---- 第 4 步：二次确认 ---- */
    printf("确认取件? (y/n): ");
    readLine(yes, sizeof(yes));
    if (yes[0] != 'y' && yes[0] != 'Y') {
        printf("已取消。\n");
        return;
    }

    /* ---- 第 5 步：出库（摘节点 + 还货架 + 记账 + 写日志）---- */
    rollDay();                                   /* 跨天则先把今日计数清零 */
    gone = hashRemove(code);
    freeShelf(gone->shelf);
    gSt.totalOut++;
    gSt.todayOut++;
    logEvent("出库", gone->code, gone->shelf);

    /* ---- 第 6 步：打印取件凭条，然后释放节点 ---- */
    uiColor(C_OK);
    printf("\n[成功] 取件完成，货架 %s 已释放。\n", gone->shelf);
    uiReset();
    uiReceipt(gone, "取 件 凭 条", now);

    free(gone);
}
