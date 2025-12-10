#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "funcs.h"

// ============================================
// CONSTANTS & DEFINITIONS
// ============================================
// Standard E24 Resistor Series Base Values
static const double E24_BASE[] = {
    1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0,
    3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1
};
// Digit and multiplier colour names for printing
static const char *COLOUR_DIGITS[10] = {
    "Black", "Brown", "Red", "Orange", "Yellow",
    "Green", "Blue", "Violet", "Grey", "White"
};

// Only 10^0 ~ 10^6
static const char *COLOUR_MULTIPLIERS[7] = {
    "Black",  // x1
    "Brown",  // x10
    "Red",    // x100
    "Orange", // x1k
    "Yellow", // x10k
    "Green",  // x100k
    "Blue"    // x1M
};
#define E24_COUNT (sizeof(E24_BASE)/sizeof(E24_BASE[0]))

// Graph settings
#define GRAPH_ROWS 20
#define GRAPH_COLS 60
#define MAX_INPUT_LEN 64

// ============================================
// WORKBENCH STATE (Interconnectivity Layer)
// ============================================
// Defaults set to 10.0V, 4.7k, 1mA, 0.7V
static double g_wb_voltage = 10.0;     // Default voltage: 10.0V
static double g_wb_resistor = 4700.0;  // Default resistor: 4.7k
static double g_wb_capacitor = 1e-6;   // Default capacitor: 1uF
static double g_wb_current = 0.001;    // Default current: 1mA
static double g_wb_vf = 0.7;           // Default diode drop: 0.7V
static double g_wb_inductor = 10e-3;   // Default inductor: 10mH

// Helper to format engineering output
static void format_eng(double val, char *buf) {
    if (val == 0) { sprintf(buf, "0"); return; }
    char *suffixes = "pnum kMG";
    double magnitude = val; int s_idx = 4; // start at ' ' (base)
    while (magnitude >= 1000.0 && s_idx < 7) { magnitude /= 1000.0; s_idx++; }
    while (magnitude < 1.0 && magnitude > 0 && s_idx > 0) { magnitude *= 1000.0; s_idx--; }
    char suffix = suffixes[s_idx];
    if (suffix == ' ') sprintf(buf, "%.2f", magnitude);
    else sprintf(buf, "%.2f%c", magnitude, suffix);
}

// ============================================
// Internal Helper Function Prototypes
// ============================================
static double get_eng_input_with_default(const char *prompt_base, double *default_val_ptr, int use_eng_format);
static int get_menu_selection(const char *prompt, int min, int max);
static double find_closest_e24_resistor(double target_r);
static double get_standard_resistor_input(const char *component_name, double initial_guess);
static void add_record_to_history(CalcRecord **history, int *count, const char *tool, const char *details, const char *result_str);
static void plot_vertical_strip_chart(double *data, int total_steps, double t_total, const char *title, const char *unit);
static int resistor_to_bands(double r_ohms, int *d1, int *d2, int *mult_idx);
// ============================================
// Internal Helper Function Implementations
// ============================================

/**
 * @brief Reads engineering input.
 * @param use_eng_format If 1, formats default value like "3.40k". If 0, like "3400.00".
 */
static double get_eng_input_with_default(const char *prompt_base, double *default_val_ptr, int use_eng_format) {
    char buf[MAX_INPUT_LEN]; double value = 0.0; int valid = 0;
    char default_str[32];
    if (default_val_ptr) {
        if (use_eng_format) {
            format_eng(*default_val_ptr, default_str);
        } else {
            sprintf(default_str, "%.2f", *default_val_ptr);
        }
    }

    do {
        if (default_val_ptr) printf("%s [default: %s]: ", prompt_base, default_str);
        else printf("%s: ", prompt_base);

        if (!fgets(buf, sizeof(buf), stdin)) exit(1);
        buf[strcspn(buf, "\r\n")] = '\0';
        
        if (strlen(buf) == 0 && default_val_ptr) return *default_val_ptr;

        // --- Standard Engineering Parsing Logic ---
        char *startptr = buf; while (isspace((unsigned char)*startptr)) startptr++;
        if (*startptr == '\0') continue;
        char *endptr; value = strtod(startptr, &endptr);
        if (endptr == startptr) { printf("Invalid input.\n"); continue; }
        char *suffix_ptr = endptr; while (isspace((unsigned char)*suffix_ptr)) suffix_ptr++;
        double multiplier = 1.0; int suffix_found = 0;
        if (*suffix_ptr != '\0') {
            switch (*suffix_ptr) {
                case 'p': multiplier = 1e-12; suffix_found=1; break; case 'n': multiplier = 1e-9;  suffix_found=1; break;
                case 'u': multiplier = 1e-6;  suffix_found=1; break; case 'm': multiplier = 1e-3;  suffix_found=1; break;
                case 'k': case 'K': multiplier = 1e3; suffix_found=1; break; case 'M': multiplier = 1e6;   suffix_found=1; break;
                case 'G': multiplier = 1e9;   suffix_found=1; break;
                default: printf("Unknown suffix.\n"); suffix_found = -1; break;
            }
            if (suffix_found == 1 && *(suffix_ptr + 1) != '\0') { printf("Trailing characters.\n"); suffix_found = -1; }
        }
        if (suffix_found != -1) { value *= multiplier; valid = 1; }
    } while (!valid);

    if (default_val_ptr && value > 0) *default_val_ptr = value;
    return value;
}

