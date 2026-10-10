/*
 * 模块：main.c —— 程序入口与角色分发
 * 职责：启动恢复数据 -> 选身份 -> 进对应模式 -> 退出统一保存
 * 依赖：全部模块（这里是唯一把所有模块串起来的地方）
 *
 * 建议最后读这个文件：看完它就能明白整个系统是怎么组装起来的。
 *
 * ★ v2.0：主循环变成两层
 *   外层：选身份（取件模式 / 管理员模式 / 退出）
 *   内层：该身份自己的菜单循环，按 [0] 退回外层
 * 两个模式共用同一批业务函数，区别只在"菜单放出了哪几项"：
 *   取件模式   能调 doPickup / doQueryByTail
 *   管理员模式 还能调 doCheckIn / doGoodsList / doStatistics / doExperiment ...
 * 这就是把权限做在界面层的好处 —— 业务函数一行都不用为角色改。
 */

#include "station.h"

/*
 * ★ 全局驿站对象 —— 整个程序唯一的一份数据，定义就在这里。
 * 其它模块通过 station.h 里的 extern 声明直接使用 gSt，
 * 所以下面的调用都不用再传参数（doPickup() 而不是 doPickup(&st)）。
 */
Station gSt;

/*
 * 把菜单输入解析成选项编号。
 * 返回 0~99 = 有效选项；-1 = 无效（空行 / 含非数字 / 超出范围）。
 *
 * 为什么不用 atoi：
 *   1) atoi 不校验尾部垃圾字符，"1abc" 会被悄悄当成 1；
 *   2) 输入超长数字时 atoi 会整型溢出，属于未定义行为；
 *   3) 旧代码用 buf[0] 判断"是不是数字"，而值又交给 atoi 解析，
 *      两处口径不一致（"  1" 前带空格时，buf[0] 判无效、atoi 却认 1）。
 * 这里用 strtol 一次把整行严格解析干净，口径统一。
 */
static int parseChoice(const char *s)
{
    char *end;
    long v;

    if (!s || *s == '\0') return -1;             /* 空行：无效 */
    v = strtol(s, &end, 10);
    if (*end != '\0')    return -1;              /* 数字后面还有字符 */
    if (v < 0 || v > 99) return -1;              /* 菜单里不会有这种编号 */
    return (int)v;
}

/* 取件模式：只开放取件和尾号查询两项 */
static void runUserMode(void)
{
    char buf[16];

    while (1) {
        int choice = -1;

        uiUserMenu();
        if (!readLine(buf, sizeof(buf))) return;    /* 输入结束，直接退回 */
        choice = parseChoice(buf);

        switch (choice) {
            /* 写操作后立即落盘一次：这样即使直接点窗口右上角的 × 关掉，
             * 数据也不会丢（退出时再存一次也毫无副作用）。 */
            case 1: doPickup();      saveData(); break;
            case 2: doQueryByTail(); break;
            case 0: return;                         /* 回到身份选择 */
            default:
                uiColor(C_WARN);
                printf("无效选择，请输入 0~2 之间的数字。\n");
                uiReset();
                break;
        }
        uiPause();
    }
}

/* 管理员模式：入库 + 全部后台功能 */
static void runAdminMode(void)
{
    char buf[16];

    while (1) {
        int choice = -1;

        uiAdminMenu();
        if (!readLine(buf, sizeof(buf))) return;
        choice = parseChoice(buf);

        switch (choice) {
            case 1: doCheckIn();     saveData(); break;
            case 2: doPickup();      saveData(); break;
            case 3: doQueryByTail(); break;
            case 4: doGoodsList();   break;
            case 5: doStatistics();  break;
            case 6: doExperiment();  break;
            case 7: doViewLog();     break;
            case 8: doSeedDemo();    break;         /* 它内部自己会保存 */
            case 9:
                saveData();
                uiColor(C_OK);
                printf("数据已保存（在站 %d 件）。\n", gSt.count);
                uiReset();
                break;
            case 0: return;
            default:
                uiColor(C_WARN);
                printf("无效选择，请输入 0~9 之间的数字。\n");
                uiReset();
                break;
        }
        uiPause();
    }
}

