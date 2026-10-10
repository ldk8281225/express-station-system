/*
 * 模块：store.c —— 数据保存与载入（持久化）
 * 职责：把内存里的哈希表写进文件，以及启动时把它读回来
 * 依赖：hash.c（读回来要重新插入哈希表）、util.c（要补今日日期戳）
 * 被依赖：main.c（启动时 loadData，退出时 saveData）
 *
 * 文件格式（packages.dat，空格分隔的文本）：
 *   第 1 行：累计入库 累计出库 今日入库 今日出库 日期戳
 *   其余行：取件码 手机号 快递公司 货架位 入库时间戳
 *
 * 为什么用文本而不是二进制：可以直接打开看，调试和答辩演示都方便。
 *
 * ★ v2.0：头行从 2 个字段扩到 5 个，让"今日入库/出库"也能扛过重启。
 * 读取时按"读几个算几个"处理：v2.0 之前的旧文件只读到 2 个字段，
 * 今日计数就从 0 起算、日期戳补今天 —— 旧数据文件拿来就能用，不用转换。
 */

#include "station.h"

/*
 * 载入数据：逐条读出 -> 重新插入哈希表 -> 回填货架占用表。
 * 相当于把上次退出时的"现场"恢复出来。
 *
 * 返回值有三种含义，调用方必须区分开：
 *   -1  数据文件不存在（第一次运行，需要引导生成演示数据）
 *   >=0  载入成功，返回值就是恢复出来的在站件数（可能是 0 —— 文件在、
 *        但包裹已全部取走，此时**不能**当成"没有数据文件"）
 */
int loadData(void)
{
    FILE *fp = fopen(DATA_FILE, "r");
    char head[160];
    long t;

    if (!fp) return -1;                           /* 第一次运行，没有数据文件 */

    /* 先给计数一个干净的初值，再让头行覆盖 */
    gSt.totalIn = gSt.totalOut = 0;
    gSt.todayIn = gSt.todayOut = 0;
    gSt.dayStamp = todayStamp();

    if (fgets(head, sizeof(head), fp)) {
        int n = sscanf(head, "%d %d %d %d %d",
                       &gSt.totalIn, &gSt.totalOut,
                       &gSt.todayIn, &gSt.todayOut, &gSt.dayStamp);
        if (n < 2) {                              /* 头行损坏，按 0 处理 */
            gSt.totalIn = gSt.totalOut = 0;
        }
        if (n < 5) {                              /* 旧格式：今日计数重新开始 */
            gSt.todayIn = gSt.todayOut = 0;
            gSt.dayStamp = todayStamp();
        }
    }

    while (1) {
        Package *p = (Package *)malloc(sizeof(Package));

        if (!p) break;
        if (fscanf(fp, "%15s %15s %15s %7s %ld",
                   p->code, p->phone, p->courier, p->shelf, &t) != 5) {
            free(p);
            break;                                 /* 读完了（或文件格式不对） */
        }
        if (strlen(p->phone) < 4) {                /* 手机号不完整，跳过该行 */
            free(p);
            continue;
        }
        p->inTime = (time_t)t;
        strcpy(p->tail, p->phone + strlen(p->phone) - 4);   /* 尾号由手机号推出 */
        p->next = NULL;

        if (hashInsert(p)) {
            markShelf(p->shelf);               /* 别忘了我还占着这个格子 */
        } else {
            free(p);                               /* 取件码重复，丢弃 */
        }
    }
    fclose(fp);
    return gSt.count;
}

/*
 * 保存数据：先写头行（累计数 + 今日数 + 日期戳），
 * 再遍历整张哈希表把在站包裹逐条写出。
 * 注意这里又是"遍历所有桶 + 沿链走"，和 report.c 的统计是同一个套路。
 */
int saveData(void)
{
    FILE *fp = fopen(DATA_FILE, "w");
    int i;

    if (!fp) return 0;
    fprintf(fp, "%d %d %d %d %d\n",
            gSt.totalIn, gSt.totalOut, gSt.todayIn, gSt.todayOut, gSt.dayStamp);

    for (i = 0; i < TABLE_SIZE; i++) {
        Package *p;
        for (p = gSt.bucket[i]; p; p = p->next) {
            fprintf(fp, "%s %s %s %s %ld\n",
                    p->code, p->phone, p->courier, p->shelf, (long)p->inTime);
        }
    }
    fclose(fp);
    return 1;
}
