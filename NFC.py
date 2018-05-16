#!/usr/bin/env python

import sys

with open('nfc_case.txt') as f:
	pcd = f.readlines()
with open('nfc_case_tag.txt') as f:
	picc = f.readlines()
for i in range(len(pcd)):
	pcd[i] = pcd[i].strip('\n')
for i in range(len(picc)):
	picc[i] = picc[i].strip('\n')

find = '93 20 '
result = [i-1 for i,v in enumerate(pcd) if v == find]

picc_ans = sys.argv[1]
picc_len = int(len(picc)/int(picc_ans))
a = 0
for i in result:
	for j in range(picc_len):
		print('reader -> ' + pcd[(i+j)])
		print('  tag  -> ' + picc[(j+a)])
	a += picc_len