static int get_menu_selection(const char *prompt, int min, int max) {
    int value;
    do {
        double d_val = get_eng_input_with_default(prompt, NULL, 0);
        value = (int)d_val;
        if (d_val < min - 0.0001 || d_val > max + 0.0001) {
            printf("Please enter an integer between %d and %d.\n", min, max); value = -1;
        }
    } while (value == -1);
    return value;
}

// Core logic to find closest E24 resistor value
static double find_closest_e24_resistor(double target_r) {
    if (target_r <= 0) return 1.0;

    double exponent = floor(log10(target_r));
    double magnitude = pow(10, exponent);
    double normalized_base = target_r / magnitude;
    double closest_base = E24_BASE[0];
    double min_diff = 1e9;

    for (int i = 0; i < E24_COUNT; i++) {
        double diff = fabs(normalized_base - E24_BASE[i]);
        if (i == E24_COUNT - 1) {
             double diff_next_decade = fabs(normalized_base - 10.0);
             if (diff_next_decade < diff && diff_next_decade < min_diff) {
                 closest_base = 1.0; magnitude *= 10; break;
             }
        }
        if (diff < min_diff) { min_diff = diff; closest_base = E24_BASE[i]; }
    }
    return closest_base * magnitude;
}

static int resistor_to_bands(double r_ohms, int *d1, int *d2, int *mult_idx) {
    if (r_ohms <= 0.0) return 0;

    double v = r_ohms;
    int exp = 0;

    // 规范化到 [10, 100)
    while (v >= 100.0) {
        v /= 10.0;
        exp++;
    }
    while (v < 10.0) {
        v *= 10.0;
        exp--;
    }

    int two_digits = (int)round(v);
    if (two_digits >= 100) {
        // 四舍五入变 100 的情况，修正一下
        two_digits /= 10;
        exp++;
    }

    int first = two_digits / 10;
    int second = two_digits % 10;

    // 你的菜单 multiplier 目前只支持 0~6 (x1 到 x1M)
    if (exp < 0 || exp > 6) {
        return 0; // 超出当前 4 环支持范围
    }

    if (d1) *d1 = first;
    if (d2) *d2 = second;
    if (mult_idx) *mult_idx = exp;
    return 1;
}

// Refactored to use find_closest_e24_resistor helper
static double get_standard_resistor_input(const char *component_name, double initial_guess) {
    printf("\n[Select Standard E24 Resistor for %s]\n", component_name);
    double target_r = get_eng_input_with_default("Enter Target Value", &initial_guess, 1);
    
    double final_R = find_closest_e24_resistor(target_r);
    
    char fmt_buf[32]; format_eng(final_R, fmt_buf);
    printf("-> Nearest Standard E24 Value: %sOhms\n", fmt_buf);
    g_wb_resistor = final_R;
    return final_R;
}

// [UPDATED] Updated to accept string result instead of double
static void add_record_to_history(CalcRecord **history, int *count, const char *tool, const char *details, const char *result_str) {
    int new_count = *count + 1;
    CalcRecord *temp = realloc(*history, new_count * sizeof(CalcRecord));
    if (temp == NULL) { printf("\n[Error] Memory allocation failed!\n"); return; }
    *history = temp;
    strncpy((*history)[*count].tool_name, tool, MAX_STR_LEN - 1);
    strncpy((*history)[*count].details, details, MAX_STR_LEN - 1);
    strncpy((*history)[*count].result_str, result_str, MAX_STR_LEN - 1); // Save string
    
    (*history)[*count].tool_name[MAX_STR_LEN - 1] = '\0';
    (*history)[*count].details[MAX_STR_LEN - 1] = '\0';
    (*history)[*count].result_str[MAX_STR_LEN - 1] = '\0';
    
    *count = new_count;
    printf("[Record added to history]\n");
}

