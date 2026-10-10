/*
 * 模块：experiment.c —— 哈希性能实验（菜单 6）
 * 职责：用实测数据证明"哈希查找比顺序查找快多少"
 * 依赖：hash.c（建表和查找）
 *
 * 这是全项目最值得写进报告的一部分：
 *   空谈 O(1) 没有说服力，拿一张实测对比表出来，分数立刻不一样。
 *
 * 实验方法：
 *   1. 随机生成 n 个取件码建表（顺便把码存进数组备用）
 *   2. 对每个码做两次查找：一次走哈希表，一次在数组里从头比
 *   3. 分别累计比较次数，算平均值
 *
 * ★ 全局化之后的特殊处理：
 *   现在 hashInsert / hashFind 都直接操作全局 gSt，会覆盖真实数据。
 *   所以实验前先把 gSt 整体备份到局部变量，把 gSt 清空当"实验台"；
 *   做完再把备份还原回去。Station 是纯数据，整体赋值就是一次干净的快照。
 */

#include "station.h"

void doExperiment(void)
{
    int sizes[3] = { 500, 1000, 2000 };
    int k, i, j;

    uiTitle("哈 希 性 能 实 验");
    printf("随机生成 N 个取件码，逐个查找一次，统计平均比较次数\n\n");
    printf("+--------+------------------+------------------+----------+\n");
    printf("| "); uiPad("数据量", 6);
    printf(" | "); uiPad("哈希查找(次/回)", 16);
    printf(" | "); uiPad("顺序查找(次/回)", 16);
    printf(" | "); uiPad("提升", 8);
    printf(" |\n");
    printf("+--------+------------------+------------------+----------+\n");

    for (k = 0; k < 3; k++) {
        int n = sizes[k];
        static Station backup;                             /* 真实数据的快照 */
        char (*codes)[16] = (char (*)[16])malloc(16 * n);  /* 当作 char codes[n][16] 用 */
        long long hashSum = 0, seqSum = 0;

        backup = gSt;                                      /* 第 1 步：备份现场 */
        stationInit();                                     /* 第 2 步：清空当实验台 */

        /* 建表：生成 n 个不重复的取件码。
         * 这里用和真实业务完全相同的码格式（货架-层数-日期+随机数），
         * 否则实验用的码和系统里的码不是一个形状，数据说服力就打折。 */
        for (i = 0; i < n; i++) {
            Package *p = (Package *)malloc(sizeof(Package));
            p->next = NULL;
            do {
                formatCode(p->code,
                           (char)('A' + rand() % SHELF_ROWS),   /* 货架号 A~F */
                           rand() % SHELF_COLS + 1,             /* 层数 01~20 */
                           rand() % 28 + 1,                     /* 日期 1~28 */
                           rand() % 10000);                     /* 4 位随机数 */
            } while (!hashInsert(p));                    /* 重复则重摇 */
            strcpy(codes[i], p->code);
        }

        /* 查找：同一个码分别用两种方式找，各记比较次数 */
        for (i = 0; i < n; i++) {
            int cmp = 0;
            hashFind(codes[i], &cmp);
            hashSum += cmp;
            for (j = 0; j < n; j++) {
                seqSum++;
                if (strcmp(codes[j], codes[i]) == 0) break;
            }
        }

        printf("| %6d | %16.2f | %16.2f | %7.1fx |\n",
               n, (double)hashSum / n, (double)seqSum / n, (double)seqSum / hashSum);

        free(codes);
        stationFree();                                     /* 第 3 步：释放实验数据 */
        gSt = backup;                                      /* 第 4 步：还原现场 */
    }
    printf("+--------+------------------+------------------+----------+\n");
    printf("\n结论：哈希查找次数几乎不随数据量增长，顺序查找则线性增长。\n");
}