/*
 * 管理员登录：最多试 PASS_TRIES 次。
 * 口令写在 station.h 的 ADMIN_PASS 里，不在本文件 —— 想改口令不用翻这里。
 * 返回 1 = 通过；0 = 未通过或输入结束。
 */
static int adminLogin(void)
{
    char pass[32];
    int i;

    uiClear();
    uiTitle("管 理 员 登 录");
#if SHOW_TECH_DETAIL
    /* 只有展示模式才提示口令位置 —— 真实后台不会把密码写在登录页上 */
    printf("提示：口令写在 station.h 的 ADMIN_PASS 里，默认 8888。\n\n");
#else
    printf("\n");
#endif

    for (i = 1; i <= PASS_TRIES; i++) {
        printf("请输入管理员口令 (第 %d/%d 次): ", i, PASS_TRIES);
        if (!readPass(pass, sizeof(pass))) return 0;

        if (strcmp(pass, ADMIN_PASS) == 0) {
            uiColor(C_OK);
            printf("[通过] 欢迎回来，管理员。\n");
            uiReset();
            logEvent("登录", "-", "管理员进入后台");
            uiPause();
            return 1;
        }

        uiColor(C_WARN);
        printf("[拒绝] 口令错误");
        uiReset();
        printf("，还可尝试 %d 次。\n", PASS_TRIES - i);
    }

    uiColor(C_WARN);
    printf("\n[锁定] 口令连续错误 %d 次，返回身份选择。\n", PASS_TRIES);
    uiReset();
    logEvent("登录", "-", "口令连续错误，已拒绝");
    uiPause();
    return 0;
}

int main(void)
{
    char buf[16];
    int running = 1;

    uiInit();
    stationInit();
    srand((unsigned int)time(NULL));

    /* 启动：有数据就恢复现场，没有就问要不要造一批演示数据 */
    uiTitle("系 统 启 动");
    if (loadData() >= 0) {
        printf("今日营业中：在站 %d 件，累计入库 %d 件 / 出库 %d 件。\n",
               gSt.count, gSt.totalIn, gSt.totalOut);
        if (gSt.count == 0) {
            printf("当前没有在站包裹，可直接入库。\n");
        }
    } else {
        printf("首次启动，是否载入一批示例包裹以便熟悉系统? (y/n): ");
        readLine(buf, sizeof(buf));
        if (buf[0] == 'y' || buf[0] == 'Y') {
            seedDemo(30);
            saveData();
        }
    }
    logEvent("启动", "-", "进入系统");
    uiPause();

    /* 外层循环：选身份 -> 进对应模式 -> 回来再选 */
    while (running) {
        int role = -1;

        uiRoleMenu();
        /* 输入流结束(EOF)时按 role=0 处理：走下面的退出分支，
         * 否则 readLine 一直返回空串，主循环会空转把 CPU 占满 */
        if (!readLine(buf, sizeof(buf))) {
            role = 0;
        } else {
            role = parseChoice(buf);
        }

        switch (role) {
            case 1:
                runUserMode();
                break;
            case 2:
                if (adminLogin()) runAdminMode();
                break;
            case 0:
                running = 0;
                break;
            default:
                uiColor(C_WARN);
                printf("无效选择，请输入 0~2 之间的数字。\n");
                uiReset();
                uiPause();
                break;
        }
    }

    /* 收尾：无论从哪个模式退出，都统一保存并归还控制台 */
    saveData();
    logEvent("退出", "-", "保存并退出");
    uiColor(C_OK);
    printf("今日数据已保存（在站 %d 件）。祝您工作顺利。\n", gSt.count);
    uiReset();
    stationFree();
    uiCleanup();          /* 把控制台代码页还回去，别影响外面的 cmd */
    return 0;
}