// [NEW Helper] Vertical Strip Chart Plotter
// Layout: [Time] | [Visual Graph bar] | [Exact Value]
static void plot_vertical_strip_chart(double *data, int total_steps, double t_total, const char *title, const char *unit) {
    int display_rows = 25; // Limit height to fit on screen
    int step_stride = total_steps / display_rows; 
    if (step_stride < 1) step_stride = 1;

    // 1. Calculate Min/Max for Scaling
    double min_val = data[0], max_val = data[0];
    for (int i = 0; i < total_steps; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }
    double range = max_val - min_val;
    if (fabs(range) < 1e-9) range = 1.0; // Prevent divide by zero for flat lines

    int graph_width = 40; // Width of the visual bar area

    printf("\n=== %s ===\n", title);
    printf(" %-9s | %-40s | %-15s\n", "Time", "Waveform (Min->Max)", "Exact Value");
    printf("-----------|------------------------------------------|-----------------\n");

    for (int i = 0; i < total_steps; i += step_stride) {
        if (i >= total_steps) break;
        
        double current_val = data[i];
        double t_current = (double)i / total_steps * t_total;

        // Calculate position (0 to graph_width)
        int pos = (int)((current_val - min_val) / range * graph_width);
        if (pos < 0) pos = 0;
        if (pos >= graph_width) pos = graph_width - 1;

        // Print Time
        printf(" %6.2f ms | ", t_current * 1000.0);

        // Print Graph Bar
        for (int k = 0; k < graph_width; k++) {
            if (k == pos) printf("O");      // The data point
            else if (k < pos) printf("-");  // Fill bar
            else printf(" ");               // Empty space
        }

        // Print Exact Value on the Right
        // Determine precision based on magnitude
        if (fabs(current_val) < 0.001 && current_val != 0)
            printf(" | %.3e %s\n", current_val, unit);
        else
            printf(" | %8.4f %s\n", current_val, unit);
    }
    printf("-----------|------------------------------------------|-----------------\n");
    printf(" Range: [%.4e] to [%.4e] %s\n", min_val, max_val, unit);
}

// ============================================
// Public Function Implementations
// ============================================

void free_history_memory(CalcRecord *history) {
    if (history != NULL) free(history);
}

