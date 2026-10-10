/*
 * 模块：hash.c —— 哈希表与货架（数据结构核心）
 * 职责：实现"拉链法哈希表"的增删改查，以及货架位的分配回收
 * 依赖：util.c（分区映射 catRowFrom/catRowTo/catOfRow）
 * 被依赖：除 ui.c / table.c 外的所有模块
 *
 * 这是整个系统的地基，建议第一个读。搞懂三个函数就够了：
 *   hashIndex  —— 取件码怎么变成桶下标
 *   hashFind   —— 怎么在桶的链表里找
 *   hashRemove —— 怎么把节点摘下来而不破坏链表
 * 货架部分看另外两个：allocShelfByCat（分区找位）、shelfStatByCat（分区统计）。
 */

#include "station.h"

/*
 * 哈希函数：djb2
 * 把取件码（形如 A-05-122222，含字母和短横线）逐字符"打散"成一个大整数，
 * 再对桶数取模。
 *
 * 为什么不能直接 atoi(code) % N：新版取件码以字母开头，atoi 遇到 'A' 直接返回 0，
 * 所有包裹会全部挤进 0 号桶，哈希表退化成一条链表。djb2 对每个字符都参与运算，
 * 字母、数字、短横线都能起到打散作用。
 */
unsigned int hashIndex(const char *code)
{
    unsigned long h = 5381;
    const unsigned char *p = (const unsigned char *)code;

    while (*p) {
        h = h * 33 + *p;
        p++;
    }
    return (unsigned int)(h % TABLE_SIZE);
}

/* 初始化：桶全空、计数归零、货架清空 */
void stationInit(void)
{
    int i, r, c;

    for (i = 0; i < TABLE_SIZE; i++) {
        gSt.bucket[i] = NULL;
    }
    gSt.count = gSt.totalIn = gSt.totalOut = 0;
    gSt.todayIn = gSt.todayOut = 0;
    gSt.dayStamp = todayStamp();
    for (r = 0; r < SHELF_ROWS; r++) {
        for (c = 0; c < SHELF_COLS; c++) {
            gSt.shelf[r][c] = 0;
        }
    }
}

/* 释放所有包裹节点 */
void stationFree(void)
{
    int i;

    for (i = 0; i < TABLE_SIZE; i++) {
        Package *p = gSt.bucket[i];
        while (p) {
            Package *next = p->next;
            free(p);
            p = next;
        }
        gSt.bucket[i] = NULL;
    }
    gSt.count = 0;
}

/*
 * 查找：定位到桶，再沿链逐个比较。
 * 没找到时 p 为 NULL；cmp 记录实际比较次数（性能实验要用）。
 */
Package *hashFind(const char *code, int *cmp)
{
    Package *p = gSt.bucket[hashIndex(code)];
    int n = 0;

    while (p) {
        n++;
        if (strcmp(p->code, code) == 0) break;
        p = p->next;
    }
    if (cmp) *cmp = n;
    return p;
}

/* 插入：头插法挂进桶链表。取件码重复则拒绝插入 */
int hashInsert(Package *p)
{
    unsigned int i = hashIndex(p->code);

    if (hashFind(p->code, NULL)) return 0;

    p->next = gSt.bucket[i];
    gSt.bucket[i] = p;
    gSt.count++;
    return 1;
}

/*
 * 删除：从桶链表中摘下节点（不 free，交给调用方处理）。
 * 注意分两种情况：删首节点要改桶指针，删中间节点要改前驱的 next。
 */
Package *hashRemove(const char *code)
{
    unsigned int i = hashIndex(code);
    Package *p = gSt.bucket[i];
    Package *prev = NULL;

    while (p && strcmp(p->code, code) != 0) {
        prev = p;
        p = p->next;
    }
    if (!p) return NULL;

    if (prev) prev->next = p->next;      /* 中间或尾部节点 */
    else      gSt.bucket[i] = p->next;   /* 首节点 */

    p->next = NULL;
    gSt.count--;
    return p;
}

/* ==================== 货架分配 ==================== */

/* 把 "A-03" 换算成下标，供下面几个函数共用 */
static void shelfPos(const char *shelf, int *r, int *c)
{
    *r = shelf[0] - 'A';
    *c = atoi(shelf + 2) - 1;
}

/* 从 r0 排扫到 r1 排，取第一个空位写成 "A-03"；扫不到返回 0 */
static int allocInRows(int r0, int r1, char *out)
{
    int r, c;

    for (r = r0; r <= r1; r++) {
        for (c = 0; c < SHELF_COLS; c++) {
            if (gSt.shelf[r][c] == 0) {
                gSt.shelf[r][c] = 1;
                sprintf(out, "%c-%02d", 'A' + r, c + 1);
                return 1;
            }
        }
    }
    return 0;
}

/* 分配货架（不分类别）：整架顺序扫描，取第一个空位 */
int allocShelf(char *out)
{
    return allocInRows(0, SHELF_ROWS - 1, out);
}

/*
 * 按类别分区分配货架 —— 分类分区存放的落地点。
 * 返回值：1 = 正常放进本区；2 = 本区已满，降级放到了其它区；0 = 全部货架都满。
 *
 * 降级是有意设计的：宁可把一件易碎品临时塞进普通区并当场提示，
 * 也不让入库直接失败 —— 真实驿站也是先收货再人工挪位。
 * 所以这个函数不会因为"某个区满了"就让业务中断。
 */
int allocShelfByCat(const char *cat, char *out)
{
    int ci = catIndex(cat);

    if (allocInRows(catRowFrom(ci), catRowTo(ci), out)) return 1;
    return allocShelf(out) ? 2 : 0;
}

/* 取件后释放货架位 */
void freeShelf(const char *shelf)
{
    int r, c;

    shelfPos(shelf, &r, &c);
    if (r >= 0 && r < SHELF_ROWS && c >= 0 && c < SHELF_COLS) {
        gSt.shelf[r][c] = 0;
    }
}

/* 载入数据时把已占用的格子标回来 */
void markShelf(const char *shelf)
{
    int r, c;

    shelfPos(shelf, &r, &c);
    if (r >= 0 && r < SHELF_ROWS && c >= 0 && c < SHELF_COLS) {
        gSt.shelf[r][c] = 1;
    }
}

/*
 * 按分区统计货架占用：used[i] / total[i] 分别是第 i 类别的已用格数和格数上限。
 * 货物清单和运营报表都要画分区占用，抽在这里免得各写一份、改漏一处。
 * total 是由分区方案算出来的，不是写死的 —— 以后调整分区比例，
 * 这里自动跟着变，不会出现"分区改了但统计还说旧数"的错。
 */
void shelfStatByCat(int *used, int *total)
{
    int i, r, c;

    for (i = 0; i < CAT_COUNT; i++) {
        used[i]  = 0;
        total[i] = 0;
    }
    for (r = 0; r < SHELF_ROWS; r++) {
        int ci = catOfRow(r);
        for (c = 0; c < SHELF_COLS; c++) {
            total[ci]++;
            if (gSt.shelf[r][c]) used[ci]++;
        }
    }
}
