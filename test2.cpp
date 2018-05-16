#include <string.h>
#include <stdio.h>
#include <iostream>
using namespace std;

#define MANCHESTER_GAP                             4 // us 
#define MANCHESTER_MEAN                            2
#define MANCHESTER_START_MIN                        4
#define MANCHESTER_START_MAX                        5

#define DEBUG

enum miller_state {
    WAIT_FOR_START,
    PRE_DECODE,
    DECODE,
    END_OF_FRAME,
};

static enum miller_state current_state = WAIT_FOR_START;
static unsigned int count_one = 0;
static unsigned int count_zero = 0;
static unsigned int decoded_bit_num = 0;
static unsigned char current_frame[1000] = { 0 };
static unsigned int pre_decoded_bit_num = 0;
static unsigned char tmp[1000] = { 0 };

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

static unsigned char
get_bit_tmp (unsigned int n)
{
    return tmp[n];
}

static unsigned char
last_two_bit_zero (void)
{
    if ( pre_decoded_bit_num > 2) {
        if (!get_bit_tmp(pre_decoded_bit_num - 1) && !get_bit_tmp(pre_decoded_bit_num - 2)){
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

static unsigned char
last_two_bit_one (void)
{
    if (pre_decoded_bit_num > 2) {
        if (get_bit_tmp(pre_decoded_bit_num - 1) && get_bit_tmp(pre_decoded_bit_num -2)) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

static void 
set_tmp (unsigned char bit)
{
    if (bit) {
        tmp[pre_decoded_bit_num] = 1;
    } else {
        tmp[pre_decoded_bit_num] = 0;
    }

    pre_decoded_bit_num++;
}

static void
clear_tmp (void)
{
    memset(tmp, 0, sizeof(tmp));
    pre_decoded_bit_num = 0;
}

static void
remove_last_bit (void)
{
    decoded_bit_num--;
    current_frame[decoded_bit_num] = 0;
}

static void 
remove_last_bit_tmp (void)
{
    pre_decoded_bit_num--;
    tmp[pre_decoded_bit_num] = 0;
}

static void 
remove_last_two_bit_tmp (void)
{
    pre_decoded_bit_num--;
    tmp[pre_decoded_bit_num] = 0;
    pre_decoded_bit_num--;
    tmp[pre_decoded_bit_num] = 0;
}


int in[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0};

int main(){

#ifdef DEBUG
    std::cout << " MANCHESTER_GAP = " << MANCHESTER_GAP << std::endl;
    std::cout << " MANCHESTER_START_MIN = " << MANCHESTER_START_MIN << std::endl;
    std::cout << " MANCHESTER_START_MAX = " << MANCHESTER_START_MAX << std::endl;
    std::cout << " MANCHESTER_MEAN = " << MANCHESTER_MEAN << std::endl;
#endif

    int decoded_bytes_num = 0;
    unsigned char no_parity_mode = 0;
    int sum = 0;
    int start_sum = 0;
    int start_sum_next = 0;

    for (int i = 0; i < sizeof(in); i++) {
        if (current_state == WAIT_FOR_START ) {
            for (int j = 0; j < MANCHESTER_GAP * 6; j++) {
                start_sum += in[(i+j)];
            }
            for (int x = 0; x < MANCHESTER_GAP; x++) {
                start_sum_next += in[(i+x)];
            }
            if ((start_sum >= MANCHESTER_START_MIN * 3 && start_sum <= MANCHESTER_START_MAX * 3) && (start_sum_next >= MANCHESTER_START_MIN && start_sum_next <= MANCHESTER_START_MAX)) {
                i = i + (MANCHESTER_GAP * 2) - 1;   
                current_state = PRE_DECODE;
            } 
            start_sum_next = 0;
            start_sum = 0;           
        } else if (current_state == PRE_DECODE) {
            for (int j = 0; j < MANCHESTER_GAP; j++) {
                sum += in[(i+j)];
            }
#ifdef DEBUG
            std::cout << " sum = " << sum << std::endl;
#endif
            if (sum >= MANCHESTER_MEAN && sum < MANCHESTER_START_MAX) {
                if (last_two_bit_one()) {
                        // There is something wrong. 
#ifdef DEBUG
                    std::cout << " 1, 1, 1 wrong" << std::endl;
#endif 
                    clear_frame();
                    clear_tmp();
                    current_state = WAIT_FOR_START;
                } else {
#ifdef DEBUG
                    std::cout << " set_tmp(1) " << std::endl;
#endif
                    set_tmp(1);
                }
            } else {
                if (last_two_bit_zero()) {
                    if ((pre_decoded_bit_num % 2) == 0) {
#ifdef DEBUG
                        std::cout << " even, remove last two bit" << std::endl;
#endif
                        remove_last_two_bit_tmp();
                        current_state = DECODE;
                    } else {
#ifdef DEBUG
                        std::cout << " odd, remove last bit" << std::endl;
#endif
                        remove_last_bit_tmp();
                        current_state = DECODE;
                    }
                } else {
#ifdef DEBUG
                    std::cout << " set_tmp(0) " << std::endl;
#endif
                    set_tmp(0);
                }
            }
            sum = 0;
            i += (MANCHESTER_GAP - 1);    
        } else if (current_state == DECODE) {
            if (tmp[0] != tmp[1]) {
                for (int j = 0; j < pre_decoded_bit_num; j += 2) {
                    if (tmp[j]) {

#ifdef DEBUG
                        std::cout << " set next bit 1" << std::endl;
#endif
                        set_next_bit(1);
                    } else {
#ifdef DEBUG
                        std::cout << " set next bit 0" << std::endl;
#endif
                        set_next_bit(0);

                    }
                }
            }
                clear_tmp();
                current_state = END_OF_FRAME;       
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