// --- Item 1: 4-Band Resistor Decoder & Encoder ---
void menu_item_1(CalcRecord **history, int *count) {
    printf("\n>> 4-Band Resistor Tool\n");
    printf("1. Colour Bands  -> Resistance\n");
    printf("2. Resistance    -> Colour Bands (nearest E24)\n");
    
    int mode = get_menu_selection("Select mode (1-2)", 1, 2);

    if (mode == 1) {
        // ========= 色环 -> 电阻 =========
        printf("\n>> Resistor Colour Code Decoder (4-Band)\n");
        printf("Colour Codes: 0:Blk 1:Brn 2:Red 3:Org 4:Yel 5:Grn 6:Blu 7:Vio 8:Gry 9:Wht\n");
        int b1 = get_menu_selection("Band 1 Digit (0-9)", 0, 9);
        int b2 = get_menu_selection("Band 2 Digit (0-9)", 0, 9);
        printf("Mults: 0:x1 1:x10 2:x100 3:x1k 4:x10k 5:x100k 6:x1M\n");
        int mult_idx = get_menu_selection("Multiplier Index (0-6)", 0, 6);
        
        char tol_str[20] = "5%";
        double final_R = ((b1 * 10) + b2) * pow(10, mult_idx);
        printf("\n>>> Result: Resistance = %.2f Ohms (+/- %s)\n", final_R, tol_str);
        
        g_wb_resistor = final_R;
        printf("(Workbench resistor updated to %.2fR)\n", g_wb_resistor);

        // ====== 这里开始：生成完整色环字符串并写到 history ======
        char colour_code_str[MAX_STR_LEN];
        snprintf(colour_code_str, MAX_STR_LEN, "%s-%s-%s-Gold",
                 COLOUR_DIGITS[b1],
                 COLOUR_DIGITS[b2],
                 COLOUR_MULTIPLIERS[mult_idx]);

        char details[MAX_STR_LEN];
        snprintf(details, MAX_STR_LEN,
                 "Bands=%s (b1=%d, b2=%d, mult=10^%d)",
                 colour_code_str, b1, b2, mult_idx);
        
        char result_str[MAX_STR_LEN];
        char fmt_res[32]; 
        format_eng(final_R, fmt_res);
        // 在结果里也带上完整色环字符串
        snprintf(result_str, MAX_STR_LEN,
                 "%sOhms +/- %s [%s]", fmt_res, tol_str, colour_code_str);

        add_record_to_history(history, count, "4-Band Decode", details, result_str);
    } 
    else {
        // ========= 电阻值 -> 最近 E24 -> 色环 =========
        printf("\n>> Resistance -> 4-Band Colour (Nearest E24)\n");
        printf("Enter a resistor value (supports p/n/u/m/k/M/G suffixes, e.g. 4.7k, 220, 1M)\n");

        double target_r = get_eng_input_with_default("Target Resistance", &g_wb_resistor, 1);
        if (target_r <= 0.0) {
            printf("Error: resistance must be positive.\n");
            return;
        }

        // 先近似到 E24
        double r_e24 = find_closest_e24_resistor(target_r);
        g_wb_resistor = r_e24; // workbench 也更新一下

        char fmt_in[32], fmt_e24[32];
        format_eng(target_r, fmt_in);
        format_eng(r_e24, fmt_e24);

        printf("\n>>> Nearest E24 Standard Value: %sOhms\n", fmt_e24);

        // 再把 E24 值转换为 4 环色环
        int d1, d2, mult_idx;
        if (!resistor_to_bands(r_e24, &d1, &d2, &mult_idx)) {
            printf("Sorry, %.3g Ohms is outside the supported 4-band range (approx 10Ω to 9.9MΩ).\n", r_e24);
            return;
        }

        printf("\n4-Band Code (assume 5%% tolerance / Gold):\n");
        printf("  Band 1 (1st digit): %d (%s)\n", d1, COLOUR_DIGITS[d1]);
        printf("  Band 2 (2nd digit): %d (%s)\n", d2, COLOUR_DIGITS[d2]);
        printf("  Band 3 (Multiplier): x10^%d (%s)\n", mult_idx, COLOUR_MULTIPLIERS[mult_idx]);
        printf("  Band 4 (Tolerance):  5%% (Gold)\n");

        // ====== 这里开始：为 encode 模式写完整色环字符串到 history ======
        char colour_code_str[MAX_STR_LEN];
        snprintf(colour_code_str, MAX_STR_LEN, "%s-%s-%s-Gold",
                 COLOUR_DIGITS[d1],
                 COLOUR_DIGITS[d2],
                 COLOUR_MULTIPLIERS[mult_idx]);

        char details[MAX_STR_LEN];
        snprintf(details, MAX_STR_LEN,
                 "Req=%sOhms,E24=%sOhms,Bands=%s",
                 fmt_in, fmt_e24, colour_code_str);

        char result_str[MAX_STR_LEN];
        snprintf(result_str, MAX_STR_LEN,
                 "%sOhms -> %s (5%%, Gold)",
                 fmt_e24, colour_code_str);

        add_record_to_history(history, count, "4-Band Encode", details, result_str);
    }
}


