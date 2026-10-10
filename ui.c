/*
 * 模块：ui.c —— 控制台界面
 * 职责：颜色、清屏、标题条、身份选择与两套菜单、凭条、安全输入
 * 依赖：util.c（凭条要格式化时间、手机号、类别）、fee.c（凭条要算超期保管费）
 * 被依赖：所有业务模块
 *
 * 这里的函数只管"怎么显示"，不碰任何业务逻辑，
 * 所以换一种界面（比如 EasyX 图形界面）时只需重写本文件。
 *
 * ★ v2.0 改动：一个主菜单拆成三个界面
 *   uiRoleMenu   身份选择（启动第一屏）
 *   uiUserMenu   取件模式：只开放 取件 / 尾号查询
 *   uiAdminMenu  管理员模式：开放全部功能
 * 三者都靠 menuLine / menuCenter 排版，宽度自动补齐 ——
 * 以前每加一个菜单项都要手工重数空格，现在只写内容就行。
 */

#include "station.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
static WORD gAttr = 7;      /* 记住默认颜色，便于还原 */
static UINT gOldCP = 0;     /* 记住原来的输出代码页，退出时还原 */
/*
 * 取一个按键。MSVC 用 _getch（getch 会触发 C4996 弃用警告），
 * MinGW 用 getch（conio.h 里两者都有，但 getch 更通用）。
 */
#ifdef _MSC_VER
#  define WB_GETCH _getch
#else
#  define WB_GETCH getch
#endif
#endif

/* 启动初始化：切到 UTF-8 代码页（源码是 UTF-8，否则中文乱码） */
void uiInit(void)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

    gOldCP = GetConsoleOutputCP();      /* 先记下来，退出时还回去 */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    if (GetConsoleScreenBufferInfo(h, &info)) gAttr = info.wAttributes;
#endif
}

/*
 * 退出前还原控制台代码页。
 * 不还的话，程序结束后面那个 cmd 窗口会停在 65001，
 * 之后它自己输出中文（比如批处理的提示语）就变成乱码了。
 */
void uiCleanup(void)
{
#ifdef _WIN32
    if (gOldCP != 0) SetConsoleOutputCP(gOldCP);
#endif
}

void uiColor(int attr)
{
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)attr);
#else
    printf("\033[%dm", attr == C_WARN ? 31 : attr == C_OK ? 32 : attr == C_KEY ? 33 : 36);
#endif
}

void uiReset(void)
{
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), gAttr);
#else
    printf("\033[0m");
#endif
}

void uiClear(void)
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void uiPause(void)
{
    char line[64];

    printf("\n按回车键继续...");
    fflush(stdout);
    /*
     * ★ 这里必须按"整行"读，不能再用 getchar()。
     * getchar() 只吃掉一个字符：用户在这一步若多按了键、或者输入是
     * 粘贴/重定向进来的，回车会留在缓冲区里，被下一轮菜单读成"空输入"，
     * 表现出来就是"按 0 没反应、菜单乱跳"。
     * 换成 readLine 后，暂停处永远消费掉完整一行，粒度与菜单一致。
     */
    readLine(line, sizeof(line));
}

/* 标题条 */
void uiTitle(const char *text)
{
    uiColor(C_TITLE);
    printf("+--------------------------------------------+\n");
    printf("  %s\n", text);
    printf("+--------------------------------------------+\n");
    uiReset();
}

/* ==================== 菜单排版工具 ==================== */

#define MENU_W 44    /* 菜单内容区宽度，与标题条、表格风格统一 */

/* 计算字符串的显示宽度：中文等宽字体下占 2 列。
 * UTF-8 的中文是 3 字节，GBK 是 2 字节，这里都能算对。 */
static int dispLen(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int n = 0;

    while (*p) {
        if (*p < 0x80)       { n += 1; p += 1; }   /* ASCII */
        else if (*p >= 0xF0) { n += 2; p += 4; }
        else if (*p >= 0xE0) { n += 2; p += 3; }   /* UTF-8 中文 */
        else if (*p >= 0xC0) { n += 2; p += 2; }
        else                 { n += 1; p += 1; }   /* GBK 首字节，两字节合计 2 列 */
    }
    return n;
}

/* 一条菜单框线 */
static void menuRule(void)
{
    int i;
    putchar('+');
    for (i = 0; i < MENU_W; i++) putchar('-');
    printf("+\n");
}

