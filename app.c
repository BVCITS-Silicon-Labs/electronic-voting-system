#include "app.h"
#include "sl_si91x_button.h"

/*
 * The generated button configuration may reference the legacy UULP port
 * symbol, which is absent from newer SDK headers.
 */
#ifndef SL_GPIO_PORT_UULP
#define SL_GPIO_PORT_UULP 6U
#endif

#include "sl_si91x_button_init_btn0_config.h"
#include "sl_si91x_button_init_btn1_config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * The generated button configuration may use the legacy UULP port name.
 */
#ifndef UULP_VBAT
#define UULP_VBAT SL_GPIO_PORT_UULP
#endif

/*
 * The generated button configuration may use the legacy HP port name.
 */
#ifndef SL_GPIO_PORT_HP
#define SL_GPIO_PORT_HP 0U
#endif

#ifndef HP
#define HP SL_GPIO_PORT_HP
#endif


/**************************************************************************
 * SMART VOTING SYSTEM
 *
 * BTN0 -> Candidate A
 * BTN1 -> Candidate B
 *
 * Total votes required = 5
 **************************************************************************/

#define TOTAL_VOTES             5U


/**************************************************************************
 * Button 0 - Candidate A
 **************************************************************************/

#define BTN0_PIN                SL_BUTTON_BTN0_PIN
#define BTN0_PORT               SL_BUTTON_BTN0_PORT
#define BTN0_NUMBER             0U


/**************************************************************************
 * Button 1 - Candidate B
 **************************************************************************/

#define BTN1_PIN                SL_BUTTON_BTN1_PIN
#define BTN1_PORT               SL_BUTTON_BTN1_PORT
#define BTN1_NUMBER             1U


/**************************************************************************
 * Vote variables
 **************************************************************************/

static volatile uint8_t btn0_event = 0;
static volatile uint8_t btn1_event = 0;

static uint32_t candidate_a_votes = 0;
static uint32_t candidate_b_votes = 0;
static uint32_t total_votes = 0;

static bool voting_active = true;


/*
 * IMPORTANT:
 *
 * During button initialization, some Si91x button configurations may
 * generate an interrupt automatically.
 *
 * This flag prevents that initialization interrupt from being counted
 * as an actual vote.
 */
static volatile bool button_initialization_done = false;


/**************************************************************************
 * Button objects
 **************************************************************************/

static const sl_button_t button0 = {
    .port = BTN0_PORT,
    .pin = BTN0_PIN,
    .button_number = BTN0_NUMBER,
    .interrupt_config = RISE_EDGE_AND_FALL_EDGE_INTERRUPT
};

static const sl_button_t button1 = {
    .port = BTN1_PORT,
    .pin = BTN1_PIN,
    .button_number = BTN1_NUMBER,
    .interrupt_config = RISE_EDGE_AND_FALL_EDGE_INTERRUPT
};


/**************************************************************************
 * Manual button initialization
 **************************************************************************/

static void buttons_manual_init(void)
{
    sl_si91x_button_init(&button0);
    sl_si91x_button_init(&button1);
}


/**************************************************************************
 * Print system header
 **************************************************************************/

static void print_start_message(void)
{
    printf("\r\n");
    printf("==================================================\r\n");
    printf("              SMART VOTING SYSTEM\r\n");
    printf("==================================================\r\n");
    printf("\r\n");

    printf("  BTN0  ->  CANDIDATE A\r\n");
    printf("  BTN1  ->  CANDIDATE B\r\n");

    printf("\r\n");
    printf("  TOTAL VOTES REQUIRED : %u\r\n", TOTAL_VOTES);
    printf("  Press a button to cast your vote.\r\n");

    printf("\r\n");
    printf("--------------------------------------------------\r\n");
}


/**************************************************************************
 * Print live voting result
 **************************************************************************/

static void print_live_result(void)
{
    printf("\r\n");

    printf("============================================\r\n");
    printf("           SMART VOTING SYSTEM\r\n");
    printf("============================================\r\n");

    printf("   Candidate A : %lu vote(s)\r\n",
           (unsigned long)candidate_a_votes);

    printf("   Candidate B : %lu vote(s)\r\n",
           (unsigned long)candidate_b_votes);

    printf("--------------------------------------------\r\n");

    printf("   VOTES CAST  : %lu / %u\r\n",
           (unsigned long)total_votes,
           TOTAL_VOTES);

    printf("   VOTES LEFT  : %lu\r\n",
           (unsigned long)(TOTAL_VOTES - total_votes));

    printf("--------------------------------------------\r\n");

    if (candidate_a_votes > candidate_b_votes)
    {
        printf("   CURRENT LEADER : CANDIDATE A\r\n");
    }
    else if (candidate_b_votes > candidate_a_votes)
    {
        printf("   CURRENT LEADER : CANDIDATE B\r\n");
    }
    else
    {
        printf("   CURRENT LEADER : TIE\r\n");
    }

    printf("============================================\r\n");
}


/**************************************************************************
 * Print final voting result
 **************************************************************************/