// --- Item 2: Ohm's Law ---
void menu_item_2(CalcRecord **history, int *count) {
    printf("\n>> Ohm's Law & Power (Interconnected)\n");
    printf("1.V=IR  2.I=V/R  3.R=V/I  4.P=VI\n");
    int mode = get_menu_selection("Selection (1-4)", 1, 4);
    double v, i, r, res_val; 
    char details[MAX_STR_LEN]; char tool[30];
    char fmt_res[32]; char unit[8];

    switch (mode) {
        case 1: // V=IR
            i = get_eng_input_with_default("Current I (Amps)", &g_wb_current, 1);
            r = get_standard_resistor_input("R", g_wb_resistor); 
            res_val = i * r;
            strcpy(tool,"Ohm's Law (V)"); snprintf(details,MAX_STR_LEN,"I=%.3eA, R=%.1eR",i,r);
            strcpy(unit, "V");
            g_wb_voltage = res_val;
            break;
        case 2: // I=V/R
            v = get_eng_input_with_default("Voltage V", &g_wb_voltage, 0);
            r = get_standard_resistor_input("R", g_wb_resistor);
            res_val = v/r;
            strcpy(tool,"Ohm's Law (I)"); snprintf(details,MAX_STR_LEN,"V=%.2fV, R=%.1eR",v,r);
            strcpy(unit, "A");
            g_wb_current = res_val;
            break;
        case 3: // R=V/I
            v = get_eng_input_with_default("Voltage V", &g_wb_voltage, 0);
            i = get_eng_input_with_default("Current I (Amps)", &g_wb_current, 1);
            if (i == 0) { printf("Error: Current cannot be zero.\n"); return; }
            res_val = v/i;
            strcpy(tool,"Ohm's Law (R)"); snprintf(details,MAX_STR_LEN,"V=%.2fV, I=%.3eA",v,i);
            strcpy(unit, "Ohms");
            g_wb_resistor = res_val;
            break;
        case 4: // P=VI
            v = get_eng_input_with_default("Voltage V", &g_wb_voltage, 0);
            i = get_eng_input_with_default("Current I (Amps)", &g_wb_current, 1);
            res_val = v * i;
            strcpy(tool,"Power Calc (P)"); snprintf(details,MAX_STR_LEN,"V=%.2fV, I=%.3eA",v,i);
            strcpy(unit, "W");
            break;
        default: return;
    }

    format_eng(res_val, fmt_res);
    printf("\n>>> Result: %s %s\n", fmt_res, unit);

    if(mode==1) printf("(Workbench Voltage updated)\n");
    if(mode==2) printf("(Workbench Current updated)\n");
    if(mode==3) printf("(Workbench Resistor updated)\n");
    
    char result_str[MAX_STR_LEN];
    snprintf(result_str, MAX_STR_LEN, "%s %s", fmt_res, unit);

    add_record_to_history(history, count, tool, details, result_str);
}

// --- Item 3: Voltage Divider ---
void menu_item_3(CalcRecord **history, int *count) {
    printf("\n>> Voltage Divider\n");
    double vin = get_eng_input_with_default("Input Voltage Vin", &g_wb_voltage, 0);
    double r1 = get_standard_resistor_input("Top Resistor R1", g_wb_resistor);
    double r2 = get_standard_resistor_input("Bottom Resistor R2", g_wb_resistor);

    if ((r1+r2) == 0) return;
    double vout = vin * (r2 / (r1 + r2));
    printf("\n>>> Result: Vout = %.4f V\n", vout);

    char details[MAX_STR_LEN]; snprintf(details, MAX_STR_LEN, "Vin=%.2fV, R1=%.1eR, R2=%.1eR", vin, r1, r2);
    char result_str[MAX_STR_LEN]; snprintf(result_str, MAX_STR_LEN, "Vout=%.4f V", vout);
    
    add_record_to_history(history, count, "Voltage Divider", details, result_str);
}