/* 打印一行菜单：内容自动补齐到 MENU_W 列，两侧用 | 封口 */
static void menuLine(const char *s)
{
    int len = dispLen(s);

    printf("|%s", s);
    while (len < MENU_W) { putchar(' '); len++; }
    printf("|\n");
}

/* 同上，但内容在 MENU_W 列里居中 */
static void menuCenter(const char *s)
{
    int len = dispLen(s);
    int left = (MENU_W - len) / 2;
    int i;

    printf("|");
    for (i = 0; i < left; i++) putchar(' ');
    fputs(s, stdout);
    for (i = left + len; i < MENU_W; i++) putchar(' ');
    printf("|\n");
}

/* ==================== 三个界面 ==================== */

/*
 * 身份选择：程序启动后的第一屏。
 * 这一屏决定后面看到哪套菜单 —— 取件人只能碰取件和查询，
 * 管理员才有入库和后台管理。
 */
void uiRoleMenu(void)
{
    char stat[80];

    /* 首屏带一行实时状态 —— 真实系统的第一屏总要先告诉你现在什么情况，
     * 而不是干巴巴地问"你要以什么身份登录"。*/
    sprintf(stat, "  在站 %d 件   累计入库 %d / 出库 %d",
            gSt.count, gSt.totalIn, gSt.totalOut);

    uiClear();
    uiColor(C_TITLE);
    menuRule();
    menuLine("");
    menuCenter("快递驿站智能管理系统  v2.0");
    menuLine("");
    menuRule();
    uiReset();

    menuLine(stat);

    uiColor(C_TITLE);
    menuRule();
    menuCenter("请 选 择 身 份");
    menuRule();
    uiReset();

    menuLine("");
    uiColor(C_OK);
    menuLine("  [1] 取件模式");
    uiReset();
    menuLine("      只开放 取件 / 尾号查询");
    menuLine("");
    uiColor(C_KEY);
    menuLine("  [2] 管理员模式");
    uiReset();
    menuLine("      入库 / 后台管理，需口令");
    menuLine("");
    menuLine("  [0] 退出系统");
    menuLine("");

    uiColor(C_TITLE);
    menuRule();
    uiReset();
    uiColor(C_KEY);
    printf("请选择身份: ");
    uiReset();
    fflush(stdout);
}

/* 取件模式菜单：只有两项，取件人视线里不该出现别的入口 */
void uiUserMenu(void)
{
    uiClear();
    uiColor(C_TITLE);
    menuRule();
    menuLine("");
    menuCenter("取 件 模 式");
    menuLine("");
    menuRule();
    uiReset();

    menuLine("");
    menuLine("  [1] 取件");
    menuLine("  [2] 尾号查询");
    menuLine("");
    menuLine("  [0] 返回");
    menuLine("");

    uiColor(C_TITLE);
    menuRule();
    uiReset();
    uiColor(C_KEY);
    printf("请选择: ");
    uiReset();
    fflush(stdout);
}

/* 管理员菜单：入库 + 全部后台功能 */
void uiAdminMenu(void)
{
    uiClear();
    uiColor(C_TITLE);
    menuRule();
    menuLine("");
    menuCenter("管 理 员 后 台");
    menuLine("");
    menuRule();
    uiReset();

    menuLine("");
    menuLine("  [1] 快递入库        [2] 用户取件");
    menuLine("  [3] 尾号查询        [4] 货物清单");
    menuLine("  [5] 运营报表        [6] 性能实验");
    menuLine("  [7] 操作日志        [8] 批量录入");
    menuLine("  [9] 手动保存数据    [0] 返回");
    menuLine("");

    uiColor(C_TITLE);
    menuRule();
    uiReset();
    uiColor(C_KEY);
    printf("请选择: ");
    uiReset();
    fflush(stdout);
}

/* ==================== 表格排版 ==================== */

/* 按显示宽度左对齐输出，不足补空格 —— 表格不歪的关键 */
void uiPad(const char *s, int width)
{
    int pad = width - dispLen(s);

    fputs(s, stdout);
    while (pad-- > 0) putchar(' ');
}

/* 在 width 列的内容区里居中输出一行，两侧用 | 封口 */
void uiCenter(const char *s, int width)
{
    int len = dispLen(s);
    int left = (width - len) / 2;
    int i;

    printf("  |");
    for (i = 0; i < left; i++)            putchar(' ');
    fputs(s, stdout);
    for (i = left + len; i < width; i++)  putchar(' ');
    printf("|\n");
}

