#!/usr/bin/env python

import sys

from os import makedirs
from os.path import exists
from os import listdir
from os.path import isfile, join
benchmarks = [join(sys.argv[1], f) for f in listdir(sys.argv[1]) if not f.startswith(".")]

class Page_Info:
    def __init__(self, _id, _fti, _num_accesses):
        self.page_id = _id
        self.fti = _fti
        self.num_accesses = _num_accesses

def pageAccess(profiling, inference, oFile):

    prof_pages = []
    prof_ftis = []

    infe_pages = []

    with open(profiling) as fp:
        lines = fp.readlines()

        for line in lines:
            prof_pages.append(line.split(",")[0])
            prof_ftis.append(line.split(",")[1])

    with open(inference) as fp:
        lines = fp.readlines()

        for line in lines:
            page_id=line.split(",")[0]
            fti=line.split(",")[1]
            accesses=line.split(",")[2]
            
            infe_pages.append(Page_Info(page_id,fti,accesses))

    w = 0
    v = 0
    u = 0

    for page_info in infe_pages:
        if page_info.fti not in prof_ftis:
            w = w + 1
            v = v + int(page_info.num_accesses)
        u = u + int(page_info.num_accesses)

    print >> oFile, w, v, u

for b in benchmarks:
    if "bwaves" not in b:
        continue

    if not exists("page_info"):
        makedirs("page_info")
    output = open(join("page_info", b.split("/")[-1]), "w")

    profiling_file = join(b, "10M.csv")
    for i in range(1,101):
        inference_file = join(b, str(i*100)+"M.csv")
        pageAccess(profiling_file, inference_file, output)

    output.close()
