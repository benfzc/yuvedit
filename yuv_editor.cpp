#include <fstream>
#include <iostream>
#include <sys/time.h>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;

#define DEFAULT_INPUT_IMAGE_HEIGHT (3040)
#define DEFAULT_INPUT_IMAGE_WIDTH (4056)

#define YUV_420_HEIGHT(mat) (mat.rows * 2 / 3)
#define YUV_420_WIDTH(mat) (mat.cols)

bool load_yuv_420_10b(const char *filename, Mat &yuv, int height, int width)
{
  yuv = Mat(height * 3 / 2, width, CV_16UC1, Scalar(0));

  std::ifstream fin(filename, std::ios::in | std::ios::binary);
  if (fin)
  {
    size_t byte = width * height * sizeof(unsigned short) * 3 / 2;

    fin.read(reinterpret_cast<char *>(yuv.data), byte);
    if (!fin)
    {
      std::cout << "read yuv failure, only " << fin.gcount() << " could be read, expect " << byte << " byte\n";
      goto load_fail;
    }

    fin.close();
    return true;
  }
  else
  {
    std::cout << "open " << filename << " fail\n";
  }
load_fail:
  if (fin)
  {
    fin.close();
  }
  return false;
}

bool show_yuv_420p_16b(Mat &yuv16b, int height, int width, const char *title)
{
  Mat yuv8b;
  yuv16b.convertTo(yuv8b, CV_8UC1, 1 / 256.);

  Mat bgr;
  cvtColor(yuv8b, bgr, COLOR_YUV2BGR_IYUV);
  namedWindow(title, WINDOW_NORMAL);
  resizeWindow(title, 1280, 720);
  imshow(title, bgr);

  return true;
}

bool show_yuv_420sp_16b(Mat &yuv16b, int height, int width, const char *title)
{
  Mat yuv8b;
  yuv16b.convertTo(yuv8b, CV_8UC1, 1 / 256.);

  Mat bgr;
  cvtColor(yuv8b, bgr, COLOR_YUV2BGR_NV12);
  namedWindow(title, WINDOW_NORMAL);
  resizeWindow(title, 1280, 720);
  imshow(title, bgr);

  return true;
}

bool save_yuv_420_10b(const char *filename, Mat &yuv)
{
  std::ofstream fout(filename, std::ios::out | std::ios::binary);
  if (fout)
  {
    fout.write(reinterpret_cast<char *>(yuv.data), YUV_420_HEIGHT(yuv) * YUV_420_WIDTH(yuv) * 3 / 2 * sizeof(unsigned short));
    fout.close();
  }
  else
  {
    std::cout << "save " << filename << " failure\n";
    return false;
  }

  return true;
}

bool zoom_yuv_420p_16b(const Mat &input, const Rect &roi, Mat &output)
{
  size_t height = YUV_420_HEIGHT(input);
  size_t width = YUV_420_WIDTH(input);
  size_t y_byte = height * width * sizeof(unsigned short);
  size_t u_byte = y_byte >> 2;

  Mat y(height, width, CV_16UC1, input.data);
  Mat u(height >> 1, width >> 1, CV_16UC1, input.data + y_byte);
  Mat v(height >> 1, width >> 1, CV_16UC1, input.data + y_byte + u_byte);

  Rect u_roi = Rect(roi.x / 2, roi.y / 2, roi.width / 2, roi.height / 2);

  Mat resized_y, resized_u, resized_v;
  resize(y(roi), resized_y, y.size(), 0, 0, INTER_CUBIC);
  resize(u(u_roi), resized_u, u.size(), 0, 0, INTER_CUBIC);
  resize(v(u_roi), resized_v, v.size(), 0, 0, INTER_CUBIC);

  output = Mat(input.rows, input.cols, CV_16UC1);
  memcpy(output.data, resized_y.data, y_byte);
  memcpy(output.data + y_byte, resized_u.data, u_byte);
  memcpy(output.data + y_byte + u_byte, resized_v.data, u_byte);
  return true;
}

