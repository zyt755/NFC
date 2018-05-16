unsigned int x = 0;

for(int i = 0; i < noutput_items; i++) {
	if(in[i] <= 0.105 && in[i] >= 0.06) {
		if(x > 6500000 && x < 7000000) {
			out[i] = in[i] + 0.016;
		} else {
			out[i] = in[i];
		}

	} else {
		out[i] = -1;
	}
x++;
}