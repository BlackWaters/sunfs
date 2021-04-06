# sunfs

> My master degree thesis, first attempt in file system.

sunfs -- a ram based file system in linux(x86-64 only).

Actually, sunfs is a reimplement of simfs[1]. I use an easier way to implement file page and file page table. Not standered, but easy to implement.

After I finished sunfs, I did some tests like cache & tlb misses, read & write throughput with other file system(ramfs, tmpfs, pmfs[2] ... ) . Here is some tests:

randread\
![randread](https://github.com/BlackWaters/sunfs/blob/main/results/randread.png  "randread")

randwrite\
![randwrite](https://github.com/BlackWaters/sunfs/blob/main/results/randwrite.png "randwrite")

seqread\
![seqread](https://github.com/BlackWaters/sunfs/blob/main/results/seqread.png "seqread")

seqwrite\
![seqwrite](https://github.com/BlackWaters/sunfs/blob/main/results/seqwrite.png "seqwrite")

Ruinan \
Bye bye UESTC.\
2021.04.06



## Refrence 

[1] Edwin H.-M. Sha, Xianzhang Chen, Qingfeng Zhuge, Liang Shi, Weiwen Jiang.A New Design 
of In-Memory File System Based on File Virtual Address Framework[J]. IEEE 
TRANSACTIONS ON COMPUTERS,2016,65(10):2959-2972

[2]Subramanya R. Dulloor,Sanjay Kumar,Anil Keshavamurthy,Philip Lantz,Dheeraj Reddy,Rajesh 
Sankaran,Jeff Jackson. System software for persistent memory[P]. Computer Systems,2014:1-15

