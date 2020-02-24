#include <iostream>
#include <memory>

#include <allheaders.h> // leptonica main header for image io
#include <tesseract/baseapi.h> // tesseract main header

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

int main(int argc, char *argv[])
{
    if (argc == 1)
        return 1;

    cv::Mat m;

    tesseract::TessBaseAPI tess;

    if (tess.Init("./tessdata", "eng"))
    {
        std::cout << "OCRTesseract: Could not initialize tesseract." << std::endl;
        return 1;
    }

    // setup
    tess.SetPageSegMode(tesseract::PageSegMode::PSM_AUTO);
    tess.SetVariable("save_best_choices", "T");

    // read image
    auto pixs = pixRead(argv[1]);
    if (!pixs)
    {
        std::cout << "Cannot open input file: " << argv[1] << std::endl;
        return 1;
    }

    // recognize
    tess.SetImage(pixs);
    tess.Recognize(0);

    // get result and delete[] returned char* string
    std::cout << std::unique_ptr<char[]>(tess.GetUTF8Text()).get() << std::endl;

    // cleanup
    tess.Clear();
    pixDestroy(&pixs);

    return 0;
}
