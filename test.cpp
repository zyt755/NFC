#include <string.h>
#include <stdio.h>
#include <iostream>
using namespace std;

#define MANCHESTER_GAP_DURATION                             4 // us 
#define MANCHESTER_GAP_WIDTH                                (MANCHESTER_GAP_DURATION)
#define MANCHESTER_GAP_MIN                                  (MANCHESTER_GAP_WIDTH - MANCHESTER_GAP_WIDTH/2)
#define MANCHESTER_GAP_MAX                                  (MANCHESTER_GAP_WIDTH + MANCHESTER_GAP_WIDTH/2 + 4)

#define MANCHESTER_GAP_LONG_DURATION                        8 // us
#define MANCHESTER_GAP_SHORT_DURATION                       4 // us
#define MANCHESTER_GAP_LONG_WIDTH                           (MANCHESTER_GAP_LONG_DURATION)                 
#define MANCHESTER_GAP_SHORT_WIDTH                          (MANCHESTER_GAP_SHORT_DURATION)
#define MANCHESTER_GAP_START_WIDTH_THRESHOLD_MIN            (MANCHESTER_GAP_LONG_WIDTH) 
#define MANCHESTER_GAP_START_WIDTH_THRESHOLD_MAX            (MANCHESTER_GAP_LONG_WIDTH) * 1.5 
#define MANCHESTER_GAP_LONG_WIDTH_THRESHOLD                 (MANCHESTER_GAP_LONG_WIDTH - MANCHESTER_GAP_LONG_WIDTH/8)
#define MANCHESTER_GAP_SHORT_WIDTH_THRESHOLD                (MANCHESTER_GAP_SHORT_WIDTH - MANCHESTER_GAP_SHORT_WIDTH/8)            
#define MANCHESTER_END                                      MANCHESTER_GAP_LONG_WIDTH * 1.5 

enum miller_state {
    WAIT_FOR_START,
    LAST_BIT_ZERO,
    LAST_BIT_ONE,
    END_OF_FRAME,
};

static enum miller_state current_state = WAIT_FOR_START;
static unsigned int count_one = 0;
static unsigned int count_zero = 0;
static unsigned int decoded_bit_num = 0;
static unsigned char current_frame[1000] = { 0 };

static unsigned char
compute_even_parity (unsigned char c)
{
    unsigned int i;
    unsigned char parity = 0;

    for (i = 0; i < 8; i++) {
        parity ^= (c & (0x1 << i)) >> i;
    }

    return parity;
}

static void
set_next_bit (unsigned char bit)
{
    if (bit) {
            /* 1 */
        current_frame[decoded_bit_num] = 1;
    } else {
            /* 0 */
        current_frame[decoded_bit_num] = 0;
    }

    decoded_bit_num++;
}

static unsigned char
get_bit (unsigned int n)
{
    return current_frame[n];
}

static void
clear_frame (void)
{
    memset(current_frame, 0, sizeof(current_frame));
    decoded_bit_num = 0;
}