bool zoom_yuv_420sp_16b(const Mat &input, const Rect &roi, Mat &output)
{
  size_t height = YUV_420_HEIGHT(input);
  size_t width = YUV_420_WIDTH(input);
  size_t y_byte = height * width * sizeof(unsigned short);
  size_t uv_byte = y_byte >> 1;

  Mat y(height, width, CV_16UC1, input.data);
  Mat uv(height >> 1, width >> 1, CV_16UC2, input.data + y_byte);

  Rect uv_roi = Rect(roi.x / 2, roi.y / 2, roi.width / 2, roi.height / 2);

  Mat resized_y, resized_uv;
  resize(y(roi), resized_y, y.size(), 0, 0, INTER_CUBIC);
  resize(uv(uv_roi), resized_uv, uv.size(), 0, 0, INTER_CUBIC);

  output = Mat(input.rows, input.cols, CV_16UC1);
  memcpy(output.data, resized_y.data, y_byte);
  memcpy(output.data + y_byte, resized_uv.data, uv_byte);
  return true;
}

typedef struct
{
  bool (*load_yuv)(const char *filename, Mat &yuv, int height, int width);
  bool (*zoom_yuv)(const Mat &input, const Rect &roi, Mat &output);
  bool (*save_yuv)(const char *filename, Mat &yuv);
  bool (*show_yuv)(Mat &yuv16b, int height, int width, const char *title);
} YUV_OPS;

YUV_OPS yuv420p_ops = {
    .load_yuv = load_yuv_420_10b,
    .zoom_yuv = zoom_yuv_420p_16b,
    .save_yuv = save_yuv_420_10b,
    .show_yuv = show_yuv_420p_16b};

YUV_OPS yuv420sp_ops = {
    .load_yuv = load_yuv_420_10b,
    .zoom_yuv = zoom_yuv_420sp_16b,
    .save_yuv = save_yuv_420_10b,
    .show_yuv = show_yuv_420sp_16b};

bool edit_yuv_batch_420_10b(const char *in_filename, const char *out_filename, size_t in_height, size_t in_width, Rect roi, YUV_OPS *ops)
{
  if (roi.x < 0 || roi.y < 0 || roi.width <= 4 || roi.height <= 4 || (roi.x + roi.width) >= in_width || (roi.y + roi.height) >= in_height)
  {
    printf("cannot zoom in to roi (%d, %d, %d, %d)\n", roi.x, roi.y, roi.width, roi.height);
    return false;
  }

  Mat yuv_in;
  if (ops->load_yuv(in_filename, yuv_in, in_height, in_width) == false)
  {
    return false;
  }

  Mat yuv_out;
  ops->zoom_yuv(yuv_in, roi, yuv_out);
  ops->save_yuv(out_filename, yuv_out);
  printf("save image with roi (%d, %d, %d, %d) to file %s\n", roi.x, roi.y, roi.width, roi.height, out_filename);

  return true;
}

