struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  struct spinlock guard;
  uint refcnt;
  struct buf *next;
  uint  last_used;
  uint  bucketno;
  uchar data[BSIZE];
};

