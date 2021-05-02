# yuvedit

yuvedit is a simple tool for editing YUV image. Currently it only support zoom (crop+scale) planar YUV420 format image.


## Usage
1. interactive mode
   ```
   yuvedit -i <input yuv filename> <outout yuv filename> [<image height> <image width>]
   ```
2. batch mode
   ```
   yuvedit -b <input yuv filename> <outout yuv filename> <roi x> <roi y> <roi width> <roi height> [<image height> <image width>]
   ```