static void print_final_result(void)
{
    printf("\r\n\r\n");

    printf("**************************************************\r\n");
    printf("              VOTING COMPLETED\r\n");
    printf("**************************************************\r\n");

    printf("\r\n");
    printf("              FINAL RESULT\r\n");
    printf("\r\n");

    printf("              CANDIDATE A : %lu VOTES\r\n",
           (unsigned long)candidate_a_votes);

    printf("              CANDIDATE B : %lu VOTES\r\n",
           (unsigned long)candidate_b_votes);

    printf("\r\n");

    printf("              TOTAL       : %lu / %u\r\n",
           (unsigned long)total_votes,
           TOTAL_VOTES);

    printf("--------------------------------------------------\r\n");
    printf("\r\n");

    if (candidate_a_votes > candidate_b_votes)
    {
        printf("              WINNER\r\n");
        printf("              CANDIDATE A\r\n");
    }
    else if (candidate_b_votes > candidate_a_votes)
    {
        printf("              WINNER\r\n");
        printf("              CANDIDATE B\r\n");
    }
    else
    {
        printf("              RESULT\r\n");
        printf("              DRAW\r\n");
    }

    printf("\r\n");

    printf("              CONGRATULATIONS!\r\n");

    printf("\r\n");
    printf("**************************************************\r\n");
    printf("              VOTING CLOSED\r\n");
    printf("**************************************************\r\n");

    printf("\r\n");
}


/**************************************************************************
 * Button interrupt callback
 *
 * Only record the event here.
 *
 * IMPORTANT:
 * Any interrupt occurring during initialization is ignored.
 * Therefore, the initial vote count remains:
 *
 * Candidate A : 0
 * Candidate B : 0
 * Total       : 0 / 5
 **************************************************************************/

void sl_si91x_button_isr(uint8_t pin, int8_t state)
{
    /*
     * Ignore any interrupt generated during button initialization.
     */
    if (!button_initialization_done)
    {
        return;
    }

    /*
     * Only react to button press.
     */
    if (state != BUTTON_PRESSED)
    {
        return;
    }

    /*
     * BTN0 -> Candidate A
     */
    if (pin == BTN0_PIN)
    {
        btn0_event = 1;
    }

    /*
     * BTN1 -> Candidate B
     */
    else if (pin == BTN1_PIN)
    {
        btn1_event = 1;
    }
}


/**************************************************************************
 * Application initialization
 **************************************************************************/

void app_init(void)
{
    /*
     * Disable logical button event processing while the buttons
     * are being initialized.
     */
    button_initialization_done = false;


    /*
     * Initialize physical buttons.
     */
    buttons_manual_init();


    /*
     * Clear any interrupt events generated during initialization.
     *
     * This is important because a startup interrupt must NOT become
     * a vote.
     */
    btn0_event = 0;
    btn1_event = 0;


    /*
     * Explicitly reset all voting variables.
     */
    candidate_a_votes = 0;
    candidate_b_votes = 0;
    total_votes = 0;

    voting_active = true;


    /*
     * Initialization is now complete.
     *
     * From this point onwards, only real button presses are accepted.
     */
    button_initialization_done = true;


    /*
     * Display startup information.
     */
    printf("\r\n\r\n");

    print_start_message();

    printf("\r\n");

    /*
     * Explicitly show initial vote count.
     */
    printf("  CANDIDATE A : 0 vote(s)\r\n");
    printf("  CANDIDATE B : 0 vote(s)\r\n");
    printf("  VOTES CAST  : 0 / %u\r\n", TOTAL_VOTES);

    printf("\r\n");
    printf("  WAITING FOR VOTES...\r\n");
    printf("\r\n");
}


/**************************************************************************
 * Application process
 **************************************************************************/

void app_process_action(void)
{
    /*
     * Once 5 votes have been received,
     * completely stop accepting new votes.
     */
    if (!voting_active)
    {
        return;
    }


    /**********************************************************************
     * Candidate A - BTN0
     **********************************************************************/

    if (btn0_event)
    {
        /*
         * Clear event first.
         */
        btn0_event = 0;


        /*
         * Make sure voting is still active.
         */
        if (!voting_active)
        {
            return;
        }


        /*
         * Check total vote limit.
         */
        if (total_votes < TOTAL_VOTES)
        {
            /*
             * Register Candidate A vote.
             */
            candidate_a_votes++;
            total_votes++;


            printf("\r\n");
            printf(">>> VOTE REGISTERED: CANDIDATE A\r\n");


            /*
             * Display current result.
             */
            print_live_result();


            /*
             * Check if voting is complete.
             */
            if (total_votes >= TOTAL_VOTES)
            {
                voting_active = false;

                print_final_result();
            }
        }
    }


    /**********************************************************************
     * Candidate B - BTN1
     **********************************************************************/

    if (btn1_event)
    {
        /*
         * Clear event first.
         */
        btn1_event = 0;


        /*
         * Make sure voting is still active.
         */
        if (!voting_active)
        {
            return;
        }


        /*
         * Check total vote limit.
         */
        if (total_votes < TOTAL_VOTES)
        {
            /*
             * Register Candidate B vote.
             */
            candidate_b_votes++;
            total_votes++;


            printf("\r\n");
            printf(">>> VOTE REGISTERED: CANDIDATE B\r\n");


            /*
             * Display current result.
             */
            print_live_result();


            /*
             * Check if voting is complete.
             */
            if (total_votes >= TOTAL_VOTES)
            {
                voting_active = false;

                print_final_result();
            }
        }
    }
}