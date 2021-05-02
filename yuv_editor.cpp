#include <fstream>
#include <iostream>
#include <sys/time.h>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;

#define DEFAULT_INPUT_IMAGE_HEIGHT (3040)
#define DEFAULT_INPUT_IMAGE_WIDTH (4056)

#define YUV420P_HEIGHT(mat) (mat.rows * 2 / 3)
#define YUV420P_WIDTH(mat) (mat.cols)

bool load_yuv_420p10b(const char *filename, Mat &yuv, int height, int width)
{
  yuv = Mat(height * 3 / 2, width, CV_16UC1, Scalar(0));

  std::ifstream fin(filename, std::ios::in | std::ios::binary);
  if (fin)
  {
    size_t y_byte = width * height * sizeof(unsigned short);
    size_t u_byte = width * height * sizeof(unsigned short) / 4;

    fin.read(reinterpret_cast<char *>(yuv.data), y_byte);
    if (!fin)
    {
      std::cout << "read y failure, only " << fin.gcount() << " could be read, expect " << y_byte << " byte\n";
      goto load_fail;
    }

    fin.read(reinterpret_cast<char *>(yuv.data + y_byte), u_byte);
    if (!fin)
    {
      std::cout << "read u failure, only " << fin.gcount() << " could be read, expect " << u_byte << " byte\n";
      goto load_fail;
    }

    fin.read(reinterpret_cast<char *>(yuv.data + y_byte + u_byte), u_byte);
    if (!fin)
    {
      std::cout << "read u failure, only " << fin.gcount() << " could be read, expect " << u_byte << " byte\n";
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
  fin.close();
  return false;
}

bool show_yuv_420p16b(Mat &yuv16b, int height, int width, const char *title)
{
  Mat yuv8b;
  yuv16b.convertTo(yuv8b, CV_8UC1, 1 / 256.);

  Mat bgr;
  cvtColor(yuv8b, bgr, COLOR_YUV2BGR_IYUV);
  namedWindow(title, WINDOW_NORMAL);
  resizeWindow(title, 1280, 720);
  imshow(title, bgr);
  //waitKey(0);
  return true;
}

bool save_yuv_420p10b(const char *filename, Mat &yuv)
{
  std::ofstream fout(filename, std::ios::out | std::ios::binary);
  if (fout)
  {
    fout.write(reinterpret_cast<char *>(yuv.data), YUV420P_HEIGHT(yuv) * YUV420P_WIDTH(yuv) * 3 / 2 * sizeof(unsigned short));
    fout.close();
  }
  else
  {
    std::cout << "save " << filename << " failure\n";
    return false;
  }

  return true;
}

bool zoom_yuv_420p16b(const Mat &input, const Rect &roi, Mat &output)
{
  size_t height = YUV420P_HEIGHT(input);
  size_t width = YUV420P_WIDTH(input);
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

bool edit_yuv_batch_420p12b(const char *in_filename, const char *out_filename, size_t in_height, size_t in_width, Rect roi)
{
  if (roi.x < 0 || roi.y < 0 || roi.width <= 4 || roi.height <= 4 || (roi.x + roi.width) >= in_width || (roi.y + roi.height) >= in_height)
  {
    printf("cannot zoom in to roi (%d, %d, %d, %d)\n", roi.x, roi.y, roi.width, roi.height);
    return false;
  }

  Mat yuv_in;
  if (load_yuv_420p10b(in_filename, yuv_in, in_height, in_width) == false)
  {
    return false;
  }

  Mat yuv_out;
  zoom_yuv_420p16b(yuv_in, roi, yuv_out);
  save_yuv_420p10b(out_filename, yuv_out);
  printf("save image with roi (%d, %d, %d, %d) to file %s\n", roi.x, roi.y, roi.width, roi.height, out_filename);

  return true;
}

bool edit_yuv_420p12b(const char *in_filename, const char *out_filename, size_t in_height, size_t in_width)
{
  Mat yuv_in;
  if (load_yuv_420p10b(in_filename, yuv_in, in_height, in_width) == false)
  {
    return false;
  }

  // convert to 16 bit for easy operation
  yuv_in = yuv_in * (1 << (16 - 10));

  Rect roi(0, 0, in_width, in_height);
  //printf("default roi (%d, %d, %d, %d)\n", roi.x, roi.y, roi.width, roi.height);
  int x, y, w, h;
  Mat yuv_out = yuv_in;
  int op;
  do
  {
    show_yuv_420p16b(yuv_out, in_height, in_width, "YUV editor");
    op = waitKey(0);
    //printf("intput %c\n", op);
    switch (op)
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
      save_yuv_420p10b(out_filename, yuv_out);
    case 'q': /* quit */
      return true;
    }

    if (x < 0 || y < 0 || w <= 4 || h <= 4 || (x + w) >= in_width || (y + h) >= in_height)
    {
      printf("cannot zoom in to roi (%d, %d, %d, %d)\n", x, y, w, h);
      continue;
    }
    //printf("try to zoom in to roi (%d, %d, %d, %d)\n", x, y, w, h);
    roi = Rect(x, y, w, h);
    zoom_yuv_420p16b(yuv_in, roi, yuv_out);
  } while (op != 'q');

  return true;
}

void show_help()
{
  const char help[] =
      "\n"
      "yuvedit - edit yuv file\n"
      "\n"
      "1. interactive mode\n"
      "   yuvedit -i <input yuv filename> <outout yuv filename> [<image height> <image width>]\n"
      "2. batch mode\n"
      "   yuvedit -b <input yuv filename> <outout yuv filename> <roi x> <roi y> <roi width> <roi height> [<image height> <image width>]\n"
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
    if (strcmp(argv[1], "-i") == 0)
    {
      if (argc == 6)
      {
        in_height = atoi(argv[4]);
        in_width = atoi(argv[5]);
      }
      if (edit_yuv_420p12b(argv[2], argv[3], in_height, in_width) == false)
      {
        show_help();
      }
    }
    else if (strcmp(argv[1], "-b") == 0)
    {
      if (argc < 8)
      {
        show_help();
      }
      Rect roi = Rect(atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), atoi(argv[7]));
      if (argc == 10)
      {
        in_height = atoi(argv[8]);
        in_width = atoi(argv[9]);
      }
      if (edit_yuv_batch_420p12b(argv[2], argv[3], in_height, in_width, roi) == false)
      {
        show_help();
      }
    }
  }
  return 0;
}