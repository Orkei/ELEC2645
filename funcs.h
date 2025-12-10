#ifndef FUNCS_H
#define FUNCS_H

#define MAX_STR_LEN 64

// 定义历史记录结构体
typedef struct {
    char tool_name[MAX_STR_LEN];
    char details[MAX_STR_LEN];    // 用于记录输入参数 (Inputs)
    char result_str[MAX_STR_LEN]; // [修改] 用于记录输出结果 (Outputs)，支持文本
} CalcRecord;

// 函数原型
void free_history_memory(CalcRecord *history);
void menu_item_1(CalcRecord **history, int *count);
void menu_item_2(CalcRecord **history, int *count);
void menu_item_3(CalcRecord **history, int *count);
void menu_item_4(CalcRecord **history, int *count);
void menu_item_5(CalcRecord **history, int *count);
void menu_item_6(CalcRecord **history, int *count);
void menu_item_7(CalcRecord **history, int *count);

#endif