// --- Item 4: Universal RLC Transient Analyser (Vertical Detail Mode) ---
void menu_item_4(CalcRecord **history, int *count) {
    printf("\n>> RLC Transient Analyser (Vertical Detail Mode)\n");
    printf("1. RC (Resistor-Capacitor)\n");
    printf("2. RL (Resistor-Inductor)\n");
    printf("3. LC (Inductor-Capacitor)\n");
    printf("4. RLC (Series Resistor-Inductor-Capacitor)\n");
    int type = get_menu_selection("Select Circuit Type", 1, 4);

    // --- 1. Inputs ---
    double vs = get_eng_input_with_default("Step Input Voltage Vs", &g_wb_voltage, 0);
    double r = 0, l = 0, c = 0;

    if (type != 3) r = get_standard_resistor_input("Series Resistor R", g_wb_resistor);
    else { r = 0.1; printf("[Info] LC: Using 0.1 Ohm internal resistance.\n"); }

    if (type != 1) l = get_eng_input_with_default("Inductance L", &g_wb_inductor, 1);
    if (type != 2) c = get_eng_input_with_default("Capacitance C", &g_wb_capacitor, 1);

    // Safety
    if (type == 1 && c <= 0) c = 1e-6;
    if ((type != 1) && l <= 0) l = 1e-3;

    // --- 2. Auto-Time Calculation ---
    double t_total = 0;
    if (type == 1) t_total = 5.0 * r * c; // RC
    else if (type == 2) t_total = 5.0 * (l / r); // RL
    else if (type == 3) t_total = 3.0 * (1.0 / (1.0/(2*M_PI*sqrt(l*c)))); // LC (3 periods)
    else if (type == 4) {
        // RLC Auto-detection
        double alpha = r / (2.0 * l);
        double omega0 = 1.0 / sqrt(l * c);
        if (alpha < omega0) {
            // Underdamped: ensure we see oscillations
            t_total = 10.0 * (2*M_PI/omega0); 
            // Cap at reasonable decay time
            double t_decay = 5.0 / alpha;
            if (t_total > t_decay) t_total = t_decay;
        } else {
            // Overdamped/Crit: settle time
            t_total = 10.0 / alpha; 
        }
    }
    t_total = get_eng_input_with_default("Total Simulation Time", &t_total, 1);

    // --- 3. High-Res Simulation ---
    // We simulate at high resolution, then the plotter downsamples for display
    #define SIM_STEPS 1000 
    double dt = t_total / SIM_STEPS;
    
    double *data_vc = malloc(SIM_STEPS * sizeof(double));
    double *data_il = malloc(SIM_STEPS * sizeof(double));
    double *data_ec = malloc(SIM_STEPS * sizeof(double));
    double *data_el = malloc(SIM_STEPS * sizeof(double));

    if (!data_vc || !data_il || !data_ec || !data_el) {
        printf("Memory Error.\n"); return;
    }

    double vc = 0, il = 0;
    // Track stats for history
    double max_vc = 0, max_il = 0;
    double max_ec = 0, max_el = 0; // Track max energy stored in each component

    printf("\nComputing %d steps...\n", SIM_STEPS);

    for (int i = 0; i < SIM_STEPS; i++) {
        // Store Snapshot
        data_vc[i] = vc;
        data_il[i] = il;
        data_ec[i] = (type == 2) ? 0 : 0.5 * c * vc * vc;
        data_el[i] = (type == 1) ? 0 : 0.5 * l * il * il;
        
        // Track Peaks
        if (fabs(vc) > max_vc) max_vc = fabs(vc);
        if (fabs(il) > max_il) max_il = fabs(il);
        if (data_ec[i] > max_ec) max_ec = data_ec[i];
        if (data_el[i] > max_el) max_el = data_el[i];

        // Euler Integration (Physics)
        for(int k=0; k<10; k++) {
            double loop_dt = dt / 10.0;
            double v_r = il * r;
            double v_c = (type == 2) ? 0 : vc;
            double v_l = vs - v_r - v_c;

            if (type == 1) { // RC
                il = (vs - vc) / r;
                vc += (il / c) * loop_dt;
            } else { // RL, LC, RLC
                double d_il = v_l / l;
                il += d_il * loop_dt;
                if (type != 2) vc += (il / c) * loop_dt;
            }
        }
    }

    // --- 4. Vertical Plotting with Values ---
    // Plot 1: Loop Current (All types have current)
    plot_vertical_strip_chart(data_il, SIM_STEPS, t_total, "Loop Current I(t)", "A");

    // Plot 2: Capacitor Voltage (if C exists)
    if (type != 2) 
        plot_vertical_strip_chart(data_vc, SIM_STEPS, t_total, "Capacitor Voltage Vc(t)", "V");

    // Plot 3: Energy Analysis
    if (type != 2)
        plot_vertical_strip_chart(data_ec, SIM_STEPS, t_total, "Stored Energy: Capacitor", "J");
    
    if (type != 1)
        plot_vertical_strip_chart(data_el, SIM_STEPS, t_total, "Stored Energy: Inductor", "J");

    // Summary for Console
    double final_energy = data_ec[SIM_STEPS-1] + data_el[SIM_STEPS-1];
    printf("\n[Result] Final Total Energy: %.4e J\n", final_energy);

    // --- 5. Save History (Intelligent Logic) ---
    char details[MAX_STR_LEN];
    snprintf(details, MAX_STR_LEN, "RLC Type %d, Vs=%.1fV", type, vs);
    
    // Create intelligent result string that distinguishes components
    char result_str[MAX_STR_LEN];
    
    if (type == 1) { 
        // RC: Only Capacitor energy matters
        snprintf(result_str, MAX_STR_LEN, "PkV:%.1fV Ec:%.2eJ", max_vc, max_ec);
    } else if (type == 2) { 
        // RL: Only Inductor energy matters
        snprintf(result_str, MAX_STR_LEN, "PkI:%.2eA El:%.2eJ", max_il, max_el);
    } else { 
        // LC/RLC: Show BOTH Capacitor and Inductor Energy clearly
        snprintf(result_str, MAX_STR_LEN, "Ec:%.2eJ El:%.2eJ", max_ec, max_el);
    }

    add_record_to_history(history, count, "RLC Analyser", details, result_str);

    // Cleanup
    free(data_vc); free(data_il); free(data_ec); free(data_el);
}

