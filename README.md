# Backup iN Large

Copyright (C) 2015 - Frédéric Lamorce - frederic.lamorce@gmail.com

This application is used to backup huge files on various medium that can be slow.

## History
I often use VM and Truecrypt volumes to store sources, pictures, and all kind of personnal data. Backuping these huge files took a lot of time on external storage, be it external HDD, USB pendrive or even micro SD cards when I wanted to keep my 30GB TC volume with me in my netbook.

I searched for a solution that only copy the blocks that have changed instead of the whole file, I could not find anything for local storage (rsync works well for remote storage), so I wrote an application to do it, named bnl.

## How it works
The first time you do a backup, no choice than to copy the whole file... at the same time bnl creates a small file with one CRC per 8MB block. The next time you backup this same file, bnl will check each source block CRC and compares it with the destination CRC. If it is identical bnl checks the next block; if it is different bnl copies only this 8MB block, update the CRC and goes to the next.

## Example
(read and write speeds are given as an indication and vary from one system to another)

First time you copy a TrueCrypt volume of 30GB size to an SD card at 10MB/s, it takes about 3000 seconds, 50 minutes. bnl creates the destination CRCs file too at the same time.

You modify some files in your TC container, update sources, recompile program, etc, then redo a backup ; bnl will read the source file from your HDD at maybe 60MB/s, calculate a CRC for each 8MB block and compare it with the destination block CRC, bnl finds a diff in 10 blocks so writes 10 * 8MB on the SD card, all this while continuing to read source file to find further diff.

The process takes about 9 minutes (read a 30GB file at 60MB/s) instead of 50. Writing the small 8MB blocks to the SD card are done in background and takes less than a second.

Subsequent backup will mostly take less than 10 minutes too.

### Another scenario
Say you already have a backup of your 30GB file to an SD card, no problem! As the destination already exists, bnl will calculate the CRCs of this file. Reading at full speed on an SD card your computer can read 30MB/s so it will take about 25 minutes to create this CRCs file. bnl will then start updating as above by reading the source file from the HDD and comparing CRCs with destination, the process will take about 9 minutes, for a total of (25+9) 34 minutes instead of 50.

## Option
```
Usage: bnl [-h] [-d] [-b n] <source> <destination>
       -h   : this help screen
       -d   : enable debug log
       -b n : set block size to n MB for creation (default 8)

-h displays the usage
-d shows all kind of debug log on screen, useful if you want to know what is going on.
-b is used to set the size of a block, by default 8MB, which I found is a common "erase block size"
   for SD cards. A 30GB file will use 3750 blocks, the CRCs file will take less than 15KB!
```