bool edit_yuv_420_10b(const char *in_filename, const char *out_filename, size_t in_height, size_t in_width, YUV_OPS *ops)
{
  Mat yuv_in;
  if (ops->load_yuv(in_filename, yuv_in, in_height, in_width) == false)
  {
    return false;
  }

  // convert to 16 bit for easy operation
  yuv_in = yuv_in * (1 << (16 - 10));

  Rect roi(0, 0, in_width, in_height);
  //printf("default roi (%d, %d, %d, %d)\n", roi.x, roi.y, roi.width, roi.height);
  int x, y, w, h;
  Mat yuv_out = yuv_in;
  int key;
  do
  {
    ops->show_yuv(yuv_out, in_height, in_width, "YUV editor");
    key = waitKey(0) & 0xfffff; // fix opencv bug
    switch (key)
    {
    case 'i': /* zoom in */
      x = roi.x + roi.width / 20;
      y = roi.y + roi.height / 20;
      w = roi.width * 18 / 20;
      h = roi.height * 18 / 20;
      break;
    case 'o': /* zoom out */
      x = roi.x - roi.width / 20;
      y = roi.y - roi.height / 20;
      w = roi.width * 22 / 20;
      h = roi.height * 22 / 20;
      break;
    case 'l':
      x = roi.x - roi.width / 20;
      y = roi.y;
      w = roi.width;
      h = roi.height;
      break;
    case 'r':
      x = roi.x + roi.width / 20;
      y = roi.y;
      w = roi.width;
      h = roi.height;
      break;
    case 'u':
      x = roi.x;
      y = roi.y + roi.height / 20;
      w = roi.width;
      h = roi.height;
      break;
    case 'd':
      x = roi.x;
      y = roi.y - roi.height / 20;
      w = roi.width;
      h = roi.height;
      break;
    case 's':
      printf("save image with roi (%d, %d, %d, %d) to file %s\n", x, y, w, h, out_filename);
      yuv_out = yuv_out / (1 << (16 - 10));
      ops->save_yuv(out_filename, yuv_out);
    case 'q': /* quit */
      return true;
    default:
      printf("unknow key: %c (0x%x)\n", key, key);
      continue;
      break;
    }

    if (x < 0 || y < 0 || w <= 4 || h <= 4 || (x + w) >= in_width || (y + h) >= in_height)
    {
      printf("cannot zoom in to roi (%d, %d, %d, %d)\n", x, y, w, h);
      continue;
    }
    //printf("try to zoom in to roi (%d, %d, %d, %d)\n", x, y, w, h);
    roi = Rect(x, y, w, h);
    ops->zoom_yuv(yuv_in, roi, yuv_out);
  } while (key != 'q');

  return true;
}

YUV_OPS *get_yuv_ops(const char *format)
{
  YUV_OPS *ops = NULL;

  if (strcmp(format, "420sp") == 0)
  {
    ops = &yuv420sp_ops;
  }
  else if (strcmp(format, "420p") == 0)
  {
    ops = &yuv420p_ops;
  }

  return ops;
}

void show_help()
{
  const char help[] =
      "\n"
      "yuvedit - edit yuv file\n"
      "\n"
      "1. interactive mode\n"
      "   yuvedit -i {420p|420sp} <input yuv filename> <outout yuv filename> [<image height> <image width>]\n"
      "2. batch mode\n"
      "   yuvedit -b {420p|420sp} <input yuv filename> <outout yuv filename> <roi x> <roi y> <roi width> <roi height> [<image height> <image width>]\n"
      "\n";
  printf("%s\n", help);
}

int main(int argc, char *argv[])
{
  size_t in_height = DEFAULT_INPUT_IMAGE_HEIGHT, in_width = DEFAULT_INPUT_IMAGE_WIDTH;
  if (argc < 4)
  {
    show_help();
  }
  else
  {
    YUV_OPS *ops = get_yuv_ops(argv[2]);
    if (ops == NULL)
    {
      show_help();
      return 0;
    }

    if (strcmp(argv[1], "-i") == 0)
    {
      if (argc == 7)
      {
        in_height = atoi(argv[5]);
        in_width = atoi(argv[6]);
      }
      if (edit_yuv_420_10b(argv[3], argv[4], in_height, in_width, &yuv420sp_ops) == false)
      {
        show_help();
      }
    }
    else if (strcmp(argv[1], "-b") == 0)
    {
      if (argc < 9)
      {
        show_help();
      }
      Rect roi = Rect(atoi(argv[5]), atoi(argv[6]), atoi(argv[7]), atoi(argv[8]));
      if (argc == 11)
      {
        in_height = atoi(argv[9]);
        in_width = atoi(argv[10]);
      }
      if (edit_yuv_batch_420_10b(argv[3], argv[4], in_height, in_width, roi, &yuv420sp_ops) == false)
      {
        show_help();
      }
    }
  }
  return 0;
}