// --- Item 5: LED Calculator (Automated) ---
void menu_item_5(CalcRecord **history, int *count) {
    printf("\n>> LED Resistor Calc (Automatic E24 Selection)\n");
    // 1. Get Inputs
    double vs = get_eng_input_with_default("Supply Voltage Vs", &g_wb_voltage, 0);
    double vf = get_eng_input_with_default("LED Forward Voltage Vf", &g_wb_vf, 0);
    double target_i = get_eng_input_with_default("Target LED Current", &g_wb_current, 1);

    // Basic Validation
    if (vf >= vs) {
        printf("Error: Supply voltage must be greater than LED forward voltage.\n"); return;
    }
    if (target_i <= 0) {
        printf("Error: Target current must be positive.\n"); return;
    }

    // 2. Calculate Ideal Resistance
    double r_ideal = (vs - vf) / target_i;

    // 3. Find Nearest E24 Standard Resistor (Automatic)
    double r_standard = find_closest_e24_resistor(r_ideal);

    // 4. Re-calculate Actual Current with Standard Resistor
    double i_actual = (vs - vf) / r_standard;

    // 5. Output Results
    printf("\n>>> Results:\n");
    printf("-----------------------------------------------------\n");
    char fmt_buf[32];
    
    format_eng(r_ideal, fmt_buf);
    printf("Theoretical Ideal Resistor: %sOhms\n", fmt_buf);
    
    format_eng(r_standard, fmt_buf);
    printf("Nearest Standard E24 Value: %sOhms  <-- Recommended\n", fmt_buf);
    
    format_eng(i_actual, fmt_buf);
    printf("Actual Current with E24 R : %sA\n", fmt_buf);
    printf("-----------------------------------------------------\n");

    // Update workbench variables with practical values
    g_wb_resistor = r_standard;
    g_wb_current = i_actual;
    printf("(Workbench set to R=%.2e, I=%.2e)\n", r_standard, i_actual);

    // 6. Save to History
    char details[MAX_STR_LEN];
    // Format history detail to show inputs and practical outputs
    snprintf(details, MAX_STR_LEN, "Vs=%.1fV,Vf=%.1fV->Rstd=%.2eR", vs, vf, r_standard);
    
    char result_str[MAX_STR_LEN];
    char s_r[32], s_i[32]; format_eng(r_standard, s_r); format_eng(i_actual, s_i);
    snprintf(result_str, MAX_STR_LEN, "R_std=%s, I_act=%sA", s_r, s_i);
    
    add_record_to_history(history, count, "LED Resistor Calc", details, result_str);
}

