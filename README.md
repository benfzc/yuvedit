# yuvedit

yuvedit is a simple tool for editing YUV image. Currently it only support zoom (crop+scale) YUV image with YUV420P and YUV420SP format.


## Usage
1. interactive mode
   ```
   yuvedit -i {420p|420sp} <input yuv filename> <outout yuv filename> [<image height> <image width>]
   ```
2. batch mode
   ```
   yuvedit -b {420p|420sp} <input yuv filename> <outout yuv filename> <roi x> <roi y> <roi width> <roi height> [<image height> <image width>]
   ```

