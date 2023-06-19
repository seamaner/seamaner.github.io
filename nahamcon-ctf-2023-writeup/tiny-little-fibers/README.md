# tiny little fibers

```
Author: @JohnHammond#6971

Oh wow, it's another of everyone's favorite. But we like to try and turn the ordinary into extraordinary!
```

use file to get file type:
```
file tiny-little-fibers
tiny-little-fibers: JPEG image data, JFIF standard 1.01, aspect ratio, density 1x1, segment length 16, progressive, precision 8, 2056x1371, components 3
```
从JPEG wiki查到，jpeg编码方式，ffd9是内容结束：
```
xxd tiny-little-fibers | grep ffd9 -A 6
0004c130: cf3f ffd9 6600 6c00 6100 0a00 6700 7b00  .?..f.l.a...g.{.
0004c140: 3200 0a00 3200 6300 3500 0a00 3300 3400  2...2.c.5...3.4.
0004c150: 6300 0a00 3500 6100 6200 0a00 6500 6100  c...5.a.b...e.a.
0004c160: 3800 0a00 3400 6200 6600 0a00 3600 6300  8...4.b.f...6.c.
0004c170: 3100 0a00 3100 3900 3300 0a00 6500 3200  1...1.9.3...e.2.
0004c180: 3600 0a00 3300 6600 3700 0a00 3200 3500  6...3.f.7...2.5.
0004c190: 3900 0a00 6600 7d00 0a00 0a00 0000 0013  9...f.}.........
```
得到flag：
`flag{22c534c5abea84bf6c1193e263f7259f}`