// --- Item 6: Op-Amp Gain Designer (Replaces Cap Energy) ---
void menu_item_6(CalcRecord **history, int *count) {
    printf("\n>> Op-Amp Gain Designer (Non-Inv & Inverting)\n");
    printf("1. Non-Inverting Amplifier (Gain = 1 + R2/R1)\n");
    printf("2. Inverting Amplifier     (Gain = - R2/R1)\n");
    int mode = get_menu_selection("Mode", 1, 2);

    double target_gain = get_eng_input_with_default("Target Gain (magnitude)", NULL, 0);
    if (target_gain < 1.0 && mode == 1) {
        printf("Error: Non-inverting gain must be >= 1.\n"); return;
    }

    // Ask for one resistor to fix, or iterate best match?
    // Let's iterate E24 logic: Fix R1 (Standard), calculate ideal R2, find closest Standard R2.
    // We try a few standard R1 bases to find the best pair.
    
    printf("\ncalculating best E24 resistor pairs...\n");
    printf("--------------------------------------------------\n");
    printf("| %-9s | %-9s | %-10s | %-8s |\n", "Fix R1", "Calc R2", "Std R2", "Error %");
    printf("--------------------------------------------------\n");

    double best_error = 100.0;
    double best_r1 = 0, best_r2 = 0, best_act_gain = 0;

    // We will test R1 values from 1k to 100k (common range) from the E24 array
    // E24_BASE has values 1.0 to 9.1. We multiply by 1k, 10k, 100k.
    double multipliers[] = {1000.0, 10000.0, 100000.0};
    
    for (int m = 0; m < 3; m++) {
        for (int i = 0; i < E24_COUNT; i++) {
            double r1 = E24_BASE[i] * multipliers[m];
            
            // Calculate ideal R2 based on gain formula
            double r2_ideal = 0;
            if (mode == 1) r2_ideal = r1 * (target_gain - 1.0); // G = 1 + R2/R1 -> R2 = R1(G-1)
            else           r2_ideal = r1 * target_gain;         // G = R2/R1     -> R2 = R1*G
            
            if (r2_ideal <= 0) continue; // Gain=1 case for Non-Inv means R2=0 (Wire)

            // Find closest E24 for R2
            double r2_std = find_closest_e24_resistor(r2_ideal);
            
            // Calc actual gain
            double act_gain = (mode == 1) ? (1.0 + r2_std/r1) : (r2_std/r1);
            double error = fabs((act_gain - target_gain) / target_gain) * 100.0;

            // Print top 5 reasonable matches or if error is very low
            if (error < 2.0 && best_error > 0.01) { // Just show a few good ones
                char s_r1[16], s_r2[16];
                format_eng(r1, s_r1); format_eng(r2_std, s_r2);
                printf("| %-9s | %-9.2f | %-10s | %5.2f%%   |\n", s_r1, r2_ideal, s_r2, error);
            }

            if (error < best_error) {
                best_error = error;
                best_r1 = r1;
                best_r2 = r2_std;
                best_act_gain = act_gain;
            }
        }
    }
    printf("--------------------------------------------------\n");
    
    char s_r1[32], s_r2[32];
    format_eng(best_r1, s_r1); format_eng(best_r2, s_r2);

    printf("\n>>> Best Recommendation:\n");
    printf("    R1 = %sOhms\n", s_r1);
    printf("    R2 = %sOhms\n", s_r2);
    printf("    Actual Gain = %.4f (Error: %.3f%%)\n", best_act_gain, best_error);

    // Update Workbench
    g_wb_resistor = best_r1; // Set R1 as default for next operations
    printf("(Workbench R set to R1: %s)\n", s_r1);

    char details[MAX_STR_LEN];
    snprintf(details, MAX_STR_LEN, "%s, Tgt G=%.2f", 
             (mode==1?"Non-Inv":"Inv"), target_gain);
    
    char result_str[MAX_STR_LEN];
    snprintf(result_str, MAX_STR_LEN, "R1=%s, R2=%s, G=%.2f", s_r1, s_r2, best_act_gain);

    add_record_to_history(history, count, "Op-Amp Designer", details, result_str);
}

// --- Item 7: History View/Save ---
void menu_item_7(CalcRecord **history, int *count) {
    printf("\n>> View/Save Calculation History\n---------------------------------\n");
    if (*count == 0) { printf("History is empty.\n"); return; }
    
    // [UPDATED] Table header for string results
    printf("%-3s | %-22s | %-25s | %-35s\n", "ID", "Tool Name", "Inputs", "Results");
    printf("--------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < *count; i++) {
        printf("#%-2d | %-22s | %-25s | %-35s\n", 
            i + 1, 
            (*history)[i].tool_name, 
            (*history)[i].details, 
            (*history)[i].result_str); // Print string instead of double
    }
    printf("--------------------------------------------------------------------------------------------\n");
    
    printf("\nSave to CSV file? (y/n): "); char buf[10]; fgets(buf, sizeof(buf), stdin);
    if (tolower(buf[0]) == 'y') {
        char fname[128]; 
        printf("Enter filename (e.g. result1): ");
        fgets(fname, sizeof(fname), stdin); 
        fname[strcspn(fname, "\r\n")] = '\0'; // strip newline
        
        if (strlen(fname) == 0) return;

        // [NEW] Automatic .csv extension logic
        int len = strlen(fname);
        const char *ext = ".csv";
        int ext_len = 4;
        
        // Check if string already ends with .csv
        int needs_ext = 1;
        if (len >= ext_len) {
            if (strcmp(fname + len - ext_len, ext) == 0) {
                needs_ext = 0;
            }
        }
        
        // Append if missing
        if (needs_ext) {
            if (len + ext_len < sizeof(fname)) {
                strcat(fname, ext);
            } else {
                printf("Filename too long to append extension.\n"); return;
            }
        }

        FILE *fp = fopen(fname, "w"); 
        if (!fp) { printf("Error opening file '%s'.\n", fname); return; }
        
        fprintf(fp, "Tool Name,Inputs,Results\n");
        for (int i = 0; i < *count; i++) 
            fprintf(fp, "%s,%s,%s\n", (*history)[i].tool_name, (*history)[i].details, (*history)[i].result_str);
        
        fclose(fp); printf("Saved to '%s'.\n", fname);
    }
}