int in[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

int main(){

#ifdef DEBUG
            std::cout << " MANCHESTER_GAP_WIDTH     " << MANCHESTER_GAP_WIDTH << std::endl;
            std::cout << " MANCHESTER_GAP_MIN   " << MANCHESTER_GAP_MIN << std::endl;
            std::cout << " MANCHESTER_GAP_MAX   " << MANCHESTER_GAP_MAX << std::endl;
            std::cout << " MANCHESTER_GAP_LONG_WIDTH    " << MANCHESTER_GAP_LONG_WIDTH << std::endl;
            std::cout << " MANCHESTER_GAP_SHORT_WIDTH   " << MANCHESTER_GAP_SHORT_WIDTH << std::endl;
            std::cout << " MANCHESTER_GAP_START_WIDTH_THRESHOLD_MIN     " << MANCHESTER_GAP_START_WIDTH_THRESHOLD_MIN << std::endl;
            std::cout << " MANCHESTER_GAP_START_WIDTH_THRESHOLD_MAX     " << MANCHESTER_GAP_START_WIDTH_THRESHOLD_MAX << std::endl;
            std::cout << " MANCHESTER_GAP_LONG_WIDTH_THRESHOLD      " << MANCHESTER_GAP_LONG_WIDTH_THRESHOLD << std::endl;
            std::cout << " MANCHESTER_GAP_SHORT_WIDTH_THRESHOLD     " << MANCHESTER_GAP_SHORT_WIDTH_THRESHOLD << std::endl;
            std::cout << " MANCHESTER_END " << MANCHESTER_END << std::endl;
#endif

    int decoded_bytes_num = 0;
    unsigned char no_parity_mode = 0;

    for (int i = 0; i < sizeof(in); i++) {
        if (current_state == WAIT_FOR_START){
            if (in[i] > 0) {
                if(count_zero > 0){
                    if(count_zero >= MANCHESTER_GAP_START_WIDTH_THRESHOLD_MIN && count_zero <= MANCHESTER_GAP_START_WIDTH_THRESHOLD_MAX) {
                        if (count_one > MANCHESTER_GAP_START_WIDTH_THRESHOLD_MIN) {
                        // This is the first pulse of a frame (START)
#ifdef DEBUG
                            std::cout << "  THIS IS WHERE OUR DREAM START" << std::endl;
#endif
                            current_state = LAST_BIT_ONE;
                        }
                        count_one = 0;
                    } else if (count_zero < MANCHESTER_GAP_MIN) {
                    // noise
                        count_one += count_zero;
                    // count_zero should be initialized.
                        count_zero = 0;
                    } else {
                        count_zero = 0;
                    }
                }
                count_one++;
            } else {
                count_zero++;
            }
        } else if (current_state == LAST_BIT_ZERO) {
            if (in[i] > 0) {
                if (count_zero > 0) {
                    if (count_zero > MANCHESTER_END && decoded_bit_num > 0) {
#ifdef DEBUG
                    /* END OF FRAME
                    * in case of of noise
                    * (1, 0, END) (0, 0, END)
                    */
                        std::cout << "  END (0, 0, END) " << std::endl;
#endif 
                        current_state = END_OF_FRAME;
                    } else if (count_zero >= MANCHESTER_GAP_MIN && count_zero <= MANCHESTER_GAP_MAX) {
                        if (count_one > MANCHESTER_GAP_LONG_WIDTH_THRESHOLD) {
#ifdef DEBUG
                            std::cout << "      1 (LAST_BIT_ZERO)" << std::endl;
#endif
                        // before_last_bit = 0, last_bit = 1
                            current_state = LAST_BIT_ONE; 
                            set_next_bit(1);
                        } else if (count_one > MANCHESTER_GAP_SHORT_WIDTH_THRESHOLD) {
#ifdef DEBUG
                            std::cout << "      0 (LAST_BIT_ZERO)" << std::endl;
#endif
                            current_state = LAST_BIT_ZERO;
                            set_next_bit(0);
                        // before_last_bit = 0, last bit = 0
                            count_zero = 0;
                        }
                        count_one = 0;
                    } else if (count_zero < MANCHESTER_GAP_MIN) {
                    // Consider the zeros as ones (noise)
                        count_one += count_zero;
                        count_zero = 0;
                    }           }
                    count_one++;
                } else {
                    if (count_zero > MANCHESTER_END && decoded_bit_num > 0) {
                        if (count_one > MANCHESTER_GAP_LONG_WIDTH_THRESHOLD) {
                            set_next_bit(1);
#ifdef DEBUG
                            std::cout << " END (0, 1, END) (last_bit_zero) " << std::endl;
#endif
                            current_state = END_OF_FRAME;
                        } else {
#ifdef DEBUG
                            // in case of 1, 0, end
                            std::cout << "  END (1, 0, END) " << std::endl;
#endif
                            current_state = END_OF_FRAME;  
                        }
                    } else {
                        count_zero++;
                    }
                }
            } else if (current_state == LAST_BIT_ONE) {
                if (count_zero > MANCHESTER_END && decoded_bit_num > 0) {
            /* in case of 1, 1, end
            * in case of 0, 1, end 
            */
#ifdef DEBUG
                    std::cout << "  END (1, 1, END) (0, 1, END) " << std::endl;
#endif
                    current_state = END_OF_FRAME;
                } else { if (in[i] == 0){
                    if (count_one > 0) {
                        if (count_one >= MANCHESTER_GAP_MIN && count_one <= MANCHESTER_GAP_MAX) {
                            if (count_zero > MANCHESTER_GAP_LONG_WIDTH_THRESHOLD) {
#ifdef DEBUG
                                std::cout << "      0 (LAST_BIT_ONE)" << std::endl;
#endif
                                current_state = LAST_BIT_ZERO;
                                set_next_bit(0);
                        // before_last_bit = 1, last_bit = 0
                            } else if (count_zero > MANCHESTER_GAP_SHORT_WIDTH_THRESHOLD) {
#ifdef DEBUG
                                std::cout << "      1 (LAST_BIT_ONE)" << std::endl;
#endif
                                current_state = LAST_BIT_ONE;
                                set_next_bit(1);
                        // before_last_bit = 1, last_bit = 1
                                count_one = 0;
                            }
                            count_zero = 0;
                        } else if (count_one < MANCHESTER_GAP_MIN) {
                    // Consider the ones as zeros (noise)
                            count_zero += count_one;
                            count_one = 0;
                        }
                    }
                    count_zero++;
                } else {
                    count_one++;
                } }
            } 




            if (current_state == END_OF_FRAME) {
                unsigned char tmp_byte = 0;
                unsigned int in_bit = 0, out_bit = 0;
                unsigned char parity_ok;

                if (decoded_bit_num > 0) {
                    /* Assume that the frame is in no parity mode if its length
                     * is valid in no parity mode, and not in Standard mode.
                     * NOTE: For a length of 9x8xN, the last known mode is used.
                     */
                    if ((decoded_bit_num % 72) != 0) {
                        no_parity_mode = (((decoded_bit_num % 9) != 0) && ((decoded_bit_num % 8) == 0));
                    }

                    /* Decode and print the frame */
                    printf("Tag ->");
#ifdef DEBUG
                    printf(" ");
#endif
                    while (in_bit < decoded_bit_num) {
                        int out[] = {};
                        out[decoded_bytes_num] |= get_bit(in_bit) << out_bit;
#ifdef DEBUG
                        printf("%u", get_bit(in_bit));
#endif
                        in_bit++;
                        out_bit++;

                        if (!no_parity_mode && (out_bit == 8 && in_bit < decoded_bit_num)) {
                            /* Check parity if needed */
                            parity_ok = (compute_even_parity(out[decoded_bytes_num]) == !get_bit(in_bit));
#ifdef DEBUG
                            printf("-%u", get_bit(in_bit));
#endif
                            in_bit++;
                        }

                        if (out_bit == 8 || in_bit == decoded_bit_num) {
                            /* Print the byte */
                            if (decoded_bit_num == 7) {
                                /* Short command */
                                printf(" [%02X]", out[decoded_bytes_num]);
                            } else if (out_bit < 8 || (decoded_bit_num == 8 && !no_parity_mode)) {
                                /* Broken */
                                printf(" /%02X\\", out[decoded_bytes_num]);
                            } else if (parity_ok || no_parity_mode) {
                                printf("  %02X ", out[decoded_bytes_num]);
                            } else {
                                printf(" (%02X)", out[decoded_bytes_num]);
                            }
#ifdef DEBUG
                            printf(" ");
#endif

                            decoded_bytes_num++;
                            out_bit = 0;
                        }
                    }

                    if (no_parity_mode) {
                        printf(" (No parity)");
                    }

                    printf("\n");
                    clear_frame();
                }

                current_state = WAIT_FOR_START;
            }
        }
        return 0; 
    }