/*
 * 凭条：入库或取件后打印的小票。
 * 若这件已超期，末尾多一行红字费用 —— 取件时当面提醒，
 * 比事后在报表里看到要直观得多。
 */
void uiReceipt(const Package *p, const char *title, time_t when)
{
    char ts[32], phone[32], fee[32];
    int days = overdueDays(p->inTime, when);

    timeStr(when, "%Y-%m-%d %H:%M:%S", ts, sizeof(ts));
    maskPhone(p, phone);

    printf("\n");
    uiColor(C_KEY);
    printf("  +-------------------------------+\n");
    uiCenter(title, 31);                       /* 标题在 31 列内容区里居中 */
    printf("  +-------------------------------+\n");
    uiReset();
    printf("  |  "); uiPad("取件码", 6); printf(" : "); uiPad(p->code, 19);            printf(" |\n");
    printf("  |  "); uiPad("货架", 6);   printf(" : "); uiPad(p->shelf, 19);           printf(" |\n");
    printf("  |  "); uiPad("类别", 6);   printf(" : "); uiPad(catOfShelf(p->shelf), 19); printf(" |\n");
    printf("  |  "); uiPad("收件人", 6); printf(" : "); uiPad(phone, 19);              printf(" |\n");
    printf("  |  "); uiPad("快递", 6);   printf(" : "); uiPad(p->courier, 19);        printf(" |\n");
    printf("  |  "); uiPad("时间", 6);   printf(" : "); uiPad(ts, 19);                printf(" |\n");

    if (days > 0) {
        sprintf(fee, "%d 天，应缴 %d 元", days, storageFee(p->inTime, when));
        uiColor(C_WARN);
        printf("  |  "); uiPad("超期", 6); printf(" : "); uiPad(fee, 19); printf(" |\n");
        uiReset();
    }
    printf("  +-------------------------------+\n");
}

/* ==================== 输入 ==================== */

/* 读一行并去掉末尾换行，避免 scanf 残留回车导致输入被跳过
 * 返回 1 = 读到输入；返回 0 = 输入流已结束(EOF)，调用方应正常退出，
 * 否则主循环会在 EOF 上反复空转（CPU 占满）。 */
int readLine(char *buf, int size)
{
    int ch;

    if (!fgets(buf, size, stdin)) {
        buf[0] = '\0';
        return 0;
    }

    /*
     * ★ 一行的长度超过缓冲区时，把剩下的字符读掉丢弃。
     * 旧写法直接返回，留下的残渣会串进下一次读取 —— 输入整体错位，
     * 用户看到的现象就是"输了一次，后面几步全乱"。
     */
    if (!strchr(buf, '\n')) {
        while ((ch = getchar()) != '\n' && ch != EOF) { }
    }

    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

/*
 * 读管理员口令：逐字符读、回显 *，回车结束。
 *
 * 分工是这样的：
 *   stdin 是交互式控制台（人在键盘上输）—— 用 getch 逐字符取，回显星号，
 *     回车、退格这些控制键不会把输入搞乱，口令也不会留在屏幕上。
 *   stdin 不是控制台（管道、文件重定向、脚本喂输入）—— 直接走 readLine。
 *     这一步判断很关键：getch 读的是**控制台键盘缓冲**而不是 stdin，
 *     不先判断的话，脚本喂进去的输入会被晾在一边，程序一直等键盘，卡死。
 */
int readPass(char *buf, int size)
{
#ifdef _WIN32
    int i = 0, ch;

    if (!_isatty(_fileno(stdin))) return readLine(buf, size);   /* 非交互环境，降级 */

    for (;;) {
        ch = WB_GETCH();
        if (ch == EOF || ch == -1) {
            if (i == 0) return readLine(buf, size);   /* 取不到键，再降级一次 */
            break;
        }
        if (ch == '\r' || ch == '\n') break;
        if (ch == '\b') {                             /* 退格：擦掉一个星号 */
            if (i > 0) { i--; printf("\b \b"); }
            continue;
        }
        if (i < size - 1) {
            buf[i++] = (char)ch;
            putchar('*');
        }
    }
    buf[i] = '\0';
    printf("\n");
    return 1;
#else
    return readLine(buf, size);
#endif
}
