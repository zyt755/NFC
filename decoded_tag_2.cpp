/* -*- c++ -*- */
/* 
 * Copyright 2017 Jean-Christophe Rona <jc@rona.fr>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "tag_decoder_impl.h"
#include "stdio.h"

#define MANCHESTER_GAP                              4.5 // us 
#define MANCHESTER_GAP_WIDTH 						((d_sample_rate/1000000) * MANCHESTER_GAP)
#define MANCHESTER_MEAN                             2.5
#define MANCHESTER_MEAN_WIDTH						((d_sample_rate/1000000) * MANCHESTER_MEAN)
#define MANCHESTER_START_MIN                        4
#define MANCHESTER_START_MAX                        5
#define MANCHESTER_START_MIN_WIDTH					((d_sample_rate/1000000) * MANCHESTER_START_MIN)
#define MANCHESTER_START_MAX_WIDTH					((d_sample_rate/1000000) * MANCHESTER_START_MAX)


/* Enable this to display the decoding process */
#define DEBUG


namespace gr {
	namespace nfc {

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

		tag_decoder::sptr
		tag_decoder::make(double sample_rate)
		{
			return gnuradio::get_initial_sptr
			(new tag_decoder_impl(sample_rate));
		}

    /*
     * The private constructor
     */
		tag_decoder_impl::tag_decoder_impl(double sample_rate)
		: gr::block("tag_decoder",
			gr::io_signature::make(1, 1, sizeof(char)),
			gr::io_signature::make(1, 1, sizeof(char))),
		d_sample_rate(sample_rate)
		{
#ifdef DEBUG
			std::cout << " MANCHESTER_GAP = " << MANCHESTER_GAP << std::endl;
			std::cout << " MANCHESTER_GAP_WIDTH = " << MANCHESTER_GAP_WIDTH << std::endl;
			std::cout << " MANCHESTER_START_MIN = " << MANCHESTER_START_MIN << std::endl;
			std::cout << " MANCHESTER_START_MIN_WIDTH = " << MANCHESTER_START_MIN_WIDTH << std::endl;
			std::cout << " MANCHESTER_START_MAX = " << MANCHESTER_START_MAX << std::endl;
			std::cout << " MANCHESTER_START_MAX_WIDTH = " << MANCHESTER_START_MAX_WIDTH << std::endl;
			std::cout << " MANCHESTER_MEAN = " << MANCHESTER_MEAN << std::endl;
			std::cout << " MANCHESTER_MEAN_WIDTH = " << MANCHESTER_MEAN_WIDTH << std::endl;
#endif
		}

    /*
     * Our virtual destructor.
     */
		tag_decoder_impl::~tag_decoder_impl()
		{
		}

		void
		tag_decoder_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
		{
			ninput_items_required[0] = (noutput_items * 8 * d_sample_rate)/1000000;
		}

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

		static unsigned char
		get_last_bit (void)
		{
			return get_bit(decoded_bit_num - 1);
		}

