/*
 * 模块：inbound.c —— 快递员入库（管理员菜单 1）
 * 职责：选类别 -> 录入包裹 -> 按分区分配货架 -> 生成取件码 -> 入哈希表 -> 记日志
 * 依赖：hash.c / util.c / ui.c / log.c
 *
 * 这是全系统"写"操作的入口，流程可以背下来：
 *   选类别 -> 输入校验 -> 造节点 -> 分区申请货架 -> 生成取件码 -> 入表 -> 记账
 * 注意三处顺序：
 *   1) allocShelf 必须在 genCode 之前 —— 取件码里含货架号；
 *   2) 拿不到货架时要 free 掉已经造好的节点，否则内存泄漏；
 *   3) rollDay 要在计数之前 —— 跨天了得先把今日计数清零再累加。
 *
 * ★ v2.0：入库前先选包裹类别，系统按类别找对应的货架区
 * （分区方案见 station.h 顶部说明，类别与排的映射见 util.c）。
 */

#include "station.h"

void doCheckIn(void)
{
    Package *p;
    char phone[32], courier[32], shelf[16], masked[32], catbuf[16];
    int  i, cat, rc;

    uiTitle("快 递 入 库");

    /* ---- 第 0 步：选类别，决定放哪个分区 ---- */
    printf("包裹类别（决定货架分区）:\n");
    for (i = 0; i < CAT_COUNT; i++) {
        printf("   [%d] %s", i + 1, catName(i));
        if (i % 3 == 2 || i == CAT_COUNT - 1) printf("\n");
    }
    printf("请选择 (1~%d，直接回车按普通件): ", CAT_COUNT);
    readLine(catbuf, sizeof(catbuf));
    cat = atoi(catbuf) - 1;
    if (cat < 0 || cat >= CAT_COUNT) cat = 0;     /* 乱输就当普通件，别把人卡住 */

    /* ---- 第 1 步：读输入并校验 ---- */
    printf("收件人手机号: ");
    readLine(phone, sizeof(phone));
    if (strlen(phone) < 4) {
        uiColor(C_WARN);
        printf("[错误] 手机号太短，入库取消。\n");
        uiReset();
        return;
    }
    printf("快递公司: ");
    readLine(courier, sizeof(courier));
    if (courier[0] == '\0') strcpy(courier, "未知");

    /* ---- 第 2 步：造一个包裹节点，填好基本信息 ---- */
    p = (Package *)malloc(sizeof(Package));
    strncpy(p->phone, phone, 15);
    p->phone[15] = '\0';
    strcpy(p->tail, phone + strlen(phone) - 4);      /* 尾 4 位，取件时核验用 */
    strncpy(p->courier, courier, 15);
    p->courier[15] = '\0';
    p->inTime = time(NULL);
    p->next = NULL;

    /* ---- 第 3 步：按类别分区申请货架位（可能降级到别的区） ---- */
    rc = allocShelfByCat(catName(cat), shelf);
    if (rc == 0) {
        uiColor(C_WARN);
        printf("[失败] 货架已满（共 %d 格），请先清理滞留件。\n", SHELF_ROWS * SHELF_COLS);
        uiReset();
        free(p);                                     /* 拿不到货架就放弃这次入库 */
        return;
    }
    strcpy(p->shelf, shelf);

    /* ---- 第 4 步：生成取件码 ----
     * 必须放在申请货架之后：取件码里带了货架号和入库日期，
     * 用户拿到码就知道去哪个货架、第几层，不用再查。 */
    genCode(p->shelf, p->inTime, p->code);

    /* ---- 第 5 步：入哈希表 + 记账 + 写日志 ---- */
    rollDay();                                       /* 跨天则先把今日计数清零 */
    hashInsert(p);
    gSt.totalIn++;
    gSt.todayIn++;
    logEvent("入库", p->code, p->shelf);

    /* ---- 第 6 步：给快递员看结果 ---- */
    maskPhone(p, masked);
    uiColor(C_OK);
    printf("\n[成功] 取件码已发送至 %s\n", masked);
    uiReset();
    if (rc == 2) {                                   /* 本区满了被降级 */
        uiColor(C_WARN);
        printf("[提示] %s 区已满，本品已临时放入 %s（%s 区），请稍后人工归位。\n",
               catName(cat), p->shelf, catOfShelf(p->shelf));
        uiReset();
    }
    uiReceipt(p, "入 库 凭 条", p->inTime);
}
