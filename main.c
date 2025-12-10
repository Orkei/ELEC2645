#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "funcs.h"

/* Prototypes with updated signatures */
static void main_menu(CalcRecord **history, int *count);
static void print_main_menu(void);
static int  get_user_input(void);
static void select_menu_item(int input, CalcRecord **history, int *count);
static void go_back_to_main(void);
static int  is_integer(const char *s);

int main(void)
{
    // === PROGRAM STATE INITIALIZATION ===
    // Initialize pointer to NULL, indicating no memory allocated yet.
    CalcRecord *history_array = NULL; 
    // Initialize counter to 0 records.
    int history_count = 0;

    /* this will run forever until exit(0) is called */
    for(;;) {
        // Pass addresses of our state variables so functions can modify them
        main_menu(&history_array, &history_count);
    }

    /* NOT REACHED in this design, but good practice for robustness */
    free_history_memory(history_array);
    return 0;
}

static void main_menu(CalcRecord **history, int *count)
{
    print_main_menu();
    {
        int input = get_user_input();
        select_menu_item(input, history, count);
    }
}

static int get_user_input(void)
{
    enum { MENU_ITEMS = 8 };    
    char buf[128];
    int valid_input = 0;
    int value = 0;

    do {
        printf("\nSelect item (1-8): ");
        if (!fgets(buf, sizeof(buf), stdin)) {
            puts("\nInput error. Exiting.");
            exit(1);
        }
        buf[strcspn(buf, "\r\n")] = '\0';

        if (!is_integer(buf)) {
            printf("Enter an integer!\n");
            valid_input = 0;
        } else {
            value = (int)strtol(buf, NULL, 10);
            if (value >= 1 && value <= MENU_ITEMS) {
                valid_input = 1;
            } else {
                printf("Invalid menu item! Please enter 1-%d.\n", MENU_ITEMS);
                valid_input = 0;
            }
        }
    } while (!valid_input);

    return value;
}

static void select_menu_item(int input, CalcRecord **history, int *count)
{
    // Pass state variables to appropriate functions
    switch (input) {
        case 1: menu_item_1(history, count); go_back_to_main(); break;
        case 2: menu_item_2(history, count); go_back_to_main(); break;
        case 3: menu_item_3(history, count); go_back_to_main(); break;
        case 4: menu_item_4(history, count); go_back_to_main(); break;
        case 5: menu_item_5(history, count); go_back_to_main(); break;
        case 6: menu_item_6(history, count); go_back_to_main(); break;
        case 7: menu_item_7(history, count); go_back_to_main(); break;
        default: // Case 8: Exit
            printf("\nCleaning up memory...\n");
            // IMPOTANT: Free memory before exiting to prevent leaks
            free_history_memory(*history); // Dereference to get the actual array pointer
            printf("Exiting Embedded Electronics Assistant. Goodbye!\n");
            exit(0);
    }
}

static void print_main_menu(void)
{
    printf("\n=================================================\n");
    printf("   EMBEDDED ELECTRONICS ASSISTANT\n");
    printf("=================================================\n");
    printf("Please select a tool:\n\n"
           "\t1. Resistor Colour Code Decoder\n"
           "\t2. Ohm's Law & Power Calculator\n"
           "\t3. Voltage Divider Designer\n"
           "\t4. Universal RLC Transient Analyser\n" // Updated
           "\t5. LED Current-Limiting Resistor Calculator\n"
           "\t6. Op-Amp Gain Designer (E24 Matcher)\n" // Updated
           "\t7. View/Save Calculation History\n"
           "\n\t8. Exit Application\n");
    printf("=================================================\n");
}


static void go_back_to_main(void)
{
    char buf[64];
    do {
        printf("\nEnter 'b' or 'B' to go back to main menu: ");
        if (!fgets(buf, sizeof(buf), stdin)) {
            puts("\nInput error. Exiting.");
            exit(1);
        }
        buf[strcspn(buf, "\r\n")] = '\0'; /* strip newline */
    } while (!(buf[0] == 'b' || buf[0] == 'B') || buf[1] != '\0');
}

static int is_integer(const char *s)
{
    if (!s || !*s) return 0;
    if (*s == '+' || *s == '-') s++;
    if (!isdigit((unsigned char)*s)) return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}
