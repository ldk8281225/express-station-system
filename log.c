/*
 * 模块：log.c —— 操作日志
 * 职责：追加写一条日志（logEvent）+ 回显最近若干条（doViewLog）
 * 依赖：util.c
 * 被依赖：几乎所有业务模块都调 logEvent；main.c 调 doViewLog
 *
 * 日志用"追加"模式打开（"a"），每次写一行就关闭，
 * 这样即使程序崩溃，之前的记录也不会丢。
 */

#include "station.h"

/* 追加一条日志：[时间] 动作 取件码 说明 */
void logEvent(const char *action, const char *code, const char *detail)
{
    FILE *fp = fopen(LOG_FILE, "a");
    char ts[32];

    if (!fp) return;
    timeStr(time(NULL), "%Y-%m-%d %H:%M:%S", ts, sizeof(ts));
    fprintf(fp, "[%s] %-6s %-8s %s\n", ts, action, code, detail);
    fclose(fp);
}

/*
 * 回显日志：只显示最后 LOG_SHOW 条。
 * 做法是先读一遍数总行数，再倒回来读第二遍跳过前面的 —— 不用把整个文件存进内存。
 */
void doViewLog(void)
{
    FILE *fp = fopen(LOG_FILE, "r");
    char line[256];
    int total = 0, i = 0;

    uiTitle("操 作 日 志");

    if (!fp) {
        printf("暂无日志记录。\n");
        return;
    }
    while (fgets(line, sizeof(line), fp)) total++;      /* 第一遍：数总行数 */

    rewind(fp);
    while (fgets(line, sizeof(line), fp)) {             /* 第二遍：只打印末尾部分 */
        if (i++ >= total - LOG_SHOW) printf("%s", line);
    }
    fclose(fp);

    printf("\n（共 %d 条记录）\n", total);
}