		static void
		remove_last_bit (void)
		{
			decoded_bit_num--;
			current_frame[decoded_bit_num] = 0;
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

		static int
		get_last_tmp (void)
		{
			if ( pre_decoded_bit_num > 1) {
				return int(get_bit_tmp(pre_decoded_bit_num - 1));
			}
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

		int
		tag_decoder_impl::general_work (int noutput_items,
			gr_vector_int &ninput_items,
			gr_vector_const_void_star &input_items,
			gr_vector_void_star &output_items)
		{
			const unsigned char *in = (const unsigned char *) input_items[0];
			unsigned char *out = (unsigned char *) output_items[0];
			int decoded_bytes_num = 0;
			unsigned char no_parity_mode = 0;
			int sum = 0;
			int start_sum = 0;
			int start_sum_next = 0;
			int queue_start = 1;

			for (int i = 0; i < ninput_items[0]; i++) {
				if (current_state == WAIT_FOR_START ) {
					if (queue_start == 1) {
						for (int j = 0; j < MANCHESTER_GAP_WIDTH * 14; j++) {
							start_sum += in[(i+j)];
							if (j < MANCHESTER_GAP_WIDTH) {
								start_sum_next += in[(i+j)];
							}
						}
						queue_start = 0;
					} else {
						start_sum = start_sum - in[(i-1)] + in[(i + int(MANCHESTER_GAP_WIDTH) * 14)];
						start_sum_next = start_sum_next - in[(i-1)] + in[(i + int(MANCHESTER_GAP_WIDTH))];
					}

					if ((start_sum >= MANCHESTER_START_MIN_WIDTH * 7 && start_sum <= MANCHESTER_START_MAX_WIDTH * 7) && (start_sum_next >= MANCHESTER_START_MIN_WIDTH && start_sum_next <= MANCHESTER_START_MAX_WIDTH)) {
						if ( int(in[i]) == 1) {
#ifdef DEBUG
							std::cout << " start_sum = " << start_sum << std::endl;
							std::cout << " start_sum_next = " << start_sum_next << std::endl;
							for (int j = 0; j < MANCHESTER_GAP_WIDTH * 14; j++) {
								std::cout << int(in[(i+j)]);
							}
#endif
							i = i + (MANCHESTER_GAP_WIDTH * 2) - 1;   
							current_state = PRE_DECODE;
						}
					} 
				} else if (current_state == PRE_DECODE) {
					for (int j = 0; j < MANCHESTER_GAP_WIDTH; j++) {
						sum += in[(i+j)];
#ifdef DEBUG
						std::cout << int(in[(i+j)]);
#endif
					}
#ifdef DEBUG
					std::cout << " " << std::endl;
					std::cout << " sum = " << sum << std::endl;
#endif
					if (sum >= MANCHESTER_MEAN_WIDTH && sum < MANCHESTER_START_MAX_WIDTH) {
						if (last_two_bit_one()) {
                        // There is something wrong. 
#ifdef DEBUG
							std::cout << " 1, 1, 1 wrong" << std::endl;
#endif 
							clear_frame();
							clear_tmp();
							current_state = WAIT_FOR_START;
							queue_start = 1;
							i = i + (MANCHESTER_GAP_WIDTH - 1);
						} else {
#ifdef DEBUG
							std::cout << " set_tmp(1) " << std::endl;
#endif
							if (sum < MANCHESTER_GAP_WIDTH) {
								for (int x = 0; x < (MANCHESTER_GAP_WIDTH + 1 - sum); x++) {
									if ( int(in[(i + x + int(MANCHESTER_GAP_WIDTH))]) == 0) {
										i = i + x + MANCHESTER_GAP_WIDTH;
										break;
									} else if (x = (MANCHESTER_GAP_WIDTH - sum)) {
										i = i + x + MANCHESTER_GAP_WIDTH;
										break;
									}
								}
							} else {
								i = i + (MANCHESTER_GAP_WIDTH - 1);
							}
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
							i = i + (MANCHESTER_GAP_WIDTH - 1);
						} else {
#ifdef DEBUG
							std::cout << " set_tmp(0) " << std::endl;
#endif
							if (sum > 3) {
								for (int x = 0; x < (sum+1); x++) {
									if ( int(in[(i + x + int(MANCHESTER_GAP_WIDTH))]) == 1) {
										i = i + x + MANCHESTER_GAP_WIDTH;
										break;
									} else if (x = sum) {
										i = i + x + MANCHESTER_GAP_WIDTH;
										break;
									}
								}
							} else{
								i = i + (MANCHESTER_GAP_WIDTH - 1);
							}
							set_tmp(0);
						}
					}
					start_sum_next = 0;
					start_sum = 0; 
					sum = 0;   
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
						current_state = END_OF_FRAME;
					} else {
#ifdef DEBUG
						std::cout << " tmp[0] = tmp[1], wrong" << std::endl;
#endif
						current_state = WAIT_FOR_START;
						queue_start = 1;
					}
					clear_tmp();

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
						FILE *fp = fopen("/home/zyt755/nfc/test_case/picc.txt", "at+");
						while (in_bit < decoded_bit_num) {
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
									fprintf(fp, " [%02X]", out[decoded_bytes_num]);
								} else if (out_bit < 8 || (decoded_bit_num == 8 && !no_parity_mode)) {
                                /* Broken */
									printf(" /%02X\\", out[decoded_bytes_num]);
									fprintf(fp, " /%02X\\", out[decoded_bytes_num]);
								} else if (parity_ok || no_parity_mode) {
									printf("  %02X ", out[decoded_bytes_num]);
									fprintf(fp, "  %02X ", out[decoded_bytes_num]);
								} else {
									printf(" (%02X)", out[decoded_bytes_num]);
									fprintf(fp, " (%02X)", out[decoded_bytes_num]);
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
							fprintf(fp, " (No parity)");
						}

						fprintf(fp, "\n");
						fclose(fp);
						printf("\n");
						clear_frame();
					}

					current_state = WAIT_FOR_START;
					queue_start = 1;
				}
			}

			consume_each (ninput_items[0]);

        // Tell runtime system how many output items we produced.
			return decoded_bytes_num;
		}
  } /* namespace nfc */
} /* namespace gr */