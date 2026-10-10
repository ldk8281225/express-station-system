/*
 * 模块：demo.c —— 演示数据生成
 * 职责：一键造 n 件随机包裹，方便演示和自测时不用手敲
 * 依赖：hash.c / util.c / store.c（生成后立刻落盘）
 * 被依赖：main.c（首次运行时的引导、管理员菜单 8）
 *
 * 这是唯一的"非业务"模块，答辩前用它铺好现场即可。
 *
 * ★ v2.0：包裹随机落在 5 个分区里，入库时间打散到最近 96 小时，
 * 保证"货物清单"里的滞留红字和超期保管费一定有内容可看。
 */

#include "station.h"

void seedDemo(int n)
{
    const char *firms[] = { "顺丰", "中通", "韵达", "圆通", "京东", "极兔" };
    int i;
    int before = gSt.count;      /* 记下起始值，末尾好报告"本次录入了几件" */

    for (i = 0; i < n; i++) {
        Package *p = (Package *)malloc(sizeof(Package));
        char phone[32], shelf[16];
        int tail = rand() % 10000;
        int cat  = rand() % CAT_COUNT;      /* 随机挑一个分区 */

        if (!p) break;

        /* 手机号：138 + 4 位 + 尾 4 位，尾号单独存一份便于查询 */
        sprintf(phone, "138%04d%04d", rand() % 10000, tail);
        strcpy(p->phone, phone);
        sprintf(p->tail, "%04d", tail);
        strcpy(p->courier, firms[rand() % 6]);

        /*
         * 入库时间随机落在最近 96 小时内，这样必有一部分超过 48 小时，
         * 方便演示滞留件的红色标注和超期保管费。
         * 注意：Windows 的 RAND_MAX 只有 32767，不能直接 rand() % 345600，
         * 否则最大只有 9 小时，滞留件永远查不出来。这里拆成「小时 + 秒」两段。
         */
        p->inTime = time(NULL) - (rand() % 97) * 3600 - rand() % 3600;
        p->next = NULL;

        if (allocShelfByCat(catName(cat), shelf) == 0) {   /* 货架满了就停 */
            free(p);
            break;
        }
        strcpy(p->shelf, shelf);

        /* 取件码必须在货架 + 入库时间都定下来之后再生成：
         * 码里含货架号和入库日期，所以演示数据的取件码日期也分布在最近 4 天内。*/
        genCode(p->shelf, p->inTime, p->code);

        hashInsert(p);
        gSt.totalIn++;
        gSt.todayIn++;
    }
    /* 注意报的是"本次录入"而不是"在站总数"——
     * 追加模式下这两个数不一样，报总数会让人以为刚才录了一大堆。*/
    printf("已录入 %d 件包裹，货架与取件码已自动分配。\n", gSt.count - before);
}

/*
 * 菜单入口：先问数量再生成。
 * 注意这里是"追加"而不是"清空重来" —— 演示时常常要在已有数据上再加几件；
 * 真想全清，删掉 packages.dat 再启动即可。
 */
void doSeedDemo(void)
{
    char buf[16];
    int n = 20;
    int room = SHELF_ROWS * SHELF_COLS - gSt.count;

    uiTitle("批 量 录 入");
    printf("批量建立入库记录，系统自动分配货架并生成取件码。\n");
    printf("注意：将在现有数据上追加，不会清空已有包裹。\n\n");
    printf("当前在站 %d 件，还能放 %d 件。\n", gSt.count, room > 0 ? room : 0);
    printf("录入数量 (直接回车按 20): ");
    readLine(buf, sizeof(buf));
    if (buf[0] != '\0') n = atoi(buf);

    if (n <= 0) {
        printf("数量无效，已取消。\n");
        return;
    }
    if (n > room) n = room;                 /* 别超过剩余货位 */
    if (n == 0) {
        uiColor(C_WARN);
        printf("[提示] 货架已满，请先清理滞留件。\n");
        uiReset();
        return;
    }
    seedDemo(n);
    saveData();                             /* 生成完立刻落盘，拔电源也不丢 */
}
