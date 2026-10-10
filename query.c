/*
 * 模块：query.c —— 按手机尾号查询（菜单 3）
 * 职责：输入尾号后 4 位，列出这个人所有在站的包裹
 * 依赖：ui.c / table.c
 *
 * 这是全系统唯一的"反向查询"，也是很好的答辩素材：
 *   哈希表擅长"按取件码查"（O(1)），
 *   但"按人查"不是关键字，只能遍历所有桶和所有链（O(n)）。
 * 想优化就得再建一张以手机号为键的索引表 —— 这正是数据库索引的思路。
 */

#include "station.h"

void doQueryByTail(void)
{
    char input[32], tail[8];
    int i, found = 0;

    uiTitle("尾 号 查 询");

    printf("手机尾号后 4 位: ");
    readLine(input, sizeof(input));
    if (strlen(input) < 4) {
        uiColor(C_WARN);
        printf("[错误] 请输入 4 位尾号。\n");
        uiReset();
        return;
    }
    strcpy(tail, input + strlen(input) - 4);      /* 允许输入完整手机号，取后 4 位 */

    /* 遍历整张哈希表：外层走桶，内层沿链 */
    for (i = 0; i < TABLE_SIZE; i++) {
        Package *p;
        for (p = gSt.bucket[i]; p; p = p->next) {
            if (strcmp(p->tail, tail) == 0) {
                if (found == 0) tableHead();      /* 第一次命中才打印表头 */
                tableRow(p);
                found++;
            }
        }
    }

    if (found) {
        tableFoot();
        printf("共 %d 件在站包裹。\n", found);
    } else {
        printf("未找到尾号 %s 的在站包裹。\n", tail);
    }
}
